# ADR-0005 — Adaptador CTest para descoberta e execução verificável com fixture sintética

- Estado: aceito
- Data: 2026-10-09
- Baseline: `84c10af`
- Missão: `CRIVO-DEV-001@0.2.0`, entrega E4
- Vinculação: `ADR-0001`, `ADR-0002`, `ADR-0003`, `ADR-0004`, `CRIVO-IMPL-001@0.1.0`

## Contexto

A entrega E4 estabelece a primeira capacidade do CRIVO de orquestrar a descoberta e a execução de suítes de teste de projetos baseados em CMake/CTest. Conforme determinado em `CRIVO-IMPL-001`, antes de interagir com projetos externos reais como TRAMA, o adaptador deve ser rigorosamente testado em uma fixture sintética local isolada (`tests/fixtures/ctest`).

## Decisão

1. **Descoberta Controlada (`crivo adapter ctest discover`):**
   - Executa `ctest --test-dir <build> --show-only=json-v1` em workspace previamente configurado e validado.
   - Valida a estrutura JSON do CTest (`kind == "ctestInfo"`, `version.major == 1`, array `tests`).
   - Retorna o inventário de testes e propriedades serializado em conformidade com `schemas/adapter-ctest-discovery/1.0.0.schema.json`.
2. **Execução Segura e Evidência Isolada (`crivo adapter ctest run`):**
   - Executa `ctest --test-dir <build> --output-junit <evidence_dir>/junit.xml`.
   - Mede duração em relógio monotônico de alta resolução.
   - Gera um pacote de evidência com checksum SHA-256 dos artefatos produzidos (JUnit XML) e metadados estruturados em conformidade com `schemas/evidence/1.0.0.schema.json`.
   - Não efetua gravação destrutiva sobre os fontes do projeto original.
3. **Validação do Piloto Sintético:**
   - Validação inicial realizada exclusivamente contra a fixture sintética local com casos de `PASS`, `FAIL` e `SKIP`.

## Consequências

- O CRIVO passa a ter um adaptador externo funcional, determinístico e auditável para suítes CMake/CTest.
- Preserva a regra de segurança de não executar testes destrutivos no host real.
- Habilita o avanço futuro para o piloto controlado do TRAMA em ambiente devidamente isolado e autorizado.
