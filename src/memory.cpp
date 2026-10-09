#include "memory.hpp"
#include <boost/json.hpp>
#include <sqlite3.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace crivo::memory {

static std::string generate_utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string join_strings(const std::vector<std::string>& vec, const std::string& sep) {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) res += sep;
        res += vec[i];
    }
    return res;
}

static std::vector<std::string> split_comma(const std::string& s) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) res.push_back(item);
    }
    return res;
}

void initialize_memory_schema(const std::string& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Falha ao abrir SQLite para schema de memoria");
    }

    const char* schema_sql = R"(
        PRAGMA journal_mode=WAL;
        CREATE TABLE IF NOT EXISTS technical_experiences (
            experience_id TEXT PRIMARY KEY,
            project_id TEXT NOT NULL,
            source_revision TEXT,
            domain TEXT,
            language TEXT,
            platform TEXT,
            problem TEXT NOT NULL,
            choice TEXT NOT NULL,
            procedure TEXT NOT NULL,
            observed_result TEXT NOT NULL,
            evidence_id TEXT,
            evidence_sha256 TEXT,
            limitations TEXT,
            applicability_tags TEXT,
            created_at TEXT NOT NULL,
            payload_json TEXT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_exp_project ON technical_experiences(project_id);
        CREATE INDEX IF NOT EXISTS idx_exp_tags ON technical_experiences(applicability_tags);
    )";

    char* err = nullptr;
    if (sqlite3_exec(db, schema_sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string err_msg = err ? err : "erro desconhecido";
        sqlite3_free(err);
        sqlite3_close(db);
        throw std::runtime_error("Falha ao criar tabelas de memoria: " + err_msg);
    }
    sqlite3_close(db);
}

