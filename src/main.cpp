#include "core.hpp"
#include "registry/validator.hpp"
#include "registry/importer.hpp"
#include "registry/plan.hpp"
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
      std::cerr << "Uso: crivo <init|catalog|services|run|runs|serve|selftest|registry|plan> [opcoes]\n";
      return 2;
    }
    const std::string command = argv[1];
    std::string services_action, service_id;
    std::string registry_action;
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
    }

    std::string db = ".run/crivo.db", file = "catalog/tests.json", web = "web", profile = "core";
    std::string catalog_dir = "catalog";
    std::string target_file = "";
    std::string bind_address = "127.0.0.1";
    std::string supported_caps_str = "";
    std::string unsupported_caps_str = "";
    bool dry_run = false;
    bool closed_world = false;
    bool fail_closed = false;
    unsigned short port = 8765;

    for (int i = option_start; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--db" && i + 1 < argc) db = argv[++i];
      else if (a == "--file" && i + 1 < argc) { file = argv[++i]; target_file = file; }
      else if (a == "--dir" && i + 1 < argc) catalog_dir = argv[++i];
      else if (a == "--web" && i + 1 < argc) web = argv[++i];
      else if (a == "--profile" && i + 1 < argc) profile = argv[++i];
      else if (a == "--bind" && i + 1 < argc) bind_address = argv[++i];
      else if (a == "--dry-run") dry_run = true;
      else if (a == "--closed-world") closed_world = true;
      else if (a == "--fail-closed") fail_closed = true;
      else if (a == "--supported-caps" && i + 1 < argc) supported_caps_str = argv[++i];
      else if (a == "--unsupported-caps" && i + 1 < argc) unsupported_caps_str = argv[++i];
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
      // Carregar coleção completa
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

      // Localizar perfil solicitado
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
        // Capacidades padrão de teste local se não fornecidas explicitamente
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
    throw std::runtime_error("Comando desconhecido: " + command);
  } catch (const std::exception& e) {
    std::cerr << "CRIVO ERROR: " << e.what() << "\n";
    return 2;
  }
}
