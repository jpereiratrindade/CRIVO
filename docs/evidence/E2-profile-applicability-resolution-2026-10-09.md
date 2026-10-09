# Evidência E2 — Resolução de Perfis e Avaliação de Aplicabilidade

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-001@0.2.0`, entrega E2
- **Documento de referência:** `CRIVO-IMPL-001@0.1.0`
- **Branch:** `feat/e1-international-test-registry`
- **Baseline de origem:** `7f121e8`
- **ADRs vinculados:** `ADR-0001`, `ADR-0002`, `ADR-0003`

---

## 1. Ambiente Observado

| Componente | Versão observada |
|---|---|
| Ambiente de execução | Toolbox `crivo-dev` (Fedora 44) |
| CMake / CTest | 4.3.0 |
| Compilador | GCC 16.2.1 20260819 (Red Hat 16.2.1-2) |
| Padrão C++ | C++26 (`-std=c++26`, flags `-Wall -Wextra -Wpedantic`) |
| SQLite3 | 3.51.2 |
| Boost | 1.90.0 (`Boost.JSON`) |

---

## 2. Entregáveis de E2 Produzidos

### 2.1 Schema de Plano de Teste
- `schemas/plan/1.0.0.schema.json`: especificação estrita para serialização e persistência de snapshots de planos de teste.

### 2.2 Módulos C++ de Aplicabilidade e Resolução de Plano
- `src/registry/applicability.hpp` e `applicability.cpp`: mecanismo tri-estado (`APPLICABLE`, `NOT_APPLICABLE`, `UNKNOWN`) com suporte a mundo aberto e mundo fechado.
- `src/registry/plan.hpp` e `plan.cpp`: resolvedor determinístico de especificações e implementações com base em perfis e capacidades ambientais.
- `src/main.cpp`: integração do comando `crivo plan --profile <id> [--supported-caps <...>] [--unsupported-caps <...>] [--fail-closed]`.

---

## 3. Matriz de Resultados do Gate E2 (CTest)

| Caso de Teste | Critério E2 | Status Observado | Duração |
|---|---|---|---|
| `e2_plan_pilot_all_applicable` | Resolução do perfil `pilot-e1` com todas capacidades presentes (3 aplicáveis) | **PASS** | 0.00 s |
| `e2_plan_not_applicable_capability` | Capacidade `sqlite` explicitamente não suportada (1 não aplicável) | **PASS** | 0.00 s |
| `e2_plan_unknown_capability_open_world` | Capacidade omitida em mundo aberto (2 desconhecidos) | **PASS** | 0.00 s |
| `e2_plan_fail_closed_rejection` | Política de falha fechada rejeita execução com capacidades pendentes | **PASS** (rejeição esperada) | 0.00 s |
| `e1_*` (11 testes) | Suíte de validação estrita E1 preservada intacta | **PASS** (11/11) | 0.12 s |
| `crivo_*` (9 testes) | Suíte legada v0.1.0 preservada intacta | **PASS** (9/9) | 0.12 s |

**Total de testes CTest:** 24/24 aprovados (100% de sucesso).

---

## 4. Declaração Factual de Encerramento do Gate E2

O incremento **E2** foi implementado e validado:
1. Resolução formal entre `Profile`, `TestSpec`, `TestImplementation` e capacidades do ambiente.
2. Avaliação tri-estado estrita sem falso-positivo de conformidade ou falso-negativo de falha do software-alvo.
3. Política de falha fechada validada contra planos com capacidades indeterminadas.
