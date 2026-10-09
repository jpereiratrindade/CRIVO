# Evidência E4 — Adaptador CTest e Execução Verificável em Fixture Sintética

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-001@0.2.0`, entrega E4
- **Documento de referência:** `CRIVO-IMPL-001@0.1.0`
- **Branch:** `feat/e1-international-test-registry`
- **Baseline de origem:** `84c10af`
- **ADRs vinculados:** `ADR-0001`, `ADR-0002`, `ADR-0003`, `ADR-0004`, `ADR-0005`

---

## 1. Ambiente Observado

| Componente | Versão observada |
|---|---|
| Ambiente de execução | Toolbox `crivo-dev` (Fedora 44) |
| CMake / CTest | 4.3.0 |
| Compilador | GCC 16.2.1 20260819 (Red Hat 16.2.1-2) |
| Padrão C++ | C++26 (`-std=c++26`, flags `-Wall -Wextra -Wpedantic`) |
| SQLite3 | 3.51.2 |
| Executor Externo | CTest v4.3.0 (`--show-only=json-v1`, `--output-junit`) |

---

## 2. Entregáveis de E4 Produzidos

### 2.1 Schemas de Descoberta e Evidência
- `schemas/adapter-ctest-discovery/1.0.0.schema.json`: schema de inventário de descoberta JSON CTest v1.
- `schemas/evidence/1.0.0.schema.json`: schema do pacote de evidência com digests SHA-256 e métricas temporais.

### 2.2 Módulo C++ do Adaptador CTest
- `src/adapters/ctest_adapter.hpp` e `ctest_adapter.cpp`:
  - Descoberta automatizada de testes com parse do formato CTest JSON v1 (`kind=ctestInfo`).
  - Execução controlada com geração de JUnit XML e metadados `evidence.json`.
  - Cálculo de integridade SHA-256 de artefatos produzidos.
- `src/main.cpp`: integração dos comandos:
  - `crivo adapter ctest discover --build <dir> [--project <id>]`
  - `crivo adapter ctest run --build <dir> --evidence-dir <dir> [--project <id>]`

### 2.3 Fixture Sintética Local
- `tests/fixtures/ctest/CMakeLists.txt`: projeto de teste controlado com casos `synthetic_pass`, `synthetic_fail`, `synthetic_skip`.

---

## 3. Matriz de Resultados do Gate E4 (CTest)

| Caso de Teste | Critério E4 | Status Observado | Duração |
|---|---|---|---|
| `e4_ctest_discover_synthetic` | Descoberta precisa de 3 testes na fixture sintética | **PASS** | 0.02 s |
| `e4_ctest_run_synthetic` | Execução isolada com JUnit e evidência SHA-256 | **PASS** | 0.03 s |
| `e4_ctest_reject_nonexistent_build` | Rejeição segura de diretório inexistente | **PASS** (rejeição esperada) | 0.00 s |
| `e3_*` (4 testes) | Suíte de eventos e ciclo de vida E3 | **PASS** (4/4) | 0.03 s |
| `e2_*` (4 testes) | Suíte de planejamento e aplicabilidade E2 | **PASS** (4/4) | 0.00 s |
| `e1_*` (11 testes) | Suíte de validação estrita E1 | **PASS** (11/11) | 0.12 s |
| `crivo_*` (9 testes) | Suíte legada v0.1.0 | **PASS** (9/9) | 0.12 s |

**Total de testes CTest:** 31/31 aprovados (100% de sucesso).

---

## 4. Declaração Factual de Encerramento da Sequência E1 → E2 → E3 → E4

A sequência completa do ciclo `CRIVO-IMPL-001` foi entregue e evidenciada:
1. **E1:** Schemas estritos, lote piloto internacional e integridade referencial.
2. **E2:** Resolução determinística de perfis e aplicabilidade tri-estado com falha fechada.
3. **E3:** Trilha de auditoria append-only com eventos de ciclo de vida e digest SHA-256.
4. **E4:** Adaptador CTest operacionalizado e testado com isolamento na fixture local.
