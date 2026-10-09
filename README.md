# CRIVO — Plataforma de Verificação e Validação de Sistemas

**v0.1.0 — protótipo funcional de infraestrutura local (não certificado para produção)**

Primeiro esqueleto executável em C++26, SQLite3 WAL, HTTP local de leitura e interface visual alinhada ao SisTer. A primeira versão contém três testes reais (`catalog.schema`, `sqlite.integrity`, `sqlite.wal`) e seis declarações planejadas, propositalmente marcadas como não implementadas.

## Pré-requisitos

- CMake >= 3.30, compilador com modo C++26 (GCC 14+ ou Clang compatível), SQLite3 development headers, Boost headers (Beast, Asio, Property Tree), threads, Linux.
- C++26 ainda possui suporte parcial conforme compilador/biblioteca. Não ativar downgrade silencioso do padrão.

## Como executar

### Script único (recomendado)

### Script único (recomendado)

O script detecta a ausência do Boost no host e usa automaticamente a Toolbox
`crivo-dev` (ou a indicada por `CRIVO_TOOLBOX`):

```bash
# Executa build, 38 testes CTest, validação estrita, importação, run e sobe o servidor web
./crivo.sh
```

Outros comandos do script:
- `./crivo.sh test` — Executa toda a suíte CTest (38 testes).
- `./crivo.sh pilot-project <manifesto-local.json>` — Executa projeto admitido em runtime, sem conhecimento prévio no código ou catálogo.
- `./crivo.sh verify-project <admissao-local.json>` — Compila alvo sem testes próprios e executa verificações CRIVO-native sobre CLI, SQLite e HTTP.
- `./crivo.sh external-runs` — Consulta índice SQLite de execuções externas e evidências.
- `./crivo.sh validate` — Valida schemas JSON v1.0.0 e integridade referencial.
- `./crivo.sh plan` — Gera e exibe o plano de teste resolvido.
- `./crivo.sh events` — Exibe a trilha de auditoria e eventos de ciclo de vida.
- `./crivo.sh status` — Mostra o ambiente, banco e endereços de rede.

Acesso local: `http://127.0.0.1:8765`.

Consultar serviços disponíveis:

```bash
./build/crivo services list
./build/crivo services show sqlite.integrity
```

Contrato público: [`docs/SERVICES.md`](docs/SERVICES.md).

Para consultar pela rede local:

```bash
./crivo.sh status
./crivo.sh serve-lan
```

Abra `http://IP-DA-MAQUINA:8765` em outro dispositivo. O servidor LAN continua
somente leitura, mas ainda não possui autenticação nem TLS: use apenas em rede
confiável e não encaminhe essa porta no roteador. O firewall do host pode exigir
liberação local deliberada da porta TCP 8765.

### Comandos manuais da CLI (v0.2.0)

```bash
# 1. Validação estrita de catálogo e integridade de schemas v1.0.0
./build/crivo registry validate --dir catalog

# 2. Importação idempotente com modo dry-run e transações SQLite
./build/crivo registry import --dir catalog --db .run/crivo.db --dry-run
./build/crivo registry import --dir catalog --db .run/crivo.db

# 3. Resolução de planos de teste e avaliação de aplicabilidade tri-estado
./build/crivo plan --profile pilot-e1 --dir catalog
./build/crivo plan --profile pilot-e1 --dir catalog --supported-caps capability:json_parser --fail-closed

# 4. Trilha de auditoria append-only e eventos de ciclo de vida (SHA-256)
./build/crivo events record --entity-type test_specification --entity-ref crivo.sqlite.transaction.rollback@0.1.0 --event-type QUALIFY --cause review_passed
./build/crivo events list
./build/crivo events reconcile

# 5. Adaptador CTest para descoberta e execução verificável com JUnit/JSON
./build/crivo adapter ctest discover --build build/synthetic_fixture --project synthetic-pilot
./build/crivo adapter ctest run --build build/synthetic_fixture --evidence-dir .run/evidence --project synthetic-pilot

# 6. Piloto TRAMA opt-in: build segregado, workspace autorizado e evidência persistente
./crivo.sh pilot-project .run/projects/trama.json

# 7. Execução clássica v0.1.0 e servidor Web SisTer
./build/crivo run --profile core
./build/crivo serve --db .run/crivo.db --web web --bind 127.0.0.1 --port 8765
```

