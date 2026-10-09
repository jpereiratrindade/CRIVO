---
document_id: CRIVO-PROJ-001
version: 0.2.0
status: development-project-proposal
date: 2026-10-09
title: "CRIVO — Projeto de desenvolvimento tecnológico"
language: pt-BR
implementation: C++26
interface: SisTer Web
principle: "Sempre pronto. Sempre incompleto."
research_incorporation: optional-future-decision
related:
  - CRIVO-001@0.1.0
  - CRIVO-DEV-001@0.2.0
  - SisTer/REA-RIT
---

# CRIVO — Projeto de desenvolvimento tecnológico

**Versão 0.2.0 — proposta de projeto**  
**Natureza atual:** desenvolvimento de software e infraestrutura de engenharia; **não** constitui, neste estágio, projeto de pesquisa formalizado.  
**Nome:** CRIVO — Plataforma de Verificação, Validação e Confiabilidade de Sistemas.  
**Lema:** **Sempre pronto. Sempre incompleto.**

## 1. Decisão inicial

O CRIVO será um produto autônomo, modular, offline-first, desenvolvido em **C++26**, com **SQLite3 em modo WAL** e **interface web de engenharia no padrão SisTer**. Ele organizará referenciais reconhecidos internacionalmente, técnicas, implementações de testes, planos de execução, resultados e evidências, de modo que sistemas distintos consumam testes adequados sem transferir sua autoridade para o CRIVO.

A ampliação do escopo não altera retroativamente o protótipo **CRIVO-001 v0.1.0**. Sua implementação inicial permanece uma baseline técnica: catálogo simples com nove entradas, três verificações builtin executáveis, CLI, SQLite WAL, API HTTP local de leitura e dashboard. A versão presente descreve o **alvo de evolução**, não funcionalidades já entregues.

**Princípio de método:** inventário potencialmente extensivo; execução seletiva, justificada e reproduzível. "Todos os testes" é objetivo de *cobertura do repertório pertinente*, não a alegação impossível de que cada técnica, cenário, referencial e ferramenta do mundo será executado em todos os projetos.

## 2. Problema e justificativa

A repetição de verificações semelhantes em múltiplos projetos provoca duplicação de código, divergência de critérios, custo de manutenção e dificuldade de comparar evidências. Além disso, uma suíte de testes costuma preservar o resultado imediato, mas perde a genealogia de sua criação, evolução, substituição e descontinuação.

O CRIVO procura resolver cinco problemas conectados:

1. **Dispersão:** critérios e ferramentas residem em diferentes repositórios.
2. **Heterogeneidade:** resultados e severidades não são comparáveis sem contexto.
3. **Reimplementação:** repetem-se testes de persistência, APIs, segurança e recuperação.
4. **Temporalidade:** faltam registros formais da história de testes e decisões.
5. **Conhecimento técnico:** resultados empíricos são pouco aproveitados para aperfeiçoar o próprio processo de verificação.

## 3. Objetivo geral

Conceber, implementar e manter uma plataforma de engenharia que permita **descobrir, classificar, selecionar, executar quando autorizado, registrar, comparar e interpretar tecnicamente** testes e critérios de qualidade de software apoiados inicialmente em referenciais internacionais reconhecidos e, em etapa posterior, em métodos e testes desenvolvidos no ecossistema do projeto.

### Objetivos específicos

- Construir um registro versionado de **referenciais**, distinguindo normas, guias, modelos, taxonomias, especificações, formatos e ferramentas.
- Catalogar **técnicas**, **especificações de verificação**, **implementações executáveis**, **perfis** e **evidências** como objetos distintos.
- Oferecer adaptadores tipados de CTest/GoogleTest e outras ferramentas, sem substituí-las.
- Preservar resultados locais com integridade referencial, trilha histórica e provenance.
- Comparar **o mesmo teste em contextos compatíveis**, sem produzir rankings artificiais.
- Relacionar observação, interpretação, decisão e revisão conforme REA, e preservar a genealogia das transições conforme RIT.
- Permitir criação, revisão, suspensão, substituição e encerramento explícitos (Nascimento/Morte) de entidades com identidade estável.
- Proporcionar métricas úteis ao desenvolvimento e **potencialmente** a pesquisa futura, com governança de dados própria.

