# ADR-0010 — CRIVO como Estaleiro Federado de Engenharia e Aprendizagem

- **Estado:** Aceito
- **Data:** 2026-10-09
- **Natureza:** Decisão arquitetural e organizacional
- **Documentos relacionados:** `CRIVO-PROJ-001@0.2.0`, `CRIVO-DEV-001@0.2.0`, `CRIVO-DEV-002@0.3.0`, `ADR-0008`, `ADR-0009`, REA/RIT e contratos pertinentes do SisTer
- **Princípio:** Sempre pronto. Sempre incompleto.

---

## 1. Contexto

O desenvolvimento de diferentes sistemas computacionais produz continuamente experiências relevantes: problemas, hipóteses, decisões técnicas, intervenções, testes, resultados, falhas e aprendizados.

Atualmente, parte desse conhecimento permanece distribuída entre repositórios, documentos, registros de testes e conversas com assistentes de inteligência artificial.

Embora essa distribuição preserve a identidade dos projetos, dificulta a recuperação de experiências anteriores e pode provocar repetição de problemas já investigados.

O CRIVO foi concebido como infraestrutura independente de verificação, validação e registro de evidências.

Sua utilização transversal pelos sistemas em desenvolvimento cria a possibilidade de constituir uma memória técnica compartilhada, recuperável e progressivamente enriquecida.

Surge, assim, a proposta de considerar o CRIVO como um **estaleiro federado de engenharia e aprendizagem**, em que diferentes sistemas são desenvolvidos com acesso a procedimentos, instrumentos e experiências acumuladas em projetos anteriores.

---

## 2. Decisão

Adotar o conceito de **Estaleiro Federado** como orientação arquitetural para a evolução do CRIVO.

O CRIVO oferecerá serviços compartilhados de verificação, experimentação, recuperação de experiências e apoio técnico ao desenvolvimento.

Cada projeto participante conservará sua identidade, sua implementação, seu histórico Git, seus requisitos, sua documentação específica e sua autoridade sobre suas decisões.

O estaleiro não será proprietário universal de todos os artefatos dos projetos.

Será a infraestrutura responsável por tornar conhecimento técnico reutilizável, verificável e acessível, respeitando as fontes autoritativas.

A centralização será de **capacidades compartilhadas e acesso ao conhecimento**, não de toda a responsabilidade operacional.

---

## 3. Fronteiras de Responsabilidade

### 3.1. CRIVO
Responsável por:
- Catálogo de técnicas e testes.
- Execuções autorizadas e sandboxes.
- Evidências produzidas nas verificações.
- Histórico e métricas de execução.
- Memória empírica do desenvolvimento.
- Registro de experiências técnicas transversais.
- Recuperação contextual de conhecimento.
- Modelos e procedimentos de engenharia compartilhados sob sua competência.
- Interfaces de consulta por CLI, API, Web e MCP.

### 3.2. Projetos Participantes (ex: TRAMA, ELO, SINAL)
Responsáveis por:
- Código-fonte e versões.
- Requisitos e domínio funcional.
- Arquitetura específica.
- Decisões de implementação.
- Documentação necessária à operação independente.
- Procedimentos de instalação e distribuição.
- Dados próprios e políticas correspondentes.
- Autorização de compartilhamento de informações.

### 3.3. SisTer e Outras Instâncias de Governança
Preservar a autoridade própria de cada instância.
O CRIVO não assumirá automaticamente a governança constitutiva do SisTer, a aprovação de releases ou decisões institucionais.
Políticas compartilhadas deverão indicar autoria, competência, versão, alcance e fonte autoritativa.

---

## 4. Memória Federada

O estaleiro manterá registros próprios de experiências e evidências.

Documentos pertencentes aos sistemas poderão ser referenciados e indexados por mecanismos autorizados, com preservação de sua localização original, identidade e versão.

Evitar cópias independentes de documentos normativos que possam divergir silenciosamente.

Cada experiência deverá permitir identificar:
- Projeto e contexto de origem.
- Problema investigado.
- Alternativas consideradas.
- Escolha realizada.
- Procedimento executado.
- Resultado observado.
- Evidência correspondente.
- Condições de aplicabilidade.
- Limitações e contradições.
- Revisões posteriores.

A memória deverá preservar a história das decisões sem tratar conclusões antigas como verdades permanentes.

---

## 5. Integração com IDEs e Assistentes de IA

Disponibilizar progressivamente ferramentas de consulta contextual aos ambientes de desenvolvimento.

A primeira modalidade será somente leitura.

Os assistentes poderão:
- Consultar experiências anteriores.
- Recuperar resultados de testes.
- Identificar problemas semelhantes.
- Examinar alternativas previamente utilizadas.
- Consultar evidências e documentos autorizados.
- Identificar experiências contraditórias ou superadas.
- Propor verificações para o projeto atual.