A interface é **somente leitura**, sem endpoint para execução remota, alterações de catálogo ou comandos do sistema. Não publique diretamente o HTTP local na rede; requer autenticação, autorização e gateway próprios antes de qualquer exposição.

### Aplicar matriz internacional completa

```bash
./crivo.sh qualify-project /caminho/do/sistema

# forma de baixo nivel:
./build/crivo check \
  --target /caminho/do/sistema \
  --profile complete-international-benchmark \
  --catalog catalog \
  --evidence-dir .run/evidence
```

`qualify-project` é a porta automática: identifica revisão e estado do Git,
configura e compila projetos CMake, executa CTest pelo adaptador, aplica o perfil
internacional e grava `qualification-summary.json` com decisão, falhas, bloqueios,
inaplicabilidades, evidências e próxima ação. Cada `test_contract` explicita o
estímulo aplicado e o comportamento esperado (oráculo) antes da comparação. O sistema analisado não precisa
conhecer o CRIVO. Memória nunca é promovida silenciosamente: um resultado aprovado
fica como `REVIEW_REQUIRED`; falha ou ausência de prova fica como
`INSUFFICIENT_EVIDENCE`.

Os 11 oráculos builtin estão qualificados. CRIVO usa artefatos observáveis: bancos
SQLite (`.db`, `.sqlite`), SARIF, CycloneDX SBOM, relatório TSan, pacotes de
evidência SHA-256 e `crivo-analysis.json` para observações HTTP. O contrato deste
último fica em `schemas/analysis/1.0.0.schema.json`. Insumo ausente não gera
aprovação: resulta em `NOT_APPLICABLE` ou `BLOCKED`.

Consumidores MCP podem chamar `test_matrix` com `target_path` e `profile`. A
ferramenta retorna plano, capabilities e aplicabilidade sem executar código. A
execução permanece deliberadamente fora do MCP somente leitura e ocorre por
`crivo check`, CI ou adaptador CTest.

Inspeção ignora por padrão `.git`, `node_modules`, `vendor`, `.cache`, `dist`,
`coverage` e diretórios iniciados por `build`, evitando falsos resultados vindos
de dependências ou artefatos gerados.

## Limites deliberados e estado atual

- **Catálogo Internacional e Validação (E1):** Implementados schemas JSON estritos v1.0.0 (`Reference`, `Technique`, `TestSpec`, `TestImplementation`, `Profile`) com checagem de tipos reais, enumerações e integridade referencial cruzada via `Boost.JSON`.
- **Resolução de Planos (E2):** Motor de aplicabilidade tri-estado (`APPLICABLE`, `NOT_APPLICABLE`, `UNKNOWN`) com suporte a política de falha fechada (`--fail-closed`).
- **Ciclo de Vida e Auditoria (E3):** Trilha append-only de eventos de ciclo de vida com digest SHA-256 canônico e reconciliação de execuções órfãs (`RUN_INTERRUPTED`).
- **Adaptador CTest (E4):** Descoberta JSON v1 (`kind=ctestInfo`) e execução isolada com emissão de JUnit XML e metadados de evidência auditáveis, validada contra fixture sintética local (`tests/fixtures/ctest`).
- **Piloto TRAMA:** Integração opt-in em modo `shadow`, build segregado sob CRIVO, execução sem shell no adaptador, timeout, evidência por execução e índice persistente no SQLite do CRIVO. Não constitui sandbox de kernel nem gate do TRAMA.
- **Verificação nativa externa:** Projetos podem ser avaliados sem CTest nem código de teste próprio. Manifesto local liga interfaces do alvo; critérios, sequência, oráculos e evidência pertencem ao CRIVO.
- **Próximos passos:** Isolamento de kernel/recursos/rede reforçado e segundo piloto ELO.

