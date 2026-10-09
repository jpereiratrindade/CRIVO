#pragma once

#include <string>
#include <vector>
#include <boost/json.hpp>
#include "inspector.hpp"
#include "memory.hpp"

namespace crivo::context {

struct DevContextReport {
    std::string schema_version = "crivo.dev-context/1.0.0";
    std::string target_id;
    std::string project_id;
    std::string identification_status = "provisional"; // "declared" ou "provisional"
    std::string memory_status = "NO_RELEVANT_EXPERIENCES"; // "RELEVANT_MATCHES" ou "NO_RELEVANT_EXPERIENCES"
    std::string source_revision = "unknown";
    std::string source_worktree = "unknown";
    std::string generated_at;
    std::vector<std::string> discovered_capabilities;
    std::vector<crivo::memory::ExperienceRecord> relevant_experiences;
    std::string recent_runs_json = "[]";
};

DevContextReport build_dev_context(
    const std::string& db_path,
    const std::string& target_path,
    const std::string& project_id_override = "",
    const std::string& query_term = "",
    const std::string& tag_filter = ""
);

std::string serialize_dev_context_json(const DevContextReport& report);

} // namespace crivo::context
