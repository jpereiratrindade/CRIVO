# CRIVO-DEV-002 — Independência dos Sistemas Participantes, Verificação sob Demanda e Execução em Sandbox

- **Estado:** Proposta de evolução arquitetural aprovada (`v0.3.0`)
- **Linguagem principal:** C++26
- **Persistência:** SQLite3 WAL
- **Interface:** Web padrão SisTer / CLI unificada
- **Princípio:** Sempre pronto. Sempre incompleto.
- **Vinculação:** `ADR-0008`, `ADR-0009`, `CRIVO-DEV-001@0.2.0`

---

## 1. Missão

Evoluir o CRIVO como uma infraestrutura independente, reutilizável e extensível de testes, verificações, validações e diagnósticos técnicos, destinada a apoiar diretamente o desenvolvimento de sistemas computacionais (como TRAMA, ELO, SINAL e outros).

O sistema em desenvolvimento deverá poder solicitar testes ao CRIVO sem precisar incorporar CTest, CMake, GoogleTest ou qualquer outra infraestrutura particular de testes.

O CRIVO será responsável por disponibilizar verificações pertinentes, preparar condições controladas de execução, utilizar sandboxes quando necessário, registrar evidências e devolver resultados tecnicamente interpretáveis.

**A relação principal será: o desenvolvimento solicita; o CRIVO verifica; as evidências fundamentam as decisões.**

---

## 2. Princípios Arquiteturais

### 2.1. Centralidade do desenvolvimento
O CRIVO existe para apoiar os sistemas que estão sendo desenvolvidos. A solicitação de verificações deverá poder partir diretamente do ambiente de desenvolvimento, da linha de comando, de uma integração autorizada ou de uma interface de operação. O CRIVO não necessita acompanhar ou fiscalizar continuamente os projetos para cumprir sua finalidade.

### 2.2. Independência tecnológica
Nenhum sistema participante será obrigado a adotar bibliotecas, frameworks, scripts ou estruturas de diretórios específicas do CRIVO. A integração ocorrerá por capacidades observáveis, interfaces existentes, arquivos, protocolos, executáveis e adaptadores opcionais.

### 2.3. Independência da verificação
O sistema participante poderá solicitar e contextualizar a verificação, mas não poderá atribuir a si mesmo um resultado aprovado. Os critérios, procedimentos, registros e resultados deverão conservar sua proveniência e independência técnica.

### 2.4. Isolamento por desenho
A execução de código potencialmente não confiável deverá ocorrer em ambientes controlados, com uma política de isolamento compatível com o risco. O sistema original, seus dados operacionais e o ambiente hospedeiro não deverão ser utilizados como espaço descartável de testes.

### 2.5. Evolução permanente
O catálogo poderá incorporar novas técnicas, ferramentas, metodologias, critérios e implementações sem exigir modificações retroativas nos sistemas participantes. O CRIVO deve permanecer utilizável em cada estágio de desenvolvimento, com suas capacidades e limitações explicitadas.

---

## 3. Fluxo Operacional

1. Um desenvolvedor ou sistema autorizado solicita uma verificação (`crivo check --target <caminho> --profile <perfil> --isolation <modo>`).
2. O CRIVO identifica o objeto, suas características e as capacidades observáveis.
3. O CRIVO determina os testes potencialmente aplicáveis a partir do catálogo.
4. Uma política versionada define os testes selecionados, as permissões e os requisitos de execução.
5. O CRIVO prepara uma representação controlada do sistema (snapshot ou montagem segura somente-leitura).
6. O CRIVO cria o ambiente isolado de execução (sandbox efêmera).
7. Os executores realizam as verificações autorizadas.
8. Os resultados são coletados, normalizados e relacionados aos critérios correspondentes.
9. As evidências são preservadas no histórico do CRIVO (SQLite WAL + artefatos canônicos com hash SHA-256).
10. O ambiente temporário é encerrado e limpo conforme a política de retenção.
11. O resultado e o diagnóstico ficam disponíveis ao processo de desenvolvimento.
12. O desenvolvedor decide quais mudanças realizará, podendo solicitar novas verificações.

---

## 4. Modalidades de Utilização

