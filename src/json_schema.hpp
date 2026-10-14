#pragma once

#include <boost/json.hpp>
#include <string>

namespace crivo::json_schema {

// Validates the JSON Schema keywords used by CRIVO contracts. Unknown keywords
// are annotations; unsupported assertion keywords fail closed.
bool validate(const boost::json::value& instance,
              const boost::json::value& schema,
              std::string& error,
              const std::string& path = "$");

} // namespace crivo::json_schema
