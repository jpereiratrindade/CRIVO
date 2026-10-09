#pragma once
#include "types.hpp"
#include "validator.hpp"
#include <string>

namespace crivo::registry {

struct ImportResult {
  bool success{false};
  bool dry_run{false};
  size_t references_imported{0};
  size_t techniques_imported{0};
  size_t specifications_imported{0};
  size_t implementations_imported{0};
  size_t profiles_imported{0};
  size_t events_recorded{0};
  ValidationReport validation_report;
  std::string message;
};

ImportResult import_registry(const std::string& catalog_dir,
                             const std::string& db_path,
                             bool dry_run);

} // namespace crivo::registry