- **A — Verificação sob demanda (modalidade principal):** O desenvolvimento solicita a execução de testes em determinado estado do sistema durante implementação, manutenção ou investigação.
- **B — Verificação integrada:** Acionamento opcional por pipelines CI/CD ou ferramentas locais sem autoridade automática para bloquear releases.
- **C — Verificação externa autorizada (`shadow`):** Avaliação independente de sistemas existentes sob autorização explícita, mantendo a capacidade de auditoria e comparação técnica.

---

## 5. Subsistema de Sandboxes

### 5.1. Ciclo de vida
- Nascimento da instância identificada e vinculada à solicitação.
- Preparação de arquivos, dependências e dados autorizados.
- Aplicação de limites de tempo (`timeout`), memória, processos e CPU.
- Execução controlada e coleta de métricas e evidências.
- Registro de interrupções, falhas e desvios.
- Destruição da instância e preservação dos metadados probatórios.

### 5.2. Requisitos de contenção
- Execução sem privilégios administrativos (rootless).
- Montagem da origem em somente-leitura (`ro`) ou snapshot efêmero.
- Bloqueio de diretórios de usuário, chaves, credenciais e sockets do host.
- Rede desabilitada por padrão (`unshare-net`).
- Falha na inicialização da sandbox resulta obrigatoriamente em `BLOCKED`, nunca em execução insegura no hospedeiro.

### 5.3. Backends no Linux
- **Bubblewrap (`bwrap`)**: isolamento de processos por namespaces e seccomp.
- **Podman rootless**: isolamento conteinerizado quando exigido pelo perfil.
- **Processo Nativo Segregado**: fallback monitorado para ambientes sem suporte a namespaces de usuário.

### 5.4. Tipos de ambiente
- `static`: análise estrutural e leitura de arquivos.
- `executable`: execução de binários CLI ou suítes de teste.
- `service`: APIs e contratos HTTP em rede isolada.
- `persistence`: validação de bancos SQLite descartáveis.
- `resilience`: simulação de falhas e interrupções.

---

## 6. Arquitetura de Execução e Adaptadores

- **Executor Nativo CRIVO:** inspeção estrutural, integridade de arquivos, integridade de bancos SQLite (WAL, FK, integridade pragmas), contratos HTTP (`health`, `nosniff`, etc.), testes de limites e diagnósticos.
- **Adaptadores:** preservação do adaptador CTest existente e suporte a executáveis CLI por contratos estruturados.
- **Modo Híbrido:** combinação de testes nativos CRIVO e suítes existentes do projeto com atribuição clara de proveniência de cada oráculo.

---

## 7. Interface de Consumo

CLI proposta:
```bash
crivo check --target . --profile core --isolation sandbox
```

A descoberta de capacidades nunca autoriza automaticamente a execução arbitrária de comandos ou scripts não autorizados do repositório.

---

## 8. Estados e Evidências

Classificação de resultados:
- `PASS`: Critério verificado com sucesso pelo oráculo.
- `FAIL`: Critério reprovado pelo oráculo.
- `ERROR`: Falha interna de execução ou anormalidade operacional.
- `TIMEOUT`: Limite de tempo de execução atingido.
- `BLOCKED`: Execução impedida por violação de política ou falha de sandbox.
- `SKIPPED`: Pulado por configuração ou dependência não atendida.
- `NOT_APPLICABLE`: Critério não se aplica às capacidades observáveis.
- `NO_TESTS`: Nenhum teste executado (jamais interpretado como aprovação).

---

## 9. Plano de Implementação Incremental

- **E1 — Preservação:** Manter todas as funcionalidades operacionais e os contratos existentes (`crivo selftest`, catálogo, SQLite, CTest).
- **E2 — Inversão do fluxo:** Implementar o comando `crivo check` para solicitação de verificações a partir do alvo.
- **E3 — Independência:** Expandir oráculos nativos e adaptadores CLI sem impor CMake/CTest.
- **E4 — Isolamento:** Implementar o subsistema de sandbox com backend Bubblewrap/Podman rootless.
- **E5 — Evidências:** Ampliar granularidade, proveniência, telemetria de recursos e histórico comparável.
- **E6 — Piloto TRAMA-RS:** Validar o fluxo completo em sandbox com TRAMA-RS.
- **E7 — Interface Web:** Visualização de solicitações sob demanda, telemetria de sandboxes e diagnósticos no padrão SisTer.
