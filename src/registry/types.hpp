#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>

namespace crivo::registry {

struct RightsInfo {
  std::string distribution;
  std::string license_review;
  std::optional<std::string> attribution;
  std::optional<std::string> author;
};

struct ReferenceLink {
  std::string id;
  std::string edition;
  std::string relation;
  std::string interpretation_limit;
};

struct ReferenceRecord {
  std::string schema_version;
  std::string id;
  std::string edition;
  std::string issuer;
  std::string kind;
  std::string title;
  std::optional<std::string> status_at_registration;
  std::string url;
  RightsInfo rights;
  std::string catalog_level;
  std::optional<std::string> notes;
  std::string file_path;
};

struct TechniqueRecord {
  std::string schema_version;
  std::string id;
  std::string version;
  std::string family;
  std::string name;
  std::string procedure_summary;
  std::optional<std::string> input_partitioning;
  std::string oracle_class;
  std::vector<std::string> applicability;
  std::vector<ReferenceLink> references;
  RightsInfo rights;
  std::string file_path;
};

struct OracleInfo {
  std::string type;
  std::string id;
  std::optional<std::string> description;
};

struct TestSpecRecord {
  std::string schema_version;
  std::string id;
  std::string version;
  std::optional<std::string> title;
  std::optional<std::string> description;
  std::string source;
  std::vector<ReferenceLink> references;
  std::string category;
  std::string subcategory;
  std::vector<std::string> techniques;
  std::string level;
  std::string purpose;
  std::vector<std::string> risk;
  std::vector<std::string> preconditions;
  OracleInfo oracle;
  std::vector<std::string> implementation_refs;
  std::vector<std::string> applicability;
  std::string qualification;
  std::string file_path;
};

struct ExecutionPolicyInfo {
  bool destructive{false};
  std::string isolation;
  std::string network;
};

struct ImplementationRecord {
  std::string schema_version;
  std::string id;
  std::string version;
  std::string implements_spec;
  std::string adapter;
  std::vector<std::string> requires_deps;
  ExecutionPolicyInfo execution_policy;
  std::string qualification;
  std::optional<std::string> entrypoint;
  std::optional<std::string> notes;
  std::string file_path;
};

struct ProfileSelection {
  std::vector<std::string> categories;
  std::vector<std::string> levels;
  std::vector<std::string> purposes;
  std::vector<std::string> include_specs;
  std::vector<std::string> exclude_specs;
};

struct ProfilePolicy {
  bool fail_fast{false};
  bool allow_unqualified{false};
};

struct ProfileRecord {
  std::string schema_version;
  std::string id;
  std::string version;
  std::string title;
  std::optional<std::string> description;
  ProfileSelection selection;
  ProfilePolicy policy;
  std::string file_path;
};

struct RegistryCollection {
  std::map<std::pair<std::string, std::string>, ReferenceRecord> references;       // (id, edition) -> ref
  std::map<std::pair<std::string, std::string>, TechniqueRecord> techniques;       // (id, version) -> technique
  std::map<std::pair<std::string, std::string>, TestSpecRecord> specifications;    // (id, version) -> spec
  std::map<std::pair<std::string, std::string>, ImplementationRecord> implementations; // (id, version) -> impl
  std::map<std::pair<std::string, std::string>, ProfileRecord> profiles;           // (id, version) -> profile
};

struct ValidationError {
  std::string file_path;
  std::string error_code;
  std::string message;
};

struct ValidationReport {
  bool valid{true};
  std::vector<ValidationError> errors;
  std::vector<std::string> warnings;
  size_t references_count{0};
  size_t techniques_count{0};
  size_t specifications_count{0};
  size_t implementations_count{0};
  size_t profiles_count{0};
};

} // namespace crivo::registry
