---
document_id: CRIVO-DEV-001
version: 0.2.0
status: implementation-directive-candidate
date: 2026-10-09
title: "CRIVO — Especificação e missão da equipe de desenvolvimento"
language: pt-BR
related:
  - CRIVO-PROJ-001@0.2.0
  - CRIVO-001@0.1.0
execution_mode: local-offline-first
source_of_truth: git-versioned-declarations
---

# CRIVO — Documento para a equipe de desenvolvimento

**Versão 0.2.0 — missão de evolução controlada**  
**Implementação:** C++26 · SQLite3 WAL · CMake/CTest · API local · SisTer Web.  
**Lema:** **Sempre pronto. Sempre incompleto.**

## 0. Leia antes de modificar o código

Este documento especifica a **evolução** do protótipo `crivo-v0.1.0`, e não descreve funcionalidades prontas. Leia também `CRIVO-001_v0.1.0.md` e o documento `CRIVO-PROJ-001_v0.2.0.md`.

### Baseline factual fornecida

- Comandos existentes: `crivo init`, `catalog`, `run --profile`, `runs`, `serve`, `selftest`.
- Catálogo simples em `catalog/tests.json`, com **9 testes declarados**; `catalog.schema`, `sqlite.integrity` e `sqlite.wal` são **3 testes builtin implementados**. Os seis restantes estão planejados.
- SQLite WAL, histórico local, quatro endpoints HTTP GET, interface local somente leitura vinculada a `127.0.0.1:8765`.
- CMake + CTest, núcleo em modo C++26 e HTML/CSS/JS locais; dependências Boost e SQLite.
- **Não existem ainda** adaptadores externos, sandbox rigoroso, agentes distribuídos, certificação, login de produção ou implementação de padrões internacionais em larga escala.

**Não reescrever a baseline** como se o projeto já estivesse na versão 0.2.0. Criar alterações e migrações progressivas; documentar que interfaces `v0.2.0` ainda são propostas até validação.

## 1. Missão explícita — fronteiras REARIT-P005

| Campo | Definição |
|---|---|
| Objective | Introduzir catálogo internacional versionado, contratos de especificação/implementação e plano de integração CTest, preservando a execução local atual |
| Baseline | `CRIVO-001 v0.1.0` + estado real do repositório no início da missão; registrar commit SHA |
| Authorized scope | `contracts/`, `catalog/`, `src/`, `include/`, `tests/`, `db/`, `docs/`, `web/`, `CMakeLists.txt` e configuração local |
| Invariants | Offline-first; C++26 sem downgrade silencioso; IDs/versões imutáveis; fonte ≠ teste ≠ implementação ≠ resultado; ausência ≠ PASS; UI não é autoridade |
| Forbidden | Execução arbitrária de shell originada da web; alterar repositórios externos sem missão; atribuir conformidade/certificação; executar testes destrutivos no host real; sobrescrever evidências históricas; importar textos ISO protegidos |
| Authority mode | `shadow` / não bloqueante para integrações novas, mantendo gates já existentes; promover apenas por decisão externa autorizada |
| Required gates | Build, CTest, validação de schemas e migração, testes negativos, API somente leitura, replay do catálogo e compatibilidade da CLI |
| Definition of Done | Entrega local reproduzível, documentação versionada e matriz de evidências por gate, com casos de PASS/FAIL/BLOCKED demonstrados |
| Escalate only on | `AUTHORITY_BOUNDARY`, `GATE_FAILURE` irrecuperável ou `MATERIAL_AMBIGUITY` incontornável |

**REARIT-P005 é princípio proposto na fonte do SisTer, não certificação automática do CRIVO.**

## 2. Mudança de modelo — de 1 catálogo para 5 registros distintos

A estrutura `catalog/tests.json` inicial reúne várias dimensões em cada linha. No novo desenho, separar:

```text
reference_registry         (norma/guia/especificação/ferramenta, versão, direitos)
technique_registry         (técnica de projeto de teste)
test_specification_registry (critério, precondições, oráculo e riscos)
test_implementation_registry (adaptador/harness, código e dependências)
profiles                   (seleção contextual, versão, política)
    |
    v
plan -> preflight -> policy -> run -> evidence -> metric
                                      |
                                      v
                               lifecycle_events
```

