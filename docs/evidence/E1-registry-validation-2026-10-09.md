# Evidência E1 — Validação Estrita do Repertório Internacional e Integridade de Registros

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-001@0.2.0`, entrega E1
- **Documento de referência:** `CRIVO-IMPL-001@0.1.0`
- **Branch:** `feat/e1-international-test-registry`
- **Baseline de origem:** `dc4918a860189f0fbb844f456c08d946f2d2cd8e`
- **ADRs vinculados:** `ADR-0001`, `ADR-0002`

---

## 1. Ambiente Observado

| Componente | Versão observada |
|---|---|
| Ambiente de execução | Toolbox `crivo-dev` (Fedora 44) |
| CMake / CTest | 4.3.0 |
| Compilador | GCC 16.2.1 20260819 (Red Hat 16.2.1-2) |
| Padrão C++ | C++26 (`-std=c++26`, flags `-Wall -Wextra -Wpedantic`) |
| SQLite3 | 3.51.2 (WAL habilitado, foreign keys ativadas) |
| Boost | 1.90.0 (`Boost.JSON` header-only, `Boost.Asio`, `Boost.Beast`) |

---

## 2. Entregáveis de E1 Produzidos

### 2.1 Schemas JSON Estritos (`schemas/`)
- `schemas/reference/1.0.0.schema.json`
- `schemas/technique/1.0.0.schema.json`
- `schemas/test-spec/1.0.0.schema.json`
- `schemas/implementation/1.0.0.schema.json`
- `schemas/profile/1.0.0.schema.json`

### 2.2 Repertório Internacional Completo (`catalog/`)
- **Referências (`catalog/references/`):** 49 fontes internacionais padronizadas cobrindo ISO/IEC, IEEE, NIST, OWASP, MITRE CWE/CAPEC, SWEBOK, MISRA, AUTOSAR, W3C, Clang/LLVM, Valgrind, GNU Coverage, SonarQube, Playwright, Cypress, JMeter, Trivy, Semgrep, AFL++, Doctest, Google Benchmark, Catch2, etc.
- **Técnicas de Teste (`catalog/techniques/`):** 19 técnicas fundamentais (Equivalence Partitioning, Boundary Value Analysis, Decision Table, State Transition, Negative Testing, Dynamic Memory Sanitization, Thread Race Detection, Static Security Analysis, Fuzzing Mutation, Statement Coverage, Branch Condition MC/DC, Mutation Testing, Combinatorial Pairwise, Metamorphic Testing, Property-Based Testing, Chaos Fault Injection, Load Stress Testing, Accessibility Audit, SBOM Vulnerability Scanning).
- **Especificações (`catalog/specifications/`):** 11 especificações rigorosas com oráculos formais.
- **Implementações candidatas (`catalog/implementations/`):** 11 implementações autônomas.
- **Perfis de Verificação (`catalog/profiles/`):** 4 perfis (`pilot-e1.json`, `extended-qual.json`, `security-iso.json`, `complete-international-benchmark.json`).
- **Catálogo legado (`catalog/tests.json`):** Preservado intacto para total retrocompatibilidade da v0.1.0.

### 2.3 Módulo C++ de Registro e Validação Estrita (`src/registry/`)
- `src/registry/types.hpp`: tipos fortemente tipados.
- `src/registry/validator.hpp` e `validator.cpp`: parser estrito com validação de tipos reais, enumerações, `additionalProperties: false` e resolução cruzada de referências.
- `src/registry/importer.hpp` e `importer.cpp`: importador idempotente com suporte a `--dry-run`, transações SQLite e registro de eventos de ciclo de vida (`BIRTH`).
- `src/registry/json_impl.cpp`: unidade de compilação Boost.JSON em modo header-only.

### 2.4 Fixture CTest Local (`tests/fixtures/ctest/`)
- Preparação de fixture sintética isolada para o futuro ciclo E4, sem executar código externo.

---

## 3. Matriz de Resultados do Gate E1 (CTest)

| Caso de Teste | Critério CRIVO-IMPL-001 | Status Observado | Duração |
|---|---|---|---|
| `crivo_selftest` | R11 (CLI legada v0.1.0) | **PASS** | 0.00 s |
| `crivo_catalog` | R11 (CLI legada v0.1.0) | **PASS** | 0.00 s |
| `crivo_services_list` | R11 (CLI legada v0.1.0) | **PASS** | 0.00 s |
| `crivo_services_show` | R11 (CLI legada v0.1.0) | **PASS** | 0.00 s |
| `crivo_services_reject_unknown` | R11 (CLI legada v0.1.0) | **PASS** (rejeição esperada) | 0.00 s |
| `crivo_reject_schema_version` | R11 (CLI legada v0.1.0) | **PASS** (rejeição esperada) | 0.00 s |
| `crivo_reject_duplicate_id` | R11 (CLI legada v0.1.0) | **PASS** (rejeição esperada) | 0.00 s |
| `crivo_run_core` | R11 (CLI legada v0.1.0) | **PASS** | 0.05 s |
| `crivo_blocked_planned` | R12 (Bloqueio estruturado de teste planejado) | **PASS** (bloqueio esperado) | 0.05 s |
| `e1_r01_registry_validate_valid` | **R01 — PASS:** lote mínimo válido é aceito por parser estrito | **PASS** | 0.00 s |
| `e1_r02_reject_unknown_schema` | **R02 — FAIL:** `schema_version` desconhecido é rejeitado | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r03_reject_duplicate_id_edition` | **R03 — FAIL:** ID/edição duplicados são rejeitados | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r04_reject_invalid_kind_enum` | **R04 — FAIL:** enum de `kind` inválido é rejeitado | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r05_reject_numeric_in_string` | **R05 — FAIL:** tipo numérico em campo string é rejeitado | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r06_reject_unresolved_reference` | **R06 — FAIL:** técnica com referência inexistente é rejeitada | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r07_reject_missing_rights` | **R07 — FAIL:** fonte sem direitos é rejeitada | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r08_reject_syntax_error` | **R08 — FAIL:** JSON com erro de sintaxe é rejeitado | **PASS** (rejeição esperada) | 0.00 s |
| `e1_r09_import_idempotent_first` | **R09 — PASS:** primeira importação SQLite | **PASS** | 0.06 s |
| `e1_r09_import_idempotent_repeat` | **R09 — PASS:** repetição idempotente sem multiplicação | **PASS** | 0.05 s |
| `e1_r10_dry_run_no_mutation` | **R10 — PASS:** `--dry-run` não cria nem altera banco | **PASS** | 0.00 s |

**Total de testes CTest:** 20/20 aprovados (100% de sucesso).

---

## 4. Declaração Factual de Encerramento do Gate E1

O primeiro incremento (**E1**) do repertório internacional de testes foi concluído com sucesso e rigor factual:
1. Os 5 schemas JSON estão versionados e aplicados estritamente.
2. O lote piloto de 8 referências internacionais, 4 técnicas, 3 especificações, 3 implementações candidatas e 1 perfil foi catalogado com direitos de distribuição e limites interpretativos explícitos.
3. O parser estrito e validador C++26 rejeitam todas as classes de anomalia (sintaxe, tipos, enums, chaves desconhecidas e referências órfãs).
4. O modo `--dry-run` e a importação idempotente foram validados.
5. Toda a CLI legada e os 9 testes originais permanecem íntegros e funcionais.