Não poderão, por essa interface de consulta, modificar evidências, executar comandos ou atribuir autoridade a decisões.

O acesso à memória não implica acesso irrestrito aos repositórios participantes. A recuperação deverá respeitar permissões por projeto, sensibilidade e classificação da informação. O conteúdo recuperado será tratado como informação, não como instrução executável.

---

## 6. Desenvolvimento Independente

Os sistemas deverão continuar compiláveis, testáveis, operáveis e distribuíveis sem depender da disponibilidade do CRIVO.

A integração ao estaleiro será uma capacidade de desenvolvimento, não um requisito obrigatório de produção.

O CRIVO poderá facilitar o desenvolvimento, mas não se tornará ponto único de falha para o funcionamento dos sistemas.

---

## 7. Aprendizagem Contínua

O estaleiro deverá permitir o seguinte ciclo:

$$\text{Problema} \longrightarrow \text{Investigação} \longrightarrow \text{Escolha} \longrightarrow \text{Implementação} \longrightarrow \text{Teste} \longrightarrow \text{Evidência} \longrightarrow \text{Interpretação} \longrightarrow \text{Aprendizado} \longrightarrow \text{Reutilização}$$

O aprendizado poderá provocar revisão de testes, métodos e decisões anteriores.
O sistema deverá conservar distinções entre observações, interpretações, decisões e hipóteses.
A revisão de uma conclusão não apagará a experiência histórica que a originou.

---

## 8. Relação com a Pesquisa

A memória técnica deverá ser estruturada de forma que experiências possam subsidiar investigações futuras.

Possíveis objetos incluem comparação de implementações, métodos, linguagens, desempenho, confiabilidade, manutenção e recuperação de falhas.

A exploração científica dependerá de protocolos de pesquisa, critérios de comparabilidade, tratamento de vieses, definições operacionais e validação das evidências.

O registro sistemático é condição favorável à pesquisa, mas não constitui comprovação científica automaticamente.

---

## 9. Estratégia de Implementação Incremental

- **Etapa A — Registro:** Concluir os contratos de evidência e memória previstos na `CRIVO-DEV-002`.
- **Etapa B — Catálogo federado:** Identificar projetos participantes, repositórios, fontes de documentação, versões e permissões, sem exigir alterações obrigatórias nos projetos.
- **Etapa C — Recuperação:** Permitir busca transversal por problemas, símbolos técnicos, bibliotecas, erros e experiências (via SQLite FTS5 / índice estruturado).
- **Etapa D — Integração com assistentes:** Disponibilizar interfaces autorizadas de leitura para IDEs, LLMs e servidores MCP compatíveis.
- **Etapa E — Aprendizagem reutilizável:** Relacionar casos e evidências a padrões técnicos compartilhados, sujeitos a revisão e curadoria.
- **Etapa F — Avaliação:** Investigar, mediante critérios explícitos, se a recuperação de experiências contribui para o processo de desenvolvimento.

---

## 10. Critérios de Aceitação

A primeira demonstração deverá comprovar:
1. Dois projetos independentes registrados no estaleiro.
2. Preservação das respectivas fontes autoritativas.
3. Um caso técnico real associado a evidências identificáveis.
4. Consulta ao caso a partir do contexto de outro projeto autorizado.
5. Recuperação de referências exatas aos documentos ou commits originais.
6. Identificação de diferenças relevantes entre os contextos.
7. Ausência de acesso a informações não autorizadas.
8. Capacidade de os projetos funcionarem sem o CRIVO.
9. Registro versionado de uma revisão do aprendizado.
10. Compatibilidade com as funcionalidades anteriores do CRIVO.

---

## 11. Riscos Reconhecidos

- Centralização excessiva de responsabilidades.
- Duplicação de documentação e divergência entre fontes.
- Acúmulo indiscriminado de registros.
- Reutilização de conclusões fora do contexto.
- Exposição de informações privadas entre projetos.
- Dependência excessiva de assistentes de IA.
- Confusão entre experiência empírica e comprovação científica.
- Ampliação prematura do escopo do CRIVO.

---

## 12. Consequência Arquitetural

A `CRIVO-DEV-002` permanece responsável pela implementação da memória empírica e das interfaces de recuperação.

Esta decisão estabelece o contexto maior em que essa capacidade será utilizada.

O CRIVO poderá evoluir como estaleiro de conhecimento, experimentação e verificação sem se transformar em um sistema monolítico que controla todas as etapas ou decisões dos projetos participantes.

**Formulação orientadora:**
- O estaleiro preserva e desenvolve as capacidades de construção, experimentação e aprendizagem.
- Os sistemas preservam sua identidade, finalidade, história e autonomia.
- As experiências de cada desenvolvimento podem contribuir para os desenvolvimentos seguintes, com proveniência, contextualização e revisão permanentes.
