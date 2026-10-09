#include "validator.hpp"
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>

namespace crivo::registry {

namespace {

namespace json = boost::json;
namespace fs = std::filesystem;

bool is_valid_identifier(const std::string& s) {
  static const std::regex id_pattern("^[a-z0-9_.-]+$");
  return !s.empty() && std::regex_match(s, id_pattern);
}

bool is_valid_version(const std::string& s) {
  static const std::regex ver_pattern("^[0-9]+(\\.[0-9]+)*$");
  return !s.empty() && std::regex_match(s, ver_pattern);
}

void report_error(ValidationReport& report, const std::string& file, const std::string& code, const std::string& msg) {
  report.valid = false;
  report.errors.push_back({file, code, msg});
}

bool check_allowed_keys(const json::object& obj, const std::set<std::string>& allowed,
                        const std::string& file, ValidationReport& report) {
  bool ok = true;
  for (const auto& [key, _] : obj) {
    std::string k(key);
    if (allowed.find(k) == allowed.end()) {
      report_error(report, file, "ADDITIONAL_PROPERTIES_FORBIDDEN",
                   "Propriedade nao permitida encontrada: '" + k + "'");
      ok = false;
    }
  }
  return ok;
}

bool check_required_string(const json::object& obj, const std::string& key,
                           const std::string& file, ValidationReport& report,
                           std::string& out_val, bool non_empty = true) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo obrigatorio ausente: '" + key + "'");
    return false;
  }
  if (!it->value().is_string()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo '" + key + "' deve ser string");
    return false;
  }
  out_val = std::string(it->value().as_string());
  if (non_empty && out_val.empty()) {
    report_error(report, file, "EMPTY_REQUIRED_STRING", "Campo '" + key + "' nao pode ser vazio");
    return false;
  }
  return true;
}

bool check_optional_string(const json::object& obj, const std::string& key,
                           const std::string& file, ValidationReport& report,
                           std::optional<std::string>& out_val) {
  auto it = obj.find(key);
  if (it == obj.end()) {
    out_val = std::nullopt;
    return true;
  }
  if (!it->value().is_string()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo opcional '" + key + "' deve ser string");
    return false;
  }
  out_val = std::string(it->value().as_string());
  return true;
}

bool check_enum(const std::string& val, const std::set<std::string>& allowed_values,
                const std::string& key, const std::string& file, ValidationReport& report) {
  if (allowed_values.find(val) == allowed_values.end()) {
    report_error(report, file, "INVALID_ENUM_VALUE",
                 "Valor invalido '" + val + "' para o campo enumerado '" + key + "'");
    return false;
  }
  return true;
}

bool parse_rights(const json::value& v, const std::string& file, ValidationReport& report, RightsInfo& rights) {
  if (!v.is_object()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo 'rights' deve ser um objeto");
    return false;
  }
  const auto& obj = v.as_object();
  std::set<std::string> allowed_keys = {"distribution", "license_review", "attribution", "author"};
  if (!check_allowed_keys(obj, allowed_keys, file, report)) return false;

  if (!check_required_string(obj, "distribution", file, report, rights.distribution)) return false;
  std::set<std::string> valid_dist = {"metadata_only", "open_access", "permissive", "proprietary_restricted", "fair_use_summary"};
  if (!check_enum(rights.distribution, valid_dist, "rights.distribution", file, report)) return false;

  if (!check_required_string(obj, "license_review", file, report, rights.license_review)) return false;
  std::set<std::string> valid_rev = {"required", "verified_open", "not_applicable", "reviewed"};
  if (!check_enum(rights.license_review, valid_rev, "rights.license_review", file, report)) return false;

  check_optional_string(obj, "attribution", file, report, rights.attribution);
  check_optional_string(obj, "author", file, report, rights.author);
  return true;
}