## Referências de arquitetura e decisões

- [`docs/CRIVO-001_v0.1.0.md`](docs/CRIVO-001_v0.1.0.md) e [`docs/ROADMAP.md`](docs/ROADMAP.md)
- [`docs/CRIVO-DEV-001_v0.2.0.md`](docs/CRIVO-DEV-001_v0.2.0.md), [`docs/CRIVO-DEV-002_v0.3.0.md`](docs/CRIVO-DEV-002_v0.3.0.md) e [`docs/CRIVO-PROJ-001_v0.2.0.md`](docs/CRIVO-PROJ-001_v0.2.0.md)
- Decisões de Arquitetura:
  - [ADR-0001 — Evolução incremental do catálogo v0.1.0](docs/adr/ADR-0001-evolucao-do-catalogo.md)
  - [ADR-0002 — Validação estrita de schemas e integridade referencial com Boost.JSON](docs/adr/ADR-0002-validador-estrito-e-resolucao-referencial.md)
  - [ADR-0003 — Resolução de perfis e avaliação de aplicabilidade com política de falha fechada](docs/adr/ADR-0003-resolucao-de-perfis-e-aplicabilidade.md)
  - [ADR-0004 — Trilha de auditoria append-only com eventos de ciclo de vida e digest SHA-256](docs/adr/ADR-0004-eventos-de-ciclo-de-vida-e-auditoria.md)
  - [ADR-0005 — Adaptador CTest para descoberta e execução verificável com fixture sintética](docs/adr/ADR-0005-adaptador-ctest-e-execucao-verificavel.md)
  - [ADR-0006 — Piloto TRAMA em modo shadow](docs/adr/ADR-0006-piloto-trama-shadow.md)
  - [ADR-0007 — Verificação externa CRIVO-native](docs/adr/ADR-0007-verificacao-externa-crivo-native.md)
  - [ADR-0008 — Inversão do fluxo de solicitação e independência tecnológica](docs/adr/ADR-0008-inversao-de-fluxo-verificacao-sob-demanda.md)
  - [ADR-0009 — Subsistema de sandboxes efêmeras e isolamento por desenho](docs/adr/ADR-0009-subsistema-sandboxes-efemeras.md)
  - [ADR-0010 — CRIVO como Estaleiro Federado de Engenharia e Aprendizagem](docs/adr/ADR-0010-crivo-como-estaleiro-federado-de-engenharia-e-aprendizagem.md)
- Relatórios Factuais de Evidência:
  - [E0 — Baseline v0.1.0](docs/evidence/E0-baseline-2026-10-09.md)
  - [E1 — Validação Estrita do Repertório Internacional](docs/evidence/E1-registry-validation-2026-10-09.md)
  - [E2 — Resolução de Perfis e Avaliação de Aplicabilidade](docs/evidence/E2-profile-applicability-resolution-2026-10-09.md)
  - [E3 — Ciclo de Vida, Trilha de Auditoria e Reconciliação](docs/evidence/E3-lifecycle-events-audit-2026-10-09.md)
  - [E4 — Adaptador CTest e Execução Verificável em Fixture Sintética](docs/evidence/E4-ctest-adapter-synthetic-pilot-2026-10-09.md)
  - [E8 — Verificação sob Demanda e Subsistema de Sandboxes Efêmeras](docs/evidence/E8-on-demand-check-and-sandbox-2026-10-09.md)
  - [E9 — Memória Técnica Federada e Aprendizagem Transversal](docs/evidence/E9-federated-engineering-memory-2026-10-09.md)

> Sempre pronto. Sempre incompleto.

## Licença

CRIVO é distribuído sob a GNU General Public License versão 3 somente
(`GPL-3.0-only`). Consulte [`LICENSE`](LICENSE).