## 4. Delimitação: produto tecnológico agora, pesquisa talvez depois

A finalidade imediata é **engenharia**, não demonstrar uma hipótese científica. O repositório, as entregas e os critérios de aceite não dependerão de aprovação de proposta de pesquisa. Se houver interesse futuro, um protocolo separado poderá aproveitar séries temporais, métricas de falha, reprodutibilidade, confiabilidade de testes, custo de manutenção e capacidade de adaptação.

**Possíveis questões de pesquisa, ainda não adotadas:**

- A reutilização de testes reduz esforço de manutenção sem prejudicar detecção de defeitos?
- Como a confiabilidade dos resultados muda com hardware, compiladores e versões de dependências?
- O histórico de nascimento, alteração e morte de testes permite explicar regressões e obsolescência?
- Que relação existe entre diversidade de técnicas, cobertura contextual e falhas encontradas?
- Como a separação entre observação e autorização melhora a governança dos testes?

Para uma futura etapa científica seriam necessários objetivos próprios, protocolo, critérios de amostragem, baselines, definição operacional de métricas, controle de vieses e, conforme o caso, revisão ética e institucional. **Não confundir telemetria disponível com evidência científica automaticamente válida.**

## 5. Taxonomia: camadas que não podem ser confundidas

| Dimensão | Pergunta | Exemplos |
|---|---|---|
| Referencial | Em que fonte se fundamenta? | ISO/IEC 25010, OWASP ASVS, WCAG |
| Categoria de qualidade/risco | O que se avalia? | Persistência, segurança, acessibilidade |
| Técnica | Como se derivam casos? | Valor-limite, partições, propriedades, fuzzing |
| Nível | Em que escala? | Unidade, componente, integração, sistema, aceitação |
| Propósito | Por que executar? | Regressão, diagnóstico, conformidade, benchmark |
| Especificação de teste | Qual critério é verificável? | Uma regra de API ou transação |
| Implementação | O que executa o critério? | GoogleTest, CTest, checker nativo |
| Perfil | Qual subconjunto se aplica? | core, web, sqlite, RPi, security |
| Ambiente | Sob quais condições? | Host, kernel, compilador, arquitetura |
| Evidência | O que foi efetivamente observado? | Resultado, log sanitizado, artefato, hash |
| Autoridade | Que efeito o resultado pode ter? | shadow / governed por decisão externa |

Uma categoria é primária para navegação, mas tags, relações e perfis são multidimensionais. Nem todos os itens de uma norma são testes executáveis; alguns representam objetivos, requisitos ou processos que exigem avaliação documental/humana.

### Famílias de categorias a contemplar no registro

1. Compilação e integridade estrutural; 2. Unidade e lógica; 3. Integração e sistema; 4. Contratos e interoperabilidade; 5. Persistência e dados; 6. Concorrência; 7. Resiliência e recuperação; 8. Desempenho e capacidade; 9. Segurança de aplicações; 10. Segurança de cadeia de suprimentos; 11. Privacidade e proteção de dados; 12. Acessibilidade e usabilidade; 13. Compatibilidade e portabilidade; 14. Observabilidade e operação; 15. Reprodutibilidade e rastreabilidade; 16. Hardware e dispositivos; 17. Validade científica e numérica; 18. Documentação/processo/governança.

A taxonomia é versionada; categorias novas não implicam renomeação silenciosa dos registros existentes.

## 6. Repertório internacional — registro prioritário

**Separar tipo e finalidade da fonte.** Uma norma ISO, uma especificação W3C, um guia OWASP, um framework NIST e uma ferramenta de testes não possuem o mesmo estatuto normativo.

