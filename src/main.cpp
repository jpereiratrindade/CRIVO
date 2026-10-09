#include "core.hpp"
#include "registry/validator.hpp"
#include "registry/importer.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

using namespace std::string_literals;

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "Uso: crivo <init|catalog|services|run|runs|serve|selftest|registry> [opcoes]\n";
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
    bool dry_run = false;
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
    throw std::runtime_error("Comando desconhecido: " + command);
  } catch (const std::exception& e) {
    std::cerr << "CRIVO ERROR: " << e.what() << "\n";
    return 2;
  }
}