A grafia e o armazenamento concretos podem ser refinados em ADR, mantendo **entidades semanticamente distintas**.

### 2.1 Schema candidato `crivo.reference/1.0.0`

```json
{
  "schema_version": "crivo.reference/1.0.0",
  "id": "iso-iec-25010",
  "edition": "2023",
  "issuer": "ISO/IEC",
  "kind": "quality_model_standard",
  "title": "Product quality model",
  "status_at_registration": "published",
  "url": "https://www.iso.org/standard/78176.html",
  "rights": {"distribution": "metadata_only", "license_review": "required"},
  "catalog_level": "inventoried"
}
```

- `kind` aceita vocabulário controlado (`standard`, `guide`, `framework`, `taxonomy`, `specification`, `tool`, `report_format`, com subtipos quando apropriado).
- `edition` e `source_url` são obrigatórios quando a fonte oferecer edições; não consultar `latest` automaticamente para alterar fonte histórica.
- `rights` controla cópia, redistribuição, atribuição e restrições.
- Registros externos são **referências documentais**, nunca cópias normativas de textos completos.

### 2.2 Schema candidato `crivo.test-spec/1.0.0`

```json
{
  "schema_version": "crivo.test-spec/1.0.0",
  "id": "crivo.sqlite.transaction.rollback",
  "version": "0.1.0",
  "source": "crivo-adaptation",
  "references": [{"id": "iso-iec-25010", "edition": "2023", "relation": "quality_attribute_mapping"}],
  "category": "persistence",
  "subcategory": "transactions",
  "techniques": ["state_transition", "negative_testing"],
  "level": "component",
  "purpose": "regression",
  "risk": ["data_integrity"],
  "preconditions": ["ephemeral_database"],
  "oracle": {"type": "predicate", "id": "rollback_preserves_preimage"},
  "implementation_refs": ["crivo.sqlite.rollback-check@0.1.0"],
  "applicability": ["capability:sqlite"],
  "qualification": "candidate"
}
```

**Este exemplo é uma adaptação autoral. Não afirmar que a ISO 25010 prescreve exatamente esse teste.** Toda relação externa deve declarar `relation` (`classifies_quality`, `informed_by`, `derived_from`, `checks_requirement`, `tooling`, `direct_reference`) e seu limite interpretativo. Se o referencial tiver item rastreável, armazenar `section_id`/`requirement_id` com edição da fonte.

### 2.3 Schema candidato `crivo.implementation/1.0.0`

```json
{
  "schema_version": "crivo.implementation/1.0.0",
  "id": "crivo.sqlite.rollback-check",
  "version": "0.1.0",
  "implements": "crivo.sqlite.transaction.rollback@0.1.0",
  "adapter": "builtin",
  "requires": ["sqlite3"],
  "execution_policy": {"destructive": false, "isolation": "ephemeral", "network": "none"},
  "qualification": "candidate"
}
```

**É obrigatório validar schema, versão, tipos, unicidade, enumerações e referências cruzadas antes de registrar.** Não implementar JSON apenas como pares de strings concatenados sem validação estrita; escolher parser e validador compatíveis com o empacotamento offline.

### 2.4 Registro de técnica

Campos: `id`, `version`, `family`, `name`, `procedure_summary`, `input_partitioning`, `oracle_class`, `applicability`, `reference_links`, `rights`. Exemplos de técnicas a mapear: particionamento em classes, análise de valores-limite, tabela de decisão, transição de estados, combinação/pairwise, propriedade/metamórfico, fuzzing, mutação, análise estática, instrumentação dinâmica e teste baseado em risco. Não inventar identificação de item normativo sem acesso autorizado ao texto completo.

## 3. Taxonomia e catálogo extensivos, sem ficção de cobertura

Criar `catalog/references/`, `catalog/techniques/`, `catalog/specifications/`, `catalog/implementations/` e `catalog/profiles/`.

Implementar **inventário inicial de todos os grupos pertinentes conhecidos e identificáveis**, com status explícito por item:

- `INVENTORIED`: fonte indexada, sem técnica mapeada;
- `MAPPED`: relação com atributos/requisitos de qualidade analisada;
- `OPERATIONALIZABLE`: critério e oráculo descritos;
- `IMPLEMENTED`: implementação disponível, com testes de contrato;
- `EVIDENCED`: execução válida no ambiente declarado.

