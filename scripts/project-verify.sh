#!/usr/bin/env bash
set -uo pipefail

crivo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest=${1:-}
[[ -n "$manifest" ]] || { printf 'Uso: project-verify.sh <manifesto-local.json>\n' >&2; exit 2; }
[[ "$manifest" = /* ]] || manifest="$crivo_root/$manifest"
[[ -f "$manifest" ]] || { printf 'CRIVO VERIFY PREFLIGHT FAILED: manifesto ausente\n' >&2; exit 2; }
for tool in jq cmake git sqlite3 curl sha256sum; do command -v "$tool" >/dev/null || { printf 'CRIVO VERIFY PREFLIGHT FAILED: %s ausente\n' "$tool" >&2; exit 2; }; done

schema=$(jq -er '.schema_version' "$manifest") || exit 2
project_id=$(jq -er '.project_id' "$manifest") || exit 2
source_dir=$(jq -er '.source_dir' "$manifest") || exit 2
[[ "$schema" == crivo.target-admission/1.0.0 && "$project_id" =~ ^[A-Za-z0-9._-]+$ ]] || { printf 'CRIVO VERIFY PREFLIGHT FAILED: manifesto invalido\n' >&2; exit 2; }
[[ "$source_dir" = /* ]] || source_dir="$crivo_root/$source_dir"
source_dir=$(realpath "$source_dir") || exit 2
[[ -f "$source_dir/CMakeLists.txt" ]] || { printf 'CRIVO VERIFY PREFLIGHT FAILED: fonte CMake invalida\n' >&2; exit 2; }

crivo_build=${CRIVO_BUILD_DIR:-build}
crivo_bin="$crivo_root/$crivo_build/crivo"
build_dir=${CRIVO_PROJECT_BUILD:-"$crivo_root/$crivo_build/targets/$project_id"}
evidence_root=${CRIVO_PROJECT_EVIDENCE:-"$crivo_root/.run/evidence/$project_id"}
record_db=${CRIVO_DB:-"$crivo_root/.run/crivo.db"}
jobs=${CRIVO_JOBS:-2}
mkdir -p "$build_dir" "$evidence_root"
build_dir=$(realpath "$build_dir")
run_dir="$evidence_root/$(date -u +%Y%m%dT%H%M%SZ)-$$"
sandbox="$run_dir/sandbox"
logs="$run_dir/logs"
mkdir -p "$sandbox" "$logs"
db_file="$sandbox/target.sqlite"
port=$(jq -er '.http.port' "$manifest") || exit 2
revision=$(git -C "$source_dir" rev-parse HEAD 2>/dev/null || printf unknown)
if git -C "$source_dir" diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then worktree=clean; else worktree=dirty; fi
server_pid=
cleanup() { if [[ -n "${server_pid:-}" ]]; then kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true; fi; }
trap cleanup EXIT

expand_arg() {
  local value=$1
  value=${value//\{source\}/$source_dir}; value=${value//\{build\}/$build_dir}
  value=${value//\{sandbox\}/$sandbox}; value=${value//\{db\}/$db_file}; value=${value//\{port\}/$port}
  printf '%s' "$value"
}
operation_args() {
  local key=$1 item
  OP_ARGS=()
  while IFS= read -r item; do OP_ARGS+=("$(expand_arg "$item")"); done < <(jq -er ".operations.${key}[]" "$manifest")
  [[ ${#OP_ARGS[@]} -gt 0 ]] || return 2
  local executable
  executable=$(realpath "${OP_ARGS[0]}") || return 2
  [[ "$executable" == "$build_dir"/* ]] || return 2
  OP_ARGS[0]=$executable
}
run_operation() { operation_args "$1" && "${OP_ARGS[@]}"; }

mapfile -t configure_args < <(jq -er '.build.configure_args[]' "$manifest")
cmake -S "$source_dir" -B "$build_dir" "${configure_args[@]}" || exit 2
cmake --build "$build_dir" -j"$jobs" || exit 2

started=$(date -u +%Y-%m-%dT%H:%M:%SZ)
start_epoch=$(date +%s%N)
total=0 passed=0 failed=0
results="$run_dir/results.tsv"
: > "$results"
run_check() {
  local id=$1; shift
  total=$((total+1))
  if "$@" > "$logs/$id.log" 2>&1; then
    passed=$((passed+1)); printf '%s\tPASS\n' "$id" >> "$results"; printf 'PASS  %s\n' "$id"
  else
    failed=$((failed+1)); printf '%s\tFAIL\n' "$id" >> "$results"; printf 'FAIL  %s\n' "$id"
  fi
}
check_sql_value() { [[ "$(sqlite3 "$db_file" "$1")" == "$2" ]]; }
check_sql_empty() { [[ -z "$(sqlite3 "$db_file" "$1")" ]]; }
check_http_health() { [[ "$(curl -sS -o "$logs/http-health.body" -w '%{http_code}' "http://127.0.0.1:$port/v1/health")" == 200 ]]; }
check_http_readonly() { [[ "$(curl -sS -o "$logs/http-post.body" -w '%{http_code}' -X POST "http://127.0.0.1:$port/v1/health")" == 405 ]]; }
check_http_nosniff() { curl -sSI "http://127.0.0.1:$port/v1/health" | tr -d '\r' | grep -qi '^X-Content-Type-Options: nosniff$'; }

run_check target.initialize.first run_operation initialize
run_check target.initialize.idempotent run_operation initialize
run_check target.seed run_operation seed
run_check crivo.sqlite.integrity check_sql_value 'PRAGMA integrity_check;' ok
run_check crivo.sqlite.wal check_sql_value 'PRAGMA journal_mode;' wal
run_check crivo.sqlite.foreign_keys check_sql_empty 'PRAGMA foreign_key_check;'
run_check target.validate run_operation validate

if operation_args serve; then
  "${OP_ARGS[@]}" > "$logs/server.log" 2>&1 & server_pid=$!
  ready=false
  for _ in $(seq 1 50); do if curl -fsS "http://127.0.0.1:$port/v1/health" >/dev/null 2>&1; then ready=true; break; fi; sleep 0.1; done
  if [[ "$ready" == true ]]; then
    run_check crivo.http.health check_http_health
    run_check crivo.http.readonly check_http_readonly
    run_check crivo.http.nosniff check_http_nosniff
  else
    run_check crivo.http.health false
    run_check crivo.http.readonly false
    run_check crivo.http.nosniff false
  fi
  cleanup; server_pid=
fi

ended=$(date -u +%Y-%m-%dT%H:%M:%SZ)
end_epoch=$(date +%s%N)
duration=$(awk -v s="$start_epoch" -v e="$end_epoch" 'BEGIN { printf "%.6f", (e-s)/1000000000 }')
{
  printf '<?xml version="1.0" encoding="UTF-8"?>\n<testsuite name="CRIVO external verification" tests="%d" failures="%d" skipped="0">\n' "$total" "$failed"
  while IFS=$'\t' read -r id status; do
    if [[ "$status" == PASS ]]; then printf '  <testcase name="%s"/>\n' "$id"; else printf '  <testcase name="%s"><failure message="verification failed"/></testcase>\n' "$id"; fi
  done < "$results"
  printf '</testsuite>\n'
} > "$run_dir/junit.xml"
digest=$(sha256sum "$run_dir/junit.xml" | cut -d' ' -f1)
evidence_id="evidence-$project_id-$started-${digest:0:12}"
jq -n --arg id "$evidence_id" --arg project "$project_id" --arg start "$started" --arg end "$ended" \
  --argjson duration "$duration" --argjson total "$total" --argjson passed "$passed" --argjson failed "$failed" --arg sha "$digest" \
  '{schema_version:"crivo.evidence/1.0.0",evidence_id:$id,project_id:$project,executor:{adapter:"crivo-native",version:"1.0.0"},timing:{start_utc:$start,end_utc:$end,duration_seconds:$duration},summary:{total:$total,passed:$passed,failed:$failed,skipped:0},artifacts:[{path:"junit.xml",format:"junit_xml",sha256:$sha}]}' \
  > "$run_dir/evidence.json"
jq -n --arg project "$project_id" --arg revision "$revision" --arg worktree "$worktree" \
  '{schema_version:"crivo.pilot-context/1.0.0",project_id:$project,mode:"shadow",source_revision:$revision,source_worktree:$worktree,discovery:null,evidence:"evidence.json"}' \
  > "$run_dir/pilot-context.json"
"$crivo_bin" external-record --db "$record_db" --evidence-dir "$run_dir" || exit 2
printf 'CRIVO NATIVE VERIFY: total=%d passed=%d failed=%d evidence=%s\n' "$total" "$passed" "$failed" "$run_dir"
(( failed == 0 ))
