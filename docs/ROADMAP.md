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

## v0.2.0 · Integração e Validação Estruturada (E1 a E4 Concluídos)

- [x] JSON Schema estrito v1.0.0 (reference, technique, test-spec, implementation, profile, plan, evidence)
- [x] Validador estrito em C++26 e resolução de integridade referencial cruzada (E1)
- [x] Avaliação tri-estado de aplicabilidade (APPLICABLE, NOT_APPLICABLE, UNKNOWN) e resolução de planos (E2)
- [x] Política de falha fechada (--fail-closed) para aplicabilidade indeterminada (E2)
- [x] Trilha de auditoria append-only, eventos de ciclo de vida e digest SHA-256 canônico (E3)
- [x] Reconciliação estruturada de execuções órfãs / interrupções (E3)
- [x] Adaptador CTest com importação de JUnit XML e CTest JSON v1 (E4)
- [x] Fixture sintética local para verificação isolada de adaptadores (E4)
- [x] Suíte de 38 testes CTest automatizados (E1 a E7, incluindo evidência CRIVO-native)
- [x] Piloto TRAMA opt-in em modo shadow, build/evidência segregados, timeout e execução sem shell
- [x] Índice SQLite de execuções externas e artefatos de evidência
- [x] Verificação externa CRIVO-native sem CTest ou código de teste no sistema-alvo
- [ ] Sandboxing de kernel, recursos e rede reforçado para projetos externos
- [ ] Segundo projeto-piloto: ELO (Raspberry Pi / offline)

## v0.3.0 · Resiliência e federação

- [ ] Segundo projeto-piloto real: ELO
- [ ] Sandboxes segregadas e execução distribuída com agente
- [ ] Diagnóstico de regressões ao longo de versões
- [ ] Projeções de resultados governadas pela fronteira SisTer
- [ ] Contrato de evidência com provenance e estado temporal
- [ ] Acessibilidade revisada com testes manuais e automatizados

## v1.0.0 · Critérios de maturidade, não promessa

Somente considerar após critérios independentes de segurança, desempenho, acessibilidade, compatibilidade C++26, recuperação, integridade de evidência e integração multiplataforma serem demonstrados.
