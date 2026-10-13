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
echo "$QUAL_JSON" | grep -q '"backend":"bubblewrap"' || {
  echo "FALHA: Driver bubblewrap nao identificado explicitamente: $QUAL_JSON"
  exit 1
}
echo "$QUAL_JSON" | grep -q '"isolation_status":"ENFORCED"' || {
  echo "FALHA: Status de isolamento nao e ENFORCED apos qualificacao: $QUAL_JSON"
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

echo "=== [Gate E13] Teste 2: Prova de Falha Segura (Fail-Closed) e Evidência Estruturada ==="
BLOCKED_EVID="$BUILD_DIR/e13_blocked_evidence"
rm -rf "$BLOCKED_EVID"
set +e
OUT=$("$CRIVO_BIN" check --target "$CATALOG_DIR" --profile pilot-e1 --backend backend_fantasma_xyz --isolation sandbox --catalog "$CATALOG_DIR" --evidence-dir "$BLOCKED_EVID" 2>&1)
RC=$?
set -e
echo "Retorno (rc=$RC): $OUT"
if [[ $RC -ne 2 ]]; then
  echo "FALHA: Execucao com backend inexistente deveria retornar exit code 2, retornou $RC"
  exit 1
fi
echo "$OUT" | grep -q 'BLOCKED' || {
  echo "FALHA: Mensagem de erro nao indicou estado BLOCKED: $OUT"
  exit 1
}

BLOCKED_REPORT=$(find "$BLOCKED_EVID" -name "sandbox-report.json" | head -1)
if [[ -z "$BLOCKED_REPORT" || ! -f "$BLOCKED_REPORT" ]]; then
  echo "FALHA: Bloqueio inicial de seguranca nao gerou sandbox-report.json estruturado!"
  exit 1
fi
grep -q '"isolation_status":"BLOCKED"' "$BLOCKED_REPORT" || {
  echo "FALHA: sandbox-report nao registrou isolation_status BLOCKED no bloqueio!"
  exit 1
}

BLOCKED_EVID_FILE=$(find "$BLOCKED_EVID" -name "evidence.json" | head -1)
if [[ -z "$BLOCKED_EVID_FILE" || ! -f "$BLOCKED_EVID_FILE" ]]; then
  echo "FALHA: Bloqueio inicial de seguranca nao gerou evidence.json estruturado!"
  exit 1
fi
grep -q '"status":"BLOCKED"' "$BLOCKED_EVID_FILE" || {
  echo "FALHA: evidence.json nao registrou status BLOCKED no bloqueio!"
  exit 1
}
grep -q 'Unknown sandbox backend' "$BLOCKED_EVID_FILE" || {
  echo "FALHA: evidence.json nao registrou a causa do bloqueio de backend desconhecido!"
  exit 1
}

echo "=== [Gate E13] Teste 3: Execução Completa de Oráculos Isolados em Bubblewrap com PASS Real ==="
E13_TARGET="$BUILD_DIR/e13_isolated_target"
rm -rf "$E13_TARGET"
mkdir -p "$E13_TARGET"
cp -r "${CATALOG_DIR}/../tests/fixtures/all_oracles/"* "$E13_TARGET/"
"$CRIVO_BIN" init --db "$E13_TARGET/app.db" --file "$CATALOG_DIR/tests.json" >/dev/null

EVID_DIR="$BUILD_DIR/e13_evidence"
rm -rf "$EVID_DIR"
mkdir -p "$EVID_DIR"

CHECK_OUT=$("$CRIVO_BIN" check --target "$E13_TARGET" --profile complete-international-benchmark --backend bubblewrap --catalog "$CATALOG_DIR" --evidence-dir "$EVID_DIR" --db "$BUILD_DIR/e13.db" --json)
RC=$?
if [[ $RC -ne 0 ]]; then
  echo "FALHA: Execucao de oraculos isolados falhou (rc=$RC): $CHECK_OUT"
  exit 1
fi

echo "$CHECK_OUT" | grep -q '"status":"PASS"' || {
  echo "FALHA: Status geral do check isolado nao foi PASS: $CHECK_OUT"
  exit 1
}

REPORT_FILE=$(find "$EVID_DIR" -name "sandbox-report.json" | head -1)
if [[ -z "$REPORT_FILE" || ! -f "$REPORT_FILE" ]]; then
  echo "FALHA: sandbox-report.json obrigatorio nao foi gerado na evidencia!"
  exit 1
fi

grep -q '"schema_version":"crivo.sandbox-report/1.1.0"' "$REPORT_FILE" || {
  echo "FALHA: Schema invalido no sandbox-report.json"
  exit 1
}
grep -q '"backend":"bubblewrap"' "$REPORT_FILE" || {
  echo "FALHA: Backend bubblewrap nao comprovado no sandbox-report.json"
  exit 1
}
grep -q '"isolation_status":"ENFORCED"' "$REPORT_FILE" || {
  echo "FALHA: Status ENFORCED nao comprovado na execucao aprovada: $(cat "$REPORT_FILE")"
  exit 1
}
grep -q '"exit_code":0' "$REPORT_FILE" || {
  echo "FALHA: Codigo de saida 0 nao registrado no execution do sandbox-report.json"
  exit 1
}
grep -q '"capabilities_enforced"' "$REPORT_FILE" || {
  echo "FALHA: capabilities_enforced ausente no sandbox-report.json"
  exit 1
}
grep -q '"ro_target_binding"' "$REPORT_FILE" || {
  echo "FALHA: ro_target_binding ausente em capabilities_enforced"
  exit 1
}
grep -q '"rw_workspace_binding"' "$REPORT_FILE" || {
  echo "FALHA: rw_workspace_binding ausente em capabilities_enforced"
  exit 1
}
grep -q '"unshare_all"' "$REPORT_FILE" || {
  echo "FALHA: unshare_all ausente em capabilities_enforced"
  exit 1
}
grep -q '"unshare_net"' "$REPORT_FILE" || {
  echo "FALHA: unshare_net ausente em capabilities_enforced"
  exit 1
}
grep -q '"rw_bind_mounts"' "$REPORT_FILE" || {
  echo "FALHA: rw_bind_mounts ausente no sandbox-report.json"
  exit 1
}
grep -q '"tmpfs_mounts"' "$REPORT_FILE" || {
  echo "FALHA: tmpfs_mounts ausente no sandbox-report.json"
  exit 1
}
grep -q '"limits_configured"' "$REPORT_FILE" || {
  echo "FALHA: limits_configured ausente no sandbox-report.json"
  exit 1
}
grep -q '"limits_enforced"' "$REPORT_FILE" || {
  echo "FALHA: limits_enforced ausente no sandbox-report.json"
  exit 1
}

# Verificacao de consistencia de recursos: max_processes nao deve constar em limits_enforced para Bubblewrap
python3 -c "
import json, sys
data = json.load(open('$REPORT_FILE'))
assert 'limits_configured' in data, 'limits_configured ausente'
assert 'max_processes' in data['limits_configured'], 'max_processes ausente em limits_configured'
assert 'limits_enforced' in data, 'limits_enforced ausente'
assert 'max_processes' not in data['limits_enforced'], 'max_processes indevidamente declarado em limits_enforced para Bubblewrap'
" || {
  echo "FALHA: Inconsistencia em limits_configured vs limits_enforced: $(cat "$REPORT_FILE")"
  exit 1
}

