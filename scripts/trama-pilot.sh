#!/usr/bin/env bash
set -euo pipefail

crivo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
crivo_build=${CRIVO_BUILD_DIR:-build}
crivo_bin=${CRIVO_BIN:-"$crivo_root/$crivo_build/crivo"}
trama_source=${CRIVO_TRAMA_SOURCE:-"$crivo_root/../TRAMA"}
trama_build=${CRIVO_TRAMA_BUILD:-"$crivo_root/$crivo_build/pilots/TRAMA"}
evidence_root=${CRIVO_TRAMA_EVIDENCE:-"$crivo_root/.run/evidence/TRAMA"}
jobs=${CRIVO_JOBS:-2}
timeout=${CRIVO_TRAMA_TIMEOUT:-300}

if [[ ! -f "$trama_source/CMakeLists.txt" ]]; then
  printf 'CRIVO TRAMA PREFLIGHT FAILED: fonte invalida: %s\n' "$trama_source" >&2
  exit 2
fi
if [[ ! -x "$crivo_bin" ]]; then
  printf 'CRIVO TRAMA PREFLIGHT FAILED: execute ./crivo.sh build primeiro\n' >&2
  exit 2
fi
if [[ ! "$timeout" =~ ^[1-9][0-9]*$ ]] || (( timeout > 86400 )); then
  printf 'CRIVO TRAMA PREFLIGHT FAILED: CRIVO_TRAMA_TIMEOUT deve estar entre 1 e 86400\n' >&2
  exit 2
fi

trama_source=$(realpath "$trama_source")
mkdir -p "$trama_build" "$evidence_root"
trama_build=$(realpath "$trama_build")
evidence_root=$(realpath "$evidence_root")
run_stamp=$(date -u +%Y%m%dT%H%M%SZ)
run_dir="$evidence_root/$run_stamp-$$"
mkdir -p "$run_dir"

record_status() {
  code=$?
  status=PASS
  if (( code != 0 )); then status=FAILED; fi
  printf '{"schema_version":"crivo.pilot-run-status/1.0.0","project_id":"TRAMA","mode":"shadow","status":"%s","exit_code":%d}\n' \
    "$status" "$code" > "$run_dir/run-status.json"
}
trap record_status EXIT

revision=$(git -C "$trama_source" rev-parse HEAD 2>/dev/null || printf unknown)
if git -C "$trama_source" diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then
  worktree_state=clean
else
  worktree_state=dirty
fi

printf '%s\n' \
  "CRIVO TRAMA SHADOW PREFLIGHT PASS" \
  "  source=$trama_source" \
  "  revision=$revision" \
  "  worktree=$worktree_state" \
  "  build=$trama_build" \
  "  evidence=$run_dir"

printf '{"schema_version":"crivo.pilot-context/1.0.0","project_id":"TRAMA","mode":"shadow","source_revision":"%s","source_worktree":"%s","discovery":"discovery.json","evidence":"evidence.json"}\n' \
  "$revision" "$worktree_state" > "$run_dir/pilot-context.json"

cmake -S "$trama_source" -B "$trama_build" \
  -DCMAKE_BUILD_TYPE=Debug -DTRAMA_BUILD_TESTS=ON
cmake --build "$trama_build" -j"$jobs"

"$crivo_bin" adapter ctest discover \
  --build "$trama_build" --workspace-root "$trama_build" \
  --project TRAMA --timeout "$timeout" > "$run_dir/discovery.json"

"$crivo_bin" adapter ctest run \
  --build "$trama_build" --workspace-root "$trama_build" \
  --evidence-dir "$run_dir" --project TRAMA --timeout "$timeout"

printf 'CRIVO TRAMA SHADOW PASS: %s\n' "$run_dir"