Esses estados não formam um único status substitutivo: o histórico registra transições e evidencia requisitos de cada estágio. Ausência de mapeamento não é falha do software-alvo; é lacuna de cobertura no CRIVO.

**Lista de prioridades de registro (não declarar implementadas):** ISO/IEC 25010:2023, ISO/IEC 25023:2016, ISO/IEC/IEEE 29119-1:2022/-2:2021/-4:2021, OWASP ASVS 5.0.0 e WSTG 4.2, NIST SSDF 1.1, WCAG 2.2, CWE, OpenSSF Scorecard, SLSA 1.2, CTest/GoogleTest, LLVM sanitizers e libFuzzer. Separar origem, edição, tipo e direitos. Atualizar versões apenas por revisão registrada.

Os testes originais serão incluídos **depois**, num namespace distinto (`crivo.original.*` ou `project.<id>.*`) e com autoria, oráculo e validação específica; não rotular como "padrão internacional".

## 4. Nascimento/Morte: contrato de ciclo de vida proposto

Estados ortogonais: `definition_state`, `qualification_state`, `execution_state`, `governance_mode`, `evidence_state`. Não colapsar tudo em `status`.

### 4.1 Transições de definição (normativas para CRIVO, propostas)

```text
PROPOSED --admit--> BORN
BORN --qualify--> ACTIVE
ACTIVE --suspend--> SUSPENDED --resume--> ACTIVE
ACTIVE --supersede--> SUPERSEDED
ACTIVE --retire--> RETIRED
SUSPENDED --retire--> RETIRED
```

`SUPERSEDED` e `RETIRED` são **formas de encerramento** da versão/implementação, nunca exclusão do registro. Não permitir `RETIRED -> ACTIVE` sem nova revisão formal/versionada. Não confundir `DEPRECATED` (ainda existente, desencorajado) com `RETIRED` (não selecionável em novos planos).

### 4.2 Transições de execução

```text
CREATED -> QUEUED -> RUNNING -> PASS | FAIL | ERROR | TIMEOUT | CANCELLED
                  \-> BLOCKED | SKIPPED | NOT_APPLICABLE
```

`RUNNING` sem confirmação de encerramento após falha do processo/host deve produzir `INTERRUPTED` ou `UNKNOWN` por política de reconciliação; não atribuir resultado positivo. A morte de uma *execução* é o evento de fechamento terminal, que conserva o resultado.

### 4.3 Evento obrigatório (`crivo.lifecycle-event/1.0.0` candidato)

```json
{
  "schema_version": "crivo.lifecycle-event/1.0.0",
  "event_id": "uuid-or-ulid",
  "entity_type": "test_specification",
  "entity_ref": "crivo.sqlite.transaction.rollback@0.1.0",
  "event_type": "BIRTH",
  "occurred_at": "2026-10-09T10:00:00Z",
  "recorded_at": "2026-10-09T10:00:01Z",
  "cause": "catalog_admission",
  "authority_ref": "local-maintainer-record",
  "baseline_ref": "git:commit-sha",
  "evidence_refs": [],
  "predecessor_event_id": null,
  "payload_digest_sha256": "64-lowercase-hex-digits"
}
```

É um **contrato ilustrativo**: hashes reais devem ser calculados sobre uma codificação canônica definida, não preenchidos por placeholders em registros válidos. Idempotência de importação não significa registrar um segundo nascimento. Implementar chave de idempotência, relógios explicitados e hash da origem; não afirmar assinatura criptográfica quando houver somente hash.

**RIT mínimo verificável:** histórico append-only; transições autorizadas; referência à baseline/versionamento; consulta de genealogia; morte como encerramento preservado; não duplicar fonte de verdade em tabelas mutáveis e geradas à mão.

## 5. Persistência SQLite3 WAL — migração incremental

Sugeridas tabelas conceituais:

