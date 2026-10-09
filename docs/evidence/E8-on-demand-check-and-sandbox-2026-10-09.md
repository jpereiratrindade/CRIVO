# Evidência E8 — Verificação sob Demanda e Subsistema de Sandboxes Efêmeras

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-002@0.3.0`, entregas E2, E3 e E4
- **ADRs vinculados:** `ADR-0008` (Inversão do fluxo e independência), `ADR-0009` (Sandboxes efêmeras)

---

## 1. Resumo Executivo

Demonstrada com 100% de aprovação (41/41 testes no CTest) a capacidade de o CRIVO:
1. Receber solicitações sob demanda do desenvolvimento (`crivo check`).
2. Descobrir dinamicamente capacidades observáveis do alvo sem impor modificações nem dependências de testes no repositório verificado.
3. Instanciar sandboxes efêmeras (com isolamento de filesystem `read-only`, `tmpfs` descartável, contenção de rede e limites monotônicos de tempo/recursos).
4. Gerar relatórios estruturados de sandbox (`schemas/sandbox-report/1.0.0.schema.json`), pacotes canônicos de evidência (`crivo.evidence/1.0.0`) com digests SHA-256 e registrar o histórico no SQLite WAL (`crivo.db`).
5. Destruir os resíduos da sandbox efêmera preservando a integridade das evidências registradas.

---

## 2. Entregáveis Produzidos

- **Contratos JSON Schema v1.0.0:**
  - `schemas/check-request/1.0.0.schema.json`
  - `schemas/sandbox-report/1.0.0.schema.json`
- **Módulos C++26:**
  - `src/sandbox.hpp` / `src/sandbox.cpp`: Drivers de isolamento (Bubblewrap, Podman rootless, HostIsolated) com limites de memória, processos e timeout rígido.
  - `src/inspector.hpp` / `src/inspector.cpp`: Descoberta de capacidades, resolução de planos de teste e emissão de evidência.
  - `src/main.cpp`: Comando unificado `crivo check` com suporte a `--json`, `--fail-closed`, `--profile`, `--isolation`.
- **Scripting & Automação:**
  - `./crivo.sh check [opções]`: Invocação automatizada e integrada ao ambiente de desenvolvimento.

---

## 3. Matriz de Resultados do Gate E8 (CTest)

| Caso de Teste | Critério de Aceite | Status Observado | Duração |
|---|---|---|---|
| `e8_check_on_demand_pass` | Verificação sob demanda bem-sucedida em sandbox efêmera | **PASS** | 0.03 s |
| `e8_check_json_summary` | Emissão de sumário estruturado JSON `crivo.check-summary/1.0.0` | **PASS** | 0.03 s |
| `e8_check_empty_target_blocked` | Rejeição sem falso-pass sob diretório vazio / ausência de capacidades com `--fail-closed` | **PASS** (Rejeição esperada) | 0.00 s |
| `e7_*` (2 testes) | Evidências nativas externas e consultas | **PASS** (2/2) | 0.04 s |
| `e6_*` (2 testes) | Persistência externa de execuções | **PASS** (2/2) | 0.09 s |
| `e4_*` (6 testes) | Adaptador CTest e fixtures sintéticas | **PASS** (6/6) | 1.14 s |
| `e3_*` (4 testes) | Ciclo de vida e eventos auditáveis | **PASS** (4/4) | 0.03 s |
| `e2_*` (4 testes) | Motor de aplicabilidade tri-estado | **PASS** (4/4) | 0.00 s |
| `e1_*` (11 testes) | Validação estrita do catálogo | **PASS** (11/11) | 0.12 s |
| `crivo_*` (9 testes) | Fundação e serviços legados v0.1.0 | **PASS** (9/9) | 0.12 s |

**Total da suíte CTest:** 41/41 testes aprovados (100%).
