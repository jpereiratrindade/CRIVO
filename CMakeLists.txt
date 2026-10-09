cmake_minimum_required(VERSION 3.30)
project(CRIVO VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_executable(crivo
  src/main.cpp
  src/core.cpp
  src/sandbox.cpp
  src/inspector.cpp
  src/registry/json_impl.cpp
  src/registry/validator.cpp
  src/registry/importer.cpp
  src/registry/applicability.cpp
  src/registry/plan.cpp
  src/registry/lifecycle.cpp
  src/adapters/ctest_adapter.cpp
)

target_include_directories(crivo PRIVATE src)
target_link_libraries(crivo PRIVATE SQLite3::SQLite3 Boost::headers Threads::Threads)
target_compile_definitions(crivo PRIVATE BOOST_ERROR_CODE_HEADER_ONLY BOOST_JSON_HEADER_ONLY)
target_compile_options(crivo PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

# Configuração da fixture sintética local para testes do adaptador E4
set(SYNTHETIC_FIXTURE_SRC ${CMAKE_SOURCE_DIR}/tests/fixtures/ctest)
set(SYNTHETIC_FIXTURE_BUILD ${CMAKE_BINARY_DIR}/synthetic_fixture)
set(TIMEOUT_FIXTURE_SRC ${CMAKE_SOURCE_DIR}/tests/fixtures/ctest_timeout)
set(TIMEOUT_FIXTURE_BUILD ${CMAKE_BINARY_DIR}/timeout_fixture)

add_custom_target(configure_synthetic_fixture ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SYNTHETIC_FIXTURE_BUILD}
  COMMAND ${CMAKE_COMMAND} -S ${SYNTHETIC_FIXTURE_SRC} -B ${SYNTHETIC_FIXTURE_BUILD} -DCMAKE_BUILD_TYPE=Debug
)

add_custom_target(configure_timeout_fixture ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TIMEOUT_FIXTURE_BUILD}
  COMMAND ${CMAKE_COMMAND} -S ${TIMEOUT_FIXTURE_SRC} -B ${TIMEOUT_FIXTURE_BUILD}
)

# Testes de regressão v0.1.0 (preservados intactos)
add_test(NAME crivo_selftest COMMAND crivo selftest --file ${CMAKE_SOURCE_DIR}/catalog/tests.json)
add_test(NAME crivo_catalog COMMAND crivo catalog --file ${CMAKE_SOURCE_DIR}/catalog/tests.json)
add_test(NAME crivo_services_list COMMAND crivo services list --file ${CMAKE_SOURCE_DIR}/catalog/tests.json)
add_test(NAME crivo_services_show COMMAND crivo services show sqlite.integrity --file ${CMAKE_SOURCE_DIR}/catalog/tests.json)
set_tests_properties(crivo_services_list PROPERTIES PASS_REGULAR_EXPRESSION "crivo.service-catalog/0.1.0")
set_tests_properties(crivo_services_show PROPERTIES PASS_REGULAR_EXPRESSION "sqlite.integrity")
add_test(NAME crivo_services_reject_unknown COMMAND crivo services show unknown.service --file ${CMAKE_SOURCE_DIR}/catalog/tests.json)
set_tests_properties(crivo_services_reject_unknown PROPERTIES WILL_FAIL TRUE)
add_test(NAME crivo_reject_schema_version COMMAND crivo catalog --file ${CMAKE_SOURCE_DIR}/tests/bad-version.json)
set_tests_properties(crivo_reject_schema_version PROPERTIES WILL_FAIL TRUE)
add_test(NAME crivo_reject_duplicate_id COMMAND crivo catalog --file ${CMAKE_SOURCE_DIR}/tests/duplicate-id.json)
set_tests_properties(crivo_reject_duplicate_id PROPERTIES WILL_FAIL TRUE)
add_test(NAME crivo_run_core COMMAND crivo run --file ${CMAKE_SOURCE_DIR}/catalog/tests.json --db ${CMAKE_BINARY_DIR}/smoke.db --profile core)
add_test(NAME crivo_blocked_planned COMMAND crivo run --file ${CMAKE_SOURCE_DIR}/catalog/tests.json --db ${CMAKE_BINARY_DIR}/smoke.db --profile api)
set_tests_properties(crivo_blocked_planned PROPERTIES WILL_FAIL TRUE DEPENDS crivo_run_core)

