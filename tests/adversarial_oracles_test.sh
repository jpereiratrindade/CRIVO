#!/usr/bin/env bash
set -uo pipefail

CRIVO_BIN=${1:-build/crivo}
WORKSPACE_ROOT=${2:-/tmp/crivo_adversarial_test}
rm -rf "$WORKSPACE_ROOT"
mkdir -p "$WORKSPACE_ROOT"

echo "================================================================="
echo "CRIVO — Suíte de Qualificação Adversarial dos Oráculos"
echo "================================================================="

FAILED_CHECKS=0
TOTAL_CHECKS=0

run_check() {
  local desc="$1"
  TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
  echo -n "[Teste $TOTAL_CHECKS] $desc ... "
}

check_pass() {
  echo "PASS"
}

check_fail() {
  local msg="$1"
  echo "FALHA: $msg"
  FAILED_CHECKS=$((FAILED_CHECKS + 1))
}

# Capture stdout/stderr without erasing exit status.  Every scenario must
# prove classification, cause, and process result together.
run_oracle() {
  set +e
  ORACLE_OUT=$("$@" 2>&1)
  ORACLE_RC=$?
  set -e
}

expect_oracle() {
  local expected_rc="$1"
  local expected_class="$2"
  local expected_cause="$3"
  if [ "$ORACLE_RC" -eq "$expected_rc" ] &&
     grep -Fq "$expected_class" <<<"$ORACLE_OUT" &&
     grep -Fq "$expected_cause" <<<"$ORACLE_OUT"; then
    check_pass
  else
    check_fail "esperado rc=$expected_rc, classe='$expected_class', causa='$expected_cause'; obtido rc=$ORACLE_RC: $ORACLE_OUT"
  fi
}

# -----------------------------------------------------------------------------
# Cenário 1: TSan detecta Data Race Real no Código do Alvo
# -----------------------------------------------------------------------------
run_check "TSan reprova data race real no código-fonte do alvo"
T1_DIR="$WORKSPACE_ROOT/t1_race"
mkdir -p "$T1_DIR"
cat >"$T1_DIR/main.cpp" <<'EOF'
#include <thread>
#include <vector>
#include <iostream>

int g_counter = 0;

void worker() {
    for (int i = 0; i < 50000; ++i) {
        g_counter++;
    }
}

int main() {
    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();
    std::cout << "Done: " << g_counter << "\n";
    return 0;
}
EOF

run_oracle "$CRIVO_BIN" oracle crivo.concurrency.thread-sanitizer --target "$T1_DIR" --workspace "$T1_DIR"
expect_oracle 1 "ORACLE FAIL" "ThreadSanitizer reported data race in target code"

# -----------------------------------------------------------------------------
# Cenário 2: Rejeição de Falsa Evidência de TSan (arquivo espúrio)
# -----------------------------------------------------------------------------
run_check "Rejeição de falsa evidência de TSan (arquivo sem rastro do sanitizador)"
T2_DIR="$WORKSPACE_ROOT/t2_fake_tsan"
mkdir -p "$T2_DIR"
cat >"$T2_DIR/tsan_report.txt" <<'EOF'
ThreadSanitizer execution completed without findings.
EOF

run_oracle "$CRIVO_BIN" oracle crivo.concurrency.thread-sanitizer --target "$T2_DIR" --workspace "$T2_DIR"
expect_oracle 2 "ORACLE BLOCKED" "untrusted TSan report"

