# ADR-0004 — Trilha de auditoria append-only com eventos de ciclo de vida e digest SHA-256

- Estado: aceito
- Data: 2026-10-09
- Baseline: `20eec66`
- Missão: `CRIVO-DEV-001@0.2.0`, entrega E3
- Vinculação: `ADR-0001`, `ADR-0002`, `ADR-0003`, `CRIVO-IMPL-001@0.1.0`

## Contexto

A preservação da genealogia e da proveniência exige que modificações no catálogo e no estado de entidades e execuções não sejam tratadas por mutação destrutiva (UPDATE/DELETE em tabelas de estado único).

A entrega E3 formaliza a disciplina de Nascimento/Morte com trilha de auditoria append-only via eventos de ciclo de vida (`BIRTH`, `QUALIFY`, `SUSPEND`, `RESUME`, `SUPERSEDE`, `RETIRE`, `RUN_STARTED`, `RUN_FINISHED`, `RUN_INTERRUPTED`), com integridade assegurada por digest SHA-256 e capacidade de reconciliação de execuções órfãs/interrompidas.

## Decisão

1. **Estrutura Append-Only de Eventos:**
   - A tabela `lifecycle_events` registra eventos imutáveis com `event_id`, `entity_type`, `entity_ref`, `event_type`, `occurred_at`, `recorded_at`, `cause` e `authority_ref`.
2. **Digest Canônico SHA-256:**
   - Para cada evento, é calculado um digest SHA-256 (FIPS 180-4) sobre a representação canônica ordenada do payload (`entity_type|entity_ref|event_type|occurred_at|cause|authority_ref`).
   - O hash atesta a integridade e proveniência do registro sem alegar assinatura digital assimétrica de autoridade externa.
3. **Reconciliação de Execuções Órfãs (`RUN_INTERRUPTED`):**
   - Execuções que permaneçam no estado intermediário `RUNNING` após encerramento anômalo ou queda do host são reconciliadas gerando eventos `RUN_INTERRUPTED`, impedindo que uma execução pendente seja interpretada como sucesso.
4. **Comandos CLI:**
   - `crivo events record`: registro explícito de transições de ciclo de vida.
   - `crivo events list`: consulta filtrada por entidade e tipo de evento.
   - `crivo events reconcile`: reconciliação estruturada de execuções órfãs.

## Consequências

- Histórico de eventos reproduzível e rastreável.
- Encerramentos anômalos não mascaram status de execução.
- Garantia de imutabilidade histórica sem sobrescrita de dados.
