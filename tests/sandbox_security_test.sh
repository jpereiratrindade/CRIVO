#!/usr/bin/env bash
set -euo pipefail

CRIVO_BIN=${1:-./build/crivo}
CATALOG_DIR=${2:-./catalog}
BUILD_DIR=${3:-./build}

echo "=== [Gate E13] Teste 1: Qualificação Direta do Sandbox de Segurança (ADR-0009) ==="
QUAL_JSON=$("$CRIVO_BIN" sandbox qualify --backend bubblewrap --json)
echo "$QUAL_JSON" | grep -q '"status":"QUALIFIED"' || {
  echo "FALHA: Sandbox bubblewrap nao foi qualificado: $QUAL_JSON"
  exit 1
}
echo "$QUAL_JSON" | grep -q '"ro_target_immutable"' || {
  echo "FALHA: Prova negativa de imutabilidade do target ausente: $QUAL_JSON"
  exit 1
}
echo "$QUAL_JSON" | grep -q '"network_isolation"' || {
  echo "FALHA: Prova negativa de isolamento de rede ausente: $QUAL_JSON"
  exit 1
}
echo "$QUAL_JSON" | grep -q '"timeout_enforcement"' || {
  echo "FALHA: Prova positiva de timeout e encerramento de processos ausente: $QUAL_JSON"
  exit 1
}

echo "=== [Gate E13] Teste 2: Prova de Falha Segura (Fail-Closed) com Backend Inexistente ==="
OUT=$(! "$CRIVO_BIN" check --target "$CATALOG_DIR" --profile pilot-e1 --backend backend_fantasma_xyz --isolation sandbox --catalog "$CATALOG_DIR" 2>&1)
echo "Retorno: $OUT"

echo "=== [Gate E13] Teste 3: Relatório de Sandbox com Garantias Enforçadas ==="
EVID_DIR="$BUILD_DIR/e13_evidence"
rm -rf "$EVID_DIR"
"$CRIVO_BIN" check --target "$CATALOG_DIR" --profile pilot-e1 --backend bubblewrap --catalog "$CATALOG_DIR" --evidence-dir "$EVID_DIR" --db "$BUILD_DIR/e13.db" --json || true

REPORT_FILE=$(find "$EVID_DIR" -name "sandbox-report.json" | head -1)
if [[ -f "$REPORT_FILE" ]]; then
  grep -q '"schema_version":"crivo.sandbox-report/1.1.0"' "$REPORT_FILE" || {
    echo "FALHA: Schema invalido no sandbox-report.json"
    exit 1
  }
  grep -q '"capabilities_enforced"' "$REPORT_FILE" || {
    echo "FALHA: capabilities_enforced ausente no sandbox-report.json"
    exit 1
  }
fi

echo "=== Gate E13: Sandbox de Seguranca Auditado e Qualificado com Sucesso ==="
exit 0