bool parse_ref_links(const json::value& v, const std::string& file, ValidationReport& report,
                     std::vector<ReferenceLink>& links) {
  if (!v.is_array()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo 'references' deve ser um array");
    return false;
  }
  const auto& arr = v.as_array();
  bool ok = true;
  for (const auto& item : arr) {
    if (!item.is_object()) {
      report_error(report, file, "INVALID_FIELD_TYPE", "Elemento de 'references' deve ser objeto");
      ok = false;
      continue;
    }
    const auto& obj = item.as_object();
    std::set<std::string> allowed = {"id", "edition", "relation", "interpretation_limit"};
    if (!check_allowed_keys(obj, allowed, file, report)) {
      ok = false;
    }
    ReferenceLink link;
    if (!check_required_string(obj, "id", file, report, link.id)) ok = false;
    if (!check_required_string(obj, "edition", file, report, link.edition)) ok = false;
    if (!check_required_string(obj, "relation", file, report, link.relation)) ok = false;
    else {
      std::set<std::string> valid_rel = {
        "direct_reference", "derived_from", "informed_by",
        "classifies_quality", "checks_requirement", "tooling",
        "quality_attribute_mapping"
      };
      if (!check_enum(link.relation, valid_rel, "reference.relation", file, report)) ok = false;
    }
    if (!check_required_string(obj, "interpretation_limit", file, report, link.interpretation_limit)) ok = false;
    if (ok) links.push_back(link);
  }
  return ok;
}

bool parse_string_array(const json::value& v, const std::string& key, const std::string& file,
                        ValidationReport& report, std::vector<std::string>& out, bool min_one = false) {
  if (!v.is_array()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo '" + key + "' deve ser um array");
    return false;
  }
  const auto& arr = v.as_array();
  if (min_one && arr.empty()) {
    report_error(report, file, "ARRAY_EMPTY", "Campo '" + key + "' deve conter ao menos um elemento");
    return false;
  }
  bool ok = true;
  for (const auto& item : arr) {
    if (!item.is_string()) {
      report_error(report, file, "INVALID_FIELD_TYPE", "Elementos de '" + key + "' devem ser strings");
      ok = false;
      continue;
    }
    out.push_back(std::string(item.as_string()));
  }
  return ok;
}

bool validate_reference_doc(const json::object& obj, const std::string& file,
                            RegistryCollection& collection, ValidationReport& report) {
  std::set<std::string> allowed = {
    "schema_version", "id", "edition", "issuer", "kind", "title",
    "status_at_registration", "url", "rights", "catalog_level", "notes"
  };
  if (!check_allowed_keys(obj, allowed, file, report)) return false;

  ReferenceRecord rec;
  rec.schema_version = "crivo.reference/1.0.0";
  rec.file_path = file;

  if (!check_required_string(obj, "id", file, report, rec.id)) return false;
  if (!is_valid_identifier(rec.id)) {
    report_error(report, file, "INVALID_IDENTIFIER_FORMAT", "ID de referencia invalido: '" + rec.id + "'");
    return false;
  }
  if (!check_required_string(obj, "edition", file, report, rec.edition)) return false;
  if (!check_required_string(obj, "issuer", file, report, rec.issuer)) return false;
  if (!check_required_string(obj, "kind", file, report, rec.kind)) return false;
  std::set<std::string> valid_kinds = {
    "standard", "quality_model_standard", "testing_standard", "specification",
    "framework", "tool", "guide", "taxonomy", "report_format"
  };
  if (!check_enum(rec.kind, valid_kinds, "kind", file, report)) return false;

  if (!check_required_string(obj, "title", file, report, rec.title)) return false;
  check_optional_string(obj, "status_at_registration", file, report, rec.status_at_registration);
  if (rec.status_at_registration) {
    std::set<std::string> valid_status = {"published", "draft", "active", "deprecated", "maintained", "candidate"};
    if (!check_enum(*rec.status_at_registration, valid_status, "status_at_registration", file, report)) return false;
  }

  if (!check_required_string(obj, "url", file, report, rec.url)) return false;
  if (rec.url.find("http://") != 0 && rec.url.find("https://") != 0) {
    report_error(report, file, "INVALID_URI_FORMAT", "Campo 'url' deve iniciar com http:// ou https://");
    return false;
  }

  auto it_rights = obj.find("rights");
  if (it_rights == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'rights' obrigatorio ausente");
    return false;
  }
  if (!parse_rights(it_rights->value(), file, report, rec.rights)) return false;

  if (!check_required_string(obj, "catalog_level", file, report, rec.catalog_level)) return false;
  std::set<std::string> valid_levels = {"inventoried", "mapped", "operationalizable", "implemented", "evidenced"};
  if (!check_enum(rec.catalog_level, valid_levels, "catalog_level", file, report)) return false;

  check_optional_string(obj, "notes", file, report, rec.notes);

  auto key = std::make_pair(rec.id, rec.edition);
  if (collection.references.find(key) != collection.references.end()) {
    report_error(report, file, "DUPLICATE_ID_EDITION",
                 "Referencia duplicada detectada: id='" + rec.id + "', edition='" + rec.edition + "'");
    return false;
  }
  collection.references[key] = rec;
  report.references_count++;
  return true;
}

