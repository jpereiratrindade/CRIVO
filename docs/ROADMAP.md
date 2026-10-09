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

## v0.3.0 · Independência, Verificação sob Demanda e Sandboxes Efêmeras (CRIVO-DEV-002)

- [x] **ADR-0008**: Inversão do fluxo de solicitação e independência tecnológica dos participantes
- [x] **ADR-0009**: Subsistema de sandboxes efêmeras e isolamento por desenho
- [x] **E1 — Preservação**: Manter 100% das capacidades operacionais v0.1/v0.2 e integridade da suíte
- [x] **E2 — Inversão do fluxo**: Implementar CLI `crivo check` para solicitação sob demanda pelo desenvolvimento
- [x] **E3 — Independência**: Oráculos CRIVO-native e adaptadores CLI sem dependência de CTest/CMake no participante
- [x] **E4 — Isolamento**: Subsistema de Sandboxes efêmeras com backends Bubblewrap e Podman rootless no Linux (41 testes CTest)
- [ ] **E5 — Evidências**: Ampliação de telemetria, consumo de recursos e proveniência estrita
- [ ] **E6 — Piloto TRAMA-RS**: Fluxo completo sob demanda em sandbox para o TRAMA-RS
- [ ] **E7 — Interface SisTer**: Acompanhamento, histórico de sandboxes e diagnósticos no dashboard Web

## v1.0.0 · Critérios de maturidade, não promessa

Somente considerar após critérios independentes de segurança, desempenho, acessibilidade, compatibilidade C++26, recuperação, integridade de evidência e integração multiplataforma serem demonstrados.