# Testes de Aceite E1 — CRIVO-IMPL-001 (R01 a R12)
add_test(NAME e1_r01_registry_validate_valid COMMAND crivo registry validate --dir ${CMAKE_SOURCE_DIR}/catalog)
set_tests_properties(e1_r01_registry_validate_valid PROPERTIES PASS_REGULAR_EXPRESSION "CRIVO REGISTRY VALIDATION PASS")

add_test(NAME e1_r02_reject_unknown_schema COMMAND crivo registry validate --file ${CMAKE_SOURCE_DIR}/tests/registry/r02_bad_schema_version.json)
set_tests_properties(e1_r02_reject_unknown_schema PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r03_reject_duplicate_id_edition COMMAND crivo registry validate --dir ${CMAKE_SOURCE_DIR}/tests/fixtures/duplicate_ref_dir)
set_tests_properties(e1_r03_reject_duplicate_id_edition PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r04_reject_invalid_kind_enum COMMAND crivo registry validate --file ${CMAKE_SOURCE_DIR}/tests/registry/r04_invalid_kind_enum.json)
set_tests_properties(e1_r04_reject_invalid_kind_enum PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r05_reject_numeric_in_string COMMAND crivo registry validate --file ${CMAKE_SOURCE_DIR}/tests/registry/r05_numeric_in_string_field.json)
set_tests_properties(e1_r05_reject_numeric_in_string PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r06_reject_unresolved_reference COMMAND crivo registry validate --dir ${CMAKE_SOURCE_DIR}/tests/fixtures/unresolved_ref_dir)
set_tests_properties(e1_r06_reject_unresolved_reference PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r07_reject_missing_rights COMMAND crivo registry validate --file ${CMAKE_SOURCE_DIR}/tests/registry/r07_missing_rights.json)
set_tests_properties(e1_r07_reject_missing_rights PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r08_reject_syntax_error COMMAND crivo registry validate --file ${CMAKE_SOURCE_DIR}/tests/registry/r08_syntax_error.json)
set_tests_properties(e1_r08_reject_syntax_error PROPERTIES WILL_FAIL TRUE)

add_test(NAME e1_r09_import_idempotent_first COMMAND crivo registry import --dir ${CMAKE_SOURCE_DIR}/catalog --db ${CMAKE_BINARY_DIR}/e1_test.db)
add_test(NAME e1_r09_import_idempotent_repeat COMMAND crivo registry import --dir ${CMAKE_SOURCE_DIR}/catalog --db ${CMAKE_BINARY_DIR}/e1_test.db)
set_tests_properties(e1_r09_import_idempotent_first PROPERTIES PASS_REGULAR_EXPRESSION "CRIVO REGISTRY IMPORT PASS")
set_tests_properties(e1_r09_import_idempotent_repeat PROPERTIES DEPENDS e1_r09_import_idempotent_first PASS_REGULAR_EXPRESSION "CRIVO REGISTRY IMPORT PASS")

add_test(NAME e1_r10_dry_run_no_mutation COMMAND crivo registry import --dir ${CMAKE_SOURCE_DIR}/catalog --db ${CMAKE_BINARY_DIR}/e1_dryrun_must_not_exist.db --dry-run)
set_tests_properties(e1_r10_dry_run_no_mutation PROPERTIES PASS_REGULAR_EXPRESSION "DRY-RUN")

# Testes de Aceite E2 — Seleção de Perfil e Avaliação de Aplicabilidade
add_test(NAME e2_plan_pilot_all_applicable COMMAND crivo plan --profile pilot-e1 --dir ${CMAKE_SOURCE_DIR}/catalog)
set_tests_properties(e2_plan_pilot_all_applicable PROPERTIES PASS_REGULAR_EXPRESSION "\"applicable_count\":3")