bool validate_technique_doc(const json::object& obj, const std::string& file,
                            RegistryCollection& collection, ValidationReport& report) {
  std::set<std::string> allowed = {
    "schema_version", "id", "version", "family", "name", "procedure_summary",
    "input_partitioning", "oracle_class", "applicability", "references", "rights"
  };
  if (!check_allowed_keys(obj, allowed, file, report)) return false;

  TechniqueRecord rec;
  rec.schema_version = "crivo.technique/1.0.0";
  rec.file_path = file;

  if (!check_required_string(obj, "id", file, report, rec.id)) return false;
  if (!is_valid_identifier(rec.id)) {
    report_error(report, file, "INVALID_IDENTIFIER_FORMAT", "ID de tecnica invalido: '" + rec.id + "'");
    return false;
  }
  if (!check_required_string(obj, "version", file, report, rec.version)) return false;
  if (!is_valid_version(rec.version)) {
    report_error(report, file, "INVALID_VERSION_FORMAT", "Versao de tecnica invalida: '" + rec.version + "'");
    return false;
  }
  if (!check_required_string(obj, "family", file, report, rec.family)) return false;
  std::set<std::string> valid_families = {
    "specification_based", "structure_based", "experience_based",
    "static_analysis", "dynamic_analysis", "fuzzing",
    "property_based", "metamorphic", "risk_based", "contract_based"
  };
  if (!check_enum(rec.family, valid_families, "family", file, report)) return false;

  if (!check_required_string(obj, "name", file, report, rec.name)) return false;
  if (!check_required_string(obj, "procedure_summary", file, report, rec.procedure_summary)) return false;
  check_optional_string(obj, "input_partitioning", file, report, rec.input_partitioning);

  if (!check_required_string(obj, "oracle_class", file, report, rec.oracle_class)) return false;
  std::set<std::string> valid_oracles = {
    "deterministic_predicate", "invariant_preservation", "crash_or_fault_freedom",
    "heuristic_bound", "differential_comparison", "static_rule_match", "manual_inspection"
  };
  if (!check_enum(rec.oracle_class, valid_oracles, "oracle_class", file, report)) return false;

  auto it_app = obj.find("applicability");
  if (it_app == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'applicability' obrigatorio ausente");
    return false;
  }
  if (!parse_string_array(it_app->value(), "applicability", file, report, rec.applicability, true)) return false;

  auto it_refs = obj.find("references");
  if (it_refs == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'references' obrigatorio ausente");
    return false;
  }
  if (!parse_ref_links(it_refs->value(), file, report, rec.references)) return false;

  auto it_rights = obj.find("rights");
  if (it_rights == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'rights' obrigatorio ausente");
    return false;
  }
  if (!parse_rights(it_rights->value(), file, report, rec.rights)) return false;

  auto key = std::make_pair(rec.id, rec.version);
  if (collection.techniques.find(key) != collection.techniques.end()) {
    report_error(report, file, "DUPLICATE_ID_VERSION",
                 "Tecnica duplicada detectada: id='" + rec.id + "', version='" + rec.version + "'");
    return false;
  }
  collection.techniques[key] = rec;
  report.techniques_count++;
  return true;
}

