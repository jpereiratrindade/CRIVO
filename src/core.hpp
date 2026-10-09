#pragma once
#include <string>
#include <vector>
namespace crivo {
struct TestDefinition {
  std::string id, name, category, subcategory, purpose, status, engine;
  std::vector<std::string> profiles, tags;
};
struct ExternalRunRecord {
  std::string evidence_id, project_id, mode, source_revision, source_worktree;
  std::string status, started_at, ended_at, adapter, adapter_version;
  std::string evidence_path, junit_path, junit_sha256;
  long long duration_ms{0};
  long long total{0}, passed{0}, failed{0}, skipped{0};
};
std::vector<TestDefinition> parse_catalog(const std::string& filename);
std::string serialize_services(const std::vector<TestDefinition>& catalog);
std::string serialize_service(const std::vector<TestDefinition>& catalog,
                              const std::string& id);
void initialize_db(const std::string& db);
void register_catalog(const std::string& db, const std::vector<TestDefinition>& catalog);
std::string serialize_catalog(const std::vector<TestDefinition>& catalog);
std::string query_json(const std::string& db, const std::string& name,
                       bool implemented_only=false);
std::string query_service_json(const std::string& db, const std::string& id,
                               bool implemented_only=false);
int run_profile(const std::string& db, const std::string& catalog_file, const std::string& profile);
void record_external_run(const std::string& db, const ExternalRunRecord& run);
std::string query_external_runs(const std::string& db);
void serve(const std::string& db, const std::string& web_directory,
           const std::string& bind_address, unsigned short port);
std::string json_escape(const std::string& s);
}
