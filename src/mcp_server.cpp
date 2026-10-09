#include "mcp_server.hpp"
#include "dev_context.hpp"
#include "memory.hpp"
#include "inspector.hpp"
#include "registry/validator.hpp"
#include "registry/plan.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <boost/json.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <vector>

namespace crivo::mcp {

namespace fs = std::filesystem;

static std::string safe_string_arg(const boost::json::object& args, const std::string& key, const std::string& def = "") {
    if (args.contains(key)) {
        const auto& v = args.at(key);
        if (v.is_string()) return std::string(v.as_string());
    }
    return def;
}

static bool is_path_safe_and_authorized(const std::string& path_str, const fs::path& authorized_root,
                                        fs::path& resolved) {
    if (path_str.empty()) return true;
    try {
        const fs::path root = fs::canonical(authorized_root);
        fs::path candidate(path_str);
        if (candidate.is_relative()) candidate = root / candidate;
        resolved = fs::canonical(candidate); // resolve tambem links simbolicos
        auto mismatch = std::mismatch(root.begin(), root.end(), resolved.begin(), resolved.end());
        return mismatch.first == root.end() && fs::is_directory(resolved);
    } catch (...) {
        return false;
    }
}

static boost::json::object make_tool_def(
    const std::string& name,
    const std::string& description,
    const boost::json::object& properties,
    const boost::json::array& required = {})
{
    boost::json::object schema;
    schema["type"] = "object";
    schema["properties"] = properties;
    if (!required.empty()) {
        schema["required"] = required;
    }

    boost::json::object tool;
    tool["name"] = name;
    tool["description"] = description;
    tool["inputSchema"] = std::move(schema);
    return tool;
}

static boost::json::array get_tools_list() {
    boost::json::array tools;

    // 1. dev_context
    {
        boost::json::object props;
        props["target_path"] = boost::json::object{{"type", "string"}, {"description", "Caminho do diretorio do projeto para inspecao."}};
        props["project_id"] = boost::json::object{{"type", "string"}, {"description", "Identificador do projeto (opcional)."}};
        props["query"] = boost::json::object{{"type", "string"}, {"description", "Termo de busca contextual."}};
        props["tag"] = boost::json::object{{"type", "string"}, {"description", "Filtro por tag tecnica (ex: sqlite, wal, acid)."}};
        tools.push_back(make_tool_def(
            "dev_context",
            "Recupera o contexto estruturado de desenvolvimento (capacidades, experiencias relevantes, execucoes recentes) do projeto sem mutacao.",
            props
        ));
    }

    // 2. knowledge_query
    {
        boost::json::object props;
        props["query"] = boost::json::object{{"type", "string"}, {"description", "Termo para busca na memoria tecnica (problema, decisao, tags)."}};
        props["tag"] = boost::json::object{{"type", "string"}, {"description", "Filtro por tag tecnica de aplicabilidade."}};
        props["project_id"] = boost::json::object{{"type", "string"}, {"description", "Filtrar por projeto de origem (opcional)."}};
        tools.push_back(make_tool_def(
            "knowledge_query",
            "Consulta a memoria tecnica do Estaleiro Federado para recuperar problemas, decisoes de engenharia, procedimentos e resultados comprovados.",
            props
        ));
    }

    // 3. catalog_list
    {
        boost::json::object props;
        props["category"] = boost::json::object{{"type", "string"}, {"description", "Tipo de catálogo: references, techniques, specifications, implementations, profiles, ou all."}};
        tools.push_back(make_tool_def(
            "catalog_list",
            "Lista as normas internacionais, tecnicas e especificacoes de teste cadastradas no catalogo do CRIVO.",
            props
        ));
    }

    // 4. evidence_get
    {
        boost::json::object props;
        props["run_id"] = boost::json::object{{"type", "string"}, {"description", "ID da execucao ou evidencia a consultar."}};
        boost::json::array req;
        req.push_back(boost::json::value(boost::json::string_view("run_id")));
        tools.push_back(make_tool_def(
            "evidence_get",
            "Consulta o registro individualizado de evidencia, com re-verificacao de digest SHA-256 e integridade de artefato.",
            props,
            req
        ));
    }

    // 5. test_matrix
    {
        boost::json::object props;
        props["target_path"] = boost::json::object{{"type", "string"}};
        props["profile"] = boost::json::object{{"type", "string"}, {"default", "complete-international-benchmark"}};
        tools.push_back(make_tool_def("test_matrix", "Resolve matriz de testes aplicavel ao alvo sem executar ou mutar.", props));
    }

    // 6. qualify_project
    {
        boost::json::object props;
        props["target_path"] = boost::json::object{{"type", "string"}, {"description", "Caminho do diretorio do alvo para qualificacao soberana."}};
        props["profile"] = boost::json::object{{"type", "string"}, {"default", "complete-international-benchmark"}, {"description", "Perfil internacional de verificacao."}};
        props["promote_on_pass"] = boost::json::object{{"type", "boolean"}, {"default", false}, {"description", "Se true e status for PASS, promove a experiencia automaticamente para a memoria federada."}};
        tools.push_back(make_tool_def("qualify_project", "Executa a qualificacao completa e soberana do projeto alvo via oraculos ativos CRIVO.", props));
    }

    // 7. memory_promote
    {
        boost::json::object props;
        props["summary_path"] = boost::json::object{{"type", "string"}, {"description", "Caminho do arquivo qualification-summary.json a ser promovido."}};
        boost::json::array req;
        req.push_back(boost::json::value(boost::json::string_view("summary_path")));
        tools.push_back(make_tool_def("memory_promote", "Promove o resultado fatico de uma qualificacao aprovada (PASS) a memoria federada do CRIVO.", props, req));
    }

    // 8. sandbox_qualify
    {
        boost::json::object props;
        props["backend"] = boost::json::object{{"type", "string"}, {"default", "bubblewrap"}, {"description", "Backend de isolamento a auditar (bubblewrap, podman, host_isolated, auto)."}};
        tools.push_back(make_tool_def("sandbox_qualify", "Executa a suite de qualificacao e auditoria de seguranca do sandbox efemero (ADR-0009 / Gate E13).", props));
    }

    return tools;
}

static boost::json::object handle_tool_call(
    const std::string& tool_name,
    const boost::json::object& args,
    const std::string& db_path,
    const fs::path& authorized_root,
    const fs::path& catalog_dir)
{
    boost::json::object result;
    boost::json::array content;

    std::string text_out;

    try {
        if (tool_name == "dev_context") {
            std::string target = safe_string_arg(args, "target_path", ".");
            std::string proj = safe_string_arg(args, "project_id", "");
            std::string q = safe_string_arg(args, "query", "");
            std::string tag = safe_string_arg(args, "tag", "");

            fs::path resolved_target;
            if (!is_path_safe_and_authorized(target, authorized_root, resolved_target)) {
                text_out = "{\"error\": \"ACCESS_DENIED_OUT_OF_SCOPE\", \"message\": \"Caminho fora do escopo autorizado\"}";
            } else {
                auto rep = crivo::context::build_dev_context(db_path, resolved_target.string(), proj, q, tag);
                text_out = crivo::context::serialize_dev_context_json(rep);
            }
        } else if (tool_name == "test_matrix") {
            std::string target=safe_string_arg(args,"target_path",".");
            std::string profile=safe_string_arg(args,"profile","complete-international-benchmark");
            fs::path resolved;
            if(!is_path_safe_and_authorized(target,authorized_root,resolved)) text_out="{\"error\":\"ACCESS_DENIED_OUT_OF_SCOPE\"}";
            else {
                registry::RegistryCollection c; registry::ValidationReport v;
                for(const auto& sub:{"references","techniques","specifications","implementations","profiles"}){fs::path dir=catalog_dir/sub;if(fs::exists(dir))for(const auto& e:fs::recursive_directory_iterator(dir))if(e.is_regular_file()&&e.path().extension()==".json")registry::validate_file(e.path().string(),c,v);}
                const registry::ProfileRecord* selected=nullptr;for(const auto& [_,p]:c.profiles)if(p.id==profile){selected=&p;break;}
                if(!v.valid||!selected) text_out="{\"error\":\"INVALID_CATALOG_OR_PROFILE\"}";
                else {registry::EnvironmentCapabilities env;for(const auto& cap:check::inspect_target(resolved).discovered_capabilities)env.supported.insert(cap);text_out=registry::serialize_test_plan_json(registry::resolve_test_plan(*selected,c,env));}
            }
        } else if (tool_name == "knowledge_query") {
            std::string q = safe_string_arg(args, "query", "");
            std::string tag = safe_string_arg(args, "tag", "");
            std::string proj = safe_string_arg(args, "project_id", "");

            auto exps = crivo::memory::query_experiences(db_path, q, proj, tag);
            text_out = crivo::memory::serialize_experiences_json(exps);
        } else if (tool_name == "evidence_get") {
            std::string run_id = safe_string_arg(args, "run_id", "");
            if (run_id.empty()) {
                text_out = "{\"error\": \"INVALID_PARAMS\", \"message\": \"Campo obrigatorio 'run_id' ausente ou invalido\"}";
            } else {
                sqlite3* db = nullptr;
                if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
                    std::string sql = "SELECT r.run_id, r.project_id, r.mode, r.status, r.total, r.passed, r.failed, "
                                      "r.duration_ms, r.started_at, COALESCE(e.artifact_sha256, ''), COALESCE(e.evidence_path, '') "
                                      "FROM external_runs r LEFT JOIN evidence_index e ON e.run_id=r.run_id "
                                      "WHERE r.run_id = ?;";
                    sqlite3_stmt* stmt = nullptr;
                    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            boost::json::object obj;
                            auto gs = [&](int col) -> std::string {
                                const auto* p = sqlite3_column_text(stmt, col);
                                return p ? reinterpret_cast<const char*>(p) : "";
                            };
                            obj["run_id"] = boost::json::string_view(gs(0));
                            obj["project_id"] = boost::json::string_view(gs(1));
                            obj["mode"] = boost::json::string_view(gs(2));
                            obj["status"] = boost::json::string_view(gs(3));
                            obj["total"] = sqlite3_column_int64(stmt, 4);
                            obj["passed"] = sqlite3_column_int64(stmt, 5);
                            obj["failed"] = sqlite3_column_int64(stmt, 6);
                            obj["duration_ms"] = sqlite3_column_int64(stmt, 7);
                            obj["started_at"] = boost::json::string_view(gs(8));
                            std::string expected_sha = gs(9);
                            std::string ev_path = gs(10);
                            obj["artifact_sha256"] = boost::json::string_view(expected_sha);
                            obj["evidence_path"] = boost::json::string_view(ev_path);

                            // Re-verificar integridade física do artefato se o caminho existir
                            if (!ev_path.empty() && fs::exists(ev_path)) {
                                std::string actual_sha = crivo::check::calculate_file_sha256(ev_path);
                                obj["integrity_verified"] = (!expected_sha.empty() && actual_sha == expected_sha);
                                if (expected_sha.empty()) obj["integrity_status"] = "UNVERIFIABLE_MISSING_EXPECTED_DIGEST";
                                obj["verified_sha256"] = boost::json::string_view(actual_sha);
                            } else {
                                obj["integrity_verified"] = false;
                            }

                            text_out = boost::json::serialize(obj);
                        } else {
                            text_out = "{\"error\": \"EVIDENCE_NOT_FOUND\", \"run_id\": \"" + run_id + "\"}";
                        }
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(db);
                } else {
                    text_out = "{\"error\": \"DATABASE_ERROR\"}";
                }
            }
        } else if (tool_name == "catalog_list") {
            std::string cat = safe_string_arg(args, "category", "all");
            sqlite3* db = nullptr;
            if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
                boost::json::object root;
                auto query_and_push = [&](const std::string& query, const std::string& key) {
                    sqlite3_stmt* stmt = nullptr;
                    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                        boost::json::array arr;
                        while (sqlite3_step(stmt) == SQLITE_ROW) {
                            boost::json::object item;
                            int cols = sqlite3_column_count(stmt);
                            for (int i = 0; i < cols; ++i) {
                                const char* cname = sqlite3_column_name(stmt, i);
                                const auto* txt = sqlite3_column_text(stmt, i);
                                item[cname] = boost::json::string_view(txt ? reinterpret_cast<const char*>(txt) : "");
                            }
                            arr.push_back(std::move(item));
                        }
                        sqlite3_finalize(stmt);
                        root[key] = std::move(arr);
                    }
                };

