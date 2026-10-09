#include "dev_context.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <sqlite3.h>

namespace crivo::context {

namespace fs = std::filesystem;

static std::string now_utc() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string compute_path_fingerprint(const std::filesystem::path& p) {
    try {
        std::string can = std::filesystem::canonical(p).string();
        std::hash<std::string> hasher;
        size_t h = hasher(can);
        std::stringstream ss;
        ss << std::hex << std::setw(12) << std::setfill('0') << (h & 0xFFFFFFFFFFFFULL);
        return ss.str();
    } catch (...) {
        return "unresolved";
    }
}

static std::string detect_declared_project_name(const std::filesystem::path& p) {
    // 1. Checar se existe manifesto crivo.project.json
    fs::path crivo_proj = p / "crivo.project.json";
    if (fs::exists(crivo_proj)) {
        try {
            std::ifstream f(crivo_proj);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto j = boost::json::parse(content);
            if (j.is_object() && j.as_object().contains("project_id") && j.as_object().at("project_id").is_string()) {
                return std::string(j.as_object().at("project_id").as_string());
            }
        } catch (...) {}
    }
    return "";
}

DevContextReport build_dev_context(
    const std::string& db_path,
    const std::string& target_path,
    const std::string& project_id_override,
    const std::string& query_term,
    const std::string& tag_filter)
{
    DevContextReport report;
    report.generated_at = now_utc();

    std::filesystem::path p(target_path.empty() ? "." : target_path);
    std::string fingerprint = compute_path_fingerprint(p);
    report.target_id = "target-" + fingerprint;
    report.source_worktree = p.string();

    // Identificação do projeto sem viés
    if (!project_id_override.empty()) {
        report.project_id = project_id_override;
        report.identification_status = "declared";
    } else {
        std::string declared = detect_declared_project_name(p);
        if (!declared.empty()) {
            report.project_id = declared;
            report.identification_status = "declared";
        } else {
            report.project_id = "provisional:" + report.target_id;
            report.identification_status = "provisional";
        }
    }

    // 1. Descobrir capacidades observáveis
    try {
        if (std::filesystem::exists(p)) {
            auto info = crivo::check::inspect_target(p);
            report.discovered_capabilities = std::move(info.discovered_capabilities);
            report.source_revision = info.revision;
        }
    } catch (...) {}

    // 2. Buscar experiências relevantes com controle de escopo e sem vazamento global
    std::vector<crivo::memory::ExperienceRecord> exps;
    if (!query_term.empty() || !tag_filter.empty()) {
        exps = crivo::memory::query_experiences(db_path, query_term, "", tag_filter);
    } else {
        // Busca orientada exclusivamente pelas capacidades técnicas observadas
        for (const auto& cap : report.discovered_capabilities) {
            auto matches = crivo::memory::query_experiences(db_path, "", "", cap);
            for (auto& m : matches) {
                bool exists = false;
                for (const auto& e : exps) {
                    if (e.experience_id == m.experience_id) { exists = true; break; }
                }
                if (!exists) {
                    exps.push_back(std::move(m));
                    if (exps.size() >= 10) break; // Limite de orçamento de contexto
                }
            }
            if (exps.size() >= 10) break;
        }
    }

    if (exps.empty()) {
        report.memory_status = "NO_RELEVANT_EXPERIENCES";
    } else {
        report.memory_status = "RELEVANT_MATCHES";
    }
    report.relevant_experiences = std::move(exps);

    // 3. Buscar execuções recentes (apenas do projeto identificado, ou vazias para alvos provisórios)
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        std::string sql = "SELECT r.run_id, r.project_id, r.mode, r.status, r.duration_ms, "
                          "COALESCE(e.artifact_sha256, ''), r.started_at "
                          "FROM external_runs r LEFT JOIN evidence_index e ON e.run_id=r.run_id "
                          "WHERE r.project_id = ? "
                          "ORDER BY r.started_at DESC LIMIT 5;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, report.project_id.c_str(), -1, SQLITE_TRANSIENT);

            boost::json::array runs_arr;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                boost::json::object r_obj;
                auto get_s = [&](int col) -> std::string {
                    const auto* txt = sqlite3_column_text(stmt, col);
                    return txt ? reinterpret_cast<const char*>(txt) : "";
                };
                r_obj["run_id"] = boost::json::string_view(get_s(0));
                r_obj["project_id"] = boost::json::string_view(get_s(1));
                r_obj["mode"] = boost::json::string_view(get_s(2));
                r_obj["status"] = boost::json::string_view(get_s(3));
                r_obj["duration_ms"] = sqlite3_column_int64(stmt, 4);
                r_obj["artifact_sha256"] = boost::json::string_view(get_s(5));
                r_obj["started_at"] = boost::json::string_view(get_s(6));
                runs_arr.push_back(std::move(r_obj));
            }
            sqlite3_finalize(stmt);
            report.recent_runs_json = boost::json::serialize(runs_arr);
        }
        sqlite3_close(db);
    }

    return report;
}

std::string serialize_dev_context_json(const DevContextReport& report) {
    boost::json::object root;
    root["schema_version"] = boost::json::string_view(report.schema_version);
    root["target_id"] = boost::json::string_view(report.target_id);
    root["project_id"] = boost::json::string_view(report.project_id);
    root["identification_status"] = boost::json::string_view(report.identification_status);
    root["memory_status"] = boost::json::string_view(report.memory_status);
    root["source_revision"] = boost::json::string_view(report.source_revision);
    root["source_worktree"] = boost::json::string_view(report.source_worktree);
    root["generated_at"] = boost::json::string_view(report.generated_at);

    boost::json::array caps_arr;
    for (const auto& cap : report.discovered_capabilities) {
        caps_arr.push_back(boost::json::value(boost::json::string_view(cap)));
    }
    root["discovered_capabilities"] = std::move(caps_arr);

    boost::json::array exps_arr;
    for (const auto& exp : report.relevant_experiences) {
        boost::json::object e_obj;
        e_obj["experience_id"] = boost::json::string_view(exp.experience_id);
        e_obj["project_id"] = boost::json::string_view(exp.project_id);
        e_obj["domain"] = boost::json::string_view(exp.domain);
        e_obj["problem"] = boost::json::string_view(exp.problem);
        e_obj["choice"] = boost::json::string_view(exp.choice);
        e_obj["observed_result"] = boost::json::string_view(exp.observed_result);
        e_obj["evidence_sha256"] = boost::json::string_view(exp.evidence_sha256);

        boost::json::array tags;
        for (const auto& t : exp.applicability_tags) {
            tags.push_back(boost::json::value(boost::json::string_view(t)));
        }
        e_obj["applicability_tags"] = std::move(tags);
        exps_arr.push_back(std::move(e_obj));
    }
    root["relevant_experiences"] = std::move(exps_arr);

    try {
        root["recent_runs"] = boost::json::parse(report.recent_runs_json);
    } catch (...) {
        root["recent_runs"] = boost::json::array{};
    }

    return boost::json::serialize(root);
}

} // namespace crivo::context
