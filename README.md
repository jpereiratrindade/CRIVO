# CRIVO — Plataforma de Verificação e Validação de Sistemas

**v0.1.0 — protótipo funcional de infraestrutura local (não certificado para produção)**

Primeiro esqueleto executável em C++26, SQLite3 WAL, HTTP local de leitura e interface visual alinhada ao SisTer. A primeira versão contém três testes reais (`catalog.schema`, `sqlite.integrity`, `sqlite.wal`) e seis declarações planejadas, propositalmente marcadas como não implementadas.

## Pré-requisitos

- CMake >= 3.30, compilador com modo C++26 (GCC 14+ ou Clang compatível), SQLite3 development headers, Boost headers (Beast, Asio, Property Tree), threads, Linux.
- C++26 ainda possui suporte parcial conforme compilador/biblioteca. Não ativar downgrade silencioso do padrão.

## Como executar

### Script único (recomendado)

O script detecta a ausência do Boost no host e usa automaticamente a Toolbox
`crivo-dev` (ou a indicada por `CRIVO_TOOLBOX`):

```bash
./crivo.sh setup
./crivo.sh run
./crivo.sh serve
```

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

### Comandos manuais

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/crivo init
./build/crivo catalog
./build/crivo run --profile core
./build/crivo runs
./build/crivo serve
# no navegador: http://127.0.0.1:8765
```

Para selecionar um grupo: `./build/crivo run --profile sqlite`.
Para testes apenas planejados, o resultado é BLOCKED e o processo termina com código 1.

A interface é **somente leitura**, sem endpoint para execução remota, alterações de catálogo ou comandos do sistema. Não publique diretamente o HTTP local na rede; requer autenticação, autorização e gateway próprios antes de qualquer exposição.

## Limites deliberados da v0.1.0

- Ainda não integra sistemas externos nem executa CTest/GoogleTest (previsto para etapa seguinte).
- A tabela de testes é persistida pelo `init` e sincronizada no `run`; importação de projetos é apenas exemplo declarativo.
- Os testes implementados validam as condições do CRIVO local, não a conformidade dos sistemas externos.
- Banco e evidências são locais, com histórico simples, não assinados; cadeia de custódia e retenção ainda pendentes.
- Servidor HTTP de demonstração síncrono e vinculado exclusivamente a `127.0.0.1`.
- Licenciamento institucional e autoria devem ser definidos antes da publicação.

## Referências de arquitetura

Ver [`docs/CRIVO-001_v0.1.0.md`](docs/CRIVO-001_v0.1.0.md) e [`docs/ROADMAP.md`](docs/ROADMAP.md).

Referência visual e conceitual: `jpereiratrindade/SisTer` (`docs/architecture/INTERFACE.md`, `SISTER-WEB-RELATIONAL-SURFACE-001`, ADR-0028, web/styles.css). Reuso de *padrão*, não incorporação de identidade ou autoridade do SisTer.

## Evolução controlada

A baseline importada está congelada no Git. A missão v0.2.0, seu documento de
projeto, a decisão inicial de migração e a evidência de reprodução estão em
[`docs/CRIVO-DEV-001_v0.2.0.md`](docs/CRIVO-DEV-001_v0.2.0.md),
[`docs/CRIVO-PROJ-001_v0.2.0.md`](docs/CRIVO-PROJ-001_v0.2.0.md),
[`docs/adr/ADR-0001-evolucao-do-catalogo.md`](docs/adr/ADR-0001-evolucao-do-catalogo.md)
e [`docs/evidence/E0-baseline-2026-10-09.md`](docs/evidence/E0-baseline-2026-10-09.md).

Capacidades descritas nesses documentos como candidatas ou planejadas não são
consideradas implementadas sem código, testes e evidência correspondentes.

> Sempre pronto. Sempre incompleto.

## Licença

CRIVO é distribuído sob a GNU General Public License versão 3 somente
(`GPL-3.0-only`). Consulte [`LICENSE`](LICENSE).