echo "=== [Gate E13] Teste 4: Não-Declaração de ENFORCED Quando Houver Falha ==="
FAIL_EVID="$BUILD_DIR/e13_fail_evidence"
rm -rf "$FAIL_EVID"
set +e
"$CRIVO_BIN" check --target "$CATALOG_DIR" --profile pilot-e1 --backend bubblewrap --catalog "$CATALOG_DIR" --evidence-dir "$FAIL_EVID" --db "$BUILD_DIR/e13_fail.db" --json >/dev/null 2>&1
FAIL_RC=$?
set -e
if [[ $FAIL_RC -eq 0 ]]; then
  echo "FALHA: Execucao com alvos faltantes deveria falhar/bloquear, retornou 0"
  exit 1
fi

FAIL_REPORT=$(find "$FAIL_EVID" -name "sandbox-report.json" | head -1)
if [[ -z "$FAIL_REPORT" || ! -f "$FAIL_REPORT" ]]; then
  echo "FALHA: sandbox-report.json ausente na execucao com falha"
  exit 1
fi

python3 -c "
import json, sys
data = json.load(open('$FAIL_REPORT'))
status = data.get('isolation_status')
assert status in ('BLOCKED', 'DEGRADED'), f'Status nao deveria ser ENFORCED em caso de falha, obteve {status}'
" || {
  echo "FALHA: isolation_status declarado como ENFORCED indevidamente em caso de falha!"
  exit 1
}

echo "=== Gate E13: Sandbox de Seguranca Auditado, Isolado e Qualificado com Sucesso ==="
exit 0