```text
schema_migrations
reference_versions       (PK: reference_id, edition)
technique_versions       (PK: technique_id, version)
test_spec_versions       (PK: test_id, version)
test_implementation_versions (PK: impl_id, version)
profiles                 (PK: profile_id, version)
projects                 (PK: project_id)
project_environments     (PK: environment_id, revision)
plan_snapshots           (PK: plan_id)
runs                     (PK: run_id)
run_test_results         (PK: run_id, test_spec_id, spec_version, implementation_id, implementation_version)
evidence_index           (PK: evidence_id)
metric_definitions       (PK: metric_id, version)
metric_observations      (PK: observation_id)
lifecycle_events         (PK: event_id, append-only)
authority_decisions      (PK: decision_id)
```

Requisitos: **migrations** transacionais, integridade referencial (`PRAGMA foreign_keys=ON` por conexão), modo WAL confirmado, `busy_timeout` explícito, estratégias de checkpoint, múltiplos leitores e escritor controlado, detecção de incompatibilidade de versão, backup consistente, índices para histórico e política de retenção de logs/artefatos.

A v0.1.0 possui tabelas legadas; a migração deverá manter relatórios anteriores interpretáveis e exportáveis. Executar `PRAGMA integrity_check` em cópia isolada quando apropriado; não tratar qualquer comando de diagnóstico como prova geral de consistência sem escopo declarado.

## 6. Registro de métricas reprodutíveis

Separar **definição** de **observação**:

```json
{
  "schema_version": "crivo.metric-definition/1.0.0",
  "id": "crivo.test.execution.duration",
  "version": "1.0.0",
  "kind": "duration",
  "unit": "ms",
  "formula": "finished_monotonic - started_monotonic",
  "scope": "individual_test_execution",
  "aggregation": ["median", "p95"],
  "missing_policy": "not_measured",
  "comparability_requirements": ["same_spec_version", "compatible_environment"]
}
```

Coletar `toolchain_id`, `OS/kernel`, `CPU/arquitetura`, `memória`, `flags`, `artefato`, `git_commit`, `dep_versions`, `run_id`, `test_version`, timestamps UTC + monotônicos locais quando aplicável. Não comparar wall-time entre máquinas sem normalização. Medir **aprovação**, **cobertura**, **maturidade do repertório**, **reprodutibilidade** e **efetividade de detecção** em eixos separados.

## 7. Adaptador CTest/GoogleTest — primeiro adaptador externo

### 7.1 Descoberta

- Executar CTest somente em **workspace explicitamente permitido**, em modo de descoberta `ctest --show-only=json-v1`, com limite de tempo e recursos.
- Parsear `tests[].name`, propriedades aplicáveis e identificar o contexto do `build`.
- Não assumir que o nome do teste é um identificador global estável: usar chave composta `project + build_snapshot + test_name + tool_version` e identidade lógica vinculada por contrato.
- O catálogo global não precisa copiar o código do GoogleTest. Manter referência ao executável/artefato testado, à ferramenta e à revisão.

### 7.2 Execução

- Execução somente via CLI/harness ou worker autorizado; sem POST web na primeira fase.
- Comando como **argv tipado de ferramenta allowlisted**, nunca concatenado a `/bin/sh -c`.
- Exportação CTest com `--output-junit <arquivo>` em diretório de evidência controlado; logs truncados/sanitizados.
- Parsear nomes, desfechos, duração e mensagens dentro dos limites de tamanho; preservar artefatos brutos em área com acesso restrito, quando autorizado.
- Distinguir erro do adaptador, falha de teste, ausência de teste e requisito de dependência não atendido.

### 7.3 Segurança

- Preflight: workspace canônico, ausência de `..` escape/symlink indevido, propriedade do workspace, allowlist de binários, caps de tempo, memória/CPU/processos e rede.
- Primeiro uso: importação e análise em modo `shadow`. Testes destrutivos somente em ambiente descartável e admitido.
- **Nunca** executar código arbitrário de repositório recém-clonado em nome do CRIVO sem classificação de confiança e autorização contextual.

## 8. Perfis e política

Exemplo candidato (não é formato aceito pela v0.1.0):

```json
{
  "schema_version": "crivo.profile/1.0.0",
  "id": "crivo.profile.sqlite-core",
  "version": "0.1.0",
  "includes": ["crivo.profile.base@0.1.0"],
  "match": {"categories": ["persistence", "structural"], "capabilities": ["sqlite"]},
  "exclude": [],
  "selection_policy": "fail_closed_on_unknown_requirement",
  "governance_mode": "shadow"
}
```