bool validate_test_spec_doc(const json::object& obj, const std::string& file,
                            RegistryCollection& collection, ValidationReport& report) {
  std::set<std::string> allowed = {
    "schema_version", "id", "version", "title", "description", "source",
    "references", "category", "subcategory", "techniques", "level", "purpose",
    "risk", "preconditions", "oracle", "implementation_refs", "applicability", "qualification"
  };
  if (!check_allowed_keys(obj, allowed, file, report)) return false;

  TestSpecRecord rec;
  rec.schema_version = "crivo.test-spec/1.0.0";
  rec.file_path = file;

  if (!check_required_string(obj, "id", file, report, rec.id)) return false;
  if (!is_valid_identifier(rec.id)) {
    report_error(report, file, "INVALID_IDENTIFIER_FORMAT", "ID de especificacao invalido: '" + rec.id + "'");
    return false;
  }
  if (!check_required_string(obj, "version", file, report, rec.version)) return false;
  if (!is_valid_version(rec.version)) {
    report_error(report, file, "INVALID_VERSION_FORMAT", "Versao de especificacao invalida: '" + rec.version + "'");
    return false;
  }
  check_optional_string(obj, "title", file, report, rec.title);
  check_optional_string(obj, "description", file, report, rec.description);

  if (!check_required_string(obj, "source", file, report, rec.source)) return false;
  std::set<std::string> valid_sources = {"crivo-adaptation", "crivo-original", "project-specific", "standard-derived"};
  if (!check_enum(rec.source, valid_sources, "source", file, report)) return false;

  auto it_refs = obj.find("references");
  if (it_refs == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'references' obrigatorio ausente");
    return false;
  }
  if (!parse_ref_links(it_refs->value(), file, report, rec.references)) return false;

  if (!check_required_string(obj, "category", file, report, rec.category)) return false;
  if (!check_required_string(obj, "subcategory", file, report, rec.subcategory)) return false;

  auto it_tech = obj.find("techniques");
  if (it_tech == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'techniques' obrigatorio ausente");
    return false;
  }
  if (!parse_string_array(it_tech->value(), "techniques", file, report, rec.techniques, true)) return false;

  if (!check_required_string(obj, "level", file, report, rec.level)) return false;
  std::set<std::string> valid_levels = {"unit", "component", "integration", "system", "acceptance"};
  if (!check_enum(rec.level, valid_levels, "level", file, report)) return false;

  if (!check_required_string(obj, "purpose", file, report, rec.purpose)) return false;
  std::set<std::string> valid_purposes = {"functional", "regression", "robustness", "performance", "security", "accessibility", "concurrency"};
  if (!check_enum(rec.purpose, valid_purposes, "purpose", file, report)) return false;

  auto it_risk = obj.find("risk");
  if (it_risk != obj.end()) {
    if (!parse_string_array(it_risk->value(), "risk", file, report, rec.risk)) return false;
  }

  auto it_prec = obj.find("preconditions");
  if (it_prec != obj.end()) {
    if (!parse_string_array(it_prec->value(), "preconditions", file, report, rec.preconditions)) return false;
  }

  auto it_oracle = obj.find("oracle");
  if (it_oracle == obj.end() || !it_oracle->value().is_object()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'oracle' obrigatorio deve ser objeto");
    return false;
  }
  const auto& o_obj = it_oracle->value().as_object();
  std::set<std::string> allowed_o = {"type", "id", "description"};
  if (!check_allowed_keys(o_obj, allowed_o, file, report)) return false;
  if (!check_required_string(o_obj, "type", file, report, rec.oracle.type)) return false;
  std::set<std::string> valid_otypes = {"predicate", "invariant", "status_code", "error_match", "exit_code", "static_check"};
  if (!check_enum(rec.oracle.type, valid_otypes, "oracle.type", file, report)) return false;
  if (!check_required_string(o_obj, "id", file, report, rec.oracle.id)) return false;
  check_optional_string(o_obj, "description", file, report, rec.oracle.description);

  auto it_impl = obj.find("implementation_refs");
  if (it_impl != obj.end()) {
    if (!parse_string_array(it_impl->value(), "implementation_refs", file, report, rec.implementation_refs)) return false;
  }

  auto it_app = obj.find("applicability");
  if (it_app == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'applicability' obrigatorio ausente");
    return false;
  }
  if (!parse_string_array(it_app->value(), "applicability", file, report, rec.applicability, true)) return false;

  if (!check_required_string(obj, "qualification", file, report, rec.qualification)) return false;
  std::set<std::string> valid_quals = {"candidate", "qualified", "provisional", "deprecated"};
  if (!check_enum(rec.qualification, valid_quals, "qualification", file, report)) return false;

  auto key = std::make_pair(rec.id, rec.version);
  if (collection.specifications.find(key) != collection.specifications.end()) {
    report_error(report, file, "DUPLICATE_ID_VERSION",
                 "Especificacao duplicada detectada: id='" + rec.id + "', version='" + rec.version + "'");
    return false;
  }
  collection.specifications[key] = rec;
  report.specifications_count++;
  return true;
}

