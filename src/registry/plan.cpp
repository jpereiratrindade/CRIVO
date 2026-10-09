#include "plan.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace crivo::registry {

namespace {

namespace json = boost::json;

std::string current_utc_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

bool contains(const std::vector<std::string>& vec, const std::string& val) {
  return std::find(vec.begin(), vec.end(), val) != vec.end();
}

} // namespace

TestPlan resolve_test_plan(const ProfileRecord& profile,
                           const RegistryCollection& collection,
                           const EnvironmentCapabilities& env) {
  TestPlan plan;
  std::string now = current_utc_iso8601();
  plan.plan_id = "plan-" + profile.id + "-" + now;
  plan.created_at = now;
  plan.profile_id = profile.id;
  plan.profile_version = profile.version;
  plan.profile_title = profile.title;
  plan.environment = env;

  for (const auto& [spec_key, spec] : collection.specifications) {
    std::string full_spec_ref = spec.id + "@" + spec.version;

    // 1. Exclude specs check
    if (contains(profile.selection.exclude_specs, spec.id) ||
        contains(profile.selection.exclude_specs, full_spec_ref)) {
      continue;
    }

    // 2. Selection criteria
    bool selected = false;
    if (!profile.selection.include_specs.empty()) {
      if (contains(profile.selection.include_specs, spec.id) ||
          contains(profile.selection.include_specs, full_spec_ref)) {
        selected = true;
      }
    } else {
      selected = true;
    }

    if (selected && !profile.selection.categories.empty()) {
      if (!contains(profile.selection.categories, spec.category)) selected = false;
    }
    if (selected && !profile.selection.levels.empty()) {
      if (!contains(profile.selection.levels, spec.level)) selected = false;
    }
    if (selected && !profile.selection.purposes.empty()) {
      if (!contains(profile.selection.purposes, spec.purpose)) selected = false;
    }

    if (!selected) continue;

    // 3. Localizar implementação correspondente
    std::string impl_id = "unimplemented";
    std::string impl_version = "0.0.0";
    std::string adapter = "none";

    for (const auto& [ik, impl] : collection.implementations) {
      if (impl.implements_spec == full_spec_ref || impl.implements_spec == spec.id) {
        impl_id = impl.id;
        impl_version = impl.version;
        adapter = impl.adapter;
        break;
      }
    }

    // 4. Avaliar aplicabilidade
    auto eval = evaluate_applicability(spec.applicability, env);

    PlannedTestCase tc;
    tc.spec_id = spec.id;
    tc.spec_version = spec.version;
    tc.implementation_id = impl_id;
    tc.implementation_version = impl_version;
    tc.adapter = adapter;
    tc.applicability_status = eval.status;
    tc.required_capabilities = spec.applicability;

    if (eval.status == ApplicabilityStatus::APPLICABLE) {
      tc.applicability_reasons = {"Todas as capacidades requeridas estao disponiveis"};
      plan.summary.applicable_count++;
    } else if (eval.status == ApplicabilityStatus::NOT_APPLICABLE) {
      for (const auto& mc : eval.missing_capabilities) {
        tc.applicability_reasons.push_back("Capacidade ausente/nao suportada: " + mc);
      }
      plan.summary.not_applicable_count++;
    } else {
      for (const auto& uc : eval.unknown_capabilities) {
        tc.applicability_reasons.push_back("Capacidade de status indeterminado: " + uc);
      }
      plan.summary.unknown_count++;
    }

    plan.planned_tests.push_back(tc);
    plan.summary.total_selected++;
  }

  return plan;
}

std::string serialize_test_plan_json(const TestPlan& plan) {
  json::object root;
  root["schema_version"] = plan.schema_version;
  root["plan_id"] = plan.plan_id;
  root["created_at"] = plan.created_at;

  json::object prof;
  prof["id"] = plan.profile_id;
  prof["version"] = plan.profile_version;
  prof["title"] = plan.profile_title;
  root["profile"] = prof;

  json::object env;
  json::array supp;
  for (const auto& c : plan.environment.supported) supp.push_back(json::string(c));
  env["supported_capabilities"] = supp;
  json::array unsupp;
  for (const auto& c : plan.environment.unsupported) unsupp.push_back(json::string(c));
  env["unsupported_capabilities"] = unsupp;
  root["environment"] = env;

  json::object sum;
  sum["total_selected"] = plan.summary.total_selected;
  sum["applicable_count"] = plan.summary.applicable_count;
  sum["not_applicable_count"] = plan.summary.not_applicable_count;
  sum["unknown_count"] = plan.summary.unknown_count;
  root["summary"] = sum;

  json::array tests;
  for (const auto& tc : plan.planned_tests) {
    json::object o;
    o["spec_id"] = tc.spec_id;
    o["spec_version"] = tc.spec_version;
    o["implementation_id"] = tc.implementation_id;
    o["implementation_version"] = tc.implementation_version;
    o["adapter"] = tc.adapter;
    o["applicability_status"] = to_string(tc.applicability_status);

    json::array reasons;
    for (const auto& r : tc.applicability_reasons) reasons.push_back(json::string(r));
    o["applicability_reasons"] = reasons;

    json::array caps;
    for (const auto& c : tc.required_capabilities) caps.push_back(json::string(c));
    o["required_capabilities"] = caps;

    tests.push_back(o);
  }
  root["planned_tests"] = tests;

  return json::serialize(root);
}

} // namespace crivo::registry
