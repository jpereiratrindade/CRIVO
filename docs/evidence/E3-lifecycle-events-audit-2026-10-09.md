# Evidência E3 — Ciclo de Vida, Trilha de Auditoria e Reconciliação

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-001@0.2.0`, entrega E3
- **Documento de referência:** `CRIVO-IMPL-001@0.1.0`
- **Branch:** `feat/e1-international-test-registry`
- **Baseline de origem:** `20eec66`
- **ADRs vinculados:** `ADR-0001`, `ADR-0002`, `ADR-0003`, `ADR-0004`

---

## 1. Ambiente Observado

| Componente | Versão observada |
|---|---|
| Ambiente de execução | Toolbox `crivo-dev` (Fedora 44) |
| CMake / CTest | 4.3.0 |
| Compilador | GCC 16.2.1 20260819 (Red Hat 16.2.1-2) |
| Padrão C++ | C++26 (`-std=c++26`, flags `-Wall -Wextra -Wpedantic`) |
| SQLite3 | 3.51.2 |
| Algoritmo Criptográfico | SHA-256 (FIPS 180-4 canônico para integridade de payload) |

---

## 2. Entregáveis de E3 Produzidos

### 2.1 Schema de Evento de Ciclo de Vida
- `schemas/lifecycle-event/1.0.0.schema.json`: especificação para serialização dos eventos de ciclo de vida e auditoria.

### 2.2 Módulo C++ de Ciclo de Vida e Digest
- `src/registry/lifecycle.hpp` e `lifecycle.cpp`: implementação de gravação append-only em SQLite, cálculo de SHA-256 de 64 caracteres hexadecimais, consultas filtradas e rotina de reconciliação para encerramentos anômalos.
- `src/main.cpp`: integração dos comandos `crivo events record`, `crivo events list`, `crivo events reconcile`.

---

## 3. Matriz de Resultados do Gate E3 (CTest)

| Caso de Teste | Critério E3 | Status Observado | Duração |
|---|---|---|---|
| `e3_events_record_and_list` | Registro de evento QUALIFY com digest SHA-256 | **PASS** | 0.02 s |
| `e3_events_list_query` | Consulta filtrada por entidade confirmando persistência e tipo | **PASS** | 0.00 s |
| `e3_events_missing_entity_ref` | Rejeição de registro de evento sem `--entity-ref` | **PASS** (rejeição esperada) | 0.00 s |
| `e3_events_reconcile_orphans` | Reconciliação estruturada de execuções órfãs | **PASS** | 0.00 s |
| `e2_*` (4 testes) | Suíte de planejamento e aplicabilidade E2 | **PASS** (4/4) | 0.00 s |
| `e1_*` (11 testes) | Suíte de validação estrita E1 | **PASS** (11/11) | 0.12 s |
| `crivo_*` (9 testes) | Suíte legada v0.1.0 | **PASS** (9/9) | 0.12 s |

**Total de testes CTest:** 28/28 aprovados (100% de sucesso).

---

## 4. Declaração Factual de Encerramento do Gate E3

O incremento **E3** foi validado e entregue:
1. Eventos de ciclo de vida implementados em persistência append-only.
2. Integridade e proveniência asseguradas por cálculo de digest SHA-256.
3. Prevenção de falsos positivos em caso de encerramento abrupto via reconciliação `RUN_INTERRUPTED`.
