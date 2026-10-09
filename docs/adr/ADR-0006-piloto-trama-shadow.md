# ADR-0006 — Piloto TRAMA em modo shadow

- Estado: aceito
- Data: 2026-10-09
- Vinculação: `ADR-0005`, `CRIVO-DEV-001@0.2.0`, SisTer/REA-RIT

## Contexto

O adaptador CTest foi validado primeiro com fixture sintética. O próximo passo autorizado é observar a suíte real do TRAMA sem transferir ao CRIVO autoridade de gate e sem modificar fontes ou build nativo do projeto.

## Decisão

1. `./crivo.sh pilot-trama` é fluxo explícito e opt-in.
2. CMake configura e compila TRAMA em `build/pilots/TRAMA`, controlado pelo CRIVO.
3. Adaptador invoca `ctest` por `argv` estruturado, sem shell, sob raiz canônica autorizada e timeout finito.
4. Cada execução recebe diretório próprio em `.run/evidence/TRAMA/<UTC>-<pid>` com descoberta, JUnit, evidência SHA-256 e contexto de revisão Git.
5. Modo é `shadow`: resultado informa; não altera TRAMA, não promove versão e não constitui certificação.
6. Worktree suja é registrada, não ocultada nem automaticamente rejeitada. Isso preserva estado observado conforme REA e proveniência temporal conforme RIT.

## Limites e riscos residuais

- Segregação de diretório e processo não equivale a sandbox de kernel.
- Testes CTest pertencem ao TRAMA e podem executar código do projeto com permissões do usuário.
- Contenção de CPU, memória, processos e rede ainda exige worker/sandbox dedicado.
- Execução requer autorização contextual de quem invoca o comando.

## Reversão

Remover comando e build/evidências derivados não muda fontes do TRAMA. Evidências já usadas em decisões devem seguir política de retenção, não ser apagadas silenciosamente.
