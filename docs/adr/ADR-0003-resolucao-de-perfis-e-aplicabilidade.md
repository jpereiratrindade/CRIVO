# ADR-0003 — Resolução de perfis e avaliação de aplicabilidade com política de falha fechada

- Estado: aceito
- Data: 2026-10-09
- Baseline: `7f121e8`
- Missão: `CRIVO-DEV-001@0.2.0`, entrega E2
- Vinculação: `ADR-0001`, `ADR-0002`, `CRIVO-IMPL-001@0.1.0`

## Contexto

Diferentes ambientes de execução (host bare-metal, containers, CI, microcontroladores como Raspberry Pi no ELO) e projetos possuem capacidades e restrições heterogêneas (suporte a SQLite WAL, compilador C++26, sockets de rede, threads, sanitizers). Testes não podem ser executados cegamente se as capacidades exigidas não existirem no ambiente, nem a ausência de capacidade deve ser rotulada como falha (`FAIL`) do software-alvo.

A missão E2 exige um resolvedor de perfis (`Profile`) e aplicabilidade que classifique cada teste como `APPLICABLE`, `NOT_APPLICABLE` ou `UNKNOWN`, com suporte a política de falha fechada (`fail_closed`).

## Decisão

1. **Modelo de Capacidades:**
   - Capacidades são identificadas por tags padronizadas (ex: `capability:sqlite`, `capability:json_parser`, `capability:http_server`, `capability:asan`).
   - O ambiente de execução declara um conjunto de capacidades suportadas (`supported`) e não suportadas (`unsupported`).
2. **Avaliação Tri-Estado (`ApplicabilityStatus`):**
   - `APPLICABLE`: todas as capacidades exigidas pela especificação estão presentes em `supported`.
   - `NOT_APPLICABLE`: ao menos uma capacidade exigida está explicitamente listada em `unsupported` (ou ausente em mundo fechado `closed_world`).
   - `UNKNOWN`: uma ou mais capacidades exigidas não constam nas listas do ambiente e o modo não é estritamente fechado.
3. **Resolução de Plano de Teste (`TestPlan`):**
   - O comando `crivo plan --profile <id>` processa os filtros declarativos do perfil (`categories`, `levels`, `purposes`, `include_specs`, `exclude_specs`).
   - Associa cada especificação selecionada à sua implementação candidata ou qualificada.
   - Avalia a aplicabilidade e produz um documento de snapshot auditável em conformidade com `schemas/plan/1.0.0.schema.json`.
4. **Política de Falha Fechada (`--fail-closed`):**
   - Quando ativada, se qualquer teste do plano resultar em `UNKNOWN` ou `NOT_APPLICABLE`, o planejamento é rejeitado imediatamente com código de erro 2 e relatório das capacidades faltantes/indeterminadas.

## Consequências

- Execuções não autorizadas em ambientes sem as capacidades necessárias são impedidas de forma reprodutível antes de qualquer tentativa de teste.
- Geração determinística de planos de teste serializados em formato JSON versionado.
