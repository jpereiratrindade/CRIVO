#include "core.hpp"
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
using namespace std::string_literals;
int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      std::cerr << "Uso: crivo <init|catalog|run|runs|serve|selftest> [opcoes]\n";
      return 2;
    }
    const std::string command = argv[1];
    std::string db = ".run/crivo.db", file = "catalog/tests.json", web = "web", profile = "core";
    unsigned short port = 8765;
    for (int i=2; i<argc; ++i) {
      std::string a=argv[i];
      if (a=="--db" && i+1<argc) db=argv[++i];
      else if (a=="--file" && i+1<argc) file=argv[++i];
      else if (a=="--web" && i+1<argc) web=argv[++i];
      else if (a=="--profile" && i+1<argc) profile=argv[++i];
      else if (a=="--port" && i+1<argc) { const auto n=std::stoi(argv[++i]); if(n<1 || n>65535) throw std::runtime_error("Porta invalida"); port=static_cast<unsigned short>(n); }
      else throw std::runtime_error("Argumento desconhecido: " + a);
    }
    if(command=="selftest") {
      auto parsed=crivo::parse_catalog(file);
      if(parsed.empty() || parsed.front().id!="catalog.schema") throw std::runtime_error("Falha no selftest: catalogo");
      std::cout << "SELFTEST PASS: catalogo valido\n"; return 0;
    }
    if (command=="init") {
      crivo::initialize_db(db);
      crivo::register_catalog(db, crivo::parse_catalog(file));
      std::cout<<"CRIVO inicializado em "<<db<<"\n"; return 0;
    }
    if (command=="catalog") {
      std::cout<<crivo::serialize_catalog(crivo::parse_catalog(file))<<"\n"; return 0;
    }
    if (command=="run") {
      crivo::initialize_db(db);
      return crivo::run_profile(db, file, profile);
    }
    if (command=="runs") { std::cout<<crivo::query_json(db,"runs")<<"\n"; return 0; }
    if (command=="serve") {
      if(!std::filesystem::exists(db)) throw std::runtime_error("Banco ausente; execute crivo init antes de serve");
      crivo::serve(db,web,port); return 0;
    }
    throw std::runtime_error("Comando desconhecido: "+command);
  } catch (const std::exception& e) { std::cerr<<"CRIVO ERROR: "<<e.what()<<"\n"; return 2; }
}
