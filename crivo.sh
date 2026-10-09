#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TOOLBOX_NAME=${CRIVO_TOOLBOX:-crivo-dev}

if [[ ${1:-} == "--inside-toolbox" ]]; then
  shift
elif [[ ( ${1:-} == "pilot-project" || ${1:-} == "verify-project" ) ]] && [[ ! -f /usr/include/boost/asio.hpp ]] && command -v toolbox >/dev/null 2>&1; then
  "$SCRIPT_DIR/crivo.sh" build
  if [[ ${1:-} == "verify-project" ]]; then
    exec "$SCRIPT_DIR/scripts/project-verify.sh" "${2:-}"
  fi
  exec "$SCRIPT_DIR/scripts/project-pilot.sh" "${2:-}"
elif [[ ! -f /usr/include/boost/asio.hpp ]] && command -v toolbox >/dev/null 2>&1; then
  exec toolbox run --container "$TOOLBOX_NAME" \
    "$SCRIPT_DIR/crivo.sh" --inside-toolbox "$@"
fi

cd "$SCRIPT_DIR"

BUILD_DIR=${CRIVO_BUILD_DIR:-build}
DB_FILE=${CRIVO_DB:-.run/crivo.db}
PORT=${CRIVO_PORT:-8765}
JOBS=${CRIVO_JOBS:-2}
COMMAND=${1:-up}

configure() {
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
}

build() {
  configure
  cmake --build "$BUILD_DIR" -j"$JOBS"
}

ensure_binary() {
  [[ -x "$BUILD_DIR/crivo" ]] || build
}

ensure_database() {
  ensure_binary
  [[ -f "$DB_FILE" ]] || {
    "$BUILD_DIR/crivo" init --db "$DB_FILE"
    "$BUILD_DIR/crivo" registry import --dir catalog --db "$DB_FILE"
  }
}

show_help() {
  cat <<'EOF'
Uso: ./crivo.sh [comando]

  up | all    Tudo em 1 comando: compila, testa, valida, importa e sobe o servidor web (PADRÃO)
  setup       Configura, compila, executa 31 CTests, valida e importa registros no banco
  build       Configura e compila
  test        Compila e executa toda a suíte CTest
  validate    Valida integridade estrita de schemas e referências cruzadas
  import      Importa catálogo internacional para persistência SQLite
  plan        Gera e exibe o plano de teste resolvido para o perfil
  check       Executa verificação sob demanda em sandbox efêmera (CRIVO-DEV-002)
  memory-record <arquivo> Registra experiência técnica na memória federada (ADR-0010)
  memory-query [termo]   Consulta experiências transversais e evidências (ADR-0010)
  init        Inicializa banco local legado
  run         Executa perfil core
  runs        Lista execuções registradas
  external-runs Lista execuções de projetos externos e evidências indexadas
  events      Lista trilha de auditoria e eventos de ciclo de vida
  pilot-project <manifesto> Executa projeto externo declarado, em modo shadow
  verify-project <manifesto> Executa testes CRIVO-native; alvo nao precisa ter testes
  serve       Web local (127.0.0.1:8765)
  serve-lan   Web na rede local (0.0.0.0:8765; sem autenticação/TLS)
  status      Mostra ambiente, banco e endereço de rede

Variáveis: CRIVO_TOOLBOX, CRIVO_BUILD_DIR, CRIVO_DB, CRIVO_PORT, CRIVO_JOBS,
           CRIVO_PROJECT_BUILD, CRIVO_PROJECT_EVIDENCE.
EOF
}

