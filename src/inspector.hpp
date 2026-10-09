#pragma once

#include "sandbox.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace crivo::check {

struct TargetInfo {
    std::filesystem::path root_path;
    std::string revision{"unknown"};
    bool is_dirty{false};
    std::vector<std::string> discovered_capabilities;
};

struct CheckOptions {
    std::filesystem::path target_path{"."};
    std::string profile{"core"};
    std::string catalog_dir{"catalog"};
    std::string db_path{".run/crivo.db"};
    std::string evidence_dir{".run/evidence"};
    std::string isolation{"sandbox"}; // "sandbox", "host_isolated", "none"
    std::string backend{"auto"};      // "auto", "bubblewrap", "podman"
    bool fail_closed{true};
    bool dry_run{false};
    bool json_output{false};
    int timeout_seconds{30};
};

struct CheckSummary {
    std::string request_id;
    std::string project_id;
    std::string overall_status; // "PASS", "FAIL", "BLOCKED", "NO_TESTS"
    int total_tests{0};
    int passed_tests{0};
    int failed_tests{0};
    int skipped_tests{0};
    int blocked_tests{0};
    std::string evidence_path;
    std::string evidence_sha256;
};

TargetInfo inspect_target(const std::filesystem::path& target_path);
CheckSummary execute_check(const CheckOptions& opts);

} // namespace crivo::check