add_test(NAME e2_plan_not_applicable_capability COMMAND crivo plan --profile pilot-e1 --dir ${CMAKE_SOURCE_DIR}/catalog --unsupported-caps capability:sqlite)
set_tests_properties(e2_plan_not_applicable_capability PROPERTIES PASS_REGULAR_EXPRESSION "\"not_applicable_count\":1")

add_test(NAME e2_plan_unknown_capability_open_world COMMAND crivo plan --profile pilot-e1 --dir ${CMAKE_SOURCE_DIR}/catalog --supported-caps capability:json_parser)
set_tests_properties(e2_plan_unknown_capability_open_world PROPERTIES PASS_REGULAR_EXPRESSION "\"unknown_count\":2")

add_test(NAME e2_plan_fail_closed_rejection COMMAND crivo plan --profile pilot-e1 --dir ${CMAKE_SOURCE_DIR}/catalog --supported-caps capability:json_parser --fail-closed)
set_tests_properties(e2_plan_fail_closed_rejection PROPERTIES WILL_FAIL TRUE)

# Testes de Aceite E3 — Ciclo de Vida, Trilha de Auditoria e Reconciliação
add_test(NAME e3_events_record_and_list COMMAND crivo events record --db ${CMAKE_BINARY_DIR}/e3_events.db --entity-type test_specification --entity-ref crivo.sqlite.transaction.rollback@0.1.0 --event-type QUALIFY --cause qualification_review_passed --authority maintainer-local)
set_tests_properties(e3_events_record_and_list PROPERTIES PASS_REGULAR_EXPRESSION "CRIVO EVENT RECORDED PASS")

add_test(NAME e3_events_list_query COMMAND crivo events list --db ${CMAKE_BINARY_DIR}/e3_events.db --entity crivo.sqlite.transaction.rollback@0.1.0)
set_tests_properties(e3_events_list_query PROPERTIES DEPENDS e3_events_record_and_list PASS_REGULAR_EXPRESSION "\"event_type\":\"QUALIFY\"")

add_test(NAME e3_events_missing_entity_ref COMMAND crivo events record --db ${CMAKE_BINARY_DIR}/e3_events.db --entity-type test_specification --event-type QUALIFY)
set_tests_properties(e3_events_missing_entity_ref PROPERTIES WILL_FAIL TRUE)

add_test(NAME e3_events_reconcile_orphans COMMAND crivo events reconcile --db ${CMAKE_BINARY_DIR}/e3_events.db)
set_tests_properties(e3_events_reconcile_orphans PROPERTIES PASS_REGULAR_EXPRESSION "CRIVO RECONCILIATION PASS")

# Testes de Aceite E4 — Adaptador CTest e Execução Isolada de Fixture Sintética
add_test(NAME e4_ctest_discover_synthetic COMMAND crivo adapter ctest discover --build ${SYNTHETIC_FIXTURE_BUILD} --project synthetic-pilot)
set_tests_properties(e4_ctest_discover_synthetic PROPERTIES PASS_REGULAR_EXPRESSION "\"total_discovered\":3")

add_test(NAME e4_ctest_run_synthetic COMMAND crivo adapter ctest run --build ${SYNTHETIC_FIXTURE_BUILD} --evidence-dir ${CMAKE_BINARY_DIR}/synthetic_evidence --project synthetic-pilot)
set_tests_properties(e4_ctest_run_synthetic PROPERTIES PASS_REGULAR_EXPRESSION "\"adapter\":\"ctest\"")

add_test(NAME e4_ctest_reject_nonexistent_build COMMAND crivo adapter ctest discover --build ${CMAKE_BINARY_DIR}/nonexistent_build_dir)
set_tests_properties(e4_ctest_reject_nonexistent_build PROPERTIES WILL_FAIL TRUE)

add_test(NAME e4_ctest_reject_outside_workspace COMMAND crivo adapter ctest discover
  --build ${SYNTHETIC_FIXTURE_BUILD} --workspace-root ${CMAKE_SOURCE_DIR}/catalog)
set_tests_properties(e4_ctest_reject_outside_workspace PROPERTIES WILL_FAIL TRUE)

