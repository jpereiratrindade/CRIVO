#pragma once
#include <string>
#include <vector>
#include <set>

namespace crivo::registry {

enum class ApplicabilityStatus {
  APPLICABLE,
  NOT_APPLICABLE,
  UNKNOWN
};

inline std::string to_string(ApplicabilityStatus status) {
  switch (status) {
    case ApplicabilityStatus::APPLICABLE: return "APPLICABLE";
    case ApplicabilityStatus::NOT_APPLICABLE: return "NOT_APPLICABLE";
    case ApplicabilityStatus::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

struct EnvironmentCapabilities {
  std::set<std::string> supported;
  std::set<std::string> unsupported;
  bool closed_world{false}; // Se true, capacidades fora de supported são NOT_APPLICABLE; se false, são UNKNOWN
};

struct ApplicabilityEvaluation {
  ApplicabilityStatus status;
  std::vector<std::string> missing_capabilities;
  std::vector<std::string> unknown_capabilities;
  std::vector<std::string> matched_capabilities;
};

ApplicabilityEvaluation evaluate_applicability(
    const std::vector<std::string>& required_capabilities,
    const EnvironmentCapabilities& env);

} // namespace crivo::registry
