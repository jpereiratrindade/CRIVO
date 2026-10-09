#include "importer.hpp"
#include <sqlite3.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace crivo::registry {

namespace {

namespace fs = std::filesystem;

std::string current_utc_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

void execute_sql(sqlite3* db, const std::string& sql) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
  if (rc != SQLITE_OK) {
    std::string err = err_msg ? err_msg : "Unknown SQLite error";
    sqlite3_free(err_msg);
    throw std::runtime_error("Erro SQLite: " + err + " ao executar: " + sql);
  }
}

void ensure_schema(sqlite3* db) {
  execute_sql(db, "PRAGMA foreign_keys = ON;");
  execute_sql(db, "PRAGMA journal_mode = WAL;");

  const char* ddl = R"(
    CREATE TABLE IF NOT EXISTS schema_migrations (
      migration_id TEXT PRIMARY KEY,
      applied_at TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS reference_versions (
      reference_id TEXT NOT NULL,
      edition TEXT NOT NULL,
      issuer TEXT NOT NULL,
      kind TEXT NOT NULL,
      title TEXT NOT NULL,
      status_at_registration TEXT,
      url TEXT NOT NULL,
      distribution TEXT NOT NULL,
      license_review TEXT NOT NULL,
      catalog_level TEXT NOT NULL,
      notes TEXT,
      file_path TEXT NOT NULL,
      imported_at TEXT NOT NULL,
      PRIMARY KEY (reference_id, edition)
    );

    CREATE TABLE IF NOT EXISTS technique_versions (
      technique_id TEXT NOT NULL,
      version TEXT NOT NULL,
      family TEXT NOT NULL,
      name TEXT NOT NULL,
      procedure_summary TEXT NOT NULL,
      oracle_class TEXT NOT NULL,
      distribution TEXT NOT NULL,
      file_path TEXT NOT NULL,
      imported_at TEXT NOT NULL,
      PRIMARY KEY (technique_id, version)
    );

    CREATE TABLE IF NOT EXISTS test_spec_versions (
      spec_id TEXT NOT NULL,
      version TEXT NOT NULL,
      title TEXT,
      source TEXT NOT NULL,
      category TEXT NOT NULL,
      subcategory TEXT NOT NULL,
      level TEXT NOT NULL,
      purpose TEXT NOT NULL,
      oracle_type TEXT NOT NULL,
      oracle_id TEXT NOT NULL,
      qualification TEXT NOT NULL,
      file_path TEXT NOT NULL,
      imported_at TEXT NOT NULL,
      PRIMARY KEY (spec_id, version)
    );

    CREATE TABLE IF NOT EXISTS test_implementation_versions (
      impl_id TEXT NOT NULL,
      version TEXT NOT NULL,
      implements_spec TEXT NOT NULL,
      adapter TEXT NOT NULL,
      destructive INTEGER NOT NULL,
      isolation TEXT NOT NULL,
      network TEXT NOT NULL,
      qualification TEXT NOT NULL,
      entrypoint TEXT,
      file_path TEXT NOT NULL,
      imported_at TEXT NOT NULL,
      PRIMARY KEY (impl_id, version)
    );

    CREATE TABLE IF NOT EXISTS profile_versions (
      profile_id TEXT NOT NULL,
      version TEXT NOT NULL,
      title TEXT NOT NULL,
      fail_fast INTEGER NOT NULL,
      allow_unqualified INTEGER NOT NULL,
      file_path TEXT NOT NULL,
      imported_at TEXT NOT NULL,
      PRIMARY KEY (profile_id, version)
    );

    CREATE TABLE IF NOT EXISTS lifecycle_events (
      event_id TEXT PRIMARY KEY,
      entity_type TEXT NOT NULL,
      entity_ref TEXT NOT NULL,
      event_type TEXT NOT NULL,
      occurred_at TEXT NOT NULL,
      recorded_at TEXT NOT NULL,
      cause TEXT NOT NULL,
      authority_ref TEXT NOT NULL
    );
  )";

  execute_sql(db, ddl);
  execute_sql(db, "INSERT OR IGNORE INTO schema_migrations (migration_id, applied_at) VALUES ('001_registry_v1_0_0', datetime('now'));");
}

} // namespace

