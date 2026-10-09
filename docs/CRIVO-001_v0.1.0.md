---
document_id: CRIVO-001
title: "CRIVO — Constituição inicial da plataforma de verificação e validação"
version: "0.1.0"
status: "prototype-implemented / architecture-candidate"
date: "2026-10-09"
language: pt-BR
kind: "technical-constitution"
implementation_language: "C++26"
interface_reference: "SisTer Web"
principle: "Sempre pronto. Sempre incompleto."
---

# CRIVO — Constituição inicial v0.1.0

> **Propósito:** organizar, selecionar, executar, correlacionar e preservar evidências verificáveis de qualidade de múltiplos sistemas, sem assumir autoridade sobre o código, os dados de domínio, a implantação ou os julgamentos científicos dos participantes.

## 1. Decisão constitutiva

O CRIVO é um **componente autônomo** de engenharia que expõe um catálogo versionado de testes reutilizáveis, executa verificações admitidas e registra resultados observáveis. Não substitui CTest, GoogleTest, ferramentas especializadas ou as equipes responsáveis por avaliação humana. Sua promessa principal é **reutilizar procedimentos sem reutilizar cegamente conclusões**.

```text
Expectativa declarada
    -> critério verificável
       -> teste (implementação + versão + proveniência)
          -> execução contextualizada
             -> evidência observada
                -> interpretação sob autoridade adequada
                   -> histórico e eventual decisão de mudança
```

Três princípios imutáveis:

1. **Catálogo não equivale a cobertura:** presença de um teste não significa que ele esteja implementado, seja aplicável ou tenha sido executado.
2. **Execução não equivale a certificação:** PASS indica apenas que um critério foi satisfeito naquela execução, no contexto declarado. Não autoriza produção, publicação nem certificação regulatória.
3. **Interface não é autoridade:** a UI apresenta resultados emitidos pelo executor e derivados da persistência; não recalcula gates nem inventa evidências.

## 2. Padrão SisTer como referência

Referências inspecionadas no repositório público `jpereiratrindade/SisTer`:

- `docs/architecture/INTERFACE.md`: estrutura visual (topo institucional, azul escuro, teal, cards compactos, métricas, dashboard e rodapé de governança);
- `web/styles.css`, `web/index.html`: tokens, layout, responsividade e organização da navegação;
- `docs/architecture/SISTER-WEB-RELATIONAL-SURFACE-001_v0.1.0.md`: projeções separadas, evidências, autoridade e estados de indisponibilidade;
- `docs/adr/ADR-0028-composable-component-runtime-deployment-boundary.md`: fronteiras entre participante, componente, runtime e implantação;
- `docs/adr/ADR-0010-maturity-dashboard.md`: dashboard de maturidade somente leitura, sem execução de scripts pelo navegador.

**Decisão:** o CRIVO adota linguagem visual compatível e as fronteiras de governança, mas mantém identidade, persistência, execução e API próprias. Não é uma extensão invasiva do `sisterd`. A integração futura com SisTer ocorre via contratos reconhecidos e admissão governada; não concede automaticamente vínculo federado, confiança, publicação nem maturidade.

## 3. Arquitetura-alvo

```text
                             CAMADA DE APRESENTAÇÃO
                      +--------------------------------+
                      | CRIVO WEB / Padrão SisTer       |
                      | dashboard, catálogo, histórico  |
                      | READ ONLY na v0.1.0            |
                      +-----------------+--------------+
                                        |
                                 API HTTP / JSON
                                 loopback 127.0.0.1
                                        |
 +------------------------- CRIVO C++26 -------------------------+
 | Contratos: catálogo · taxonomia · perfis · execução · evidência|
 |                                                                |
 |  Registro -> Seleção -> Política -> Executor -> Evidências      |
 |      |          |                        |           |           |
 |  JSON Schema  Perfis                Adapters      SQLite WAL     |
 |                                    / harness                     |
 |  Adaptadores futuros: CTest / GoogleTest / JSON / HTTP / HW     |
 +----------------------+------------------------+---------------+
                        |                        |
                Sistemas autônomos      SisTer por contrato
                ELO / ENTE / TRAMA / SINAL (integração futura)
```

