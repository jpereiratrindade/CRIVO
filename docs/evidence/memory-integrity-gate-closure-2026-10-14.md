# Encerramento do gate de integridade da memória — 14/10/2026

## Decisão

O gate constitutivo de integridade da memória técnica está concluído. Esta
decisão cobre o contrato de promoção, a integridade dos artefatos, a
rastreabilidade das referências e a regressão local do mecanismo. Não declara
certificação universal de proveniência nem conclui a qualificação operacional
do CRIVO em sistemas externos.

## Evidências reproduzidas

- Compilação Debug concluída no Fedora Toolbox `crivo-dev`.
- Regressão CTest: **75/75 testes aprovados em 13,43 segundos**.
- Promoção válida preserva, byte a byte, o resumo e `evidence.json`.
- Evidência adulterada após o cálculo do SHA-256 é rejeitada.
- Resumos sem campo obrigatório, com tipo inválido ou com `PASS` contraditório
  a achados de falha são rejeitados na própria fronteira de promoção.
- O documento é validado pelo contrato versionado
  `crivo.qualification-summary/1.0.0` antes das regras de promoção.
- Alvo sem Git pode produzir experiência válida com revisão `unknown`, desde
  que essa limitação seja registrada e não seja apresentada como atestação.

## Proveniência e limite de confiança

A evidência canônica registra revisão e estado observáveis, sistema de revisão,
nível de confiança e limitações. Quando Git está disponível, a revisão é obtida
por execução direta do programa, sem interpolação de shell. Quando não existe
revisão observável, o CRIVO registra `revision_system: none`, revisão `unknown`
e a limitação correspondente.

Esses dados descrevem contexto e rastreabilidade. `locally_observed` não
significa atestação independente nem autenticidade criptográfica do repositório.
Ausência de atestação não vira aprovação; também não invalida automaticamente
software sem sistema de versionamento.

## Escopo após o fechamento

- Execução em dois sistemas externos: marco separado de qualificação
  operacional e interoperabilidade.
- CI pública: melhoria recomendada de engenharia, não condição constitutiva do
  mecanismo de memória.
- Assinatura e atestação criptográfica: evolução futura e perfil específico,
  não dependência universal.

## Regra de governança para novas exigências

Toda nova exigência deve declarar:

1. qual responsabilidade do CRIVO protege;
2. qual evidência demonstra sua necessidade;
3. se pertence ao núcleo constitutivo, à qualificação operacional ou à evolução
   futura.

Uma etapa pode ser concluída dentro de fronteira de validade explícita sem
declarar a plataforma completa. **Sempre pronto. Sempre incompleto.**