Resolver inclusão por DAG, detectar ciclos, múltiplas versões conflitantes e duplicação; produzir snapshot determinístico de seleção por execução. Os modos `shadow` e `governed` não devem ser autoconcedidos pelo manifesto de um componente: a política externa decide autoridade e escopo de bloqueio.

## 9. Interface SisTer Web — desenho e contratos

Manter tokens existentes do protótipo e as referências do SisTer (`web/styles.css`, `docs/architecture/INTERFACE.md`, `SISTER-WEB-RELATIONAL-SURFACE-001`, `ADR-0010`). Topo azul escuro, teal, sidebar, cards brancos, métricas compactas, responsividade e acessibilidade.

### Navegação prevista

| Seção | Conteúdo | Fonte |
|---|---|---|
| Visão geral | Quantidade por estágio, testes aplicáveis e execuções | Projeção materializada do registry e runs |
| Referenciais | Fonte, edição, tipo, licença, nível de catalogação | `reference_versions` |
| Técnicas | Técnicas e relações com referenciais | `technique_versions` |
| Catálogo | Especificações e implementações separadas | registros versionados |
| Perfis | Critérios de seleção e conflitos de versão | `profiles` |
| Execuções | histórico, filtros, PASS/FAIL/BLOCKED/SKIPPED | `runs` e resultados |
| Evidências | fontes, digests e estado de conservação | `evidence_index` |
| Ciclo de vida | linha do tempo Nascimento/Morte, sucessões | `lifecycle_events` |
| Métricas | séries temporalmente contextualizadas | `metric_observations` |
| Engenharia | preflight, saúde e limitações do ambiente | projeções autorizadas |

**Sem scores inventados, sem polling intensivo, sem endpoints mutantes por padrão.** Distinguir `NOT_IMPLEMENTED`, `NOT_APPLICABLE`, `NO_EVIDENCE`, `BLOCKED`, `STALE` e `UNKNOWN`. A web não calcula gates de promoção; mostra apenas decisões do verificador/autoridade.

### GET endpoints candidatos para a v0.2.0

```text
GET /api/v1/references?kind=&status=&limit=&cursor=
GET /api/v1/techniques?reference_id=&limit=&cursor=
GET /api/v1/test-specifications?category=&purpose=&limit=&cursor=
GET /api/v1/implementations?spec_id=&limit=&cursor=
GET /api/v1/profiles?project_id=&limit=&cursor=
GET /api/v1/runs?project_id=&from=&to=&limit=&cursor=
GET /api/v1/lifecycle?entity_ref=&limit=&cursor=
GET /api/v1/metrics?metric_id=&project_id=&limit=&cursor=
GET /api/v1/health
```

**Compatibilidade:** manter os quatro GET existentes até documentar migração; não criar endpoints cuja fonte de verdade não exista, nem retornar dados fictícios como reais. Sanitizar paths e payloads, limitar tamanho e paginar. O modo local somente leitura não dispensa validação das respostas.

## 10. Sequência prática de implementação

### Entrega E0 — preflight e congelamento da baseline

1. Inspecionar a árvore atual e `git status` antes de editar.
2. Rodar a baseline com CMake, build e CTest no toolchain disponível; anotar versões.
3. Fazer inventário de comandos, rotas, esquemas, testes atuais e seus limites.
4. Criar ADR para a migração `catalog/tests.json` -> múltiplos registros, sem apagar a origem.

**Gate E0:** baseline reproduzida ou impedimento documentado com erro verificável.

### Entrega E1 — registry internacional confiável

1. Implementar schemas JSON versionados, parser estrito e referências cruzadas.
2. Criar bibliografia de fontes com URLs, edições, licenças e tipos.
3. Popular *metadados*, não conteúdo integral de normas restritas.
4. Importação `--dry-run`, idempotência e relatórios de inconsistência.
5. CLI: `crivo references list`, `crivo references show <id>`, `crivo catalog validate` — novas opções, preservando CLI antiga.

**Gate E1:** erros de schema, IDs duplicados, edição inexistente, relações órfãs e fonte sem licença declarada são recusados; importação repetida não duplica entradas.

