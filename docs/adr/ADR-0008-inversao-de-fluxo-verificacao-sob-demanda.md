# ADR-0008 — Inversão do fluxo de solicitação e independência tecnológica dos sistemas participantes

- Estado: aceito
- Data: 2026-10-09
- Vinculação: `ADR-0005`, `ADR-0006`, `ADR-0007`, `CRIVO-DEV-001@0.2.0`, `CRIVO-DEV-002@0.3.0`

## Contexto

Originalmente, o CRIVO atuava buscando sistemas para avaliar externamente ou integrando-se via manifestos rígidos. Para apoiar o ciclo de desenvolvimento de sistemas computacionais (como TRAMA, ELO, SINAL e outros), é necessário inverter a relação operacional: o sistema em desenvolvimento solicita verificações pertinentes ao CRIVO sob demanda, sem que o CRIVO exerça fiscalização contínua intrusiva e sem obrigar o sistema participante a incorporar CTest, CMake, GoogleTest ou qualquer biblioteca/framework do CRIVO.

## Decisão

1. **Centralidade do Desenvolvimento e Inversão de Fluxo:**
   - O desenvolvimento solicita verificações ao CRIVO (via CLI, integração de ferramenta ou interface local), fornecendo o alvo/snapshot.
   - O CRIVO identifica capacidades observáveis do alvo, seleciona testes pertinentes do catálogo, prepara as condições de execução e devolve diagnósticos e evidências tecnicamente interpretáveis.
2. **Independência Tecnológica Total:**
   - Nenhum participante é forçado a adotar ferramentas ou estruturas específicas do CRIVO.
   - A integração ocorre por capacidades observáveis (arquivos, portas, executáveis CLI, contratos HTTP, bancos SQLite).
3. **Independência da Verificação e Proveniência:**
   - O participante solicita e contextualiza o teste, mas não pode atribuir a si mesmo o resultado de aprovação.
   - Os critérios, procedimentos, oráculos e registros conservam sua independência técnica e integridade probatória.
4. **Execução sob Demanda e Auditoria Externa:**
   - A modalidade principal passa a ser a verificação sob demanda durante implementação, revisão e correção.
   - A verificação externa independente (`shadow`) e auditorias permanecem suportadas como capacidades autorizadas, sem confusão de papéis.
5. **Modo Híbrido:**
   - Uma mesma solicitação pode orquestrar verificações nativas do CRIVO e adaptadores de suítes existentes do projeto, com proveniência e atribuição de oráculo claramente segregadas nos resultados.

## Consequências

- O CRIVO atua como infraestrutura neutra e reutilizável de testes e produção de evidências a serviço do desenvolvimento.
- Elimina acoplamento de código ou dependências de build nos projetos participantes.
- O sistema participante não precisa alterar seus arquivos ou versionar configurações do CRIVO.
- Requer CLI unificada para invocação de verificações locais (`crivo check`).