bool record_experience(const std::string& db_path, const ExperienceRecord& rec) {
    initialize_memory_schema(db_path);

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        return false;
    }

    const char* sql = R"(
        INSERT INTO technical_experiences (
            experience_id, project_id, source_revision, domain, language, platform,
            problem, choice, procedure, observed_result, evidence_id, evidence_sha256,
            limitations, applicability_tags, created_at, payload_json
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(experience_id) DO UPDATE SET
            project_id=excluded.project_id,
            source_revision=excluded.source_revision,
            domain=excluded.domain,
            language=excluded.language,
            platform=excluded.platform,
            problem=excluded.problem,
            choice=excluded.choice,
            procedure=excluded.procedure,
            observed_result=excluded.observed_result,
            evidence_id=excluded.evidence_id,
            evidence_sha256=excluded.evidence_sha256,
            limitations=excluded.limitations,
            applicability_tags=excluded.applicability_tags,
            created_at=excluded.created_at,
            payload_json=excluded.payload_json;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    std::string tags_str = join_strings(rec.applicability_tags, ",");
    std::string ts = rec.created_at.empty() ? generate_utc_timestamp() : rec.created_at;

    sqlite3_bind_text(stmt, 1, rec.experience_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.project_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rec.source_revision.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rec.domain.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, rec.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rec.platform.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, rec.problem.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, rec.choice.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, rec.procedure.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, rec.observed_result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, rec.evidence_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, rec.evidence_sha256.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, rec.limitations.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, tags_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, rec.raw_json.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

bool load_and_record_experience_file(const std::string& db_path, const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    boost::system::error_code ec;
    auto jv = boost::json::parse(content, ec);
    if (ec || !jv.is_object()) return false;

    auto obj = jv.as_object();
    if (!obj.contains("experience_id") || !obj.contains("project_id") ||
        !obj.contains("problem") || !obj.contains("choice") ||
        !obj.contains("procedure") || !obj.contains("observed_result")) {
        return false;
    }

    ExperienceRecord rec;
    rec.raw_json = content;
    rec.experience_id = obj["experience_id"].as_string().c_str();
    rec.project_id = obj["project_id"].as_string().c_str();
    rec.problem = obj["problem"].as_string().c_str();
    rec.choice = obj["choice"].as_string().c_str();
    rec.procedure = obj["procedure"].as_string().c_str();
    rec.observed_result = obj["observed_result"].as_string().c_str();

    if (obj.contains("source_revision")) rec.source_revision = obj["source_revision"].as_string().c_str();
    if (obj.contains("limitations")) rec.limitations = obj["limitations"].as_string().c_str();
    if (obj.contains("created_at")) rec.created_at = obj["created_at"].as_string().c_str();

    if (obj.contains("context") && obj["context"].is_object()) {
        auto ctx = obj["context"].as_object();
        if (ctx.contains("domain")) rec.domain = ctx["domain"].as_string().c_str();
        if (ctx.contains("language")) rec.language = ctx["language"].as_string().c_str();
        if (ctx.contains("platform")) rec.platform = ctx["platform"].as_string().c_str();
    }

    if (obj.contains("evidence_ref") && obj["evidence_ref"].is_object()) {
        auto ev = obj["evidence_ref"].as_object();
        if (ev.contains("evidence_id")) rec.evidence_id = ev["evidence_id"].as_string().c_str();
        if (ev.contains("sha256")) rec.evidence_sha256 = ev["sha256"].as_string().c_str();
    }

    if (obj.contains("applicability_tags") && obj["applicability_tags"].is_array()) {
        for (const auto& tag_val : obj["applicability_tags"].as_array()) {
            if (tag_val.is_string()) {
                rec.applicability_tags.push_back(tag_val.as_string().c_str());
            }
        }
    }

    return record_experience(db_path, rec);
}

std::vector<ExperienceRecord> query_experiences(
    const std::string& db_path,
    const std::string& search_term,
    const std::string& project_id,
    const std::string& tag)
{
    std::vector<ExperienceRecord> results;
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        return results;
    }

    std::string sql = "SELECT experience_id, project_id, source_revision, domain, language, platform, "
                      "problem, choice, procedure, observed_result, evidence_id, evidence_sha256, "
                      "limitations, applicability_tags, created_at, payload_json "
                      "FROM technical_experiences WHERE 1=1 ";

    if (!project_id.empty()) {
        sql += " AND project_id = ? ";
    }
    if (!tag.empty()) {
        sql += " AND applicability_tags LIKE ? ";
    }
    if (!search_term.empty()) {
        sql += " AND (problem LIKE ? OR choice LIKE ? OR procedure LIKE ? OR applicability_tags LIKE ?) ";
    }
    sql += " ORDER BY created_at DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return results;
    }

    int bind_idx = 1;
    if (!project_id.empty()) {
        sqlite3_bind_text(stmt, bind_idx++, project_id.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!tag.empty()) {
        std::string tag_pattern = "%" + tag + "%";
        sqlite3_bind_text(stmt, bind_idx++, tag_pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!search_term.empty()) {
        std::string term_pattern = "%" + search_term + "%";
        sqlite3_bind_text(stmt, bind_idx++, term_pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, term_pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, term_pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, bind_idx++, term_pattern.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ExperienceRecord rec;
        auto get_str = [&](int col) -> std::string {
            const auto* p = sqlite3_column_text(stmt, col);
            return p ? reinterpret_cast<const char*>(p) : "";
        };

        rec.experience_id = get_str(0);
        rec.project_id = get_str(1);
        rec.source_revision = get_str(2);
        rec.domain = get_str(3);
        rec.language = get_str(4);
        rec.platform = get_str(5);
        rec.problem = get_str(6);
        rec.choice = get_str(7);
        rec.procedure = get_str(8);
        rec.observed_result = get_str(9);
        rec.evidence_id = get_str(10);
        rec.evidence_sha256 = get_str(11);
        rec.limitations = get_str(12);
        rec.applicability_tags = split_comma(get_str(13));
        rec.created_at = get_str(14);
        rec.raw_json = get_str(15);

        results.push_back(std::move(rec));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return results;
}

std::string serialize_experiences_json(const std::vector<ExperienceRecord>& experiences) {
    boost::json::array arr;
    for (const auto& exp : experiences) {
        boost::json::object obj;
        obj["experience_id"] = exp.experience_id;
        obj["project_id"] = exp.project_id;
        obj["source_revision"] = exp.source_revision;
        obj["domain"] = exp.domain;
        obj["language"] = exp.language;
        obj["platform"] = exp.platform;
        obj["problem"] = exp.problem;
        obj["choice"] = exp.choice;
        obj["procedure"] = exp.procedure;
        obj["observed_result"] = exp.observed_result;
        obj["evidence_id"] = exp.evidence_id;
        obj["evidence_sha256"] = exp.evidence_sha256;
        obj["limitations"] = exp.limitations;
        obj["created_at"] = exp.created_at;

        boost::json::array tags_arr;
        for (const auto& t : exp.applicability_tags) {
            tags_arr.push_back(boost::json::value(t));
        }
        obj["applicability_tags"] = tags_arr;
        arr.push_back(obj);
    }
    return boost::json::serialize(arr);
}

} // namespace crivo::memory