case "$COMMAND" in
  up|all)
    build
    printf '\n=== [1/5] Executando CTest (46 testes) ===\n'
    ctest --test-dir "$BUILD_DIR" --output-on-failure
    printf '\n=== [2/5] Validando Catálogo Estrito ===\n'
    "$BUILD_DIR/crivo" registry validate --dir catalog
    printf '\n=== [3/5] Inicializando Banco e Importando Registros ===\n'
    "$BUILD_DIR/crivo" init --db "$DB_FILE"
    "$BUILD_DIR/crivo" registry import --dir catalog --db "$DB_FILE"
    printf '\n=== [4/5] Executando Perfil Core ===\n'
    "$BUILD_DIR/crivo" run --db "$DB_FILE" --profile core
    printf '\n=== [5/5] Subindo SisTer Web Interface ===\n'
    printf 'Acesse localmente em: http://127.0.0.1:%s\n\n' "$PORT"
    exec "$BUILD_DIR/crivo" serve --db "$DB_FILE" --web web --bind 127.0.0.1 --port "$PORT"
    ;;
  setup)
    build
    ctest --test-dir "$BUILD_DIR" --output-on-failure
    "$BUILD_DIR/crivo" registry validate --dir catalog
    "$BUILD_DIR/crivo" init --db "$DB_FILE"
    "$BUILD_DIR/crivo" registry import --dir catalog --db "$DB_FILE"
    ;;
  build)
    build
    ;;
  test)
    build
    ctest --test-dir "$BUILD_DIR" --output-on-failure
    ;;
  check)
    ensure_binary
    shift || true
    "$BUILD_DIR/crivo" check --db "$DB_FILE" "$@"
    ;;
  memory-record)
    ensure_binary
    "$BUILD_DIR/crivo" memory record --file "${2:-}" --db "$DB_FILE"
    ;;
  memory-query)
    ensure_binary
    shift || true
    "$BUILD_DIR/crivo" memory query --db "$DB_FILE" "$@"
    ;;
  validate)
    ensure_binary
    "$BUILD_DIR/crivo" registry validate --dir catalog
    ;;
  import)
    ensure_binary
    "$BUILD_DIR/crivo" registry import --dir catalog --db "$DB_FILE"
    ;;
  plan)
    ensure_binary
    "$BUILD_DIR/crivo" plan --profile "${2:-pilot-e1}" --dir catalog
    ;;
  events)
    ensure_database
    "$BUILD_DIR/crivo" events list --db "$DB_FILE"
    ;;
  pilot-project)
    build
    exec "$SCRIPT_DIR/scripts/project-pilot.sh" "${2:-}"
    ;;
  verify-project)
    build
    exec "$SCRIPT_DIR/scripts/project-verify.sh" "${2:-}"
    ;;
  init)
    ensure_binary
    "$BUILD_DIR/crivo" init --db "$DB_FILE"
    ;;
  run)
    ensure_database
    "$BUILD_DIR/crivo" run --db "$DB_FILE" --profile "${2:-core}"
    ;;
  runs)
    ensure_database
    "$BUILD_DIR/crivo" runs --db "$DB_FILE"
    ;;
  external-runs)
    ensure_database
    "$BUILD_DIR/crivo" external-runs --db "$DB_FILE"
    ;;
  serve)
    ensure_database
    printf 'CRIVO SisTer Web iniciado em: http://127.0.0.1:%s\n' "$PORT"
    exec "$BUILD_DIR/crivo" serve --db "$DB_FILE" --web web --bind 127.0.0.1 --port "$PORT"
    ;;
  serve-lan)
    ensure_database
    printf '%s\n' \
      'AVISO: CRIVO sem autenticação/TLS. Use somente em rede local confiável.' \
      "Acesso: http://IP-DESTA-MAQUINA:$PORT"
    exec "$BUILD_DIR/crivo" serve --db "$DB_FILE" --web web --bind 0.0.0.0 --port "$PORT"
    ;;
  status)
    printf 'Toolbox: %s\nBuild: %s\nBanco: %s\nPorta: %s\n' \
      "$TOOLBOX_NAME" "$BUILD_DIR" "$DB_FILE" "$PORT"
    ip -brief address 2>/dev/null || hostname -I 2>/dev/null || true
    ;;
  help|-h|--help)
    show_help
    ;;
  *)
    printf 'Comando desconhecido: %s\n\n' "$COMMAND" >&2
    show_help >&2
    exit 2
    ;;
esac