**Estado realizado na v0.1.0:** CLI e núcleo C++26, SQLite WAL, catálogo JSON, perfis, 3 verificações `builtin`, quatro endpoints GET, página web local de consulta. **Estado ainda não realizado:** adaptadores externos, projetos reais integrados, execução distribuída, sandbox rigoroso, controle de acesso autenticado, cadeia de custódia assinada, API federada e certificação.

## 4. Taxonomia multidimensional de testes

A categoria é **o domínio do risco ou atributo avaliado**. A finalidade informa **por que** o teste é feito. O perfil informa **quando e para quem** selecionar. Etiquetas constituem a dimensão transversal. Portanto, **não tratar categoria, finalidade, perfil e tecnologia como sinônimos**.

### 4.1 Categorias primárias propostas

| ID | Categoria | Subcategorias exemplares |
|---|---|---|
| `structural` | Estrutura e integridade | build, dependências, configuração, catálogo |
| `functional` | Funcionalidade | unidade, integração, regressão, propriedade |
| `interoperability` | Contratos e interoperabilidade | JSON Schema, HTTP, versão, serialização |
| `persistence` | Persistência | migração, WAL, transação, backup, recuperação |
| `concurrency` | Concorrência | thread safety, races, lock, deadlock |
| `resilience` | Resiliência | offline, reinício, falha parcial, retomada |
| `performance` | Desempenho | latência, CPU, RSS, throughput, energia |
| `security` | Segurança | autenticação, autorização, isolamento, dados |
| `reproducibility` | Reprodutibilidade | seeds, reconstrução, proveniência, precisão |
| `accessibility` | Experiência e acessibilidade | teclado, foco, leitores de tela, contraste |
| `hardware` | Hardware e dispositivos | sensores, câmera, GPIO, RPi, interface |
| `scientific` | Validade científica | referência, parâmetros, incerteza, sensibilidade |

O catálogo inicial contém exemplos de algumas categorias, não as implementações de todas elas. O sistema deve permitir inclusão de novas classes via catálogo versionado, com revisão de governança.

### 4.2 Dimensões ortogonais

Um teste declara:

- `id` estável, único e versionável (o protótipo ainda não mantém revisão individual por teste);
- `category` primária e `subcategory` específica;
- `purpose`: unit, integration, diagnostic, conformance, contract, benchmark, resilience, security, validation ou outra finalidade governada;
- `profiles`: listas reutilizáveis como `core`, `sqlite`, `api`, `offline`, `hardware`, `scientific`;
- `tags`: tecnologias, riscos, ambientes e características;
- `engine`: `builtin` ou `adapter`, com outras engines somente mediante contrato de executor;
- `status`: no protótipo `implemented` ou `planned`; posteriormente `deprecated`, `quarantined` e outros estados definidos por schema;
- requisitos, políticas e evidências esperadas (a evoluir no contrato v0.2.0).

**Regra:** um teste tem uma categoria primária, mas pode participar de diversos perfis; futuras associações secundárias devem usar etiquetas ou relações explícitas, sem clonar o teste.

### 4.3 Exemplo executável no protótipo

```json
{
  "id": "sqlite.integrity",
  "name": "Integridade SQLite",
  "category": "persistence",
  "subcategory": "integrity",
  "purpose": "diagnostic",
  "profiles": ["core", "sqlite"],
  "tags": ["sqlite", "wal"],
  "status": "implemented",
  "engine": "builtin"
}
```

## 5. Agrupamento e resolução de perfis

Seleção declarativa futura:

```text
perfil core
  -> catalog.schema
  -> sqlite.integrity
  -> sqlite.wal

perfil territorial
  -> contracts.json-schema
  -> spatial.municipality.ecoregion
  -> spatial.biome.inclusion

perfil hardware
  -> device.probe
  -> camera.presence
  -> device.recovery
```

Um perfil deve poder incorporar outros perfis (`includes`), aplicar filtros por `category`, `tags`, capacidades e riscos, e declarar exclusões justificadas. Detecção de ciclos e resolução determinística são critérios de aceitação para a v0.2.0. **Não prometer resolução por inclusão no protótipo**, que usa apenas a lista plana `profiles`.