| Fonte/versionamento de referência | Natureza | Emprego no CRIVO |
|---|---|---|
| [ISO/IEC 25010:2023](https://www.iso.org/standard/78176.html) | Modelo normativo de qualidade | Classificação de características de qualidade |
| [ISO/IEC 25023:2016](https://www.iso.org/standard/35747.html) | Norma de medidas de qualidade | Referencial de mensuração, sujeito a análise de edição |
| [ISO/IEC/IEEE 29119-1:2022](https://www.iso.org/standard/81291.html) | Norma de conceitos | Vocabulário de testes |
| [ISO/IEC/IEEE 29119-2:2021](https://www.iso.org/standard/79428.html) | Norma de processos | Organização do processo de teste |
| [ISO/IEC/IEEE 29119-4:2021](https://www.iso.org/standard/79430.html) | Norma de técnicas | Técnicas e derivação de casos de teste |
| [OWASP ASVS 5.0.0](https://github.com/OWASP/ASVS) | Referencial de requisitos de segurança | Mapeamento de requisitos verificáveis |
| [OWASP WSTG v4.2](https://owasp.github.io/www-project-web-security-testing-guide/v42/) | Guia de testes de segurança | Metodologias e cenários, com IDs versionados |
| [NIST SSDF 1.1](https://csrc.nist.gov/pubs/sp/800/218/final) | Framework de práticas seguras | Gestão do desenvolvimento seguro |
| [WCAG 2.2](https://www.w3.org/TR/WCAG22/) | Recomendação W3C | Critérios de acessibilidade web |
| [CWE](https://cwe.mitre.org/) | Taxonomia de fraquezas | Classificação de classes de defeito |
| [OpenSSF Scorecard](https://openssf.org/projects/scorecard/) | Ferramenta de avaliação | Riscos de repositórios e dependências |
| [SLSA 1.2](https://slsa.dev/spec/v1.2/) | Especificação de supply chain | Proveniência de artefatos e builds |
| [GoogleTest](https://github.com/google/googletest) | Framework de execução | Testes C++ de unidade/integração |
| [CTest](https://cmake.org/cmake/help/v3.30/manual/ctest.1.html) | Executor/integração | Descoberta, execução e relatório JUnit |
| [LLVM sanitizers](https://clang.llvm.org/docs/AddressSanitizer.html) | Instrumentação de diagnóstico | Memória, corrida de dados etc., conforme toolchain |
| [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html) | Técnica/ferramenta de fuzzing | Testes orientados por cobertura com corpus |

**Situação editorial em 09/10/2026:** NIST SSDF 1.1 é a versão final; a revisão 1.2 está em *draft*, e não deve ser tratada como final. As convenções OpenTelemetry para CI/CD, se adotadas, devem explicitar seu estágio de release candidate, evitando promessas de estabilidade.

**Direitos:** normas publicadas por organismos de normalização frequentemente possuem conteúdo protegido; manter IDs, edição, URLs, citações permitidas e critérios *independentes* elaborados pelo projeto. Não importar textos normativos integrais sem licença. Documentos abertos também exigem cumprimento das respectivas licenças e atribuições, inclusive quando usados para gerar artefatos derivados.

### Estratégia de cobertura do repertório

- **Nível A — inventariado:** referencial identificado, versão, tipo, escopo, autoridade emissora, URL e licença.
- **Nível B — mapeado:** requisitos/técnicas relevantes relacionados a categorias e riscos.
- **Nível C — operacionalizável:** procedimento, oráculo, entradas/saídas e ambiente descritos.
- **Nível D — implementado:** executor ou adaptador disponível e validado.
- **Nível E — evidenciado:** execução reproduzível, evidência válida e contexto declarado.

A interface exibirá cobertura **por nível e aplicabilidade**, jamais um número bruto de "normas cumpridas" sem método de correspondência.

## 7. REA / RIT como princípios operacionais, sem falsa conformidade

**REA — Reflexive Engineering Attitude:** perceber (estado observado), interpretar (desvio/riscos) e revisar (decisão ou proposta de melhoria). O CRIVO armazena fatos, explicações e vínculos entre decisões; a execução de um teste não autoriza automaticamente uma correção.

**RIT — Integridade Temporal:** preservar identidade retrospectiva, plasticidade prospectiva, proveniência e legitimidade de cada transição. Uma alteração precisa ter baseline, autor, motivo, evidências e versão; a recuperação histórica deve continuar possível após a mudança.

O SisTer registra princípios **propostos**, incluindo `REARIT-P001` (Manutenção Reflexiva Automatizável) e `REARIT-P005` (Autonomia Delegada por Fronteiras). A integração no CRIVO deverá manter referência a esses IDs **e ao estado/versionamento vigente na fonte**, sem autodeclarar aderência à REA/RIT como certificação.

**Duas salvaguardas herdadas:** não duplicar conhecimento derivável de fontes autoritativas e não agir na ausência de evidência/autoridade (`fail-closed`).

## 8. Nascimento/Morte: novo eixo do ciclo de vida

Adotamos **Nascimento** e **Morte** como termos constitutivos propostos do CRIVO. Isso não significa afirmar que os registros prévios da REA/RIT já os prescrevem com esta semântica específica.

- **Nascimento:** primeira admissão formal de uma identidade versionada, com fonte, justificativa, responsável e instante.
- **Vida:** qualificações, execuções, interpretações, revisões, suspensão e revalidação.
- **Morte:** encerramento explícito de uma entidade ou instância, com causa, decisão, instante, estado final e referências; **não é exclusão física do passado**.
- **Sucessão:** nova versão/implementação pode nascer relacionada à anterior sem sobrescrever sua genealogia.

| Entidade | Nascimento | Morte/encerramento |
|---|---|---|
| Definição de teste | Admissão de uma versão | Retirada/supersessão formal |
| Implementação | Qualificação no catálogo | Desativação/substituição |
| Execução | Início autorizado | Estado terminal PASS/FAIL/ERROR etc. |
| Perfil | Publicação de uma versão | Retirada formal, com referência ao sucessor |
| Evidência | Emissão observada | Fim de retenção/autorização de acesso, preservando metadados cabíveis |

**Invariantes:** a identidade não é reciclada; versões anteriores não são reescritas; a morte não apaga evidências obrigatórias; ausência de conclusão de execução é `INTERRUPTED` ou `UNKNOWN`, não PASS; perda de acesso a evidência não permite inventar resultado.

## 9. Arquitetura em camadas

```text
                           CRIVO WEB (padrão SisTer)
                   consulta / filtros / história / métricas
                                      |
                         API local somente leitura
                                      |
            +------------------------+--------------------------+
            |                   CRIVO C++26                     |
            |  Registry: Reference -> Technique -> TestSpec     |
            |  Registry: Implementation -> Profile -> Project   |
            |  Planner -> Preflight -> Policy -> Executor       |
            |  Evidence -> Metrics -> Lifecycle (Birth/Death)  |
            |  REA: Observe -> Interpret -> Review (proposta)   |
            +-------------+--------------------+---------------+
                          |                    |
                     SQLite WAL          Adaptadores tipados
                     + artefatos         CTest / GoogleTest / ...
                          |                    |
                   Histórico local       Projetos autônomos
                                         ELO / TRAMA / ENTE / SINAL
                             [Integração federada futura]
```

**Fronteiras:** CRIVO avalia e emite resultados dentro do seu escopo. Não decide sozinho promoção, certificação ou publicação; não altera bases dos participantes; não substitui avaliadores humanos; não assume autoridade do SisTer. A interface expõe projeções derivadas, jamais executa shell arbitrário no navegador.

## 10. Métricas úteis ao desenvolvimento — possibilidade de pesquisa posterior

Cada métrica deverá possuir **definição operacional**, unidade, denominador, contexto, ferramenta coletora, política de ausências e versão. Exemplos:

| Métrica | Medida com ressalva |
|---|---|
| Cobertura de catálogo | Mapeados / aplicáveis identificados (escopo versionado) |
| Cobertura de execução | Executados / selecionados e aplicáveis |
| Taxa de sucesso contextual | PASS / executados com desfecho, distinguindo BLOCKED e SKIPPED |
| Flakiness | Divergência entre repetições sob condições equivalentes |
| Tempo de diagnóstico | Tempo entre FAIL observado e identificação confirmada de causa |
| Tempo de recuperação | Tempo entre perturbação autorizada e restabelecimento comprovado |
| Sobrevida útil do teste | Período entre admissão e aposentadoria (com censura explícita) |
| Regressões detectadas | Defeitos observados e confirmados por comparação válida |
| Custo do teste | CPU, memória, wall-time, energia, armazenamento e manutenção quando mensuráveis |
| Proveniência completa | Execuções com ambiente, versão, fonte e evidência válidos |

**Proibições:** somar resultados de ambientes incompatíveis; tratar `SKIPPED` como PASS; calcular confiabilidade com amostras insuficientes sem exibir incerteza; ranquear projetos por porcentagem de PASS isolada; inferir certificação de associação nominal a norma.

## 11. Pacotes de trabalho e marcos

| Etapa | Entrega técnica | Evidência de aceite |
|---|---|---|
| M0: preservação | baseline v0.1.0 intacta e inventário dos gaps | ctest e CLI existentes continuam reproduzíveis |
| M1: registro internacional | fontes versionadas, taxonomia, direitos, importação declarativa | consultas e verificações negativas do registry |
| M2: semântica de teste | separação source/spec/implementation/profile/execution | schemas validados, sem conversões ambíguas |
| M3: ciclo temporal | eventos append-only Nascimento/Morte e sucessão | transições válidas, inválidas e replay testados |
| M4: adaptadores | CTest/GoogleTest, JUnit, inventário, dry-run | importação/normalização comprovadas, sem shell arbitrário |
| M5: evidências/métricas | medidores versionados e consultas históricas | comparações reproduzíveis e estados ausentes corretos |
| M6: SisTer Web | painéis por referencial, categoria, projeto, período | leitura factual, acessibilidade, proibição de mutação web |
| M7: pilotos | TRAMA e ELO com perfis de risco próprios | relatórios contextualizados; nenhuma dependência central obrigatória |
| M8: segunda origem | abertura de catálogo `crivo.original` | revisão metodológica e autoria rastreáveis |

**Recorte do próximo incremento:** M0–M2 com esqueleto M3. Não bloquear a entrega inicial pela implementação imediata de todas as ferramentas nem pelo projeto de pesquisa eventual.

## 12. Governança e manutenção

- **Registros de referência** são imutáveis por `id@versão`; uma edição cria nova revisão.
- **Aplicabilidade** depende do projeto e é declarada/avaliada, nunca presumida a partir da tecnologia.
- **Qualificação** técnica é separada de **admissão** para composições federadas.
- **Modos shadow/governed** podem refletir o SGR do SisTer, mas apenas decisões autorizadas do plano competente atribuem efeitos bloqueantes.
- **Registros históricos** devem distinguir observação, interpretação e decisão.
- **Retenção e dados sensíveis** requerem política explícita, limitação de coleta, proteção de segredos, acesso e eventual expurgo autorizado, sem quebrar a integridade das referências históricas remanescentes.
- **Desenvolvimento** adota documentação arquitetural (ADR), testes de contrato, versões semânticas e gates definidos por risco.

## 13. Critérios de sucesso do projeto

O CRIVO deverá ser considerado útil quando: (i) um sistema novo descobrir técnicas/referenciais compatíveis sem conhecer a implementação interna de outros; (ii) testes existentes serem reaproveitados sem duplicar indevidamente seus oráculos; (iii) um resultado ser reconstruível a partir da versão, ambiente e evidências; (iv) substituições e mortes de testes não destruírem a história; (v) o painel apresentar ausências e bloqueios explicitamente; (vi) os sistemas integrados continuarem independentes da disponibilidade do CRIVO.

## 14. Referências internas e bibliografia de trabalho

- `CRIVO-001_v0.1.0.md` — constituição e baseline implementada.
- `CRIVO-DEV-001_v0.2.0.md` — missão e desenho de implementação da próxima fase.
- [SisTer — Interface](https://github.com/jpereiratrindade/SisTer/blob/main/docs/architecture/INTERFACE.md).
- [SisTer — Projeções web](https://github.com/jpereiratrindade/SisTer/blob/main/docs/architecture/SISTER-WEB-RELATIONAL-SURFACE-001_v0.1.0.md).
- [SisTer — REA/RIT](https://github.com/jpereiratrindade/SisTer/blob/main/docs/rea-rit/README.md).
- [SisTer — SGR, engines e governança](https://github.com/jpereiratrindade/SisTer/blob/main/docs/architecture/sgr/verification-engines-and-governance-modes.md).
- [SisTer — ADR-0028](https://github.com/jpereiratrindade/SisTer/blob/main/docs/adr/ADR-0028-composable-component-runtime-deployment-boundary.md).

**Decisão final:** primeiro reconhecer, classificar e operacionalizar com rigor o repertório internacional. Só então ampliar com testes autorais versionados, explicitando o que é reutilização, adaptação, novo teste ou hipótese de pesquisa.
