#include "builtin_oracles.hpp"
#include "inspector.hpp"
#include "registry/validator.hpp"
#include <boost/json.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <fstream>
#include <optional>
#include <thread>
#include <vector>

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
  auto scan=fs::recursive_directory_iterator(root,fs::directory_options::skip_permission_denied);
  for(auto it=scan;it!=fs::recursive_directory_iterator();++it){const auto& e=*it;if(e.is_directory()){auto n=e.path().filename().string();if(n==".git"||n=="node_modules"||n=="vendor"||n==".cache"||n=="dist"||n=="coverage"||n.starts_with("build"))it.disable_recursion_pending();continue;}if(e.is_regular_file()&&(ext.empty()||e.path().extension()==ext))out.push_back(e.path());}
  return out;
}

static boost::json::value read_json(const fs::path& p) {
  std::ifstream in(p);
  std::string bytes((std::istreambuf_iterator<char>(in)), {});
  return boost::json::parse(bytes);
}

static Result schema_strict(const fs::path& root) {
  registry::RegistryCollection collection;
  registry::ValidationReport report;
  for (const auto& p : files(root, ".json")) {
    if (p.filename() == "tests.json") continue;
    try {
      auto j = read_json(p);
      if (j.is_object() && j.as_object().contains("schema_version")) {
        std::string version(j.as_object().at("schema_version").as_string());
        if (version.starts_with("crivo.reference/") || version.starts_with("crivo.technique/") ||
            version.starts_with("crivo.test-spec/") || version.starts_with("crivo.implementation/") ||
            version.starts_with("crivo.profile/")) registry::validate_file(p.string(), collection, report);
      }
    } catch (const std::exception& e) { return {Status::Fail, "invalid JSON: " + p.string() + ": " + e.what()}; }
  }
  return report.valid ? Result{Status::Pass, "JSON syntax and declared CRIVO schemas valid"}
                      : Result{Status::Fail, "declared CRIVO schema invalid"};
}

static fs::path first_db(const fs::path& root) {
  for (const auto& p : files(root)) {
    auto x = p.extension().string();
    if (x == ".db" || x == ".sqlite" || x == ".sqlite3") return p;
  }
  return {};
}

