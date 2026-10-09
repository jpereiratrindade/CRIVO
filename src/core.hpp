#pragma once
#include <string>
#include <vector>
namespace crivo {
struct TestDefinition {
  std::string id, name, category, subcategory, purpose, status, engine;
  std::vector<std::string> profiles, tags;
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
void serve(const std::string& db, const std::string& web_directory,
           const std::string& bind_address, unsigned short port);
std::string json_escape(const std::string& s);
}