## 6. Estados: jamais reduzir tudo a PASS/FAIL

Estados de **definição**: `PLANNED`, `IMPLEMENTED`, `DEPRECATED`, `QUARANTINED`.

Estados de **seleção/execução** previstos: `PASS`, `FAIL`, `BLOCKED`, `SKIPPED`, `ERROR`, `TIMEOUT`, `CANCELLED`, `NOT_APPLICABLE`, `NOT_EXECUTED`.

Estados de **evidência** previstos: `PRESENT`, `MISSING`, `STALE`, `INVALID`, `REDACTED`.

Estados de **admissão** previstos: `CANDIDATE`, `QUALIFIED`, `REJECTED`, `SUSPENDED`.

Esses eixos são distintos. Na v0.1.0 o executor registra somente `PASS`, `FAIL`, `BLOCKED` e o catálogo somente `implemented`/`planned`. Não exibir categorias sem resultados como se tivessem sido aprovadas.

## 7. Plataforma e implantação

- Linguagem: **C++26**, exigida no CMake, sem downgrade silencioso;
- Build: CMake >= 3.30, CTest e compiladores C++26;
- Persistência: **SQLite3 WAL**, schema evolutivo com migrações versionadas no próximo incremento;
- Web: HTML5 + CSS + JavaScript sem CDN, servidos por **servidor C++** local;
- HTTP local: Boost.Asio / Boost.Beast, **somente GET** no primeiro protótipo;
- JSON manifesto: Boost Property Tree para bootstrap; substituição por parser com validação estrita e JSON Schema no próximo incremento;
- Offline-first: catálogo, binário, dashboard, JS, CSS e DB totalmente locais;
- Linux/Raspberry Pi 5: alvos previstos, ainda não validados em hardware real;
- Segurança inicial: vinculação fixa a `127.0.0.1`, sem controles para publicação externa;
- Dependências: sistema de construção deve verificá-las explicitamente. Não presumir compatibilidade completa do compilador com toda a biblioteca padrão C++26.

C++26 continua evoluindo em suporte por compiladores e bibliotecas. O alvo é **compilação no modo do padrão**, com feature detection onde APIs novas forem necessárias; não declarar conformidade total do toolchain.

## 8. Persistência inicial

```text
test_catalog
    id, name, category, subcategory, purpose, status,
    engine, profiles_json, tags_json

runs
    id, test_id, category, profile, status, detail,
    duration_ms, created_at
```

Este é um esquema mínimo. A modelagem-alvo deve incorporar entidades `projects`, `catalog_versions`, `test_versions`, `test_suites`, `profiles`, `executions`, `observations`, `evidence_artifacts`, `environments` e `decisions`, com constraints, relações e migrações. A entidade `runs` atual é um **registro local de demonstração**, não uma cadeia de custódia científica.

## 9. API inicial de leitura

```text
GET /api/v1/overview    -> contagens de testes e últimos estados por teste
GET /api/v1/categories  -> categorias e contagens
GET /api/v1/tests       -> definições do catálogo registrado
GET /api/v1/runs        -> até 100 resultados mais recentes
```

Em produção, exigir esquema JSON versionado, paginação, limites de payload, autenticador e capacidades por endpoint. A execução permanecerá na CLI até existir controle administrativo com fila, autorização server-side e política de execução; **não criar endpoint genérico para executar shell**.

## 10. Execução de testes externos: próxima fase

A descoberta deve ser feita **sem reescrever os testes de cada repositório**:

```text
CRIVO adapter.ctest
  -> recebe projeto autorizado, build_dir e perfil
  -> invoca binário fixo ctest com argumentos estruturados (sem shell)
  -> valida saída JUnit ou CTest JSON
  -> preserva run_id, exit code, duração e contexto
  -> registra resultados e referencia artefatos
```

O executor valida listas de permissões de raízes/workspaces, usuário de execução, tempo máximo, memória, I/O, política de rede e ambientes segregados. Testes destrutivos somente em sandbox aprovada, NUNCA sobre bancos produtivos ou dispositivos reais sem autorização explícita.

