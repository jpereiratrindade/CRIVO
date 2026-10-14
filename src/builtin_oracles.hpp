#pragma once

#include <filesystem>
#include <string>
#include <utility>

namespace crivo::oracles {

enum class Status { Pass, Fail, Blocked, NotApplicable };

struct Result {
  Status status{Status::Blocked};
  std::string message;
  std::string object;
  std::string stimulus;
  std::string observation;
  std::string criterion;
  std::string limitations;

  Result() = default;
  Result(Status value, std::string text) : status(value), message(std::move(text)) {}
  Result(Status value, std::string text, std::string target_object,
         std::string applied_stimulus, std::string observed,
         std::string applied_criterion, std::string limits)
      : status(value), message(std::move(text)), object(std::move(target_object)),
        stimulus(std::move(applied_stimulus)), observation(std::move(observed)),
        criterion(std::move(applied_criterion)), limitations(std::move(limits)) {}
};

Result run(const std::string& spec_id, const std::filesystem::path& target,
           const std::filesystem::path& workspace);
const char* to_string(Status status) noexcept;

} // namespace crivo::oracles