# -----------------------------------------------------------------------------
# Cenário 3: Violação de Contrato HTTP ReadOnly (POST retorna 200 em vez de 405)
# -----------------------------------------------------------------------------
run_check "HTTP ReadOnly reprova API mutante que responde 200 para POST"
T3_DIR="$WORKSPACE_ROOT/t3_http_mutating"
mkdir -p "$T3_DIR"
cat >"$T3_DIR/index.html" <<'EOF'
<!DOCTYPE html><html><body>Mutating App</body></html>
EOF
python3 -c "
import http.server, socketserver
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        self.send_response(200)
        self.send_header('x-crivo-target-id', 't3-http-mutating')
        self.send_header('content-security-policy', \"default-src 'self'\")
        self.send_header('x-content-type-options', 'nosniff')
        self.send_header('cache-control', 'no-store')
        self.end_headers()
        self.wfile.write(b'Mutated OK')
    def do_GET(self):
        self.send_response(200)
        self.send_header('x-crivo-target-id', 't3-http-mutating')
        self.end_headers()
    def log_message(self, *args): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', 8771), H) as httpd:
    httpd.serve_forever()
" &
P3_PID=$!
sleep 0.5

run_oracle env CRIVO_HTTP_PORT=8771 CRIVO_HTTP_EXPECTED_ID=t3-http-mutating "$CRIVO_BIN" oracle crivo.http.contract-readonly --target "$T3_DIR" --workspace "$T3_DIR"
kill $P3_PID 2>/dev/null || true
wait $P3_PID 2>/dev/null || true
expect_oracle 1 "ORACLE FAIL" "mutating HTTP method not rejected with 405"

# -----------------------------------------------------------------------------
# Cenário 4: Violação de Cabeçalhos de Segurança HTTP (omissão de CSP)
# -----------------------------------------------------------------------------
run_check "Header Hardening reprova API que omite Content-Security-Policy"
T4_DIR="$WORKSPACE_ROOT/t4_headers"
mkdir -p "$T4_DIR"
cat >"$T4_DIR/index.html" <<'EOF'
<!DOCTYPE html><html><body>Insecure Headers App</body></html>
EOF
python3 -c "
import http.server, socketserver
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('x-crivo-target-id', 't4-headers')
        self.send_header('x-content-type-options', 'nosniff')
        self.send_header('cache-control', 'no-store')
        # Content-Security-Policy omitido deliberadamente
        self.end_headers()
        self.wfile.write(b'No CSP')
    def log_message(self, *args): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', 8772), H) as httpd:
    httpd.serve_forever()
" &
P4_PID=$!
sleep 0.5

run_oracle env CRIVO_HTTP_PORT=8772 CRIVO_HTTP_EXPECTED_ID=t4-headers "$CRIVO_BIN" oracle crivo.security.header-hardening --target "$T4_DIR" --workspace "$T4_DIR"
kill $P4_PID 2>/dev/null || true
wait $P4_PID 2>/dev/null || true
expect_oracle 1 "ORACLE FAIL" "required security headers missing"

# -----------------------------------------------------------------------------
# Cenário 5: Falha de Validação de Entrada CWE (Traversal aceito com 200)
# -----------------------------------------------------------------------------
run_check "CWE Input Validation reprova API que aceita directory traversal"
T5_DIR="$WORKSPACE_ROOT/t5_cwe"
mkdir -p "$T5_DIR"
cat >"$T5_DIR/index.html" <<'EOF'
<!DOCTYPE html><html><body>Vulnerable API</body></html>
EOF
python3 -c "
import http.server, socketserver
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200) # Aceita traversal indevidamente
        self.send_header('x-crivo-target-id', 't5-cwe')
        self.end_headers()
        self.wfile.write(b'root:x:0:0:root')
    def log_message(self, *args): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', 8773), H) as httpd:
    httpd.serve_forever()
" &
P5_PID=$!
sleep 0.5

run_oracle env CRIVO_HTTP_PORT=8773 CRIVO_HTTP_EXPECTED_ID=t5-cwe "$CRIVO_BIN" oracle crivo.security.cwe-input-validation --target "$T5_DIR" --workspace "$T5_DIR"
kill $P5_PID 2>/dev/null || true
wait $P5_PID 2>/dev/null || true
expect_oracle 1 "ORACLE FAIL" "malicious input not rejected by contract"

# -----------------------------------------------------------------------------
# Cenário 6: Detecção de CVE Crítica em SBOM CycloneDX
# -----------------------------------------------------------------------------
run_check "SBOM Audit reprova CycloneDX contendo vulnerabilidade crítica"
T6_DIR="$WORKSPACE_ROOT/t6_sbom_cve"
mkdir -p "$T6_DIR"
cat >"$T6_DIR/sbom.json" <<'EOF'
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.6",
  "components": [
    { "type": "library", "name": "openssl", "version": "1.0.1f" }
  ],
  "vulnerabilities": [
    {
      "id": "CVE-2014-0160",
      "ratings": [
        { "severity": "critical", "score": 9.8 }
      ]
    }
  ]
}
EOF

run_oracle "$CRIVO_BIN" oracle crivo.supply-chain.sbom-audit --target "$T6_DIR" --workspace "$T6_DIR"
expect_oracle 1 "ORACLE FAIL" "high/critical SBOM vulnerability detected: CVE-2014-0160"