                if (cat == "references" || cat == "all") {
                    query_and_push("SELECT reference_id, edition, issuer, kind, title FROM reference_versions ORDER BY reference_id;", "references");
                }
                if (cat == "techniques" || cat == "all") {
                    query_and_push("SELECT technique_id, version, family, name, oracle_class FROM technique_versions ORDER BY technique_id;", "techniques");
                }
                if (cat == "specifications" || cat == "all") {
                    query_and_push("SELECT spec_id, version, title, category, purpose FROM test_spec_versions ORDER BY spec_id;", "specifications");
                }
                if (cat == "services" || cat == "all") {
                    query_and_push("SELECT id, name, category, subcategory, purpose, status FROM test_catalog ORDER BY category, id;", "services");
                }

                sqlite3_close(db);
                text_out = boost::json::serialize(root);
            } else {
                text_out = "[]";
            }
        } else if (tool_name == "qualify_project") {
            std::string target = safe_string_arg(args, "target_path", ".");
            std::string profile = safe_string_arg(args, "profile", "complete-international-benchmark");
            bool promote = false;
            if (args.contains("promote_on_pass") && args.at("promote_on_pass").is_bool()) {
                promote = args.at("promote_on_pass").as_bool();
            }

            fs::path resolved;
            if (!is_path_safe_and_authorized(target, authorized_root, resolved)) {
                text_out = "{\"error\": \"ACCESS_DENIED_OUT_OF_SCOPE\", \"message\": \"Caminho fora do escopo autorizado\"}";
            } else {
                check::CheckOptions opts;
                opts.target_path = resolved;
                opts.profile = profile;
                opts.catalog_dir = catalog_dir.string();
                fs::path ev_dir = fs::path(db_path).parent_path() / "qualification" / resolved.filename().string() / "mcp-evidence";
                fs::create_directories(ev_dir);
                opts.evidence_dir = ev_dir.string();
                opts.db_path = db_path;
                opts.json_output = true;
                opts.timeout_seconds = 30;

                auto summary = check::execute_check(opts);
                boost::json::object out_obj;
                out_obj["schema_version"] = "crivo.qualification-summary/1.0.0";
                out_obj["project_id"] = summary.project_id;
                out_obj["target_path"] = resolved.string();
                out_obj["profile"] = profile;
                out_obj["status"] = summary.overall_status;
                out_obj["total"] = summary.total_tests;
                out_obj["passed"] = summary.passed_tests;
                out_obj["failed"] = summary.failed_tests;
                out_obj["skipped"] = summary.skipped_tests;
                out_obj["blocked"] = summary.blocked_tests;
                out_obj["evidence_path"] = summary.evidence_path;
                out_obj["evidence_sha256"] = summary.evidence_sha256;

                if (summary.overall_status == "PASS") {
                    out_obj["learning_status"] = promote ? "PROMOTED" : "REVIEW_REQUIRED";
                    out_obj["next_action"] = promote ? "experiencia integrada a memoria federada com sucesso" : "revisar evidencias antes de promover aprendizado";
                    if (promote && !summary.evidence_path.empty()) {
                        memory::promote_qualification_to_experience(db_path, summary.evidence_path);
                    }
                } else {
                    out_obj["learning_status"] = "INSUFFICIENT_EVIDENCE";
                    out_obj["next_action"] = "corrigir falhas e repetir a qualificacao";
                }

                text_out = boost::json::serialize(out_obj);
            }
        } else if (tool_name == "memory_promote") {
            std::string sum_path = safe_string_arg(args, "summary_path", "");
            fs::path resolved;
            if (!is_path_safe_and_authorized(sum_path, authorized_root, resolved) && !fs::exists(sum_path)) {
                text_out = "{\"error\": \"INVALID_PATH\", \"message\": \"Caminho do resumo invalido\"}";
            } else {
                std::string target_file = fs::exists(resolved) ? resolved.string() : sum_path;
                if (memory::promote_qualification_to_experience(db_path, target_file)) {
                    text_out = "{\"status\": \"PROMOTED\", \"message\": \"Experiencia factual promovida com sucesso para a memoria federada\"}";
                } else {
                    text_out = "{\"error\": \"PROMOTION_REJECTED\", \"message\": \"Veredito nao e PASS ou resumo invalido\"}";
                }
            }
        } else if (tool_name == "sandbox_qualify") {
            std::string backend_str = safe_string_arg(args, "backend", "bubblewrap");
            sandbox::BackendType btype = sandbox::BackendType::Bubblewrap;
            if (backend_str == "podman") btype = sandbox::BackendType::Podman;
            else if (backend_str == "host_isolated") btype = sandbox::BackendType::HostIsolated;
            else if (backend_str == "auto") btype = sandbox::BackendType::Auto;

            auto qres = sandbox::qualify_backend(btype, "");
            text_out = sandbox::serialize_qualification_result(qres);
        } else {
            text_out = "{\"error\": \"UNKNOWN_TOOL\", \"name\": \"" + tool_name + "\"}";
        }
    } catch (const std::exception& e) {
        text_out = "{\"error\": \"INTERNAL_ERROR\", \"message\": \"" + std::string(e.what()) + "\"}";
    }

    boost::json::object item;
    item["type"] = "text";
    item["text"] = boost::json::string_view(text_out);
    content.push_back(std::move(item));

    result["content"] = std::move(content);
    return result;
}