bool validate_implementation_doc(const json::object& obj, const std::string& file,
                                 RegistryCollection& collection, ValidationReport& report) {
  std::set<std::string> allowed = {
    "schema_version", "id", "version", "implements", "adapter",
    "requires", "execution_policy", "qualification", "entrypoint", "notes"
  };
  if (!check_allowed_keys(obj, allowed, file, report)) return false;

  ImplementationRecord rec;
  rec.schema_version = "crivo.implementation/1.0.0";
  rec.file_path = file;

  if (!check_required_string(obj, "id", file, report, rec.id)) return false;
  if (!is_valid_identifier(rec.id)) {
    report_error(report, file, "INVALID_IDENTIFIER_FORMAT", "ID de implementacao invalido: '" + rec.id + "'");
    return false;
  }
  if (!check_required_string(obj, "version", file, report, rec.version)) return false;
  if (!is_valid_version(rec.version)) {
    report_error(report, file, "INVALID_VERSION_FORMAT", "Versao de implementacao invalida: '" + rec.version + "'");
    return false;
  }
  if (!check_required_string(obj, "implements", file, report, rec.implements_spec)) return false;

  if (!check_required_string(obj, "adapter", file, report, rec.adapter)) return false;
  std::set<std::string> valid_adapters = {"builtin", "ctest", "gtest", "custom_script", "http_runner"};
  if (!check_enum(rec.adapter, valid_adapters, "adapter", file, report)) return false;

  auto it_req = obj.find("requires");
  if (it_req == obj.end()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'requires' obrigatorio ausente");
    return false;
  }
  if (!parse_string_array(it_req->value(), "requires", file, report, rec.requires_deps)) return false;

  auto it_ep = obj.find("execution_policy");
  if (it_ep == obj.end() || !it_ep->value().is_object()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'execution_policy' deve ser objeto");
    return false;
  }
  const auto& ep_obj = it_ep->value().as_object();
  std::set<std::string> allowed_ep = {"destructive", "isolation", "network"};
  if (!check_allowed_keys(ep_obj, allowed_ep, file, report)) return false;

  auto it_destr = ep_obj.find("destructive");
  if (it_destr == ep_obj.end() || !it_destr->value().is_bool()) {
    report_error(report, file, "INVALID_FIELD_TYPE", "Campo 'execution_policy.destructive' deve ser booleano");
    return false;
  }
  rec.execution_policy.destructive = it_destr->value().as_bool();

  if (!check_required_string(ep_obj, "isolation", file, report, rec.execution_policy.isolation)) return false;
  std::set<std::string> valid_iso = {"ephemeral", "process_isolated", "filesystem_sandbox", "none"};
  if (!check_enum(rec.execution_policy.isolation, valid_iso, "execution_policy.isolation", file, report)) return false;

  if (!check_required_string(ep_obj, "network", file, report, rec.execution_policy.network)) return false;
  std::set<std::string> valid_net = {"none", "loopback_only", "full"};
  if (!check_enum(rec.execution_policy.network, valid_net, "execution_policy.network", file, report)) return false;

  if (!check_required_string(obj, "qualification", file, report, rec.qualification)) return false;
  std::set<std::string> valid_quals = {"candidate", "qualified", "provisional", "deprecated"};
  if (!check_enum(rec.qualification, valid_quals, "qualification", file, report)) return false;

  check_optional_string(obj, "entrypoint", file, report, rec.entrypoint);
  check_optional_string(obj, "notes", file, report, rec.notes);

  auto key = std::make_pair(rec.id, rec.version);
  if (collection.implementations.find(key) != collection.implementations.end()) {
    report_error(report, file, "DUPLICATE_ID_VERSION",
                 "Implementacao duplicada detectada: id='" + rec.id + "', version='" + rec.version + "'");
    return false;
  }
  collection.implementations[key] = rec;
  report.implementations_count++;
  return true;
}

