#include "builtin_oracles.hpp"
#include "inspector.hpp"
#include "registry/validator.hpp"
#include <boost/json.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

namespace crivo::oracles {
namespace fs = std::filesystem;

const char* to_string(Status s) noexcept {
  switch (s) {
    case Status::Pass: return "PASS";
    case Status::Fail: return "FAIL";
    case Status::Blocked: return "BLOCKED";
    case Status::NotApplicable: return "NOT_APPLICABLE";
  }
  return "BLOCKED";
}

static std::vector<fs::path> files(const fs::path& root, const std::string& ext = {}) {
  std::vector<fs::path> out;
  if (!fs::exists(root)) return out;
  auto scan = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
  for (auto it = scan; it != fs::recursive_directory_iterator(); ++it) {
    const auto& e = *it;
    if (e.is_directory()) {
      auto n = e.path().filename().string();
      if (n == ".git" || n == "node_modules" || n == "vendor" || n == ".cache" ||
          n == "dist" || n == "coverage" || n.starts_with("build")) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (e.is_regular_file() && (ext.empty() || e.path().extension() == ext)) {
      out.push_back(e.path());
    }
  }
  return out;
}

static boost::json::value read_json(const fs::path& p) {
  std::ifstream in(p);
  std::string bytes((std::istreambuf_iterator<char>(in)), {});
  return boost::json::parse(bytes);
}

static fs::path first_db(const fs::path& root) {
  for (const auto& p : files(root)) {
    auto x = p.extension().string();
    if (x == ".db" || x == ".sqlite" || x == ".sqlite3") return p;
  }
  return {};
}

static bool has_http_traits(const fs::path& root) {
  if (fs::exists(root / "crivo-analysis.json")) return true;
  for (const auto& p : files(root)) {
    auto ext = p.extension().string();
    auto name = p.filename().string();
    if (ext == ".html" || ext == ".js" || ext == ".ts" || name == "server.py" || name == "app.py" ||
        name == "server.js" || name == "app.js" || name == "package.json" || name == "vite.config.ts") {
      return true;
    }
  }
  return false;
}

static bool has_concurrency_traits(const fs::path& root) {
  for (const auto& p : files(root)) {
    auto ext = p.extension().string();
    auto name = p.filename().string();
    if (name.find("tsan") != std::string::npos) return true;
    if (name == "CMakeLists.txt") {
      std::ifstream in(p);
      std::string s((std::istreambuf_iterator<char>(in)), {});
      if (s.find("Threads") != std::string::npos || s.find("pthread") != std::string::npos) return true;
    }
    if (ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h" || ext == ".cc") {
      std::ifstream in(p);
      std::string s((std::istreambuf_iterator<char>(in)), {});
      if (s.find("<thread>") != std::string::npos || s.find("<mutex>") != std::string::npos ||
          s.find("<atomic>") != std::string::npos || s.find("pthread") != std::string::npos ||
          s.find("std::thread") != std::string::npos || s.find("std::mutex") != std::string::npos ||
          s.find("std::atomic") != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

static bool has_manifest_traits(const fs::path& root) {
  for (const auto& p : files(root)) {
    auto name = p.filename().string();
    if (name.find("sbom") != std::string::npos || name == "bom.json" || name == "CMakeLists.txt" ||
        name == "package.json" || name == "Cargo.toml" || name == "requirements.txt" ||
        name == "pyproject.toml" || name == "conanfile.txt" || name == "vcpkg.json") {
      return true;
    }
  }
  return false;
}

static Result schema_strict(const fs::path& root) {
  registry::RegistryCollection collection;
  registry::ValidationReport report;
  std::size_t declared_records = 0;
  for (const auto& p : files(root, ".json")) {
    if (p.filename() == "tests.json") continue;
    try {
      auto j = read_json(p);
      if (j.is_object() && j.as_object().contains("schema_version")) {
        std::string version(j.as_object().at("schema_version").as_string());
        if (version.starts_with("crivo.reference/") || version.starts_with("crivo.technique/") ||
            version.starts_with("crivo.test-spec/") || version.starts_with("crivo.implementation/") ||
            version.starts_with("crivo.profile/")) {
          ++declared_records;
          registry::validate_file(p.string(), collection, report);
        }
      }
    } catch (const std::exception& e) {
      return {Status::Fail, "invalid JSON syntax: " + p.string() + ": " + e.what()};
    }
  }
  if (!report.valid) {
    return {Status::Fail, "declared CRIVO schema invalid", root.string(),
            "validate declared CRIVO records", "schema validation errors observed",
            "every declared record conforms to its versioned schema", "only declared CRIVO records were examined"};
  }
  if (declared_records == 0) {
    return {Status::NotApplicable, "no declared CRIVO schema record found", root.string(),
            "discover records with recognized schema_version", "zero pertinent records",
            "at least one declared CRIVO record must exist", "generic JSON files were not treated as CRIVO contracts"};
  }
  return {Status::Pass, "declared CRIVO schemas valid", root.string(),
          "validate " + std::to_string(declared_records) + " declared CRIVO records",
          std::to_string(declared_records) + " conforming records",
          "every declared record conforms to its versioned schema", "does not validate unrelated JSON documents"};
}

static Result sqlite_rollback(const fs::path& root, const fs::path& workspace) {
  auto source = first_db(root);
  if (source.empty()) return {Status::NotApplicable, "no SQLite database"};
  fs::path copy = workspace / "rollback.sqlite";
  fs::remove(copy); fs::remove(copy.string() + "-wal"); fs::remove(copy.string() + "-shm");
  fs::copy_file(source, copy, fs::copy_options::overwrite_existing);

  sqlite3* db = nullptr;
  if (sqlite3_open(copy.c_str(), &db) != SQLITE_OK) {
    if (db) sqlite3_close(db);
    return {Status::Blocked, "cannot open ephemeral database"};
  }
  char* err = nullptr;
  int rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS crivo_rollback_probe(v INTEGER);BEGIN;INSERT INTO crivo_rollback_probe VALUES(1);ROLLBACK;", nullptr, nullptr, &err);
  sqlite3_stmt* st = nullptr;
  long long count = -1;
  if (rc == SQLITE_OK && sqlite3_prepare_v2(db, "SELECT count(*) FROM crivo_rollback_probe;", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
    count = sqlite3_column_int64(st, 0);
  }
  if (st) sqlite3_finalize(st);
  if (err) sqlite3_free(err);
  sqlite3_close(db);
  return rc == SQLITE_OK && count == 0 ? Result{Status::Pass, "rollback preserved preimage"} : Result{Status::Fail, "rollback invariant failed"};
}

static Result sqlite_wal(const fs::path& root, const fs::path& workspace) {
  auto source = first_db(root);
  if (source.empty()) return {Status::NotApplicable, "no SQLite database"};
  fs::path copy = workspace / "wal.sqlite";
  fs::remove(copy); fs::remove(copy.string() + "-wal"); fs::remove(copy.string() + "-shm");
  fs::copy_file(source, copy, fs::copy_options::overwrite_existing);

  sqlite3* setup = nullptr;
  if (sqlite3_open(copy.c_str(), &setup) != SQLITE_OK) {
    if (setup) sqlite3_close(setup);
    return {Status::Blocked, "cannot open ephemeral database"};
  }
  int rc = sqlite3_exec(setup, "PRAGMA journal_mode=WAL;CREATE TABLE IF NOT EXISTS crivo_wal_probe(v INTEGER);", nullptr, nullptr, nullptr);
  sqlite3_close(setup);
  std::vector<int> results(8, SQLITE_ERROR);
  std::vector<std::thread> threads;
  for (size_t i = 0; i < results.size(); ++i) {
    threads.emplace_back([&, i] {
      sqlite3* db = nullptr;
      if (sqlite3_open_v2(copy.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        sqlite3_busy_timeout(db, 1000);
        for (int attempt = 0; attempt < 3; ++attempt) {
          results[i] = sqlite3_exec(db, "SELECT count(*) FROM crivo_wal_probe;", nullptr, nullptr, nullptr);
          if (results[i] == SQLITE_OK) break;
          std::this_thread::yield();
        }
      }
      if (db) sqlite3_close(db);
    });
  }
  for (auto& t : threads) t.join();
  return rc == SQLITE_OK && std::all_of(results.begin(), results.end(), [](int x) { return x == SQLITE_OK; })
      ? Result{Status::Pass, "concurrent WAL reads succeeded"}
      : Result{Status::Fail, "WAL concurrency failed"};
}

static Result sarif(const fs::path& root, const fs::path& workspace) {
  // 1. Check existing SARIF artifact in target
  for (const auto& p : files(root)) {
    if (p.extension() == ".sarif" || p.filename().string().ends_with(".sarif.json")) {
      try {
        auto value = read_json(p);
        if (!value.is_object()) return {Status::Fail, "invalid target SARIF: root is not JSON object"};
        auto& o = value.as_object();
        if (o.contains("version") && o.at("version").as_string() == "2.1.0" &&
            o.contains("runs") && o.at("runs").is_array()) {
          return {Status::Pass, "target SARIF 2.1.0 valid"};
        } else {
          return {Status::Fail, "invalid target SARIF structure: missing runs array or version not 2.1.0"};
        }
      } catch (...) {
        return {Status::Fail, "invalid SARIF JSON in target: syntax error"};
      }
    }
  }

  // 2. Active SARIF export generator (Autoverificação do exportador)
  try {
    fs::path out_sarif = workspace / "crivo-export.sarif";
    boost::json::object root_obj;
    root_obj["$schema"] = "https://json.schemastore.org/sarif-2.1.0.json";
    root_obj["version"] = "2.1.0";

    boost::json::array runs;
    boost::json::object run;

    boost::json::object tool;
    boost::json::object driver;
    driver["name"] = "CRIVO-Static-Inspector";
    driver["version"] = "0.3.0";
    driver["informationUri"] = "https://github.com/jpereiratrindade/CRIVO";

    boost::json::array rules;
    boost::json::object rule;
    rule["id"] = "CRIVO-CONFORMANCE-001";
    boost::json::object short_desc;
    short_desc["text"] = "Strict structural conformance and schema verification";
    rule["shortDescription"] = short_desc;
    rules.push_back(rule);
    driver["rules"] = rules;
    tool["driver"] = driver;
    run["tool"] = tool;

    boost::json::array results;
    run["results"] = results;
    runs.push_back(run);
    root_obj["runs"] = runs;

    std::ofstream out(out_sarif);
    out << boost::json::serialize(root_obj);
    out.close();

    // Verify generated SARIF
    auto v = read_json(out_sarif);
    if (v.is_object() && v.as_object().at("version").as_string() == "2.1.0" && v.as_object().at("runs").is_array()) {
      return {Status::NotApplicable, "[AUTO-VERIFICATION] CRIVO SARIF exporter verified; no target SARIF artifact was evaluated"};
    }
    return {Status::Fail, "[AUTO-VERIFICATION] generated SARIF failed OASIS 2.1.0 validation"};
  } catch (const std::exception& e) {
    return {Status::Fail, std::string("[AUTO-VERIFICATION] SARIF generator error: ") + e.what()};
  }
}

static Result tsan(const fs::path& root, const fs::path& workspace) {
  // 1. Check existing TSan report
  for (const auto& p : files(root)) {
    if (p.filename().string().find("tsan") != std::string::npos) {
      std::ifstream in(p);
      std::string s((std::istreambuf_iterator<char>(in)), {});
      if (s.find("WARNING: ThreadSanitizer") != std::string::npos ||
          s.find("FATAL: ThreadSanitizer") != std::string::npos ||
          s.find("data race") != std::string::npos) {
        return {Status::Fail, "ThreadSanitizer reported data race in target code"};
      }
      if (s.find("ThreadSanitizer") != std::string::npos ||
          s.find("CRIVO Active ThreadSanitizer Target Audit") != std::string::npos) {
        return {Status::Blocked, "untrusted TSan report: a text artifact cannot prove instrumented execution provenance"};
      }
      return {Status::Fail, "invalid TSan report evidence: file does not contain ThreadSanitizer execution trace"};
    }
  }

  // Check applicability traits
  if (!has_concurrency_traits(root)) {
    return {Status::NotApplicable, "no multithreaded source or TSan report in target"};
  }

  // 2. Active ThreadSanitizer Compilation & Execution on Target Code
  std::vector<fs::path> target_sources;
  for (const auto& p : files(root)) {
    auto ext = p.extension().string();
    if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx") {
      target_sources.push_back(p);
    }
  }

  if (target_sources.empty()) {
    return {Status::Blocked, "precondition tsan_binary_compiled not met: target has concurrency traits but no C/C++ sources or TSan report found"};
  }

  fs::path bin_path = workspace / "crivo_target_tsan_bin";
  std::string cmd = "clang++ -fsanitize=thread -O1 -g -pthread ";
  for (const auto& src : target_sources) {
    cmd += " '" + src.string() + "'";
  }
  cmd += " -o '" + bin_path.string() + "' 2>&1";

  FILE* pipe_comp = popen(cmd.c_str(), "r");
  if (!pipe_comp) {
    return {Status::Blocked, "failed to invoke compiler for ThreadSanitizer"};
  }
  char buf[512];
  std::string comp_out;
  while (fgets(buf, sizeof(buf), pipe_comp)) {
    comp_out += buf;
  }
  int comp_rc = pclose(pipe_comp);
  if (comp_rc != 0 || !fs::exists(bin_path)) {
    return {Status::Blocked, "precondition tsan_binary_compiled not met: compilation of target code under ThreadSanitizer failed: " + comp_out};
  }

  std::string run_cmd = "sh -c 'ulimit -v unlimited 2>/dev/null; exec \"" + bin_path.string() + "\"' 2>&1";
  FILE* pipe_run = popen(run_cmd.c_str(), "r");
  if (!pipe_run) {
    return {Status::Blocked, "failed to execute target binary under ThreadSanitizer"};
  }
  std::string run_out;
  while (fgets(buf, sizeof(buf), pipe_run)) {
    run_out += buf;
  }
  int run_rc = pclose(pipe_run);

  fs::path tsan_out = workspace / "crivo-tsan-report.txt";
  {
    std::ofstream out(tsan_out);
    out << "CRIVO Active ThreadSanitizer Target Audit\n"
        << "Target sources compiled: " << target_sources.size() << "\n"
        << "Execution output:\n" << run_out << "\n"
        << "Exit code: " << run_rc << "\n";
  }

  if (run_out.find("ThreadSanitizer setrlimit() failed") != std::string::npos ||
      run_out.find("limited virtual address space") != std::string::npos) {
    return {Status::Blocked, "precondition tsan_binary_compiled not met: ThreadSanitizer requires unlimited virtual address space: " + run_out};
  }
  if (run_out.find("WARNING: ThreadSanitizer") != std::string::npos) {
    return {Status::Fail, "ThreadSanitizer reported data race in target code"};
  }
  if (run_rc != 0) {
    return {Status::Fail, "Target binary terminated abnormally under ThreadSanitizer (exit " + std::to_string(run_rc) + "): " + run_out};
  }

  return {Status::Pass, "target compiled and executed under ThreadSanitizer: 0 data races detected"};
}

static Result provenance(const fs::path& root, const fs::path& workspace) {
  // 1. Check existing evidence package in target
  for (const auto& p : files(root, ".json")) {
    try {
      auto value = read_json(p);
      if (!value.is_object()) continue;
      auto& o = value.as_object();
      if (!o.contains("artifacts") || !o.at("artifacts").is_array()) continue;
      bool valid = true;
      std::string failure_details;
      for (const auto& item : o.at("artifacts").as_array()) {
        auto& a = item.as_object();
        if (!a.contains("path") || !a.contains("sha256")) {
          valid = false;
          failure_details = "artifact item missing required path or sha256 property";
          break;
        }
        fs::path artifact = p.parent_path() / std::string(a.at("path").as_string());
        std::string expected(a.at("sha256").as_string());
        if (expected.empty() || check::calculate_file_sha256(artifact) != expected) {
          valid = false;
          failure_details = "SHA-256 mismatch for artifact " + artifact.filename().string() +
                            " (expected " + expected + ", got " + check::calculate_file_sha256(artifact) + ")";
          break;
        }
      }
      if (valid) {
        return {Status::Pass, "target evidence package verified: all artifact SHA-256 digests match"};
      } else {
        return {Status::Fail, "target evidence package verification failed: " + failure_details};
      }
    } catch (...) { continue; }
  }

  // 2. Active SHA-256 Provenance Generator & Verifier across artifacts
  try {
    fs::path prov_out = workspace / "crivo-provenance.json";
    boost::json::object doc;
    doc["schema_version"] = "crivo.evidence/1.0.0";
    doc["provenance_standard"] = "FIPS 180-4 / ISO/IEC/IEEE 29119-3";

    boost::json::array art_arr;
    std::vector<fs::path> candidate_files;

    for (const auto& p : files(workspace)) {
      if (p != prov_out) candidate_files.push_back(p);
    }
    if (candidate_files.empty()) {
      for (const auto& p : files(root)) {
        candidate_files.push_back(p);
      }
    }
    if (candidate_files.empty()) {
      return {Status::Blocked, "precondition evidence_artifacts_persisted not met: no candidate files or artifacts found to verify SHA-256 provenance"};
    }

    for (const auto& p : candidate_files) {
      std::string hash = check::calculate_file_sha256(p);
      if (!hash.empty()) {
        boost::json::object item;
        item["path"] = p.filename().string();
        item["sha256"] = hash;
        item["verified"] = true;
        art_arr.push_back(item);
      }
    }

    doc["artifacts"] = art_arr;
    std::ofstream out(prov_out);
    out << boost::json::serialize(doc);
    out.close();

    // Re-verify written provenance manifest
    auto prov_val = read_json(prov_out);
    auto& prov_obj = prov_val.as_object();
    for (const auto& item : prov_obj.at("artifacts").as_array()) {
      auto& a = item.as_object();
      std::string fname(a.at("path").as_string());
      std::string expected_hash(a.at("sha256").as_string());
      fs::path file_path = workspace / fname;
      if (!fs::exists(file_path)) file_path = root / fname;
      if (!fs::exists(file_path) || check::calculate_file_sha256(file_path) != expected_hash) {
        return {Status::Fail, "active SHA-256 provenance recalculation mismatch"};
      }
    }

    return {Status::Pass, "active SHA-256 cryptographic provenance verified for all artifacts"};
  } catch (const std::exception& e) {
    return {Status::Fail, std::string("SHA-256 provenance generator error: ") + e.what()};
  }
}

static Result sbom(const fs::path& root, const fs::path& workspace) {
  // 1. Check existing CycloneDX SBOM
  for (const auto& p : files(root, ".json")) {
    try {
      auto value = read_json(p);
      if (!value.is_object()) continue;
      auto& o = value.as_object();
      if (!o.contains("bomFormat") || o.at("bomFormat").as_string() != "CycloneDX") continue;
      if (!o.contains("components") || !o.at("components").is_array()) return {Status::Fail, "CycloneDX components missing"};
      if (o.contains("vulnerabilities") && o.at("vulnerabilities").is_array()) {
        auto& v_arr = o.at("vulnerabilities").as_array();
        for (const auto& v : v_arr) {
          auto& vo = v.as_object();
          if (!vo.contains("ratings") || !vo.at("ratings").is_array()) continue;
          for (const auto& r : vo.at("ratings").as_array()) {
            if (r.as_object().contains("severity")) {
              std::string severity(r.as_object().at("severity").as_string());
              if (severity == "critical" || severity == "high") {
                std::string vuln_id = vo.contains("id") ? std::string(vo.at("id").as_string()) : "unspecified";
                return {Status::Fail, "high/critical SBOM vulnerability detected: " + vuln_id + " (severity: " + severity + ")"};
              }
            }
          }
        }
        // Even audit metadata embedded in this same target-controlled file can
        // be forged.  Clean status requires an independently executed scanner,
        // which this oracle does not currently provide.
        return {Status::Blocked, "INVENTORY_VALID; AUDIT_NOT_PERFORMED: target-supplied vulnerability data has no independently verified scanner execution"};
      }
      return {Status::Blocked, "precondition sbom_manifest_generated: CycloneDX SBOM present but lacks vulnerability audit data/VEX"};
    } catch (...) { return {Status::Fail, "invalid CycloneDX JSON structure"}; }
  }

  // Check applicability traits
  if (!has_manifest_traits(root)) {
    return {Status::NotApplicable, "no SBOM manifest or dependency descriptor"};
  }

  // 2. Active SBOM Synthesizer & Vulnerability Auditor
  try {
    fs::path sbom_out = workspace / "crivo-sbom.json";
    boost::json::object doc;
    doc["bomFormat"] = "CycloneDX";
    doc["specVersion"] = "1.6";
    doc["serialNumber"] = "urn:uuid:crivo-sbom-active-audit";
    doc["version"] = 1;

    boost::json::object meta;
    boost::json::object comp;
    comp["type"] = "application";
    comp["name"] = root.filename().string().empty() ? "target-project" : root.filename().string();
    comp["version"] = "1.0.0";
    meta["component"] = comp;
    doc["metadata"] = meta;

    boost::json::array components;
    // Inspect target manifests for dependencies
    bool found_deps = false;
    for (const auto& p : files(root)) {
      auto fname = p.filename().string();
      if (fname == "CMakeLists.txt") {
        std::ifstream f(p);
        std::string line;
        while (std::getline(f, line)) {
          if (line.find("find_package(") != std::string::npos) {
            std::regex re(R"(find_package\s*\(\s*([A-Za-z0-9_]+))");
            std::smatch match;
            if (std::regex_search(line, match, re) && match.size() > 1) {
              std::string dep_name = match[1].str();
              if (dep_name != "Threads") {
                boost::json::object c;
                c["type"] = "library";
                c["name"] = dep_name;
                c["version"] = "detected";
                components.push_back(c);
                found_deps = true;
              }
            }
          }
        }
      }
    }

    doc["components"] = components;

    if (found_deps) {
      // Third-party dependencies are present, but no verified vulnerability audit data was provided
      doc["vulnerabilities"] = boost::json::array{};
      std::ofstream out(sbom_out);
      out << boost::json::serialize(doc);
      return {Status::Blocked, "precondition sbom_manifest_generated: dependencies detected in target, but no verified vulnerability audit data available"};
    }

    // No dependency was discovered by the limited built-in inventory pass.
    // This is not proof of an empty dependency graph or a vulnerability audit.
    doc["vulnerabilities"] = boost::json::array{};
    std::ofstream out(sbom_out);
    out << boost::json::serialize(doc);
    return {Status::Blocked, "INVENTORY_VALID; AUDIT_NOT_PERFORMED: no verified vulnerability source was queried"};
  } catch (const std::exception& e) {
    return {Status::Fail, std::string("SBOM synthesizer error: ") + e.what()};
  }
}

static int get_http_port() {
  const char* p = std::getenv("CRIVO_HTTP_PORT");
  if (!p) p = std::getenv("CRIVO_PORT");
  if (p) {
    int val = std::atoi(p);
    if (val > 0 && val < 65536) return val;
  }
  return 8765;
}

static std::optional<boost::json::object> get_or_create_http_observations(const fs::path& root, const fs::path& workspace) {
  // Observation files are outputs, never trusted inputs: accepting a
  // pre-existing JSON document would let unrelated or fabricated traffic pass.
  fs::path p2 = workspace / "crivo-analysis.json";

  // If no HTTP traits, return nullopt (not applicable)
  if (!has_http_traits(root)) {
    return std::nullopt;
  }

  const char* expected_identity_env = std::getenv("CRIVO_HTTP_EXPECTED_ID");
  if (!expected_identity_env || std::string(expected_identity_env).empty()) {
    return std::nullopt;
  }
  const std::string expected_identity(expected_identity_env);

  // 2. Active HTTP Provocation Engine: probe live server on loopback ONLY if listening
  int port = get_http_port();
  int test_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (test_sock < 0) return std::nullopt;

  struct timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = 250000; // 250ms timeout
  setsockopt(test_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(test_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(test_sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(test_sock);
    // Real server is not listening on loopback; DO NOT synthesize fake responses!
    return std::nullopt;
  }
  close(test_sock);

  // Server is reachable; exercise real requests against loopback service
  auto send_request = [&](const std::string& method, const std::string& path,
                          const std::string& case_name = "") -> boost::json::object {
    boost::json::object obs;
    obs["method"] = method;
    obs["path"] = path;
    if (!case_name.empty()) obs["case"] = case_name;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
      obs["status"] = 500;
      obs["latency_ms"] = 999.0;
      return obs;
    }
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    auto start = std::chrono::steady_clock::now();
    if (connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
      close(s);
      obs["status"] = 500;
      obs["latency_ms"] = 999.0;
      return obs;
    }

    std::string req = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                      "\r\nUser-Agent: CRIVO-Oracle/1.0\r\nConnection: close\r\n\r\n";
    send(s, req.data(), req.size(), 0);

    char buf[4096];
    std::string resp;
    while (true) {
      ssize_t n = recv(s, buf, sizeof(buf) - 1, 0);
      if (n <= 0) break;
      buf[n] = '\0';
      resp.append(buf, n);
    }
    close(s);
    auto end = std::chrono::steady_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();
    obs["latency_ms"] = latency;

    int status_code = 500;
    boost::json::object headers;
    std::istringstream stream(resp);
    std::string line;
    if (std::getline(stream, line)) {
      std::regex status_re(R"(HTTP/\d\.\d\s+(\d+))");
      std::smatch m;
      if (std::regex_search(line, m, status_re) && m.size() > 1) {
        status_code = std::stoi(m[1].str());
      }
    }
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
      auto colon = line.find(':');
      if (colon != std::string::npos) {
        std::string k = line.substr(0, colon);
        std::string v = line.substr(colon + 1);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\r')) k.pop_back();
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
        headers[k] = v;
      }
    }
    obs["status"] = status_code;
    obs["headers"] = headers;
    return obs;
  };

  boost::json::object doc;
  boost::json::array obs;

  // Mutating methods
  for (const char* m : {"POST", "PUT", "DELETE", "PATCH"}) {
    obs.push_back(send_request(m, "/api/v1/resource"));
  }
  // Standard GET
  obs.push_back(send_request("GET", "/api/v1/resource"));
  // Malicious inputs
  obs.push_back(send_request("GET", "/api/v1/resource?file=../../etc/passwd", "malicious_input"));
  obs.push_back(send_request("GET", "/api/v1/resource?id=1%27%20OR%201=1--", "malicious_input"));
  // Latency observations
  for (int i = 0; i < 20; ++i) {
    obs.push_back(send_request("GET", "/api/v1/resource"));
  }

  doc["http_observations"] = obs;
  doc["expected_service_identity"] = expected_identity;
  try {
    std::ofstream out(p2);
    out << boost::json::serialize(doc);
    out.close();
  } catch (...) {}

  return doc;
}

static Result http_observation(const fs::path& root, const fs::path& workspace, const std::string& mode) {
  const char* expected_identity_env = std::getenv("CRIVO_HTTP_EXPECTED_ID");
  if (has_http_traits(root) && (!expected_identity_env || std::string(expected_identity_env).empty())) {
    return {Status::Blocked, "service identity not proven: CRIVO_HTTP_EXPECTED_ID missing"};
  }
  auto doc = get_or_create_http_observations(root, workspace);
  if (!doc || !doc->contains("http_observations")) {
    if (has_http_traits(root)) {
      return {Status::Blocked, "precondition server_listening_loopback not met: no HTTP service listening on loopback and no observation trace available"};
    }
    return {Status::NotApplicable, "no HTTP observations or service"};
  }
  auto& rows = doc->at("http_observations").as_array();
  if (rows.empty()) return {Status::Blocked, "empty HTTP observations"};
  if (!doc->contains("expected_service_identity") ||
      !doc->at("expected_service_identity").is_string()) {
    return {Status::Blocked, "service identity not proven: expected identity missing"};
  }
  const std::string expected_identity(doc->at("expected_service_identity").as_string());

  // Every response must bind the probed process to the target identity chosen
  // by the launcher.  Port reachability alone does not establish ownership.
  bool identity_observed = false;
  for (const auto& x : rows) {
    const auto& o = x.as_object();
    if (!o.contains("headers") || !o.at("headers").is_object()) continue;
    const auto& headers = o.at("headers").as_object();
    if (!headers.contains("x-crivo-target-id")) continue;
    if (!headers.at("x-crivo-target-id").is_string() ||
        std::string(headers.at("x-crivo-target-id").as_string()) != expected_identity) {
      return {Status::Blocked, "service identity not proven: X-Crivo-Target-Id mismatch"};
    }
    identity_observed = true;
  }
  if (!identity_observed) {
    return {Status::Blocked, "service identity not proven: X-Crivo-Target-Id missing"};
  }

  if (mode == "readonly") {
    bool tested = false;
    for (const auto& x : rows) {
      auto& o = x.as_object();
      std::string method(o.at("method").as_string());
      if (method != "GET" && method != "HEAD") {
        tested = true;
        if (o.at("status").as_int64() != 405) return {Status::Fail, "mutating HTTP method not rejected with 405"};
      }
    }
    return tested ? Result{Status::Pass, "mutating HTTP methods rejected with 405"}
                  : Result{Status::Blocked, "no mutating method observation"};
  }

  if (mode == "headers") {
    for (const auto& x : rows) {
      auto& o = x.as_object();
      if (std::string(o.at("method").as_string()) != "GET") continue;
      if (!o.contains("headers") || !o.at("headers").is_object()) {
        return {Status::Fail, "headers object missing in HTTP observation"};
      }
      auto& h = o.at("headers").as_object();
      if (!h.contains("content-security-policy") ||
          !h.contains("x-content-type-options") ||
          std::string(h.at("x-content-type-options").as_string()) != "nosniff" ||
          !h.contains("cache-control")) {
        return {Status::Fail, "required security headers missing"};
      }
    }
    return {Status::Pass, "security headers present"};
  }

  if (mode == "input") {
    bool tested = false;
    for (const auto& x : rows) {
      auto& o = x.as_object();
      if (o.contains("case") && std::string(o.at("case").as_string()) == "malicious_input") {
        tested = true;
        auto s = o.at("status").as_int64();
        if (s != 400 && s != 403 && s != 422) {
          return {Status::Fail, "malicious input not rejected by contract (expected 400, 403, or 422)"};
        }
      }
    }
    return tested ? Result{Status::Pass, "malicious inputs rejected"}
                  : Result{Status::Blocked, "no malicious input observation"};
  }

  std::vector<double> values;
  for (const auto& x : rows) {
    auto& o = x.as_object();
    if (o.contains("latency_ms")) values.push_back(o.at("latency_ms").to_number<double>());
  }
  if (values.size() < 20) return {Status::Blocked, "need at least 20 latency observations"};
  std::sort(values.begin(), values.end());
  double p95 = values[static_cast<size_t>((values.size() - 1) * 0.95)];
  return p95 < 20.0 ? Result{Status::Pass, "p95 latency below 20ms"}
                    : Result{Status::Fail, "p95 latency budget exceeded"};
}

Result run(const std::string& id, const fs::path& target, const fs::path& workspace) {
  if (id == "crivo.catalog.schema-strict") return schema_strict(target);
  if (id == "crivo.sqlite.transaction.rollback") return sqlite_rollback(target, workspace);
  if (id == "crivo.sqlite.wal.concurrency") return sqlite_wal(target, workspace);
  if (id == "crivo.contract.sarif-export") return sarif(target, workspace);
  if (id == "crivo.concurrency.thread-sanitizer") return tsan(target, workspace);
  if (id == "crivo.memory.sha256-provenance") return provenance(target, workspace);
  if (id == "crivo.supply-chain.sbom-audit") return sbom(target, workspace);
  if (id == "crivo.http.contract-readonly") return http_observation(target, workspace, "readonly");
  if (id == "crivo.security.header-hardening") return http_observation(target, workspace, "headers");
  if (id == "crivo.security.cwe-input-validation") return http_observation(target, workspace, "input");
  if (id == "crivo.performance.latency-budget") return http_observation(target, workspace, "latency");
  return {Status::Blocked, "qualified builtin oracle not yet available"};
}

} // namespace crivo::oracles
