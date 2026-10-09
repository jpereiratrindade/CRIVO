# ADR-0007 — Verificação externa CRIVO-native

- Estado: aceito
- Data: 2026-10-09
- Vinculação: `ADR-0005`, `ADR-0006`, `CRIVO-DEV-001@0.2.0`

## Contexto

O piloto CTest comprovou descoberta e coleta de evidência, mas dependia de testes mantidos pelo sistema-alvo. Isso não atende ao objetivo central de reutilização: um projeto deve poder consumir verificações do CRIVO sem conter suíte, framework ou integração CRIVO próprios.

## Decisão

1. CTest permanece adaptador opcional e não constitui requisito para um alvo.
2. `verify-project` configura o build com testes do alvo desativados e executa critérios mantidos pelo CRIVO.
3. Admissão ocorre por manifesto local não versionado no CRIVO. Manifesto descreve bindings operacionais; não define testes nem concede autoridade.
4. Primeiros oráculos CRIVO-native cobrem inicialização repetida, preparação declarada, validação independente, integridade/WAL/FK SQLite e contrato HTTP de saúde, somente leitura e `nosniff`.
5. Executáveis admitidos devem resolver sob build segregado. Dados e banco ficam em sandbox sob diretório de evidência do CRIVO.
6. Evidência JUnit/JSON recebe SHA-256 verificado antes da indexação SQLite.
7. Execução permanece `shadow`. Mapeamento nominal a ISO/IEC 25010 ou OWASP ASVS informa origem e limite interpretativo; não declara conformidade ou certificação.

## Consequências

- Sistema-alvo pode não possuir testes.
- CRIVO não conhece identidade nem caminhos do projeto antes da admissão.
- Bindings ainda pressupõem interfaces compatíveis e autorização contextual.
- Contenção de kernel, CPU, memória e rede continua pendente.
- Critérios internacionais adicionais devem nascer no catálogo antes de entrar no runner, com oráculo e aplicabilidade explícitos.
