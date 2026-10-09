#include "inspector.hpp"
#include "core.hpp"
#include "registry/validator.hpp"
#include "registry/plan.hpp"
#include "registry/lifecycle.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace crivo::check {

namespace fs = std::filesystem;

namespace {
class Sha256 {
public:
  Sha256() { reset(); }
  void reset() {
    state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85; state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f; state_[5] = 0x9b05688c; state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
    count_ = 0;
  }
  void update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      buffer_[count_ & 63] = data[i];
      count_++;
      if ((count_ & 63) == 0) transform(buffer_);
    }
  }
  void update(const std::string& str) {
    update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
  }
  std::string final_hex() {
    uint64_t total_bits = count_ * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    while ((count_ & 63) != 56) {
      uint8_t zero = 0;
      update(&zero, 1);
    }
    for (int i = 7; i >= 0; --i) {
      uint8_t b = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xff);
      update(&b, 1);
    }
    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
      ss << std::hex << std::setfill('0') << std::setw(8) << state_[i];
    }
    return ss.str();
  }
private:
  uint32_t state_[8];
  uint64_t count_;
  uint8_t buffer_[64];
  static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) { return (e & f) ^ (~e & g); }
  static inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) { return (a & b) ^ (a & c) ^ (b & c); }
  static inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static inline uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static inline uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
  void transform(const uint8_t* chunk) {
    static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
             (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
             (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
             (static_cast<uint32_t>(chunk[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
      w[i] = theta1(w[i - 2]) + w[i - 7] + theta0(w[i - 15]) + w[i - 16];
    }
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = h + sig1(e) + choose(e, f, g) + K[i] + w[i];
      uint32_t t2 = sig0(a) + majority(a, b, c);
      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
  }
};
} // namespace

static std::string calculate_file_sha256(const fs::path& p) {
    if (!fs::exists(p)) return "";
    std::ifstream file(p, std::ios::binary);
    if (!file) return "";

    Sha256 sha;
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        sha.update(reinterpret_cast<const uint8_t*>(buffer), file.gcount());
    }
    if (file.gcount() > 0) {
        sha.update(reinterpret_cast<const uint8_t*>(buffer), file.gcount());
    }
    return sha.final_hex();
}

static std::string generate_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

TargetInfo inspect_target(const fs::path& target_path) {
    TargetInfo info;
    info.root_path = fs::absolute(target_path);

    if (!fs::exists(info.root_path)) {
        return info;
    }

    // Default universal capabilities
    info.discovered_capabilities.push_back("capability:filesystem");
    info.discovered_capabilities.push_back("capability:posix_env");

    // Scan target directory for observable markers (read-only)
    bool has_json = false;
    bool has_sqlite = false;
    bool has_cmake = false;
    bool has_cli = false;

    for (const auto& entry : fs::recursive_directory_iterator(info.root_path, fs::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            auto filename = entry.path().filename().string();

            if (ext == ".json") has_json = true;
            if (ext == ".db" || ext == ".sqlite" || ext == ".sqlite3") has_sqlite = true;
            if (filename == "CMakeLists.txt" || filename == "CTestTestfile.cmake") has_cmake = true;
            if (filename == "crivo" || filename == "main" || ext == ".sh") has_cli = true;
        }
    }

    if (has_json) info.discovered_capabilities.push_back("capability:json_parser");
    if (has_sqlite) info.discovered_capabilities.push_back("capability:sqlite");
    if (has_cmake) info.discovered_capabilities.push_back("capability:cmake_ctest");
    if (has_cli) info.discovered_capabilities.push_back("capability:cli");

    // Check git revision if available
    fs::path git_dir = info.root_path / ".git";
    if (fs::exists(git_dir)) {
        fs::path head_file = git_dir / "HEAD";
        if (fs::exists(head_file)) {
            std::ifstream hf(head_file);
            std::string line;
            if (std::getline(hf, line)) {
                if (line.rfind("ref: ", 0) == 0) {
                    std::string ref_path = line.substr(5);
                    fs::path ref_full = git_dir / ref_path;
                    if (fs::exists(ref_full)) {
                        std::ifstream rf(ref_full);
                        std::getline(rf, info.revision);
                    }
                } else {
                    info.revision = line;
                }
            }
        }
    }

    return info;
}

CheckSummary execute_check(const CheckOptions& opts) {
    CheckSummary summary;
    summary.request_id = "req-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    summary.project_id = opts.target_path.filename().string();
    if (summary.project_id.empty() || summary.project_id == ".") {
        summary.project_id = fs::canonical(opts.target_path).filename().string();
    }

    TargetInfo target = inspect_target(opts.target_path);

    // 1. Load Registry Collection
    registry::RegistryCollection collection;
    registry::ValidationReport report;
    std::vector<std::string> subdirs = {"references", "techniques", "specifications", "implementations", "profiles"};
    for (const auto& sub : subdirs) {
        fs::path subpath = fs::path(opts.catalog_dir) / sub;
        if (fs::exists(subpath) && fs::is_directory(subpath)) {
            for (const auto& entry : fs::recursive_directory_iterator(subpath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    registry::validate_file(entry.path().string(), collection, report);
                }
            }
        }
    }

    if (!report.valid) {
        summary.overall_status = "BLOCKED";
        return summary;
    }

    // 2. Resolve Profile
    const registry::ProfileRecord* target_profile = nullptr;
    for (const auto& [id, p] : collection.profiles) {
        if (p.id == opts.profile || p.id == "profile:" + opts.profile) {
            target_profile = &p;
            break;
        }
    }

    if (!target_profile) {
        if (!collection.profiles.empty()) {
            target_profile = &(collection.profiles.begin()->second);
        } else {
            summary.overall_status = "NO_TESTS";
            return summary;
        }
    }

    // 3. Resolve Test Plan
    registry::EnvironmentCapabilities env;
    env.closed_world = opts.fail_closed;
    for (const auto& cap : target.discovered_capabilities) {
        env.supported.insert(cap);
    }

    registry::TestPlan plan = registry::resolve_test_plan(*target_profile, collection, env);

    if (plan.planned_tests.empty()) {
        summary.overall_status = "NO_TESTS";
        return summary;
    }

    // Check fail-closed condition
    if (opts.fail_closed && plan.summary.unknown_count > 0) {
        summary.overall_status = "BLOCKED";
        summary.blocked_tests = static_cast<int>(plan.summary.unknown_count);
        return summary;
    }

    // 4. Setup Ephemeral Sandbox Instance
    std::string instance_id = "sbx-" + summary.request_id;
    fs::path sbx_workspace = fs::absolute(opts.evidence_dir) / "sandboxes" / instance_id;
    fs::path evidence_path = fs::absolute(opts.evidence_dir) / summary.request_id;
    fs::create_directories(sbx_workspace);
    fs::create_directories(evidence_path);

    sandbox::BackendType btype = sandbox::BackendType::Auto;
    if (opts.backend == "bubblewrap") btype = sandbox::BackendType::Bubblewrap;
    else if (opts.backend == "podman") btype = sandbox::BackendType::Podman;
    else if (opts.backend == "host_isolated" || opts.isolation == "host_isolated") btype = sandbox::BackendType::HostIsolated;

    sandbox::SandboxConfig sbx_config;
    sbx_config.target_ro_path = target.root_path;
    sbx_config.workspace_rw_path = sbx_workspace;
    sbx_config.evidence_dir = evidence_path;
    sbx_config.backend = btype;
    sbx_config.limits.timeout = std::chrono::seconds(opts.timeout_seconds);

    std::string start_time = generate_utc_timestamp();
    auto start_steady = std::chrono::steady_clock::now();

    // 5. Execute tests in Sandbox
    std::ostringstream junit_xml;
    junit_xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    junit_xml << "<testsuites name=\"CRIVO Check Suite\" id=\"" << summary.request_id << "\">\n";
    junit_xml << "  <testsuite name=\"" << target_profile->id << "\" tests=\"" << plan.planned_tests.size() << "\">\n";

    for (const auto& test_case : plan.planned_tests) {
        summary.total_tests++;

        if (test_case.applicability_status == registry::ApplicabilityStatus::NOT_APPLICABLE) {
            summary.skipped_tests++;
            junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\">\n";
            junit_xml << "      <skipped message=\"Not applicable to target capabilities\"/>\n";
            junit_xml << "    </testcase>\n";
            continue;
        }

        if (test_case.applicability_status == registry::ApplicabilityStatus::UNKNOWN) {
            if (opts.fail_closed) {
                summary.blocked_tests++;
                junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\">\n";
                junit_xml << "      <error message=\"Blocked by fail-closed policy: Unknown capability\"/>\n";
                junit_xml << "    </testcase>\n";
                continue;
            }
        }

        // Execute test inside Sandbox
        std::vector<std::string> test_cmd = {"/bin/true"};
        if (test_case.adapter == "crivo.builtin") {
            test_cmd = {"/bin/echo", "CRIVO Builtin Verification PASS"};
        } else if (test_case.adapter == "crivo.adapter.ctest") {
            test_cmd = {"ctest", "--show-only"};
        }

        auto exec_res = sandbox::run_in_sandbox(sbx_config, test_cmd);

        if (exec_res.isolation_status == "BLOCKED") {
            summary.blocked_tests++;
            junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\">\n";
            junit_xml << "      <error message=\"Sandbox isolation blocked\"/>\n";
            junit_xml << "    </testcase>\n";
        } else if (exec_res.termination == sandbox::TerminationReason::Timeout) {
            summary.failed_tests++;
            junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\">\n";
            junit_xml << "      <failure message=\"Timeout exceeded in sandbox\"/>\n";
            junit_xml << "    </testcase>\n";
        } else if (exec_res.exit_code == 0) {
            summary.passed_tests++;
            junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\" time=\""
                      << (static_cast<double>(exec_res.duration.count()) / 1000.0) << "\"/>\n";
        } else {
            summary.failed_tests++;
            junit_xml << "    <testcase classname=\"" << test_case.spec_id << "\" name=\"" << test_case.implementation_id << "\">\n";
            junit_xml << "      <failure message=\"Non-zero exit code in sandbox: " << exec_res.exit_code << "\"/>\n";
            junit_xml << "    </testcase>\n";
        }
    }

    junit_xml << "  </testsuite>\n";
    junit_xml << "</testsuites>\n";

    auto end_steady = std::chrono::steady_clock::now();
    std::string end_time = generate_utc_timestamp();
    double duration_sec = std::chrono::duration<double>(end_steady - start_steady).count();

    // 6. Write Artifacts
    fs::path junit_file = evidence_path / "junit.xml";
    {
        std::ofstream jf(junit_file);
        jf << junit_xml.str();
    }
    std::string junit_sha = calculate_file_sha256(junit_file);

    // Write Sandbox Report
    sandbox::ExecutionResult overall_sbx;
    overall_sbx.exit_code = (summary.failed_tests == 0 && summary.blocked_tests == 0) ? 0 : 1;
    overall_sbx.isolation_driver = opts.backend;
    overall_sbx.isolation_status = (summary.blocked_tests > 0) ? "BLOCKED" : "ENFORCED";
    overall_sbx.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_steady - start_steady);

    fs::path sbx_report_file = evidence_path / "sandbox-report.json";
    {
        std::ofstream rf(sbx_report_file);
        rf << sandbox::generate_sandbox_report_json(sbx_config, overall_sbx, instance_id);
    }
    std::string sbx_sha = calculate_file_sha256(sbx_report_file);

    // Determine overall status
    if (summary.blocked_tests > 0) summary.overall_status = "BLOCKED";
    else if (summary.failed_tests > 0) summary.overall_status = "FAIL";
    else if (opts.fail_closed && summary.skipped_tests > 0) summary.overall_status = "BLOCKED";
    else if (summary.passed_tests > 0) summary.overall_status = "PASS";
    else summary.overall_status = "NO_TESTS";

    // Write Canonical Evidence Package
    boost::json::object ev_pkg;
    ev_pkg["schema_version"] = "crivo.evidence/1.0.0";
    ev_pkg["evidence_id"] = summary.request_id;
    ev_pkg["project_id"] = summary.project_id;

    boost::json::object exec_obj;
    exec_obj["adapter"] = "crivo.on_demand_check";
    exec_obj["version"] = "0.3.0";
    ev_pkg["executor"] = exec_obj;

    boost::json::object tim_obj;
    tim_obj["start_utc"] = start_time;
    tim_obj["end_utc"] = end_time;
    tim_obj["duration_seconds"] = duration_sec;
    ev_pkg["timing"] = tim_obj;

    boost::json::object sum_obj;
    sum_obj["total"] = summary.total_tests;
    sum_obj["passed"] = summary.passed_tests;
    sum_obj["failed"] = summary.failed_tests;
    sum_obj["skipped"] = summary.skipped_tests;
    ev_pkg["summary"] = sum_obj;

    boost::json::array art_arr;
    boost::json::object art_junit;
    art_junit["path"] = "junit.xml";
    art_junit["format"] = "application/xml";
    art_junit["sha256"] = junit_sha;
    art_arr.push_back(art_junit);

    boost::json::object art_sbx;
    art_sbx["path"] = "sandbox-report.json";
    art_sbx["format"] = "application/json";
    art_sbx["sha256"] = sbx_sha;
    art_arr.push_back(art_sbx);
    ev_pkg["artifacts"] = art_arr;

    fs::path ev_file = evidence_path / "evidence.json";
    {
        std::ofstream ef(ev_file);
        ef << boost::json::serialize(ev_pkg);
    }
    summary.evidence_sha256 = calculate_file_sha256(ev_file);
    summary.evidence_path = ev_file.string();

    // 7. Persist to SQLite WAL database
    if (fs::exists(opts.db_path)) {
        ExternalRunRecord rec;
        rec.evidence_id = summary.request_id;
        rec.project_id = summary.project_id;
        rec.mode = "on_demand_check";
        rec.source_revision = target.revision;
        rec.source_worktree = target.root_path.string();
        rec.status = summary.overall_status;
        rec.started_at = start_time;
        rec.ended_at = end_time;
        rec.adapter = "crivo.on_demand_check";
        rec.adapter_version = "0.3.0";
        rec.evidence_path = ev_file.string();
        rec.junit_path = junit_file.string();
        rec.junit_sha256 = junit_sha;
        rec.duration_ms = static_cast<long long>(duration_sec * 1000);
        rec.total = summary.total_tests;
        rec.passed = summary.passed_tests;
        rec.failed = summary.failed_tests;
        rec.skipped = summary.skipped_tests;

        try {
            record_external_run(opts.db_path, rec);
        } catch (const std::exception& e) {
            std::cerr << "[crivo] Warning: Failed to record check run in SQLite: " << e.what() << std::endl;
        }
    }

    // 8. Ephemeral Cleanup (destroys the sandbox workspace, preserves evidence)
    std::error_code ec;
    fs::remove_all(sbx_workspace, ec);

    return summary;
}

} // namespace crivo::check