### Entrega E2 — separação técnica e perfis

1. Diferenciar `TestSpec` de `TestImplementation` e `Technique`.
2. Implementar perfis versionados com seleção previsível.
3. Semântica de aplicabilidade em três valores: `APPLICABLE`, `NOT_APPLICABLE`, `UNKNOWN` (UNKNOWN falha fechado para execução que o exija).
4. Projetar migrador idempotente de v0.1.0 sem perder a visão antiga.

**Gate E2:** seleção determinística, resolução de ciclos, caminhos negativos, persistência e replay.

### Entrega E3 — nascimento/morte com RIT

1. Registrar `BIRTH`, `QUALIFY`, `SUSPEND`, `RESUME`, `SUPERSEDE`, `RETIRE`, `RUN_STARTED`, `RUN_FINISHED` conforme tipo de entidade.
2. Impedir regressão ilegal de estado e duplicação por retry.
3. Referenciar versão anterior, evento predecessor, decisão de autoridade e evidência.
4. Consultar trajetória histórica em CLI e web (sem inventar relações).

**Gate E3:** casos válidos e inválidos, gravação append-only, reexecução idempotente, reinício durante RUNNING, morte sem exclusão, reconstrução do estado.

### Entrega E4 — adaptador CTest/GoogleTest

1. Criar `adapter.ctest` com descoberta JSON e execução JUnit sem shell.
2. Integrar resultados ao modelo interno sem mudar semântica de testes.
3. Capturar ambiente, toolchain, hashes e limites de execução.
4. Usar fixture local sintética antes de qualquer projeto externo.

**Gate E4:** teste discovery, PASS, FAIL, SKIP, timeout, falha de parser, workspace não autorizado e ausência de CTest; não permitir interferir no host.

### Entrega E5 — métricas e interface

1. Implementar definições de métrica versionadas e observações contextualizadas.
2. Expor no SisTer Web catálogo por referencial/categoria, histórico e Nascimento/Morte.
3. Revisar teclado, foco, contraste, informações por texto e estados vazios.

**Gate E5:** dados da UI rastreáveis ao banco, resultados de teste distintos dos níveis de cobertura, nenhuma ação remota via navegador.

### Entrega E6 — pilotos em *shadow*

TRAMA primeiro (contratos, JSON, APIs, SQLite), ELO depois (resiliência offline, RPi e recursos de hardware), **somente** após preflight e autorização própria. Projetos mantêm seus testes e decisão de gate locais; CRIVO coleta e classifica evidências, sem modificar seus repositórios. ENTE e SINAL não entram no primeiro gate de integração sem análise específica.

## 11. Organização sugerida do repositório

```text
crivo/
├── CMakeLists.txt
├── VERSION
├── README.md
├── docs/
│   ├── CRIVO-001_v0.1.0.md
│   ├── CRIVO-PROJ-001_v0.2.0.md
│   ├── CRIVO-DEV-001_v0.2.0.md
│   └── adr/
├── contracts/
│   ├── reference/1.0.0.schema.json
│   ├── technique/1.0.0.schema.json
│   ├── test-spec/1.0.0.schema.json
│   ├── implementation/1.0.0.schema.json
│   ├── profile/1.0.0.schema.json
│   ├── lifecycle-event/1.0.0.schema.json
│   └── metric-definition/1.0.0.schema.json
├── catalog/
│   ├── tests.json                 # legado v0.1.0 preservado
│   ├── references/
│   ├── techniques/
│   ├── specifications/
│   ├── implementations/
│   └── profiles/
├── include/crivo/
│   ├── registry/
│   ├── lifecycle/
│   ├── execution/
│   ├── metrics/
│   └── adapters/
├── src/
├── db/migrations/
├── adapters/ctest/
├── web/
├── tests/
│   ├── contracts/
│   ├── registry/
│   ├── lifecycle/
│   ├── adapters/
│   └── web/
└── .run/                       # não versionado
```

**Não recriar diretórios se o projeto já os possui com propósito equivalente; refatorar preservando compatibilidade.**

## 12. Testes obrigatórios do próprio CRIVO