bool validate_profile_doc(const json::object& obj, const std::string& file,
                          RegistryCollection& collection, ValidationReport& report) {
  std::set<std::string> allowed = {
    "schema_version", "id", "version", "title", "description", "selection", "policy"
  };
  if (!check_allowed_keys(obj, allowed, file, report)) return false;

  ProfileRecord rec;
  rec.schema_version = "crivo.profile/1.0.0";
  rec.file_path = file;

  if (!check_required_string(obj, "id", file, report, rec.id)) return false;
  if (!is_valid_identifier(rec.id)) {
    report_error(report, file, "INVALID_IDENTIFIER_FORMAT", "ID de perfil invalido: '" + rec.id + "'");
    return false;
  }
  if (!check_required_string(obj, "version", file, report, rec.version)) return false;
  if (!is_valid_version(rec.version)) {
    report_error(report, file, "INVALID_VERSION_FORMAT", "Versao de perfil invalida: '" + rec.version + "'");
    return false;
  }
  if (!check_required_string(obj, "title", file, report, rec.title)) return false;
  check_optional_string(obj, "description", file, report, rec.description);

  auto it_sel = obj.find("selection");
  if (it_sel == obj.end() || !it_sel->value().is_object()) {
    report_error(report, file, "MISSING_REQUIRED_PROPERTY", "Campo 'selection' deve ser objeto");
    return false;
  }
  const auto& sel_obj = it_sel->value().as_object();
  std::set<std::string> allowed_sel = {"categories", "levels", "purposes", "include_specs", "exclude_specs"};
  if (!check_allowed_keys(sel_obj, allowed_sel, file, report)) return false;

  auto it_cat = sel_obj.find("categories");
  if (it_cat != sel_obj.end()) parse_string_array(it_cat->value(), "selection.categories", file, report, rec.selection.categories);
  auto it_lev = sel_obj.find("levels");
  if (it_lev != sel_obj.end()) parse_string_array(it_lev->value(), "selection.levels", file, report, rec.selection.levels);
  auto it_pur = sel_obj.find("purposes");
  if (it_pur != sel_obj.end()) parse_string_array(it_pur->value(), "selection.purposes", file, report, rec.selection.purposes);
  auto it_inc = sel_obj.find("include_specs");
  if (it_inc != sel_obj.end()) parse_string_array(it_inc->value(), "selection.include_specs", file, report, rec.selection.include_specs);
  auto it_exc = sel_obj.find("exclude_specs");
  if (it_exc != sel_obj.end()) parse_string_array(it_exc->value(), "selection.exclude_specs", file, report, rec.selection.exclude_specs);

  auto it_pol = obj.find("policy");
  if (it_pol != obj.end()) {
    if (!it_pol->value().is_object()) {
      report_error(report, file, "INVALID_FIELD_TYPE", "Campo 'policy' deve ser objeto");
      return false;
    }
    const auto& pol_obj = it_pol->value().as_object();
    std::set<std::string> allowed_pol = {"fail_fast", "allow_unqualified"};
    if (!check_allowed_keys(pol_obj, allowed_pol, file, report)) return false;

    auto it_ff = pol_obj.find("fail_fast");
    if (it_ff != pol_obj.end() && it_ff->value().is_bool()) rec.policy.fail_fast = it_ff->value().as_bool();
    auto it_au = pol_obj.find("allow_unqualified");
    if (it_au != pol_obj.end() && it_au->value().is_bool()) rec.policy.allow_unqualified = it_au->value().as_bool();
  }

  auto key = std::make_pair(rec.id, rec.version);
  if (collection.profiles.find(key) != collection.profiles.end()) {
    report_error(report, file, "DUPLICATE_ID_VERSION",
                 "Perfil duplicado detectado: id='" + rec.id + "', version='" + rec.version + "'");
    return false;
  }
  collection.profiles[key] = rec;
  report.profiles_count++;
  return true;
}

} // namespace

