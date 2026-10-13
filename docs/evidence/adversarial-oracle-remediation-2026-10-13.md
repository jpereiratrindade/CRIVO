# Remediação da auditoria adversarial — 2026-10-13

Revisão de trabalho: `feat/e1-international-test-registry` (alterações ainda não publicadas).

## Decisões implementadas

- Testes adversariais validam conjuntamente código de saída, classe e causa específica. Não usam mais `|| true` nem aceitam `ORACLE FAIL` genérico.
- Relatório textual TSan pré-existente nunca produz `PASS`. Achado de corrida continua falhando de modo conservador; ausência de achado só é aprovada após compilação e execução instrumentadas pelo CRIVO.
- SBOM separa inventário de auditoria. Dados fornecidos pelo alvo, inclusive metadados de fonte, não provam execução independente; sem scanner executado pelo CRIVO, resulta em `AUDIT_NOT_PERFORMED`/`BLOCKED`.
- Autoverificação SARIF valida apenas o exportador CRIVO e retorna `NOT_APPLICABLE` para o alvo. Agregação registra esse estado como ignorado, não aprovado nem bloqueado.
- Rede habilitada por Bubblewrap usa `--share-net`; relatório agora declara `HOST_SHARED`, não `LOOPBACK_ONLY`.
- `qualify_project` só declara `PROMOTED` quando `promote_qualification_to_experience()` confirma sucesso.
- CLI distingue `ORACLE NOT_APPLICABLE` (código 3) de `ORACLE BLOCKED` (código 2).
- HTTP ignora `crivo-analysis.json` preexistente, executa observação ativa e exige `X-Crivo-Target-Id` igual a `CRIVO_HTTP_EXPECTED_ID`. Entrada maliciosa só conta como rejeitada com `400`, `403` ou `422`; `404` não satisfaz contrato.

## Validação local

- `bash -n tests/adversarial_oracles_test.sh`: aprovado.
- `git diff --check`: aprovado.
- Build/CTest: não executado neste ambiente. Reconfiguração para antes da compilação porque pacote/headers Boost não estão instalados (`BoostConfig.cmake` e `boost/json.hpp` ausentes).

## Limites restantes

- Identidade HTTP usa vínculo operacional por segredo/identificador acordado no lançamento, não atestado criptográfico remoto.
- Inventário automático de dependências continua incompleto para ecossistemas fora de CMake. Esses casos não podem obter auditoria verificada sem SBOM externo com proveniência.
- Validação em repositórios externos reais permanece trabalho operacional separado; nenhum repositório externo foi alterado ou fabricado nesta revisão.