add_test(NAME e4_ctest_reject_invalid_project_id COMMAND crivo adapter ctest discover
  --build ${SYNTHETIC_FIXTURE_BUILD} --project "invalid/project")
set_tests_properties(e4_ctest_reject_invalid_project_id PROPERTIES WILL_FAIL TRUE)

add_test(NAME e4_ctest_enforce_timeout COMMAND crivo adapter ctest run
  --build ${TIMEOUT_FIXTURE_BUILD} --evidence-dir ${CMAKE_BINARY_DIR}/timeout_evidence
  --project timeout-fixture --timeout 1)
set_tests_properties(e4_ctest_enforce_timeout PROPERTIES WILL_FAIL TRUE TIMEOUT 5)

add_test(NAME e6_external_run_persist COMMAND crivo adapter ctest run
  --build ${SYNTHETIC_FIXTURE_BUILD} --evidence-dir ${CMAKE_BINARY_DIR}/external_run_evidence
  --project persistence-fixture --record-db ${CMAKE_BINARY_DIR}/external_runs.db
  --source-revision fixture-revision --source-worktree clean)
set_tests_properties(e6_external_run_persist PROPERTIES PASS_REGULAR_EXPRESSION "\"project_id\":\"persistence-fixture\"")

add_test(NAME e6_external_run_query COMMAND crivo external-runs --db ${CMAKE_BINARY_DIR}/external_runs.db)
set_tests_properties(e6_external_run_query PROPERTIES DEPENDS e6_external_run_persist
  PASS_REGULAR_EXPRESSION "\"source_revision\":\"fixture-revision\"")

add_test(NAME e7_native_evidence_record COMMAND crivo external-record
  --db ${CMAKE_BINARY_DIR}/native_external_runs.db
  --evidence-dir ${CMAKE_SOURCE_DIR}/tests/fixtures/external_evidence)
set_tests_properties(e7_native_evidence_record PROPERTIES
  PASS_REGULAR_EXPRESSION "CRIVO EXTERNAL EVIDENCE RECORDED PASS")

add_test(NAME e7_native_evidence_query COMMAND crivo external-runs
  --db ${CMAKE_BINARY_DIR}/native_external_runs.db)
set_tests_properties(e7_native_evidence_query PROPERTIES DEPENDS e7_native_evidence_record
  PASS_REGULAR_EXPRESSION "\"adapter\":\"crivo-native\"")

# Testes de Aceite E8 — Verificação sob Demanda e Sandbox Efêmera (CRIVO-DEV-002 / ADR-0008 / ADR-0009)
add_test(NAME e8_check_on_demand_pass COMMAND crivo check
  --target ${CMAKE_SOURCE_DIR}/catalog
  --profile pilot-e1
  --catalog ${CMAKE_SOURCE_DIR}/catalog
  --evidence-dir ${CMAKE_BINARY_DIR}/check_evidence
  --db ${CMAKE_BINARY_DIR}/check_test.db)
set_tests_properties(e8_check_on_demand_pass PROPERTIES
  PASS_REGULAR_EXPRESSION "CRIVO CHECK PASS")

add_test(NAME e8_check_json_summary COMMAND crivo check
  --target ${CMAKE_SOURCE_DIR}/catalog
  --profile pilot-e1
  --catalog ${CMAKE_SOURCE_DIR}/catalog
  --evidence-dir ${CMAKE_BINARY_DIR}/check_evidence_json
  --db ${CMAKE_BINARY_DIR}/check_test.db
  --json)
set_tests_properties(e8_check_json_summary PROPERTIES
  PASS_REGULAR_EXPRESSION "\"schema_version\":\"crivo.check-summary/1.0.0\"")

add_test(NAME e8_check_empty_target_blocked COMMAND crivo check
  --target ${CMAKE_BINARY_DIR}/nonexistent_target_dir
  --profile pilot-e1
  --catalog ${CMAKE_SOURCE_DIR}/catalog
  --evidence-dir ${CMAKE_BINARY_DIR}/check_evidence_fail
  --fail-closed)
set_tests_properties(e8_check_empty_target_blocked PROPERTIES WILL_FAIL TRUE)

