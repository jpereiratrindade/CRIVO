#pragma once

#include <filesystem>
#include <string>

namespace crivo::oracles {

enum class Status { Pass, Fail, Blocked, NotApplicable };

struct Result {
  Status status{Status::Blocked};
  std::string message;
};

Result run(const std::string& spec_id, const std::filesystem::path& target,
           const std::filesystem::path& workspace);
const char* to_string(Status status) noexcept;

} // namespace crivo::oracles
