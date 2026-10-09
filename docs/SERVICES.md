# CRIVO — catálogo público de serviços

Esta interface apresenta somente capacidades seguras para descoberta. Ela é uma
projeção do catálogo autoritativo: não constitui uma segunda fonte de verdade.

## Consultar pela CLI

```bash
./build/crivo services list
./build/crivo services show sqlite.integrity
```

## Consultar pela API somente leitura

```text
GET /api/v1/services
GET /api/v1/services/{id}
```

Exemplo:

```bash
curl http://127.0.0.1:8765/api/v1/services
curl http://127.0.0.1:8765/api/v1/services/sqlite.integrity
```

## Semântica pública

| Campo | Significado |
|---|---|
| `schema_version` | Versão do contrato público da projeção |
| `id` | Identidade estável no catálogo atual |
| `purpose` | Finalidade declarada, não certificação |
| `availability` | `IMPLEMENTED` ou `PLANNED` |
| `consumption` | Forma disponível; `NOT_AVAILABLE` quando planejado |
| `output_states` | Estados que a capacidade pode produzir atualmente |
| `authority` | Autoridade registrada; `NOT_DEFINED` na baseline |
| `evidence` | Nível público da evidência |
| `limitations` | Limites obrigatórios de interpretação |

## Fronteira de exposição

A projeção não publica caminhos, comandos internos, SQL, variáveis de ambiente,
topologia, logs brutos, segredos ou detalhes de contenção. Evidências detalhadas
e contexto de ambiente exigirão interface autorizada futura.

Na API exposta em bind de rede, somente serviços `IMPLEMENTED` são publicados.
Serviços `PLANNED` permanecem consultáveis pela CLI e pela API em loopback para
engenharia, sempre com modo `NOT_AVAILABLE` e evidência `NONE`. Sua presença
interna não indica implementação, execução ou evidência.

Endpoints de catálogo, categorias e execuções são restritos ao bind loopback.
O modo LAN oferece apenas a projeção mínima de serviços implementados e arquivos
estáticos da apresentação.

## Relação com o Repertório Internacional (v0.2.0)

A projeção de serviços legada (`/api/v1/services` e `crivo services list/show`) coexiste
de forma estável com os 5 registros internacionais tipados (`catalog/references/`,
`catalog/techniques/`, `catalog/specifications/`, `catalog/implementations/`, `catalog/profiles/`).
A validação estrita dessas entidades é executada via `crivo registry validate` e `crivo plan`.
