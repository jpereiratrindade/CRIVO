# ADR-0002 — Validação estrita de schemas e integridade referencial com Boost.JSON

- Estado: aceito
- Data: 2026-10-09
- Baseline: `dc4918a860189f0fbb844f456c08d946f2d2cd8e`
- Missão: `CRIVO-DEV-001@0.2.0`, entrega E1
- Vinculação: `ADR-0001`, `CRIVO-IMPL-001@0.1.0`

## Contexto

A versão v0.1.0 utilizava `boost::property_tree` para parsing simples do arquivo legado `catalog/tests.json`. O `boost::property_tree` não distingue tipos reais JSON (números, booleanos, strings, objetos, arrays), não valida enumerações, não rejeita chaves desconhecidas (`additionalProperties: false`) e não implementa integridade referencial cruzada.

Para o ciclo E1, o CRIVO deve implementar validação estrita para os cinco registros (`Reference`, `Technique`, `TestSpec`, `TestImplementation`, `Profile`) e seus respectivos schemas JSON v1.0.0, suportando modo `--dry-run`, idempotência e testes de rejeição negativa com mensagens e códigos de erro estruturados.

## Decisão

1. **Parser e Validação:** Adotar `boost::json` (`BOOST_JSON_HEADER_ONLY`) para parsing e validação estrutural e semântica estrita em C++26.
2. **Camadas de Validação:**
   - **Camada 1 (Sintaxe e Tipos):** Validação de formato JSON válido, verificação estrita de tipos de cada campo (`is_string()`, `is_object()`, `is_array()`, `is_bool()`, `is_int64()`), proibição de campos não declarados (`additionalProperties: false`) e obrigatoriedade de campos requeridos.
   - **Camada 2 (Enumerações e Padrões):** Validação contra vocabulário controlado (`kind`, `status_at_registration`, `distribution`, `license_review`, `catalog_level`, `family`, `oracle_class`, `relation`, `source`, `level`, `purpose`, `qualification`, `adapter`, `isolation`, `network`) e padrões de identificador.
   - **Camada 3 (Integridade Referencial Cruzada):** Verificação de que técnicas e especificações apontam para referências existentes (`id` + `edition`), especificações apontam para técnicas existentes, e implementações apontam para especificações existentes (`id` + `version`).
3. **Comando CLI `crivo registry`:**
   - `crivo registry validate [--catalog-dir <dir>] [--file <file>]`: valida catálogo e integridade de registros sem tocar em banco.
   - `crivo registry import --catalog-dir <dir> --db <db> [--dry-run]`: validação e importação idempotente para persistência SQLite com suporte a transações e rollback.
4. **Preservação Retrocompatível:** Manter intactos todos os comandos v0.1.0 (`init`, `catalog`, `services`, `run`, `runs`, `serve`, `selftest`) e o arquivo `catalog/tests.json`.

## Consequências

- Erros de schema, campos espúrios, tipos trocados ou referências pendentes são detectados e rejeitados deterministicamente com mensagens informativas.
- Operações de importação em modo `--dry-run` não realizam nenhuma mutação de banco ou estado.
- O sistema opera totalmente offline sem dependência de rede para download de validadores externos.
