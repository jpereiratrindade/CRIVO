---
document_id: CRIVO-DEV-003-ADENDO-001
version: 0.1.0
status: approved
date: 2026-10-09
title: "CRIVO — Acesso Operacional ao Estaleiro em Tempo de Desenvolvimento"
kind: architecture-and-operating-contract
language: pt-BR
implementation: C++26
persistence: SQLite3 WAL
principle: "Sempre pronto. Sempre incompleto."
related:
  - CRIVO-DEV-001
  - CRIVO-DEV-002
  - CRIVO-DEV-003
  - ADR-0008
  - ADR-0009
  - ADR-0010
---

# CRIVO-DEV-003 — ADENDO 001
## Acesso operacional ao Estaleiro: desenvolvedor, IDE, chat/LLM e execução de testes

**Destinatários:** equipe de desenvolvimento CRIVO e responsáveis pelos sistemas participantes.  
**Natureza:** complemento transversal à memória técnica e ao Catálogo de Coisas.  
**Decisão:** antecipar os contratos de consulta e operação do Estaleiro, sem antecipar a complexidade do explorador visual, da indexação semântica ou da integração irrestrita com assistentes.

## 0. Problema que motivou o adendo

A CRIVO-DEV-003 especifica adequadamente *o que* catalogar e *como* vincular coisas, evidências, decisões e aprendizados. Entretanto, para o uso real do Estaleiro, precisamos especificar **como uma equipe formada por pessoa desenvolvedora, IDE e assistente conversacional encontra esse conhecimento e solicita testes enquanto desenvolve**, sem copiar históricos para chats e sem modificar obrigatoriamente os sistemas participantes.

O resultado esperado é um **circuito operacional de desenvolvimento**, não somente mais um catálogo ou painel.

> O sistema em desenvolvimento continua sendo o centro do trabalho. O CRIVO oferece serviços de verificação e recuperação de conhecimento. A pessoa desenvolvedora mantém a decisão, a autorização e o julgamento de aplicabilidade.

## 1. Baseline factual e limites de verificação

- `./crivo.sh check --target . --profile pilot-e1`: `PASS` com verificações locais.
- `./crivo.sh check --target <dir> --profile pilot-e1 --fail-closed --json`: Rejeição rigorosa de falsos positivos quando testes obrigatórios são omitidos ou bloqueados.
- `./crivo.sh up`: 46+ verificações CTest aprovadas, validação e importação de registro concluídas.
- Testes E9 e E10 de memória técnica e contexto de desenvolvimento comprovam recuperação de experiências e proveniência integral.

## 2. As três interfaces da equipe

| Participante | O que precisa | Como acessará |
|---|---|---|
| Pessoa desenvolvedora | Conhecer capacidades, experiências e estado de verificações; autorizar intervenções | CLI e Web local SisTer |
| IDE/editor/agente de programação | Recuperar contexto do projeto/revisão; consultar casos; receber resultados; propor testes | CLI/JSON como base, MCP `stdio` para clientes compatíveis |
| Chat/LLM externo | Obter contexto autorizado, evidências e experiências sem acesso irrestrito ao host | Ferramenta MCP configurada; alternativamente, exportação JSON controlada |

### 2.1. Interface humana
Manter três ações óbvias: **Consultar**, **Solicitar verificação**, **Examinar evidências**. A Web opera em modo estritamente somente leitura.

### 2.2. Interface da IDE
Respostas JSON estáveis e códigos de saída documentados na CLI (`crivo context`, `crivo check --json`, `crivo memory query --json`), além de adaptador MCP local `stdio` somente leitura embutido no CRIVO (`crivo mcp`).

### 2.3. Interface de chat/LLM
Acesso via MCP local `stdio` ou exportação estruturada sanitizada. Sem acesso arbitrário ao shell do host ou SQL irrestrito.

## 3. Contrato de contexto de desenvolvimento (`crivo.dev-context/1.0.0`)

**Entradas:**
- `project_id`, revisão Git ou identidade do snapshot de trabalho;
- objetivo/pergunta de desenvolvimento;
- categorias e filtros de consulta (`tag`, `search_term`);
- limite de resultados e orçamento de contexto.

**Saídas:**
- capacidades/"coisas" pertinentes identificadas no projeto;
- experiências anteriores potencialmente relevantes na memória do Estaleiro;
- histórico de verificações recentes e evidências associadas;
- links para artefatos e referências normativas.

## 4. Contrato único de execução de verificações

```
Solicitação -> Contexto/revisão -> Plano -> Política/autorização
            -> Executor/sandbox apropriada -> Resultado individual
            -> Evidência -> Memória/Coisas -> Consulta posterior
```

### 4.1. Semântica estrita de resultados e Fail-Closed
- `PASS`: critério aplicado e integralmente atendido;
- `FAIL`: critério aplicado e não atendido;
- `ERROR`/`TIMEOUT`: erro operacional ou estouro de limite monotônico;
- `BLOCKED`: execução não autorizada ou requisitos ausentes;
- `SKIPPED`: omitido com motivo explícito (quando `--fail-closed` for fornecido, a presença de omissões impede o status global `PASS`).

## 5. Interoperabilidade: Servidor MCP `stdio` Embutido

O CRIVO incorpora nativamente um servidor MCP com transporte `stdio` (`./build/crivo mcp --db .run/crivo.db`), expondo as seguintes ferramentas de leitura:
- `dev_context`: Recupera o contexto estruturado de desenvolvimento do projeto e tecnologias relacionadas.
- `knowledge_query`: Consulta experiências empíricas transversais no Estaleiro por termo ou tag.
- `catalog_list`: Lista especificações, técnicas de teste e referências internacionais.
- `evidence_get`: Consulta evidências e resumos de execuções com digest SHA-256.

## 6. Governança e Curadoria de Experiências

1. O CRIVO coleta fatos e evidências verificáveis do teste em sandbox.
2. A IDE ou assistente propõe interpretação e experiência estruturada.
3. A pessoa desenvolvedora avalia relevância, limites e autoriza registro.
4. Registro é indexado no SQLite WAL com genealogia e proveniência mantidas.

**Sempre pronto. Sempre incompleto.**
