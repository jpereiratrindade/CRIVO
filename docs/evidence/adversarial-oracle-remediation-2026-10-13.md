# Remediação da auditoria adversarial — 2026-10-13

Branch validada: `main`.

Revisões:

- endurecimento inicial dos oráculos: `7fbb61a6f62b9d4a37bee2065a0848f8d28cfdd1`;
- correção e validação operacional: `1243294`.

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

Data: 2026-10-13. Ambiente: Toolbox `crivo-dev`, Fedora 44, Boost/Boost.JSON 1.90.0, CMake 4.3.0 e GCC 16.2.1.

- Dependências confirmadas por RPM: `boost-devel-1.90.0-7.fc44.x86_64` e `boost-json-1.90.0-7.fc44.x86_64`.
- Header confirmado: `/usr/include/boost/json.hpp`.
- Configuração e compilação Debug: aprovadas.
- Qualificação adversarial `e13_06_adversarial_oracles_qualification`: aprovada, 11/11 verificações.
- Regressão completa: aprovada, 74/74 testes, zero falhas, 17,15 segundos.
- `git diff --check` e validação sintática dos scripts shell: aprovados.

## Classificação dos achados durante a validação

### Defeitos de implementação corrigidos

- Colisão de nome em `expected_identity` impedia compilar `builtin_oracles.cpp`.
- Verificação de identidade HTTP exigia cabeçalho em respostas automáticas `501`; agora exige ao menos uma identidade correta e bloqueia qualquer identidade explícita divergente.
- `evidence.json` canônico não registrava o veredito geral; agora preserva `status` independentemente de `isolation_status`.
- Relatório agregado podia declarar `unshare_net` mesmo após oráculo HTTP usar `--share-net`; capacidade e `network_mode` agora refletem execução observada.

### Limitação ambiental recuperada

- Falha inicial de configuração/compilação ocorreu pela ausência de Boost e Boost.JSON no ambiente usado fora do Toolbox. Dependências foram recuperadas e confirmadas no Toolbox `crivo-dev`; nenhum critério de aceite foi alterado para contornar essa falha.
- As quatro falhas adversariais observadas antes do rebuild vieram da execução de binário antigo após compilação abortada. Depois do rebuild, TSan falso, SBOM falso e `NOT_APPLICABLE` produziram os estados endurecidos esperados.

### Evidências insuficientes preservadas

- TSan textual fornecido pelo alvo não gera aprovação.
- SBOM limpo fornecido pelo alvo não comprova consulta independente a base de vulnerabilidades.
- Autoverificação SARIF não aprova o alvo.
- Perfil com verificações ignoradas resulta em `PARTIAL`, não `PASS`.
- Qualificação `BLOCKED`/`PARTIAL` não pode ser promovida à memória.
- Testes E12/E13 foram ajustados para comprovar esses bloqueios, não para convertê-los artificialmente em aprovação.

## Limites restantes

- Identidade HTTP usa vínculo operacional por segredo/identificador acordado no lançamento, não atestado criptográfico remoto.
- Inventário automático de dependências continua incompleto para ecossistemas fora de CMake. Esses casos não podem obter auditoria verificada sem SBOM externo com proveniência.
- Validação em repositórios externos reais permanece trabalho operacional separado; nenhum repositório externo foi alterado ou fabricado nesta revisão.

## Estado da missão

Validação operacional local concluída e reproduzível no Toolbox indicado. A publicação destas evidências não transforma limitações declaradas em garantias. Princípio mantido: **Sempre pronto. Sempre incompleto.**
