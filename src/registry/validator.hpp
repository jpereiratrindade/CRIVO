#pragma once
#include "types.hpp"
#include <string>

namespace crivo::registry {

// Validação de arquivo individual (determina o schema pela chave schema_version)
bool validate_file(const std::string& filepath,
                   RegistryCollection& collection,
                   ValidationReport& report);

// Validação de todo o diretório de catálogo estruturado
ValidationReport validate_catalog(const std::string& catalog_dir);

// Resolução de integridade referencial cruzada entre as entidades carregadas
bool resolve_cross_references(const RegistryCollection& collection,
                              ValidationReport& report);

} // namespace crivo::registry