ImportResult import_registry(const std::string& catalog_dir,
                             const std::string& db_path,
                             bool dry_run) {
  ImportResult result;
  result.dry_run = dry_run;

  // 1. Validar catálogo completo
  RegistryCollection collection;
  result.validation_report = validate_catalog(catalog_dir);

  if (!result.validation_report.valid) {
    result.success = false;
    result.message = "Importacao abortada: catalogo contem erros de validacao.";
    return result;
  }

  // Se tudo válido, preencher contagens
  result.references_imported = result.validation_report.references_count;
  result.techniques_imported = result.validation_report.techniques_count;
  result.specifications_imported = result.validation_report.specifications_count;
  result.implementations_imported = result.validation_report.implementations_count;
  result.profiles_imported = result.validation_report.profiles_count;

  if (dry_run) {
    result.success = true;
    result.message = "Validação DRY-RUN concluida com sucesso. Nenhuma mutacao de banco realizada.";
    return result;
  }

  // 2. Persistir no banco SQLite com transação
  fs::path db_file(db_path);
  if (db_file.has_parent_path()) {
    fs::create_directories(db_file.parent_path());
  }

  sqlite3* db = nullptr;
  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc != SQLITE_OK) {
    result.success = false;
    result.message = "Falha ao abrir banco SQLite: " + std::string(sqlite3_errmsg(db));
    if (db) sqlite3_close(db);
    return result;
  }

  try {
    ensure_schema(db);
    execute_sql(db, "BEGIN IMMEDIATE TRANSACTION;");

    std::string now = current_utc_iso8601();

    // Carregar coleção novamente para inserção tipada
    RegistryCollection col;
    ValidationReport rep;
    std::vector<std::string> subdirs = {"references", "techniques", "specifications", "implementations", "profiles"};
    for (const auto& sub : subdirs) {
      fs::path subpath = fs::path(catalog_dir) / sub;
      if (fs::exists(subpath) && fs::is_directory(subpath)) {
        for (const auto& entry : fs::recursive_directory_iterator(subpath)) {
          if (entry.is_regular_file() && entry.path().extension() == ".json") {
            validate_file(entry.path().string(), col, rep);
          }
        }
      }
    }

    // Insert References
    sqlite3_stmt* stmt = nullptr;
    const char* sql_ref = R"(
      INSERT OR REPLACE INTO reference_versions
      (reference_id, edition, issuer, kind, title, status_at_registration, url, distribution, license_review, catalog_level, notes, file_path, imported_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_ref, -1, &stmt, nullptr);
    for (const auto& [k, r] : col.references) {
      sqlite3_bind_text(stmt, 1, r.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, r.edition.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, r.issuer.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, r.kind.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, r.title.c_str(), -1, SQLITE_TRANSIENT);
      if (r.status_at_registration) sqlite3_bind_text(stmt, 6, r.status_at_registration->c_str(), -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(stmt, 6);
      sqlite3_bind_text(stmt, 7, r.url.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, r.rights.distribution.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, r.rights.license_review.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 10, r.catalog_level.c_str(), -1, SQLITE_TRANSIENT);
      if (r.notes) sqlite3_bind_text(stmt, 11, r.notes->c_str(), -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(stmt, 11);
      sqlite3_bind_text(stmt, 12, r.file_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 13, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // Insert Techniques
    const char* sql_tech = R"(
      INSERT OR REPLACE INTO technique_versions
      (technique_id, version, family, name, procedure_summary, oracle_class, distribution, file_path, imported_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_tech, -1, &stmt, nullptr);
    for (const auto& [k, t] : col.techniques) {
      sqlite3_bind_text(stmt, 1, t.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, t.version.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, t.family.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, t.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, t.procedure_summary.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, t.oracle_class.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, t.rights.distribution.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, t.file_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // Insert TestSpecs
    const char* sql_spec = R"(
      INSERT OR REPLACE INTO test_spec_versions
      (spec_id, version, title, source, category, subcategory, level, purpose, oracle_type, oracle_id, qualification, file_path, imported_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_spec, -1, &stmt, nullptr);
    for (const auto& [k, s] : col.specifications) {
      sqlite3_bind_text(stmt, 1, s.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, s.version.c_str(), -1, SQLITE_TRANSIENT);
      if (s.title) sqlite3_bind_text(stmt, 3, s.title->c_str(), -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(stmt, 3);
      sqlite3_bind_text(stmt, 4, s.source.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 5, s.category.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 6, s.subcategory.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, s.level.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, s.purpose.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 9, s.oracle.type.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 10, s.oracle.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 11, s.qualification.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 12, s.file_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 13, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // Insert Implementations
    const char* sql_impl = R"(
      INSERT OR REPLACE INTO test_implementation_versions
      (impl_id, version, implements_spec, adapter, destructive, isolation, network, qualification, entrypoint, file_path, imported_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_impl, -1, &stmt, nullptr);
    for (const auto& [k, im] : col.implementations) {
      sqlite3_bind_text(stmt, 1, im.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, im.version.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, im.implements_spec.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, im.adapter.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 5, im.execution_policy.destructive ? 1 : 0);
      sqlite3_bind_text(stmt, 6, im.execution_policy.isolation.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, im.execution_policy.network.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 8, im.qualification.c_str(), -1, SQLITE_TRANSIENT);
      if (im.entrypoint) sqlite3_bind_text(stmt, 9, im.entrypoint->c_str(), -1, SQLITE_TRANSIENT);
      else sqlite3_bind_null(stmt, 9);
      sqlite3_bind_text(stmt, 10, im.file_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 11, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // Insert Profiles
    const char* sql_prof = R"(
      INSERT OR REPLACE INTO profile_versions
      (profile_id, version, title, fail_fast, allow_unqualified, file_path, imported_at)
      VALUES (?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_prof, -1, &stmt, nullptr);
    for (const auto& [k, p] : col.profiles) {
      sqlite3_bind_text(stmt, 1, p.id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, p.version.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, p.title.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 4, p.policy.fail_fast ? 1 : 0);
      sqlite3_bind_int(stmt, 5, p.policy.allow_unqualified ? 1 : 0);
      sqlite3_bind_text(stmt, 6, p.file_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    // Record BIRTH event for admission batch
    const char* sql_event = R"(
      INSERT OR IGNORE INTO lifecycle_events
      (event_id, entity_type, entity_ref, event_type, occurred_at, recorded_at, cause, authority_ref)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql_event, -1, &stmt, nullptr);
    std::string event_id = "birth-admission-" + now;
    sqlite3_bind_text(stmt, 1, event_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "catalog_batch", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, catalog_dir.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, "BIRTH", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, "catalog_admission", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, "local-cli", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    result.events_recorded++;

    execute_sql(db, "COMMIT;");
    sqlite3_close(db);

    result.success = true;
    result.message = "Importação concluída com sucesso no banco: " + db_path;
  } catch (const std::exception& e) {
    execute_sql(db, "ROLLBACK;");
    sqlite3_close(db);
    result.success = false;
    result.message = "Erro durante importacao: " + std::string(e.what());
  }

  return result;
}

} // namespace crivo::registry
