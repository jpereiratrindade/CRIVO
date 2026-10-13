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

out=$("$CRIVO_BIN" oracle crivo.concurrency.thread-sanitizer --target "$T1_DIR" --workspace "$T1_DIR" 2>&1 || true)
if grep -q "ThreadSanitizer reported data race" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "TSan não reprovou data race real: $out"
fi

# -----------------------------------------------------------------------------
# Cenário 2: Rejeição de Falsa Evidência de TSan (arquivo espúrio)
# -----------------------------------------------------------------------------
run_check "Rejeição de falsa evidência de TSan (arquivo sem rastro do sanitizador)"
T2_DIR="$WORKSPACE_ROOT/t2_fake_tsan"
mkdir -p "$T2_DIR"
cat >"$T2_DIR/tsan_report.txt" <<'EOF'
All tests succeeded cleanly. No data races in this project.
EOF

out=$("$CRIVO_BIN" oracle crivo.concurrency.thread-sanitizer --target "$T2_DIR" --workspace "$T2_DIR" 2>&1 || true)
if grep -q "invalid TSan report evidence" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "Falso relatório de TSan foi indevidamente aceito: $out"
fi

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
        self.send_header('content-security-policy', \"default-src 'self'\")
        self.send_header('x-content-type-options', 'nosniff')
        self.send_header('cache-control', 'no-store')
        self.end_headers()
        self.wfile.write(b'Mutated OK')
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
    def log_message(self, *args): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', 8771), H) as httpd:
    httpd.serve_forever()
" &
P3_PID=$!
sleep 0.5

out=$(CRIVO_HTTP_PORT=8771 "$CRIVO_BIN" oracle crivo.http.contract-readonly --target "$T3_DIR" --workspace "$T3_DIR" 2>&1 || true)
kill -9 $P3_PID 2>/dev/null || true

if grep -q "mutating HTTP method not rejected with 405" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "HTTP ReadOnly não reprovou método mutante: $out"
fi

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

out=$(CRIVO_HTTP_PORT=8772 "$CRIVO_BIN" oracle crivo.security.header-hardening --target "$T4_DIR" --workspace "$T4_DIR" 2>&1 || true)
kill -9 $P4_PID 2>/dev/null || true

if grep -q "required security headers missing" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "Header Hardening não reprovou omissão de CSP: $out"
fi

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
        self.end_headers()
        self.wfile.write(b'root:x:0:0:root')
    def log_message(self, *args): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', 8773), H) as httpd:
    httpd.serve_forever()
" &
P5_PID=$!
sleep 0.5

out=$(CRIVO_HTTP_PORT=8773 "$CRIVO_BIN" oracle crivo.security.cwe-input-validation --target "$T5_DIR" --workspace "$T5_DIR" 2>&1 || true)
kill -9 $P5_PID 2>/dev/null || true

if grep -q "malicious input accepted" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "CWE Input Validation não reprovou directory traversal: $out"
fi

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

out=$("$CRIVO_BIN" oracle crivo.supply-chain.sbom-audit --target "$T6_DIR" --workspace "$T6_DIR" 2>&1 || true)
if grep -q "high/critical SBOM vulnerability detected: CVE-2014-0160" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "SBOM Audit não reprovou CVE crítica: $out"
fi

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

out=$("$CRIVO_BIN" oracle crivo.memory.sha256-provenance --target "$T7_DIR" --workspace "$T7_DIR" 2>&1 || true)
if grep -q "SHA-256 mismatch for artifact" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "Provenance não reprovou artefato adulterado: $out"
fi

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

out=$("$CRIVO_BIN" oracle crivo.contract.sarif-export --target "$T8_DIR" --workspace "$T8_DIR" 2>&1 || true)
if grep -q "invalid target SARIF structure" <<<"$out" || grep -q "ORACLE FAIL" <<<"$out"; then
  check_pass
else
  check_fail "SARIF não reprovou arquivo sem array runs: $out"
fi

# -----------------------------------------------------------------------------
# Cenário 9: Ambiente Heterogêneo - Ausência de Serviço HTTP Bloqueia
# -----------------------------------------------------------------------------
run_check "Ambiente sem serviço HTTP escutando bloqueia oráculo com precondição não atendida"
T9_DIR="$WORKSPACE_ROOT/t9_no_http"
mkdir -p "$T9_DIR"
cat >"$T9_DIR/index.html" <<'EOF'
<!DOCTYPE html><html><body>Web App</body></html>
EOF

out=$(CRIVO_HTTP_PORT=59999 "$CRIVO_BIN" oracle crivo.http.contract-readonly --target "$T9_DIR" --workspace "$T9_DIR" 2>&1 || true)
if grep -q "precondition server_listening_loopback not met" <<<"$out" || grep -q "ORACLE BLOCKED" <<<"$out"; then
  check_pass
else
  check_fail "Falta de servidor HTTP não resultou em BLOCKED: $out"
fi

# -----------------------------------------------------------------------------
# Cenário 10: Ambiente Heterogêneo - Ausência de Banco SQLite declara Inaplicabilidade
# -----------------------------------------------------------------------------
run_check "Ambiente sem SQLite declara NOT_APPLICABLE com clareza"
T10_DIR="$WORKSPACE_ROOT/t10_no_db"
mkdir -p "$T10_DIR"
echo "int main() { return 0; }" >"$T10_DIR/main.cpp"

out=$("$CRIVO_BIN" oracle crivo.sqlite.transaction.rollback --target "$T10_DIR" --workspace "$T10_DIR" 2>&1 || true)
if grep -q "no SQLite database" <<<"$out" || grep -q "NOT_APPLICABLE" <<<"$out"; then
  check_pass
else
  check_fail "Falta de banco SQLite não resultou em NOT_APPLICABLE: $out"
fi

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
