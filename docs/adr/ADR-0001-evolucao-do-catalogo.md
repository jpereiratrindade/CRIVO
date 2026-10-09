# ADR-0001 — Evolução incremental do catálogo v0.1.0

- Estado: aceito
- Data: 2026-10-09
- Baseline: `1d38184459d2f1173eb2b7065fef27f5e5c1411a`
- Missão: `CRIVO-DEV-001@0.2.0`, entrega E0

## Contexto

O catálogo legado `catalog/tests.json` reúne referência conceitual, especificação,
implementação e seleção por perfil em uma única estrutura. A missão v0.2.0 exige
entidades distintas, versionadas e validáveis, sem tornar o histórico v0.1.0
ilegível nem apresentar interfaces propostas como já implementadas.

## Decisão

1. Preservar `catalog/tests.json` e a CLI v0.1.0 durante a migração.
2. Introduzir, por incrementos, registros separados em `catalog/references/`,
   `catalog/techniques/`, `catalog/specifications/`,
   `catalog/implementations/` e `catalog/profiles/`.
3. Tratar arquivos declarativos versionados no Git como fonte de admissão e o
   SQLite como persistência migrada e projeção operacional. A relação exata será
   refinada antes de E1, evitando duas fontes mutáveis concorrentes.
4. Exigir validação estrita e referências cruzadas antes de persistir registros.
5. Manter uma visão de compatibilidade do catálogo legado; a migração será
   idempotente e não apagará execuções anteriores.
6. Separar explicitamente os estados de inventário, especificação,
   implementação, execução, evidência e autoridade.

## Consequências

- A v0.1.0 permanece reproduzível pelo commit de baseline.
- Nenhum item inventariado será apresentado como implementado ou evidenciado.
- IDs e versões admitidos não serão reciclados ou sobrescritos.
- O primeiro incremento funcional após E0 deve definir contratos e o importador
  em modo `--dry-run`, antes de criar novos endpoints ou executar adaptadores.

## Alternativas rejeitadas

- Substituir imediatamente o catálogo legado: quebra compatibilidade e dificulta
  a comparação com a baseline.
- Usar uma tabela ou arquivo único com um campo de estado genérico: mistura
  dimensões que possuem ciclos de vida independentes.
- Copiar textos integrais de normas: desnecessário ao inventário e potencialmente
  incompatível com seus direitos de distribuição.
