#include "json_schema.hpp"

#include <set>

namespace crivo::json_schema {
namespace {

namespace json = boost::json;

bool type_matches(const json::value& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "integer") return value.is_int64() || value.is_uint64();
    if (type == "number") return value.is_number();
    if (type == "boolean") return value.is_bool();
    if (type == "null") return value.is_null();
    return false;
}

bool validate_impl(const json::value& instance, const json::value& schema,
                   std::string& error, const std::string& path) {
    if (!schema.is_object()) {
        error = path + ": schema node must be object";
        return false;
    }
    const auto& rule = schema.as_object();

    if (auto it = rule.find("type"); it != rule.end()) {
        bool matches = false;
        if (it->value().is_string()) {
            matches = type_matches(instance, std::string(it->value().as_string()));
        } else if (it->value().is_array()) {
            for (const auto& candidate : it->value().as_array()) {
                if (candidate.is_string() && type_matches(instance, std::string(candidate.as_string()))) {
                    matches = true;
                    break;
                }
            }
        } else {
            error = path + ": invalid type assertion in schema";
            return false;
        }
        if (!matches) {
            error = path + ": type mismatch";
            return false;
        }
    }

    if (auto it = rule.find("const"); it != rule.end() && instance != it->value()) {
        error = path + ": const mismatch";
        return false;
    }
    if (auto it = rule.find("enum"); it != rule.end()) {
        if (!it->value().is_array()) {
            error = path + ": invalid enum assertion in schema";
            return false;
        }
        bool found = false;
        for (const auto& candidate : it->value().as_array()) {
            if (instance == candidate) { found = true; break; }
        }
        if (!found) {
            error = path + ": value outside enum";
            return false;
        }
    }
    if (instance.is_string()) {
        if (auto it = rule.find("minLength"); it != rule.end()) {
            if (!it->value().is_int64() && !it->value().is_uint64()) {
                error = path + ": invalid minLength assertion in schema";
                return false;
            }
            auto minimum = it->value().is_uint64() ? it->value().as_uint64()
                                                    : static_cast<std::uint64_t>(it->value().as_int64());
            if (instance.as_string().size() < minimum) {
                error = path + ": string shorter than minLength";
                return false;
            }
        }
    }
    if (instance.is_number()) {
        if (auto it = rule.find("minimum"); it != rule.end()) {
            if (!it->value().is_number()) {
                error = path + ": invalid minimum assertion in schema";
                return false;
            }
            auto number = instance.is_double() ? instance.as_double() :
                (instance.is_int64() ? static_cast<double>(instance.as_int64()) : static_cast<double>(instance.as_uint64()));
            auto minimum = it->value().is_double() ? it->value().as_double() :
                (it->value().is_int64() ? static_cast<double>(it->value().as_int64()) : static_cast<double>(it->value().as_uint64()));
            if (number < minimum) {
                error = path + ": number below minimum";
                return false;
            }
        }
    }

    if (instance.is_object()) {
        const auto& object = instance.as_object();
        if (auto it = rule.find("required"); it != rule.end()) {
            if (!it->value().is_array()) {
                error = path + ": invalid required assertion in schema";
                return false;
            }
            for (const auto& name : it->value().as_array()) {
                if (!name.is_string() || !object.contains(name.as_string())) {
                    error = path + ": missing required property " +
                            (name.is_string() ? std::string(name.as_string()) : "<invalid>");
                    return false;
                }
            }
        }

        const json::object* properties = nullptr;
        if (auto it = rule.find("properties"); it != rule.end()) {
            if (!it->value().is_object()) {
                error = path + ": invalid properties assertion in schema";
                return false;
            }
            properties = &it->value().as_object();
            for (const auto& [name, child_schema] : *properties) {
                if (auto value = object.find(name); value != object.end() &&
                    !validate_impl(value->value(), child_schema, error, path + "." + std::string(name))) return false;
            }
        }
        if (auto it = rule.find("additionalProperties"); it != rule.end() &&
            it->value().is_bool() && !it->value().as_bool()) {
            for (const auto& [name, _] : object) {
                if (!properties || !properties->contains(name)) {
                    error = path + ": additional property forbidden: " + std::string(name);
                    return false;
                }
            }
        }
    }

    if (instance.is_array()) {
        if (auto it = rule.find("items"); it != rule.end()) {
            std::size_t index = 0;
            for (const auto& item : instance.as_array()) {
                if (!validate_impl(item, it->value(), error, path + "[" + std::to_string(index) + "]")) return false;
                ++index;
            }
        }
    }

    if (auto it = rule.find("allOf"); it != rule.end()) {
        if (!it->value().is_array()) {
            error = path + ": invalid allOf assertion in schema";
            return false;
        }
        for (const auto& child : it->value().as_array()) {
            if (!validate_impl(instance, child, error, path)) return false;
        }
    }
    if (auto condition = rule.find("if"); condition != rule.end()) {
        std::string ignored;
        if (validate_impl(instance, condition->value(), ignored, path)) {
            if (auto consequence = rule.find("then"); consequence != rule.end() &&
                !validate_impl(instance, consequence->value(), error, path)) return false;
        } else if (auto alternative = rule.find("else"); alternative != rule.end() &&
                   !validate_impl(instance, alternative->value(), error, path)) return false;
    }
    return true;
}

} // namespace

bool validate(const json::value& instance, const json::value& schema,
              std::string& error, const std::string& path) {
    error.clear();
    return validate_impl(instance, schema, error, path);
}

} // namespace crivo::json_schema
