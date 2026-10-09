#pragma once

#include <string>

namespace crivo::mcp {

/**
 * @brief Executa o servidor MCP sobre stdio (JSON-RPC 2.0).
 * @param db_path Caminho para a base de dados SQLite do CRIVO.
 * @param catalog_dir Caminho para o diretório de catálogo do CRIVO.
 * @return Código de saída (0 em encerramento limpo).
 */
int run_stdio_server(const std::string& db_path, const std::string& catalog_dir = "catalog",
                     const std::string& workspace_root = ".");

} // namespace crivo::mcp
