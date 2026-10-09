# CRIVO — Roadmap (2026-10-09)

## v0.1.0 · Fundação local

- [x] CMake / C++26 e execução CLI
- [x] SQLite WAL
- [x] Catalogação por categoria, subcategoria, finalidade, perfil, etiquetas
- [x] Distinção entre implementado e planejado
- [x] Executor builtin (3 verificações)
- [x] Histórico e API local GET
- [x] Dashboard Web com identidade visual compatível com SisTer
- [x] CTest smoke local
- [ ] Auditoria de segurança e testes de carga

## v0.2.0 · Integração útil

- [ ] JSON Schema estrito, migração de banco e testes negativos
- [ ] Adaptador CTest/GoogleTest com importação de JUnit/CTest JSON
- [ ] Perfis hierárquicos (`includes`, filtros, política)
- [ ] Registro de projeto e ambiente com versionamento
- [ ] Política de execução por host, privilégio, tempo e recursos
- [ ] Evidências referenciadas, digests e exportação portável
- [ ] Primeiro projeto-piloto real: TRAMA

## v0.3.0 · Resiliência e federação

- [ ] Segundo projeto-piloto real: ELO
- [ ] Sandboxes segregadas e execução distribuída com agente
- [ ] Diagnóstico de regressões ao longo de versões
- [ ] Projeções de resultados governadas pela fronteira SisTer
- [ ] Contrato de evidência com provenance e estado temporal
- [ ] Acessibilidade revisada com testes manuais e automatizados

## v1.0.0 · Critérios de maturidade, não promessa

Somente considerar após critérios independentes de segurança, desempenho, acessibilidade, compatibilidade C++26, recuperação, integridade de evidência e integração multiplataforma serem demonstrados.