- **Compilação C++26:** falha clara quando toolchain incompatível; sem downgrade.
- **Schemas:** válida/inválida, enum desconhecido, versão errada, IDs duplicados, campo faltante, referência órfã.
- **Registros:** reimportação idempotente, revisões imutáveis, licença ausente, origem externa não confundida com implementação.
- **Perfis:** includes, ciclo, conflito, NOT_APPLICABLE, UNKNOWN, política fail-closed.
- **Temporalidade:** nascimento único, transição ilícita, supersessão, morte terminal, replay determinístico.
- **SQLite:** migrations, WAL, concorrência de leitura, integridade, rollback, reinício durante transação, cópia/restore.
- **CTest:** discover JSON-v1, JUnit XML, códigos de saída, timeout, log limitado, nomes repetidos, projeto sem testes.
- **Web:** GET factual, paginação, 405 em mutações, payload sanitarizado, acessibilidade mínima e ausência de score sem prova.
- **Segurança:** process execution allowlisted, isolamento de diretórios, negação de symlink escape, segredos mascarados, sem invocação de shell arbitrário.
- **Métricas:** contextos compatíveis/incompatíveis, mediana/p95, ausência de amostra, denominador versionado.

Adotar testes negativos antes de declarar módulo pronto. Registrar quais testes requerem ambiente isolado e quais são apenas propostas.

## 13. Critérios de não conformidade e antiobjetivos

Não aceitar como entrega:

- página bonita com números sintéticos apresentados como reais;
- botão de executar sem executor, autorização e contenção;
- afirmar "ISO certified" ou "conforme ASVS" porque uma etiqueta referencia esses documentos;
- converter um catálogo amplo em alegação de execução ampla;
- copiar conteúdos restritos de padrões como se fossem dados livres;
- deletar a versão ou o teste após sua aposentadoria sem trilha de retenção;
- usar hashes como se fossem assinaturas autenticadas;
- importar resultados de máquinas distintas como série comparável sem metadados;
- promover o CRIVO a autoridade de gates do SisTer por configuração local;
- misturar referência internacional, método autoral e artefato executável na mesma entidade.

## 14. Entrega final da missão ao mantenedor

Fornecer ao concluir cada etapa:

1. Sumário do que foi implementado, do que continua planejado e dos limites do ambiente.
2. Mudanças em contratos e ADRs, com justificativa de compatibilidade.
3. Comandos exatos de compilação, testes, migrações e execução local.
4. Tabela dos gates executados e seus resultados, com evidências e ambiente.
5. Dados de exemplo claramente marcados como sintéticos.
6. Comparação objetiva com a baseline anterior e lista de pendências não bloqueantes.
7. Critérios de nascimento/morte e REA/RIT atendidos **por evidência**, não por autodeclaração.

## 15. Comandos de baseline (existentes no v0.1.0)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/crivo init
./build/crivo catalog
./build/crivo run --profile core
./build/crivo runs
./build/crivo serve
# Web: http://127.0.0.1:8765
```

Os comandos `crivo references ...`, `crivo catalog validate` e APIs `/api/v1/` deste documento são **alvos**, não interface já disponível.

## 16. Fontes técnicas e institucionais

- [SisTer REA/RIT](https://github.com/jpereiratrindade/SisTer/blob/main/docs/rea-rit/README.md) e [REARIT-P001](https://github.com/jpereiratrindade/SisTer/blob/main/docs/rea-rit/principios/principio_manutencao_reflexiva_automatizavel_v0.1.0.tex), [REARIT-P005](https://github.com/jpereiratrindade/SisTer/blob/main/docs/rea-rit/principios/principio_autonomia_delegada_por_fronteiras_v0.1.0.tex).
- [SisTer SGR/engines/modes](https://github.com/jpereiratrindade/SisTer/blob/main/docs/architecture/sgr/verification-engines-and-governance-modes.md).
- [CMake CTest 3.30](https://cmake.org/cmake/help/v3.30/manual/ctest.1.html).
- [GoogleTest/CMake](https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md).
- Referenciais bibliográficos externos: ver `CRIVO-PROJ-001_v0.2.0.md`, seção 6.

**Condição de encerramento:** gates cumpridos; baseline anterior preservada; novas capacidades evidenciadas; limites explícitos; histórico de decisões e testes reconstruível. Encerrar esta missão não significa encerrar a evolução do CRIVO.