## 11. Integração futura com SisTer

O CRIVO poderá declarar `participant`, `component`, `runtime` e receber binding concreto de implantação, mas **não copiar** contextos dos sistemas que verifica. O SisTer poderá consumir **projeções de evidências do CRIVO** mediante acordos e admissão explícitos. `SisTer-Infra` permanece responsável por gateway, TLS e exposição LAN; CRIVO não deve assumir esse papel. Contratos ainda DRAFT no SisTer continuam DRAFT, sem promoção por conveniência.

## 12. Critérios de aceitação da versão 0.1.0

1. Compilar no modo C++26 com toolchain compatível, sem fallback C++23/20.
2. Criar e abrir banco SQLite em WAL.
3. Ler catálogo JSON categorizado, rejeitando versão incompatível e IDs duplicados.
4. Selecionar por perfil, distinguindo teste implementado de teste planejado.
5. Executar 3 testes locais, preservar resultado e motivo em SQLite.
6. Expor quatro endpoints HTTP GET no loopback.
7. Apresentar catálogo, categorias e evidências em UI padrão visual SisTer.
8. Não permitir execução remota via web.
9. Dispor de CTest smoke automatizado e instruções de reprodução.

### Fora do escopo de aceitação 0.1.0

Não afirmar integração com ELO, ENTE, TRAMA ou SINAL; testes de segurança, resiliência e validade científica estão **planejados, não disponíveis**. Aferição de requisitos regulatórios ou emissão de certificados externos também não integra o MVP.

## 13. Pacotes de trabalho

| Pacote | Entrega | Prioridade |
|---|---|---|
| CRIVO-P01 | bootstrap executável e dashboard de consulta | realizado como protótipo |
| CRIVO-P02 | catálogo validado por JSON Schema; taxonomia governada; suites e perfis hierárquicos | alta |
| CRIVO-P03 | adaptador CTest/GoogleTest com importação de JUnit e teste negativo | alta |
| CRIVO-P04 | identidade de projeto, versões, histórico e evidência verificável | alta |
| CRIVO-P05 | isolamento do runner, limites de recursos e auditoria | alta |
| CRIVO-P06 | integração declarativa piloto com TRAMA e ELO | média |
| CRIVO-P07 | contrato SisTer e projeção autorizada de evidências | média |
| CRIVO-P08 | testes de stress, acessibilidade e validação científica especializados | por perfil |

## 14. Instrução ao Codex / Gemini

**Construir incrementalmente; sempre deixar um artefato executável e testes reprodutíveis.** Antes de ampliar funcionalidades, executar CMake, CTest, invocar CLI e testar HTTP local. Cada contribuição deve documentar contrato, motivação, risco, teste e evidência.

Ao evoluir para v0.2.0:

1. Proteger a separação entre `catalog`, `policy`, `runner`, `evidence`, `persistence`, `api` e `web` em bibliotecas internas bem definidas.
2. Introduzir testes negativos de IDs duplicados, JSON malformado, dependências ausentes, perfil não reconhecido, DB corrompido, e origem do workspace fora de raiz autorizada.
3. Empregar migrações transacionais, schema versionado e índices eficientes; não quebrar registros existentes.
4. Substituir JSON heurístico por validação de esquema explícita e mensagens diagnósticas determinísticas.
5. Implementar adapter CTest sem executar scripts arbitrários provenientes do catálogo.
6. Modelar `result`, `evidence`, `decision` e `authorization` em dimensões separadas.
7. Garantir que o UI não faça inferências artificiais de confiabilidade ou certificação.
8. Não introduzir nome de sistema específico no núcleo ou no contrato genérico.
9. Preservar o loopback até o conjunto de controles de autenticação, gateway e implantação passar por revisão.
10. Validar builds Linux amd64 e ARM64/Raspberry Pi 5 em ambientes apropriados; não marcar plataformas não testadas como suportadas.

> **Sempre pronto. Sempre incompleto.** Não significa permanentemente improvisado: significa operacional dentro de uma fronteira de validade explicitamente evidenciada, e evolutivo por constituição.
