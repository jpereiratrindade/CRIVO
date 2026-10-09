# Evidência E9 — Memória Técnica Federada e Aprendizagem Transversal

- **Data da execução:** 2026-10-09 (America/Sao_Paulo)
- **Missão:** `CRIVO-DEV-002@0.3.0`, entrega E5 / Estaleiro Federado
- **ADRs vinculados:** `ADR-0010` (CRIVO como Estaleiro Federado de Engenharia e Aprendizagem)

---

## 1. Resumo Executivo

Demonstrada com 100% de aprovação (46/46 testes no CTest) a capacidade do CRIVO de:
1. Indexar experiências técnicas de engenharia em schema JSON estrito (`schemas/experience-record/1.0.0.schema.json`).
2. Persistir a memória empírica no SQLite WAL (`crivo.db`) preservando o contexto de origem (domínio, linguagem C++26, plataforma POSIX, problemas, alternativas consideradas, escolha técnica, procedimentos e limitações).
3. Associar experiências a evidências auditáveis (`evidence_id` e digest SHA-256 de artefatos de testes de integridade).
4. Permitir recuperação contextual e busca transversal por termo, projeto ou tags de aplicabilidade (`capability:sqlite`, `embedded:resilience`, `offline_first`).
5. Fornecer base para que sistemas subsequentes (como ELO ou SINAL) consultem o conhecimento acumulado sem acoplamento de código ou duplicação de documentação.

---

## 2. Entregáveis Produzidos

- **Contrato JSON Schema v1.0.0:**
  - `schemas/experience-record/1.0.0.schema.json`: Estrutura canônica de registro de experiência técnica.
- **Módulos C++26:**
  - `src/memory.hpp` / `src/memory.cpp`: Persistência SQLite WAL, tabela `technical_experiences`, índices e consultas estruturadas.
  - `src/main.cpp`: Comandos `crivo memory record --file <json>` e `crivo memory query [--query <termo>] [--tag <tag>] [--project <id>]`.
- **Scripting & Automação:**
  - `./crivo.sh memory-record <arquivo>`
  - `./crivo.sh memory-query [opções]`
- **Fixture de Aprendizagem Real:**
  - `tests/fixtures/experiences/trama_sqlite_wal_resilience.json`: Caso empírico real do TRAMA-RS com persistência WAL e integridade transacional.

---

## 3. Matriz de Resultados do Gate E9 (CTest)

| Caso de Teste | Critério de Aceite | Status Observado | Duração |
|---|---|---|---|
| `e9_memory_record_experience` | Gravação e validação de registro de experiência no SQLite | **PASS** | 0.05 s |
| `e9_memory_query_by_term` | Busca textual transversal na memória empírica | **PASS** | 0.00 s |
| `e9_memory_query_by_tag` | Filtro por tag de aplicabilidade (`capability:sqlite`) | **PASS** | 0.00 s |
| `e9_memory_query_cross_project` | Recuperação contextual por projeto (`TRAMA-RS`) | **PASS** | 0.00 s |
| `e9_memory_reject_invalid_file` | Rejeição estrita de arquivos mal formatados ou sem campos obrigatórios | **PASS** (Rejeição esperada) | 0.00 s |
| `e8_*` (3 testes) | Verificação sob demanda e sandboxes efêmeras | **PASS** (3/3) | 0.06 s |
| `e7_*` (2 testes) | Evidências nativas externas e consultas | **PASS** (2/2) | 0.04 s |
| `e6_*` (2 testes) | Persistência externa de execuções | **PASS** (2/2) | 0.10 s |
| `e4_*` (6 testes) | Adaptador CTest e fixtures sintéticas | **PASS** (6/6) | 1.14 s |
| `e3_*` (4 testes) | Ciclo de vida e eventos auditáveis | **PASS** (4/4) | 0.04 s |
| `e2_*` (4 testes) | Motor de aplicabilidade tri-estado | **PASS** (4/4) | 0.00 s |
| `e1_*` (11 testes) | Validação estrita do catálogo | **PASS** (11/11) | 0.12 s |
| `crivo_*` (9 testes) | Fundação e serviços legados v0.1.0 | **PASS** (9/9) | 0.12 s |

**Total da suíte CTest:** 46/46 testes aprovados (100%).
