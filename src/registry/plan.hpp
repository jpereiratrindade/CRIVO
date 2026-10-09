#pragma once
#include "types.hpp"
#include "applicability.hpp"
#include <string>
#include <vector>

namespace crivo::registry {

struct PlannedTestCase {
  std::string spec_id;
  std::string spec_version;
  std::string implementation_id;
  std::string implementation_version;
  std::string adapter;
  ApplicabilityStatus applicability_status;
  std::vector<std::string> applicability_reasons;
  std::vector<std::string> required_capabilities;
};

struct TestPlanSummary {
  size_t total_selected{0};
  size_t applicable_count{0};
  size_t not_applicable_count{0};
  size_t unknown_count{0};
};

struct TestPlan {
  std::string schema_version{"crivo.test-plan/1.0.0"};
  std::string plan_id;
  std::string created_at;
  std::string profile_id;
  std::string profile_version;
  std::string profile_title;
  EnvironmentCapabilities environment;
  TestPlanSummary summary;
  std::vector<PlannedTestCase> planned_tests;
};

TestPlan resolve_test_plan(const ProfileRecord& profile,
                           const RegistryCollection& collection,
                           const EnvironmentCapabilities& env);

std::string serialize_test_plan_json(const TestPlan& plan);

} // namespace crivo::registry