# A syntactically valid empty array is not evidence that OSV/NVD (or another
# named source) was queried.
run_check "SBOM vazio sem proveniência de auditoria não gera aprovação"
T6B_DIR="$WORKSPACE_ROOT/t6b_sbom_fake_clean"
mkdir -p "$T6B_DIR"
cat >"$T6B_DIR/sbom.json" <<'EOF'
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.6",
  "components": [{"type": "library", "name": "unknown", "version": "1.0"}],
  "vulnerabilities": []
}
EOF
run_oracle "$CRIVO_BIN" oracle crivo.supply-chain.sbom-audit --target "$T6B_DIR" --workspace "$T6B_DIR"
expect_oracle 2 "ORACLE BLOCKED" "AUDIT_NOT_PERFORMED"

# -----------------------------------------------------------------------------
# Cenário 7: Adulteração de Digest em Pacote de Evidências (SHA-256 mismatch)
# -----------------------------------------------------------------------------
run_check "SHA-256 Provenance reprova evidência quando artefato foi adulterado"
T7_DIR="$WORKSPACE_ROOT/t7_provenance_tamper"
mkdir -p "$T7_DIR"
echo "Original Content" >"$T7_DIR/artifact.txt"
orig_hash=$(sha256sum "$T7_DIR/artifact.txt" | cut -d' ' -f1)
cat >"$T7_DIR/evidence.json" <<EOF
{
  "schema_version": "crivo.evidence/1.0.0",
  "artifacts": [
    { "path": "artifact.txt", "sha256": "$orig_hash" }
  ]
}
EOF

# Adulteração deliberada do artefato
echo "Tampered Content!" >"$T7_DIR/artifact.txt"

run_oracle "$CRIVO_BIN" oracle crivo.memory.sha256-provenance --target "$T7_DIR" --workspace "$T7_DIR"
expect_oracle 1 "ORACLE FAIL" "SHA-256 mismatch for artifact"

# -----------------------------------------------------------------------------
# Cenário 8: Rejeição de SARIF com Estrutura Corrompida (sem runs array)
# -----------------------------------------------------------------------------
run_check "SARIF Export reprova relatório com estrutura inválida (missing runs)"
T8_DIR="$WORKSPACE_ROOT/t8_bad_sarif"
mkdir -p "$T8_DIR"
cat >"$T8_DIR/report.sarif" <<'EOF'
{
  "version": "2.1.0",
  "missing_runs": true
}
EOF

run_oracle "$CRIVO_BIN" oracle crivo.contract.sarif-export --target "$T8_DIR" --workspace "$T8_DIR"
expect_oracle 1 "ORACLE FAIL" "invalid target SARIF structure"

# -----------------------------------------------------------------------------
# Cenário 9: Ambiente Heterogêneo - Ausência de Serviço HTTP Bloqueia
# -----------------------------------------------------------------------------
run_check "Ambiente sem serviço HTTP escutando bloqueia oráculo com precondição não atendida"
T9_DIR="$WORKSPACE_ROOT/t9_no_http"
mkdir -p "$T9_DIR"
cat >"$T9_DIR/index.html" <<'EOF'
<!DOCTYPE html><html><body>Web App</body></html>
EOF

run_oracle env CRIVO_HTTP_PORT=59999 CRIVO_HTTP_EXPECTED_ID=t9-no-http "$CRIVO_BIN" oracle crivo.http.contract-readonly --target "$T9_DIR" --workspace "$T9_DIR"
expect_oracle 2 "ORACLE BLOCKED" "precondition server_listening_loopback not met"

# -----------------------------------------------------------------------------
# Cenário 10: Ambiente Heterogêneo - Ausência de Banco SQLite declara Inaplicabilidade
# -----------------------------------------------------------------------------
run_check "Ambiente sem SQLite declara NOT_APPLICABLE com clareza"
T10_DIR="$WORKSPACE_ROOT/t10_no_db"
mkdir -p "$T10_DIR"
echo "int main() { return 0; }" >"$T10_DIR/main.cpp"

run_oracle "$CRIVO_BIN" oracle crivo.sqlite.transaction.rollback --target "$T10_DIR" --workspace "$T10_DIR"
expect_oracle 3 "ORACLE NOT_APPLICABLE" "no SQLite database"

# -----------------------------------------------------------------------------
# Resumo Final
# -----------------------------------------------------------------------------
echo "================================================================="
echo "Resumo dos Testes Adversariais: $((TOTAL_CHECKS - FAILED_CHECKS))/$TOTAL_CHECKS aprovados"
echo "================================================================="

if [ "$FAILED_CHECKS" -gt 0 ]; then
  echo "FALHA: $FAILED_CHECKS teste(s) falharam na qualificação adversarial!"
  exit 1
fi

echo "QUALIFICAÇÃO ADVERSARIAL CONCLUÍDA COM 100% DE SUCESSO."
exit 0
