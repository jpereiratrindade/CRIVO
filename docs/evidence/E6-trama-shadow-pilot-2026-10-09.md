# E6 — Piloto TRAMA em modo shadow

- Data UTC: 2026-10-09
- Projeto observado: TRAMA
- Revisão declarada: `4812538352d4b518604de0c4b3177ef8b1c2e3b2`
- Estado observado: worktree com alteração local preexistente
- Autoridade: execução local opt-in; sem efeito de gate

## Resultado factual

O CRIVO configurou e compilou TRAMA em build segregado, descobriu sua suíte CTest e executou o teste `trama-integration` sem modificar fontes do projeto.

| Observação | Resultado |
|---|---:|
| Testes descobertos | 1 |
| PASS | 1 |
| FAIL | 0 |
| SKIP | 0 |
| Executor | CTest 4.3.0 |
| SHA-256 do JUnit | `9c520e6cb67eb4b59420f0ee6ef1161c4fa2a515f2bcf3da3cd023b1d3c4012a` |

Evidência local não versionada: `.run/evidence/TRAMA/20261009T135504Z-192851/`.

## REA — observação, interpretação, revisão

- Observação: suíte CTest atual do TRAMA possui um teste e passou nesta execução.
- Interpretação: integração técnica mínima funciona. Uma execução e um teste não demonstram cobertura ampla, estabilidade nem certificação.
- Revisão: manter modo shadow; ampliar isolamento e repetir sob worktree limpa/revisão publicada antes de qualquer política de gate.

## RIT — integridade temporal

Revisão Git, estado dirty, horário, descoberta, JUnit, metadados e digest foram preservados. Alteração local preexistente em `TRAMA/src/trama.cpp` não foi tocada pelo CRIVO e impede atribuir resultado somente ao commit registrado.

## Incidente de ambiente

Primeira tentativa via Toolbox `crivo-dev` parou na configuração: headers/biblioteca de desenvolvimento OpenSSL ausentes. Isso é falha de ambiente do executor, não falha do teste TRAMA. Harness foi então executado no host, que possui OpenSSL 3.5.9. Launcher foi ajustado para compilar CRIVO na Toolbox e executar piloto TRAMA no host quando Boost não existir no host.
