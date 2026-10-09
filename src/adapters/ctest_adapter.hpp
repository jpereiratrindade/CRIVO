#pragma once
#include <string>
#include <vector>

namespace crivo::adapters {

struct CTestProperty {
  std::string name;
  std::string value;
};

struct DiscoveredTest {
  std::string name;
  std::vector<std::string> command;
  std::vector<CTestProperty> properties;
};

struct CTestDiscoveryResult {
  bool success{false};
  std::string schema_version{"crivo.adapter-ctest-discovery/1.0.0"};
  std::string project_id;
  std::string build_dir;
  std::string discovered_at;
  std::string ctest_version;
  size_t total_discovered{0};
  std::vector<DiscoveredTest> tests;
  std::string raw_json;
  std::string error_message;
};

struct CTestExecutionResult {
  bool success{false};
  std::string schema_version{"crivo.evidence/1.0.0"};
  std::string evidence_id;
  std::string project_id;
  std::string ctest_version;
  std::string start_utc;
  std::string end_utc;
  double duration_seconds{0.0};
  size_t total{0};
  size_t passed{0};
  size_t failed{0};
  size_t skipped{0};
  std::string junit_path;
  std::string junit_sha256;
  std::string evidence_json_path;
  std::string evidence_json_content;
  std::string error_message;
};

CTestDiscoveryResult discover_ctest(const std::string& build_dir,
                                    const std::string& project_id = "local-project");

CTestExecutionResult run_ctest(const std::string& build_dir,
                               const std::string& evidence_dir,
                               const std::string& project_id = "local-project");

std::string serialize_discovery_json(const CTestDiscoveryResult& disc);

} // namespace crivo::adapters
