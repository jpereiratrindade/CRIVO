#include "core.hpp"
#include "registry/validator.hpp"
#include "registry/importer.hpp"
#include "registry/plan.hpp"
#include "registry/lifecycle.hpp"
#include "adapters/ctest_adapter.hpp"
#include "inspector.hpp"
#include "sandbox.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

using namespace std::string_literals;

namespace {
std::vector<std::string> split_comma(const std::string& s) {
  std::vector<std::string> res;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) res.push_back(item);
  }
  return res;
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "Uso: crivo <init|catalog|services|run|runs|external-runs|external-record|check|serve|selftest|registry|plan|events|adapter> [opcoes]\n";
      return 2;
    }
    const std::string command = argv[1];
    std::string services_action, service_id;
    std::string registry_action;
    std::string events_action;
    std::string adapter_type, adapter_action;
    int option_start = 2;

    if (command == "services") {
      if (argc < 3) throw std::runtime_error("Uso: crivo services <list|show ID> [opcoes]");
      services_action = argv[2];
      option_start = 3;
      if (services_action == "show") {
        if (argc < 4) throw std::runtime_error("Uso: crivo services show <id> [opcoes]");
        service_id = argv[3];
        option_start = 4;
      } else if (services_action != "list") {
        throw std::runtime_error("Acao de servicos desconhecida: " + services_action);
      }
    } else if (command == "registry") {
      if (argc < 3) throw std::runtime_error("Uso: crivo registry <validate|import> [opcoes]");
      registry_action = argv[2];
      option_start = 3;
      if (registry_action != "validate" && registry_action != "import") {
        throw std::runtime_error("Acao de registry desconhecida: " + registry_action);
      }
    } else if (command == "events") {
      if (argc < 3) throw std::runtime_error("Uso: crivo events <list|record|reconcile> [opcoes]");
      events_action = argv[2];
      option_start = 3;
      if (events_action != "list" && events_action != "record" && events_action != "reconcile") {
        throw std::runtime_error("Acao de events desconhecida: " + events_action);
      }
    } else if (command == "adapter") {
      if (argc < 4) throw std::runtime_error("Uso: crivo adapter <ctest> <discover|run> [opcoes]");
      adapter_type = argv[2];
      adapter_action = argv[3];
      option_start = 4;
      if (adapter_type != "ctest") throw std::runtime_error("Tipo de adaptador desconhecido: " + adapter_type);
      if (adapter_action != "discover" && adapter_action != "run") {
        throw std::runtime_error("Acao de adaptador desconhecida: " + adapter_action);
      }
    }

    std::string db = ".run/crivo.db", file = "catalog/tests.json", web = "web", profile = "core";
    std::string catalog_dir = "catalog";
    std::string target_file = "";
    std::string target_path = ".";
    std::string isolation_mode = "sandbox";
    std::string sandbox_backend = "auto";
    bool json_output = false;
    std::string bind_address = "127.0.0.1";
    std::string supported_caps_str = "";
    std::string unsupported_caps_str = "";
    std::string entity_type = "test_specification";
    std::string entity_ref = "";
    std::string event_type = "";
    std::string cause = "manual_registration";
    std::string authority_ref = "local-cli";
    std::string build_dir = "build";
    std::string evidence_dir = ".run/evidence";
    std::string project_id = "local-project";
    std::string workspace_root = "";
    std::string record_db = "";
    std::string source_revision = "unknown";
    std::string source_worktree = "unknown";
    unsigned int timeout_seconds = 0;
    bool dry_run = false;
    bool closed_world = false;
    bool fail_closed = false;
    unsigned short port = 8765;

    for (int i = option_start; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--db" && i + 1 < argc) db = argv[++i];
      else if (a == "--file" && i + 1 < argc) { file = argv[++i]; target_file = file; }
      else if ((a == "--dir" || a == "--catalog") && i + 1 < argc) catalog_dir = argv[++i];
      else if (a == "--target" && i + 1 < argc) target_path = argv[++i];
      else if (a == "--isolation" && i + 1 < argc) isolation_mode = argv[++i];
      else if (a == "--backend" && i + 1 < argc) sandbox_backend = argv[++i];
      else if (a == "--build" && i + 1 < argc) build_dir = argv[++i];
      else if (a == "--evidence-dir" && i + 1 < argc) evidence_dir = argv[++i];
      else if (a == "--project" && i + 1 < argc) project_id = argv[++i];
      else if (a == "--workspace-root" && i + 1 < argc) workspace_root = argv[++i];
      else if (a == "--record-db" && i + 1 < argc) record_db = argv[++i];
      else if (a == "--source-revision" && i + 1 < argc) source_revision = argv[++i];
      else if (a == "--source-worktree" && i + 1 < argc) source_worktree = argv[++i];
      else if (a == "--timeout" && i + 1 < argc) {
        const auto n = std::stoul(argv[++i]);
        if (n < 1 || n > 86400) throw std::runtime_error("Timeout invalido (1..86400 segundos)");
        timeout_seconds = static_cast<unsigned int>(n);
      }
      else if (a == "--web" && i + 1 < argc) web = argv[++i];
      else if (a == "--profile" && i + 1 < argc) profile = argv[++i];
      else if (a == "--bind" && i + 1 < argc) bind_address = argv[++i];
      else if (a == "--dry-run") dry_run = true;
      else if (a == "--closed-world") closed_world = true;
      else if (a == "--fail-closed") fail_closed = true;
      else if (a == "--json") json_output = true;
      else if (a == "--supported-caps" && i + 1 < argc) supported_caps_str = argv[++i];
      else if (a == "--unsupported-caps" && i + 1 < argc) unsupported_caps_str = argv[++i];
      else if (a == "--entity-type" && i + 1 < argc) entity_type = argv[++i];
      else if ((a == "--entity-ref" || a == "--entity") && i + 1 < argc) entity_ref = argv[++i];
      else if (a == "--event-type" && i + 1 < argc) event_type = argv[++i];
      else if (a == "--cause" && i + 1 < argc) cause = argv[++i];
      else if (a == "--authority" && i + 1 < argc) authority_ref = argv[++i];
      else if (a == "--port" && i + 1 < argc) {
        const auto n = std::stoi(argv[++i]);
        if (n < 1 || n > 65535) throw std::runtime_error("Porta invalida");
        port = static_cast<unsigned short>(n);
      }
      else throw std::runtime_error("Argumento desconhecido: " + a);
    }

    if (command == "selftest") {
      auto parsed = crivo::parse_catalog(file);
      if (parsed.empty() || parsed.front().id != "catalog.schema") throw std::runtime_error("Falha no selftest: catalogo");
      std::cout << "SELFTEST PASS: catalogo valido\n";
      return 0;
    }
    if (command == "init") {
      crivo::initialize_db(db);
      crivo::register_catalog(db, crivo::parse_catalog(file));
      std::cout << "CRIVO inicializado em " << db << "\n";
      return 0;
    }
    if (command == "catalog") {
      std::cout << crivo::serialize_catalog(crivo::parse_catalog(file)) << "\n";
      return 0;
    }
    if (command == "services") {
      const auto catalog = crivo::parse_catalog(file);
      std::cout << (services_action == "list" ? crivo::serialize_services(catalog) :
                    crivo::serialize_service(catalog, service_id)) << "\n";
      return 0;
    }
    if (command == "run") {
      crivo::initialize_db(db);
      return crivo::run_profile(db, file, profile);
    }
    if (command == "runs") {
      std::cout << crivo::query_json(db, "runs") << "\n";
      return 0;
    }
    if (command == "external-runs") {
      std::cout << crivo::query_external_runs(db) << "\n";
      return 0;
    }
    if (command == "external-record") {
      crivo::record_external_evidence(db, evidence_dir);
      std::cout << "CRIVO EXTERNAL EVIDENCE RECORDED PASS\n";
      return 0;
    }
    if (command == "serve") {
      if (!std::filesystem::exists(db)) throw std::runtime_error("Banco ausente; execute crivo init antes de serve");
      crivo::serve(db, web, bind_address, port);
      return 0;
    }
    if (command == "registry") {
      if (registry_action == "validate") {
        crivo::registry::ValidationReport report;
        if (!target_file.empty()) {
          crivo::registry::RegistryCollection collection;
          crivo::registry::validate_file(target_file, collection, report);
        } else {
          report = crivo::registry::validate_catalog(catalog_dir);
        }

        if (!report.valid) {
          std::cerr << "CRIVO REGISTRY VALIDATION FAILED (" << report.errors.size() << " erros):\n";
          for (const auto& err : report.errors) {
            std::cerr << "  - [" << err.error_code << "] " << err.file_path << ": " << err.message << "\n";
          }
          return 2;
        }

        std::cout << "CRIVO REGISTRY VALIDATION PASS:\n"
                  << "  - References: " << report.references_count << "\n"
                  << "  - Techniques: " << report.techniques_count << "\n"
                  << "  - Specifications: " << report.specifications_count << "\n"
                  << "  - Implementations: " << report.implementations_count << "\n"
                  << "  - Profiles: " << report.profiles_count << "\n";
        return 0;
      }
      if (registry_action == "import") {
        auto res = crivo::registry::import_registry(catalog_dir, db, dry_run);
        if (!res.success) {
          std::cerr << "CRIVO REGISTRY IMPORT FAILED: " << res.message << "\n";
          for (const auto& err : res.validation_report.errors) {
            std::cerr << "  - [" << err.error_code << "] " << err.file_path << ": " << err.message << "\n";
          }
          return 2;
        }
        std::cout << "CRIVO REGISTRY IMPORT PASS" << (dry_run ? " (DRY-RUN)" : "") << ":\n"
                  << "  - References: " << res.references_imported << "\n"
                  << "  - Techniques: " << res.techniques_imported << "\n"
                  << "  - Specifications: " << res.specifications_imported << "\n"
                  << "  - Implementations: " << res.implementations_imported << "\n"
                  << "  - Profiles: " << res.profiles_imported << "\n"
                  << "  - Lifecycle Events: " << res.events_recorded << "\n"
                  << "  - Status: " << res.message << "\n";
        return 0;
      }
    }
    if (command == "plan") {
      crivo::registry::RegistryCollection collection;
      crivo::registry::ValidationReport report;
      std::vector<std::string> subdirs = {"references", "techniques", "specifications", "implementations", "profiles"};
      for (const auto& sub : subdirs) {
        std::filesystem::path subpath = std::filesystem::path(catalog_dir) / sub;
        if (std::filesystem::exists(subpath) && std::filesystem::is_directory(subpath)) {
          for (const auto& entry : std::filesystem::recursive_directory_iterator(subpath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
              crivo::registry::validate_file(entry.path().string(), collection, report);
            }
          }
        }
      }

      if (!report.valid) {
        std::cerr << "CRIVO PLAN FAILED: Catalogo invalido (" << report.errors.size() << " erros)\n";
        return 2;
      }

      const crivo::registry::ProfileRecord* target_profile = nullptr;
      for (const auto& [k, p] : collection.profiles) {
        if (p.id == profile) {
          target_profile = &p;
          break;
        }
      }

      if (!target_profile) {
        std::cerr << "CRIVO PLAN FAILED: Perfil '" << profile << "' nao encontrado no catalogo.\n";
        return 2;
      }

      crivo::registry::EnvironmentCapabilities env;
      env.closed_world = closed_world;
      if (!supported_caps_str.empty()) {
        auto caps = split_comma(supported_caps_str);
        for (const auto& c : caps) env.supported.insert(c);
      } else {
        std::set<std::string> default_caps = {
          "capability:sqlite",
          "capability:json_parser",
          "capability:http_server"
        };
        for (const auto& dc : default_caps) {
          if (env.unsupported.find(dc) == env.unsupported.end()) {
            env.supported.insert(dc);
          }
        }
      }
      if (!unsupported_caps_str.empty()) {
        auto caps = split_comma(unsupported_caps_str);
        for (const auto& c : caps) env.unsupported.insert(c);
      }

      auto plan = crivo::registry::resolve_test_plan(*target_profile, collection, env);
      std::cout << crivo::registry::serialize_test_plan_json(plan) << "\n";

      if (fail_closed && (plan.summary.unknown_count > 0 || plan.summary.not_applicable_count > 0)) {
        std::cerr << "CRIVO PLAN FAIL-CLOSED: Plano contem "
                  << plan.summary.unknown_count << " testes UNKNOWN e "
                  << plan.summary.not_applicable_count << " testes NOT_APPLICABLE.\n";
        return 2;
      }

      return 0;
    }
    if (command == "events") {
      sqlite3* db_handle = nullptr;
      if (sqlite3_open(db.c_str(), &db_handle) != SQLITE_OK) {
        std::cerr << "CRIVO EVENTS FAILED: Nao foi possivel abrir banco SQLite: " << db << "\n";
        if (db_handle) sqlite3_close(db_handle);
        return 2;
      }

      if (events_action == "list") {
        auto evs = crivo::registry::list_lifecycle_events(db_handle, entity_ref, event_type);
        std::cout << crivo::registry::serialize_events_json(evs) << "\n";
        sqlite3_close(db_handle);
        return 0;
      }
      if (events_action == "record") {
        if (entity_ref.empty()) {
          std::cerr << "CRIVO EVENTS RECORD FAILED: --entity-ref obrigatorio\n";
          sqlite3_close(db_handle);
          return 2;
        }
        crivo::registry::LifecycleEvent ev;
        ev.entity_type = entity_type;
        ev.entity_ref = entity_ref;
        ev.event_type = event_type.empty() ? "BIRTH" : event_type;
        ev.cause = cause;
        ev.authority_ref = authority_ref;

        if (!crivo::registry::record_lifecycle_event(db_handle, ev)) {
          std::cerr << "CRIVO EVENTS RECORD FAILED: Falha ao persistir evento no banco\n";
          sqlite3_close(db_handle);
          return 2;
        }
        std::cout << "CRIVO EVENT RECORDED PASS: event_id=" << ev.event_id
                  << ", digest=" << ev.payload_digest_sha256 << "\n";
        sqlite3_close(db_handle);
        return 0;
      }
      if (events_action == "reconcile") {
        size_t rec_count = crivo::registry::reconcile_interrupted_runs(db_handle, authority_ref);
        std::cout << "CRIVO RECONCILIATION PASS: " << rec_count << " execucoes orfas reconciliadas.\n";
        sqlite3_close(db_handle);
        return 0;
      }
    }
    if (command == "check") {
      crivo::check::CheckOptions opts;
      opts.target_path = target_path.empty() ? "." : target_path;
      opts.profile = profile;
      opts.catalog_dir = catalog_dir;
      opts.db_path = db;
      opts.evidence_dir = evidence_dir;
      opts.isolation = isolation_mode;
      opts.backend = sandbox_backend;
      opts.fail_closed = fail_closed;
      opts.dry_run = dry_run;
      opts.json_output = json_output;
      opts.timeout_seconds = timeout_seconds == 0 ? 30 : timeout_seconds;

      auto summary = crivo::check::execute_check(opts);
      if (json_output) {
        std::cout << "{\"schema_version\":\"crivo.check-summary/1.0.0\",\"request_id\":\""
                  << summary.request_id << "\",\"project_id\":\"" << summary.project_id
                  << "\",\"status\":\"" << summary.overall_status
                  << "\",\"total\":" << summary.total_tests
                  << ",\"passed\":" << summary.passed_tests
                  << ",\"failed\":" << summary.failed_tests
                  << ",\"skipped\":" << summary.skipped_tests
                  << ",\"blocked\":" << summary.blocked_tests
                  << ",\"evidence_path\":\"" << summary.evidence_path
                  << "\",\"evidence_sha256\":\"" << summary.evidence_sha256 << "\"}\n";
      } else {
        std::cout << "CRIVO CHECK " << summary.overall_status << ": "
                  << "total=" << summary.total_tests
                  << ", passed=" << summary.passed_tests
                  << ", failed=" << summary.failed_tests
                  << ", skipped=" << summary.skipped_tests
                  << ", blocked=" << summary.blocked_tests
                  << " [evidence: " << summary.evidence_path << "]\n";
      }
      if (summary.overall_status == "PASS") {
        return 0;
      }
      return 2;
    }
    if (command == "adapter") {
      if (adapter_type == "ctest") {
        if (adapter_action == "discover") {
          auto disc = crivo::adapters::discover_ctest(
              build_dir, project_id, workspace_root, timeout_seconds == 0 ? 30 : timeout_seconds);
          if (!disc.success) {
            std::cerr << "CRIVO ADAPTER CTEST DISCOVER FAILED: " << disc.error_message << "\n";
            return 2;
          }
          std::cout << crivo::adapters::serialize_discovery_json(disc) << "\n";
          return 0;
        }
        if (adapter_action == "run") {
          auto res = crivo::adapters::run_ctest(
              build_dir, evidence_dir, project_id, workspace_root, timeout_seconds == 0 ? 300 : timeout_seconds);
          if (!res.success) {
            std::cerr << "CRIVO ADAPTER CTEST RUN FAILED: " << res.error_message << "\n";
            return 2;
          }
          if (!record_db.empty()) {
            crivo::ExternalRunRecord record;
            record.evidence_id = res.evidence_id;
            record.project_id = res.project_id;
            record.mode = "shadow";
            record.source_revision = source_revision;
            record.source_worktree = source_worktree;
            record.status = res.failed == 0 ? "PASS" : "FAIL";
            record.started_at = res.start_utc;
            record.ended_at = res.end_utc;
            record.duration_ms = static_cast<long long>(res.duration_seconds * 1000.0);
            record.total = static_cast<long long>(res.total);
            record.passed = static_cast<long long>(res.passed);
            record.failed = static_cast<long long>(res.failed);
            record.skipped = static_cast<long long>(res.skipped);
            record.adapter = "ctest";
            record.adapter_version = res.ctest_version;
            record.evidence_path = res.evidence_json_path;
            record.junit_path = res.junit_path;
            record.junit_sha256 = res.junit_sha256;
            crivo::record_external_run(record_db, record);
          }
          std::cout << res.evidence_json_content << "\n";
          return 0;
        }
      }
    }
    throw std::runtime_error("Comando desconhecido: " + command);
  } catch (const std::exception& e) {
    std::cerr << "CRIVO ERROR: " << e.what() << "\n";
    return 2;
  }
}