bool validate_file(const std::string& filepath,
                   RegistryCollection& collection,
                   ValidationReport& report) {
  if (!fs::exists(filepath)) {
    report_error(report, filepath, "FILE_NOT_FOUND", "Arquivo nao encontrado: " + filepath);
    return false;
  }

  std::ifstream f(filepath);
  if (!f) {
    report_error(report, filepath, "FILE_READ_ERROR", "Nao foi possivel abrir o arquivo: " + filepath);
    return false;
  }

  std::stringstream buffer;
  buffer << f.rdbuf();
  std::string content = buffer.str();

  boost::system::error_code ec;
  json::value jv = json::parse(content, ec);
  if (ec) {
    report_error(report, filepath, "JSON_SYNTAX_ERROR", "Erro de sintaxe JSON: " + ec.message());
    return false;
  }

  if (!jv.is_object()) {
    report_error(report, filepath, "INVALID_ROOT_TYPE", "Raiz do documento JSON deve ser um objeto");
    return false;
  }

  const auto& obj = jv.as_object();
  auto it_ver = obj.find("schema_version");
  if (it_ver == obj.end()) {
    report_error(report, filepath, "MISSING_SCHEMA_VERSION", "Campo 'schema_version' ausente");
    return false;
  }
  if (!it_ver->value().is_string()) {
    report_error(report, filepath, "INVALID_SCHEMA_VERSION_TYPE", "Campo 'schema_version' deve ser string");
    return false;
  }

  std::string schema_ver(it_ver->value().as_string());
  if (schema_ver == "crivo.reference/1.0.0") {
    return validate_reference_doc(obj, filepath, collection, report);
  } else if (schema_ver == "crivo.technique/1.0.0") {
    return validate_technique_doc(obj, filepath, collection, report);
  } else if (schema_ver == "crivo.test-spec/1.0.0") {
    return validate_test_spec_doc(obj, filepath, collection, report);
  } else if (schema_ver == "crivo.implementation/1.0.0") {
    return validate_implementation_doc(obj, filepath, collection, report);
  } else if (schema_ver == "crivo.profile/1.0.0") {
    return validate_profile_doc(obj, filepath, collection, report);
  } else {
    report_error(report, filepath, "UNKNOWN_SCHEMA_VERSION", "Schema version desconhecido: '" + schema_ver + "'");
    return false;
  }
}