static Result sqlite_rollback(const fs::path& root, const fs::path& workspace) {
  auto source = first_db(root);
  if (source.empty()) return {Status::NotApplicable, "no SQLite database"};
  fs::path copy = workspace / "rollback.sqlite";
  fs::remove(copy); fs::remove(copy.string()+"-wal"); fs::remove(copy.string()+"-shm");
  fs::copy_file(source, copy, fs::copy_options::overwrite_existing);
  sqlite3* db = nullptr;
  if (sqlite3_open(copy.c_str(), &db) != SQLITE_OK) return {Status::Blocked, "cannot open ephemeral database"};
  char* err = nullptr;
  int rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS crivo_rollback_probe(v INTEGER);BEGIN;INSERT INTO crivo_rollback_probe VALUES(1);ROLLBACK;", nullptr, nullptr, &err);
  sqlite3_stmt* st = nullptr;
  long long count = -1;
  if (rc == SQLITE_OK && sqlite3_prepare_v2(db, "SELECT count(*) FROM crivo_rollback_probe;", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
  if (st) sqlite3_finalize(st);
  if (err) sqlite3_free(err);
  sqlite3_close(db);
  return rc == SQLITE_OK && count == 0 ? Result{Status::Pass, "rollback preserved preimage"} : Result{Status::Fail, "rollback invariant failed"};
}

static Result sqlite_wal(const fs::path& root, const fs::path& workspace) {
  auto source = first_db(root);
  if (source.empty()) return {Status::NotApplicable, "no SQLite database"};
  fs::path copy = workspace / "wal.sqlite";
  fs::remove(copy); fs::remove(copy.string()+"-wal"); fs::remove(copy.string()+"-shm");
  fs::copy_file(source, copy, fs::copy_options::overwrite_existing);
  sqlite3* setup = nullptr; sqlite3_open(copy.c_str(), &setup);
  int rc = sqlite3_exec(setup, "PRAGMA journal_mode=WAL;CREATE TABLE IF NOT EXISTS crivo_wal_probe(v INTEGER);", nullptr, nullptr, nullptr);
  sqlite3_close(setup);
  std::vector<int> results(8, SQLITE_ERROR); std::vector<std::thread> threads;
  for (size_t i=0;i<results.size();++i) threads.emplace_back([&,i]{
    sqlite3* db=nullptr;
    if(sqlite3_open_v2(copy.c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK) {
      sqlite3_busy_timeout(db, 1000);
      for(int attempt=0; attempt<3; ++attempt) {
        results[i]=sqlite3_exec(db,"SELECT count(*) FROM crivo_wal_probe;",nullptr,nullptr,nullptr);
        if(results[i]==SQLITE_OK) break;
        std::this_thread::yield();
      }
    }
    if(db) sqlite3_close(db);
  });
  for (auto& t : threads) t.join();
  return rc == SQLITE_OK && std::all_of(results.begin(), results.end(), [](int x){return x==SQLITE_OK;}) ? Result{Status::Pass,"concurrent WAL reads succeeded"} : Result{Status::Fail,"WAL concurrency failed"};
}

static Result sarif(const fs::path& root) {
  for (const auto& p : files(root)) if (p.extension()==".sarif" || p.filename().string().ends_with(".sarif.json")) try { auto value=read_json(p); auto& o=value.as_object(); if(o.at("version").as_string()=="2.1.0" && o.at("runs").is_array()) return {Status::Pass,"SARIF 2.1.0 valid"}; else return {Status::Fail,"invalid SARIF structure"}; } catch(...) { return {Status::Fail,"invalid SARIF JSON"}; }
  return {Status::NotApplicable,"no SARIF artifact"};
}

static Result tsan(const fs::path& root) {
  for (const auto& p : files(root)) if (p.filename().string().find("tsan") != std::string::npos) { std::ifstream in(p); std::string s((std::istreambuf_iterator<char>(in)),{}); return s.find("WARNING: ThreadSanitizer") == std::string::npos ? Result{Status::Pass,"TSan report has no data race"} : Result{Status::Fail,"ThreadSanitizer reported data race"}; }
  return {Status::NotApplicable,"no TSan report"};
}

static Result provenance(const fs::path& root) {
  for (const auto& p : files(root, ".json")) try {
    auto value=read_json(p); if(!value.is_object()) continue; auto& o=value.as_object();
    if(!o.contains("artifacts") || !o.at("artifacts").is_array()) continue;
    for(const auto& item:o.at("artifacts").as_array()) {
      auto& a=item.as_object(); if(!a.contains("path") || !a.contains("sha256")) return {Status::Fail,"artifact digest metadata missing"};
      fs::path artifact=p.parent_path()/std::string(a.at("path").as_string());
      std::string expected(a.at("sha256").as_string());
      if(expected.empty() || check::calculate_file_sha256(artifact)!=expected) return {Status::Fail,"artifact SHA-256 mismatch"};
    }
    return {Status::Pass,"all artifact SHA-256 digests match"};
  } catch(...) { continue; }
  return {Status::NotApplicable,"no evidence package"};
}

static Result sbom(const fs::path& root) {
  for(const auto& p:files(root,".json")) try {
    auto value=read_json(p); if(!value.is_object()) continue; auto& o=value.as_object();
    if(!o.contains("bomFormat") || o.at("bomFormat").as_string()!="CycloneDX") continue;
    if(!o.contains("components") || !o.at("components").is_array()) return {Status::Fail,"CycloneDX components missing"};
    if(o.contains("vulnerabilities")) for(const auto& v:o.at("vulnerabilities").as_array()) {
      auto& vo=v.as_object(); if(!vo.contains("ratings")) continue;
      for(const auto& r:vo.at("ratings").as_array()) if(r.as_object().contains("severity")) {
        std::string severity(r.as_object().at("severity").as_string());
        if(severity=="critical" || severity=="high") return {Status::Fail,"high/critical SBOM vulnerability"};
      }
    }
    return {Status::Pass,"CycloneDX SBOM has no high/critical vulnerability"};
  } catch(...) { continue; }
  return {Status::NotApplicable,"no CycloneDX SBOM"};
}

static std::optional<boost::json::object> observations(const fs::path& root) {
  fs::path p=root/"crivo-analysis.json"; if(!fs::exists(p)) return std::nullopt;
  try { auto v=read_json(p); return v.as_object(); } catch(...) { return std::nullopt; }
}

static Result http_observation(const fs::path& root, const std::string& mode) {
  auto doc=observations(root); if(!doc || !doc->contains("http_observations")) return {Status::NotApplicable,"no HTTP observations"};
  auto& rows=doc->at("http_observations").as_array(); if(rows.empty()) return {Status::Blocked,"empty HTTP observations"};
  if(mode=="readonly") { bool tested=false; for(const auto& x:rows){auto& o=x.as_object(); std::string method(o.at("method").as_string()); if(method!="GET"&&method!="HEAD"){tested=true;if(o.at("status").as_int64()!=405)return {Status::Fail,"mutating HTTP method not rejected with 405"};}} return tested?Result{Status::Pass,"mutating HTTP methods rejected"}:Result{Status::Blocked,"no mutating method observation"}; }
  if(mode=="headers") { for(const auto& x:rows){auto& o=x.as_object();if(std::string(o.at("method").as_string())!="GET")continue;auto& h=o.at("headers").as_object();if(!h.contains("content-security-policy")||!h.contains("x-content-type-options")||std::string(h.at("x-content-type-options").as_string())!="nosniff"||!h.contains("cache-control"))return {Status::Fail,"required security headers missing"};} return {Status::Pass,"security headers present"}; }
  if(mode=="input") { bool tested=false;for(const auto& x:rows){auto& o=x.as_object();if(o.contains("case")&&std::string(o.at("case").as_string())=="malicious_input"){tested=true;auto s=o.at("status").as_int64();if(s<400||s>=500)return {Status::Fail,"malicious input accepted"};}}return tested?Result{Status::Pass,"malicious inputs rejected"}:Result{Status::Blocked,"no malicious input observation"}; }
  std::vector<double> values;for(const auto& x:rows){auto& o=x.as_object();if(o.contains("latency_ms"))values.push_back(o.at("latency_ms").to_number<double>());}if(values.size()<20)return {Status::Blocked,"need at least 20 latency observations"};std::sort(values.begin(),values.end());double p95=values[static_cast<size_t>((values.size()-1)*0.95)];return p95<20.0?Result{Status::Pass,"p95 latency below 20ms"}:Result{Status::Fail,"p95 latency budget exceeded"};
}

Result run(const std::string& id, const fs::path& target, const fs::path& workspace) {
  if (id=="crivo.catalog.schema-strict") return schema_strict(target);
  if (id=="crivo.sqlite.transaction.rollback") return sqlite_rollback(target, workspace);
  if (id=="crivo.sqlite.wal.concurrency") return sqlite_wal(target, workspace);
  if (id=="crivo.contract.sarif-export") return sarif(target);
  if (id=="crivo.concurrency.thread-sanitizer") return tsan(target);
  if (id=="crivo.memory.sha256-provenance") return provenance(target);
  if (id=="crivo.supply-chain.sbom-audit") return sbom(target);
  if (id=="crivo.http.contract-readonly") return http_observation(target,"readonly");
  if (id=="crivo.security.header-hardening") return http_observation(target,"headers");
  if (id=="crivo.security.cwe-input-validation") return http_observation(target,"input");
  if (id=="crivo.performance.latency-budget") return http_observation(target,"latency");
  return {Status::Blocked, "qualified builtin oracle not yet available"};
}
} // namespace crivo::oracles