int run_stdio_server(const std::string& db_path, const std::string& catalog_dir,
                     const std::string& workspace_root) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line.starts_with("Content-Length:") || line.starts_with("content-length:")) {
            continue;
        }

        boost::json::value req_val;
        try {
            req_val = boost::json::parse(line);
        } catch (...) {
            continue;
        }

        if (!req_val.is_object()) continue;
        const auto& req = req_val.as_object();

        std::string method;
        if (req.contains("method") && req.at("method").is_string()) {
            method = req.at("method").as_string().c_str();
        }

        boost::json::value id_val = nullptr;
        if (req.contains("id")) {
            id_val = req.at("id");
        }

        boost::json::object resp;
        resp["jsonrpc"] = "2.0";
        if (!id_val.is_null()) {
            resp["id"] = id_val;
        }

        if (method == "initialize") {
            boost::json::object res;
            // O servidor ecoa uma versao suportada solicitada; caso contrario,
            // negocia sua versao mais recente, conforme o lifecycle MCP.
            std::string requested_version = "2025-11-25";
            if (req.contains("params") && req.at("params").is_object()) {
                const auto& p = req.at("params").as_object();
                if (p.contains("protocolVersion") && p.at("protocolVersion").is_string()) {
                    requested_version = std::string(p.at("protocolVersion").as_string());
                }
            }
            static const std::vector<std::string> supported = {
                "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"
            };
            const std::string negotiated_version =
                std::find(supported.begin(), supported.end(), requested_version) != supported.end()
                    ? requested_version : supported.front();
            res["protocolVersion"] = boost::json::string_view(negotiated_version);

            boost::json::object server_info;
            server_info["name"] = "crivo-mcp-server";
            server_info["version"] = "0.3.0";
            res["serverInfo"] = std::move(server_info);

            boost::json::object caps;
            caps["tools"] = boost::json::object{};
            res["capabilities"] = std::move(caps);

            resp["result"] = std::move(res);
        } else if (method == "notifications/initialized") {
            if (id_val.is_null()) continue;
            resp["result"] = boost::json::object{};
        } else if (method == "ping") {
            resp["result"] = boost::json::object{};
        } else if (method == "tools/list") {
            boost::json::object res;
            res["tools"] = get_tools_list();
            resp["result"] = std::move(res);
        } else if (method == "tools/call") {
            if (req.contains("params") && req.at("params").is_object()) {
                const auto& params = req.at("params").as_object();
                std::string t_name = "";
                if (params.contains("name") && params.at("name").is_string()) {
                    t_name = std::string(params.at("name").as_string());
                }
                if (params.contains("arguments") && !params.at("arguments").is_object()) {
                    boost::json::object err;
                    err["code"] = -32602;
                    err["message"] = "Invalid params: arguments must be an object";
                    resp["error"] = std::move(err);
                } else {
                    boost::json::object t_args;
                    if (params.contains("arguments")) t_args = params.at("arguments").as_object();
                    resp["result"] = handle_tool_call(t_name, t_args, db_path, workspace_root, catalog_dir);
                }
            } else {
                boost::json::object err;
                err["code"] = -32602;
                err["message"] = "Invalid params: object expected";
                resp["error"] = std::move(err);
            }
        } else {
            boost::json::object err;
            err["code"] = -32601;
            err["message"] = "Method not found: " + method;
            resp["error"] = std::move(err);
        }

        std::string out_str = boost::json::serialize(resp);
        std::cout << out_str << "\n" << std::flush;
    }
    return 0;
}

} // namespace crivo::mcp