bool resolve_cross_references(const RegistryCollection& collection, ValidationReport& report) {
  bool ok = true;

  // 1. Validar referências de Techniques -> References
  for (const auto& [tech_key, tech] : collection.techniques) {
    for (const auto& ref_link : tech.references) {
      auto ref_key = std::make_pair(ref_link.id, ref_link.edition);
      if (collection.references.find(ref_key) == collection.references.end()) {
        report_error(report, tech.file_path, "UNRESOLVED_REFERENCE",
                     "Tecnica '" + tech.id + "@" + tech.version + "' referencia fonte inexistente: '" +
                     ref_link.id + "@" + ref_link.edition + "'");
        ok = false;
      }
    }
  }

  // 2. Validar referências de TestSpecs -> References e Techniques
  for (const auto& [spec_key, spec] : collection.specifications) {
    for (const auto& ref_link : spec.references) {
      auto ref_key = std::make_pair(ref_link.id, ref_link.edition);
      if (collection.references.find(ref_key) == collection.references.end()) {
        report_error(report, spec.file_path, "UNRESOLVED_REFERENCE",
                     "Especificacao '" + spec.id + "@" + spec.version + "' referencia fonte inexistente: '" +
                     ref_link.id + "@" + ref_link.edition + "'");
        ok = false;
      }
    }
    for (const auto& tech_id : spec.techniques) {
      bool found = false;
      for (const auto& [tk, _] : collection.techniques) {
        if (tk.first == tech_id) { found = true; break; }
      }
      if (!found) {
        report_error(report, spec.file_path, "UNRESOLVED_TECHNIQUE",
                     "Especificacao '" + spec.id + "@" + spec.version + "' referencia tecnica inexistente: '" +
                     tech_id + "'");
        ok = false;
      }
    }
  }

  // 3. Validar referências de Implementations -> TestSpecs
  for (const auto& [impl_key, impl] : collection.implementations) {
    std::string target_spec = impl.implements_spec;
    std::string spec_id = target_spec;
    std::string spec_ver = "";
    auto at_pos = target_spec.find('@');
    if (at_pos != std::string::npos) {
      spec_id = target_spec.substr(0, at_pos);
      spec_ver = target_spec.substr(at_pos + 1);
    }
    bool found = false;
    for (const auto& [sk, _] : collection.specifications) {
      if (sk.first == spec_id && (spec_ver.empty() || sk.second == spec_ver)) {
        found = true;
        break;
      }
    }
    if (!found) {
      report_error(report, impl.file_path, "UNRESOLVED_SPECIFICATION",
                   "Implementacao '" + impl.id + "@" + impl.version + "' declara implementar especificacao inexistente: '" +
                   target_spec + "'");
      ok = false;
    }
  }

  // 4. Validar referências de Profiles -> TestSpecs
  for (const auto& [prof_key, prof] : collection.profiles) {
    for (const auto& target_spec : prof.selection.include_specs) {
      std::string spec_id = target_spec;
      std::string spec_ver = "";
      auto at_pos = target_spec.find('@');
      if (at_pos != std::string::npos) {
        spec_id = target_spec.substr(0, at_pos);
        spec_ver = target_spec.substr(at_pos + 1);
      }
      bool found = false;
      for (const auto& [sk, _] : collection.specifications) {
        if (sk.first == spec_id && (spec_ver.empty() || sk.second == spec_ver)) {
          found = true;
          break;
        }
      }
      if (!found) {
        report_error(report, prof.file_path, "UNRESOLVED_SPECIFICATION",
                     "Perfil '" + prof.id + "@" + prof.version + "' inclui especificacao inexistente: '" +
                     target_spec + "'");
        ok = false;
      }
    }
  }

  return ok;
}

ValidationReport validate_catalog(const std::string& catalog_dir) {
  ValidationReport report;
  RegistryCollection collection;

  if (!fs::exists(catalog_dir) || !fs::is_directory(catalog_dir)) {
    report_error(report, catalog_dir, "DIRECTORY_NOT_FOUND", "Diretorio de catalogo nao encontrado: " + catalog_dir);
    return report;
  }

  std::vector<std::string> subdirs = {
    "references", "techniques", "specifications", "implementations", "profiles"
  };

  for (const auto& sub : subdirs) {
    fs::path subpath = fs::path(catalog_dir) / sub;
    if (fs::exists(subpath) && fs::is_directory(subpath)) {
      for (const auto& entry : fs::recursive_directory_iterator(subpath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
          validate_file(entry.path().string(), collection, report);
        }
      }
    }
  }

  if (report.valid) {
    resolve_cross_references(collection, report);
  }

  return report;
}

} // namespace crivo::registry
