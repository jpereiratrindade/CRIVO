#!/usr/bin/env bash
set -euo pipefail

crivo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest=${1:-}
if [[ -z "$manifest" ]]; then
  printf 'Uso: project-pilot.sh <manifesto.json>\n' >&2
  exit 2
fi
[[ "$manifest" = /* ]] || manifest="$crivo_root/$manifest"
if [[ ! -f "$manifest" ]]; then
  printf 'CRIVO PROJECT PREFLIGHT FAILED: manifesto ausente: %s\n' "$manifest" >&2
  exit 2
fi
command -v jq >/dev/null || { printf 'CRIVO PROJECT PREFLIGHT FAILED: jq ausente\n' >&2; exit 2; }

schema=$(jq -er '.schema_version' "$manifest")
project_id=$(jq -er '.project_id' "$manifest")
source_dir=$(jq -er '.source_dir' "$manifest")
adapter=$(jq -er '.adapter' "$manifest")
governance=$(jq -er '.governance_mode' "$manifest")
timeout=$(jq -er '.timeout_seconds' "$manifest")
if [[ "$schema" != crivo.project-integration/1.0.0 || "$adapter" != ctest || "$governance" != shadow ]]; then
  printf 'CRIVO PROJECT PREFLIGHT FAILED: contrato, adaptador ou governanca invalidos\n' >&2
  exit 2
fi
if [[ ! "$project_id" =~ ^[A-Za-z0-9._-]+$ ]]; then
  printf 'CRIVO PROJECT PREFLIGHT FAILED: project_id invalido\n' >&2
  exit 2
fi
if [[ ! "$timeout" =~ ^[1-9][0-9]*$ ]] || (( timeout > 86400 )); then
  printf 'CRIVO PROJECT PREFLIGHT FAILED: timeout deve estar entre 1 e 86400\n' >&2
  exit 2
fi

crivo_build=${CRIVO_BUILD_DIR:-build}
crivo_bin=${CRIVO_BIN:-"$crivo_root/$crivo_build/crivo"}
[[ "$source_dir" = /* ]] || source_dir="$crivo_root/$source_dir"
source_dir=$(realpath "$source_dir")
build_dir=${CRIVO_PROJECT_BUILD:-"$crivo_root/$crivo_build/pilots/$project_id"}
evidence_root=${CRIVO_PROJECT_EVIDENCE:-"$crivo_root/.run/evidence/$project_id"}
record_db=${CRIVO_DB:-"$crivo_root/.run/crivo.db"}
jobs=${CRIVO_JOBS:-2}

[[ -f "$source_dir/CMakeLists.txt" ]] || { printf 'CRIVO PROJECT PREFLIGHT FAILED: fonte CMake invalida\n' >&2; exit 2; }
[[ -x "$crivo_bin" ]] || { printf 'CRIVO PROJECT PREFLIGHT FAILED: compile CRIVO primeiro\n' >&2; exit 2; }
mkdir -p "$build_dir" "$evidence_root"
build_dir=$(realpath "$build_dir")
evidence_root=$(realpath "$evidence_root")
run_dir="$evidence_root/$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p "$run_dir"

record_status() {
  code=$?
  status=PASS
  if (( code != 0 )); then status=FAILED; fi
  jq -n --arg project "$project_id" --arg mode "$governance" --arg status "$status" --argjson code "$code" \
    '{schema_version:"crivo.pilot-run-status/1.0.0",project_id:$project,mode:$mode,status:$status,exit_code:$code}' \
    > "$run_dir/run-status.json"
}
trap record_status EXIT

revision=$(git -C "$source_dir" rev-parse HEAD 2>/dev/null || printf unknown)
if git -C "$source_dir" diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then worktree_state=clean; else worktree_state=dirty; fi
printf 'CRIVO PROJECT SHADOW PREFLIGHT PASS\n  project=%s\n  source=%s\n  revision=%s\n  worktree=%s\n  evidence=%s\n' \
  "$project_id" "$source_dir" "$revision" "$worktree_state" "$run_dir"
jq -n --arg project "$project_id" --arg revision "$revision" --arg worktree "$worktree_state" \
  '{schema_version:"crivo.pilot-context/1.0.0",project_id:$project,mode:"shadow",source_revision:$revision,source_worktree:$worktree,discovery:"discovery.json",evidence:"evidence.json"}' \
  > "$run_dir/pilot-context.json"

mapfile -t cmake_args < <(jq -er '.configure_args[]' "$manifest")
cmake -S "$source_dir" -B "$build_dir" "${cmake_args[@]}"
cmake --build "$build_dir" -j"$jobs"
"$crivo_bin" adapter ctest discover --build "$build_dir" --workspace-root "$build_dir" \
  --project "$project_id" --timeout "$timeout" > "$run_dir/discovery.json"
"$crivo_bin" adapter ctest run --build "$build_dir" --workspace-root "$build_dir" \
  --evidence-dir "$run_dir" --project "$project_id" --timeout "$timeout" \
  --record-db "$record_db" --source-revision "$revision" --source-worktree "$worktree_state"
printf 'CRIVO PROJECT SHADOW PASS: %s\n' "$run_dir"
