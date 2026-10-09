#pragma once

#include <string>
#include <vector>

namespace crivo::memory {

struct ExperienceRecord {
    std::string experience_id;
    std::string project_id;
    std::string source_revision;
    std::string domain;
    std::string language;
    std::string platform;
    std::string problem;
    std::string choice;
    std::string procedure;
    std::string observed_result;
    std::string evidence_id;
    std::string evidence_sha256;
    std::string limitations;
    std::string created_at;
    std::vector<std::string> applicability_tags;
    std::string raw_json;
};

void initialize_memory_schema(const std::string& db_path);
bool record_experience(const std::string& db_path, const ExperienceRecord& record);
bool load_and_record_experience_file(const std::string& db_path, const std::string& filepath);
std::vector<ExperienceRecord> query_experiences(
    const std::string& db_path,
    const std::string& search_term = "",
    const std::string& project_id = "",
    const std::string& tag = "");
std::string serialize_experiences_json(const std::vector<ExperienceRecord>& experiences);

} // namespace crivo::memory
