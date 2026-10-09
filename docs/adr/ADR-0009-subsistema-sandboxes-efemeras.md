# ADR-0009 — Subsistema de sandboxes efêmeras e isolamento por desenho

- Estado: aceito
- Data: 2026-10-09
- Vinculação: `ADR-0007`, `ADR-0008`, `CRIVO-DEV-002@0.3.0`

## Contexto

A execução de testes em código sob desenvolvimento exige contenção estrita para evitar poluição do sistema hospedeiro, corrupção acidental de dados de produção e vazamento de privilégios. No Linux (especialmente em ambientes modernos e imutáveis como Fedora Silverblue), Toolbx serve como ambiente de desenvolvimento, mas não estabelece barreira de segurança suficiente para contenção de execução não confiável.

## Decisão

1. **Subsistema de Sandboxes Efêmeras:**
   - Cada verificação é executada em uma instância isolada, descartável e identificada, criada especificamente para a solicitação e destruída ao término da coleta de evidências.
   - O encerramento da sandbox elimina resíduos voláteis sem excluir ou invalidar as evidências registradas.
2. **Requisitos e Controles de Isolamento:**
   - Execução rootless sem privilégios administrativos.
   - Sistema de arquivos do projeto montado estritamente como somente leitura (`read-only`) ou operado via snapshot independente. Diretórios temporários de escrita segregados e descartáveis.
   - Isolamento de rede: desabilitada por padrão (`--unshare-net`), liberada apenas mediante perfil explícito com escopo delimitado (ex.: testes de contrato HTTP local).
   - Bloqueio de acesso a diretórios pessoais, chaves SSH, credenciais e IPC do hospedeiro.
   - Aplicação e monitoramento de limites de tempo (`timeout`), memória, processos e CPU.
   - Destruição segura de árvores de processos descendentes ao término ou em caso de timeout/cancelamento.
3. **Mecanismos e Backends no Linux:**
   - **Bubblewrap (`bwrap`)**: isolamento leve de processos via namespaces (`user`, `pid`, `net`, `ipc`, `uts`), seccomp e montagens restritivas.
   - **Podman rootless**: sandboxes conteinerizadas completas quando houver dependências de ambiente segregado.
   - **Processo Nativo Seguro**: fallback restrito com isolamento de diretórios temporários para ambientes onde contenção avançada de kernel for indisponível, sinalizando limitações nos metadados.
4. **Política de Falha Fechada (Fail-Closed):**
   - Se o isolamento exigido pelo perfil não puder ser instanciado, a execução é classificada como `BLOCKED`, jamais permitindo execução silenciosa insegura no hospedeiro.
5. **Tipologia de Ambientes:**
   - `static`: leitura e análise estrutural de arquivos sem execução.
   - `executable`: execução de binários CLI ou suítes de teste isoladas.
   - `service`: inicialização de serviços locais em rede isolada para testes de contrato/API.
   - `persistence`: execução de testes contra bancos SQLite descartáveis.
   - `resilience`: simulação controlada de interrupções, falhas e recuperação.

## Consequências

- Protege a integridade do código-fonte original e do ambiente hospedeiro.
- Garante reproducibilidade e proveniência estrita dos resultados e métricas de consumo.
- A persistência de evidências é fisicamente separada do ambiente temporário de execução.
