#include "applicability.hpp"

namespace crivo::registry {

ApplicabilityEvaluation evaluate_applicability(
    const std::vector<std::string>& required_capabilities,
    const EnvironmentCapabilities& env) {
  ApplicabilityEvaluation result;
  result.status = ApplicabilityStatus::APPLICABLE;

  for (const auto& cap : required_capabilities) {
    if (env.unsupported.find(cap) != env.unsupported.end()) {
      result.missing_capabilities.push_back(cap);
      result.status = ApplicabilityStatus::NOT_APPLICABLE;
    } else if (env.supported.find(cap) != env.supported.end()) {
      result.matched_capabilities.push_back(cap);
    } else {
      if (env.closed_world) {
        result.missing_capabilities.push_back(cap);
        result.status = ApplicabilityStatus::NOT_APPLICABLE;
      } else {
        result.unknown_capabilities.push_back(cap);
        if (result.status != ApplicabilityStatus::NOT_APPLICABLE) {
          result.status = ApplicabilityStatus::UNKNOWN;
        }
      }
    }
  }

  return result;
}

} // namespace crivo::registry
