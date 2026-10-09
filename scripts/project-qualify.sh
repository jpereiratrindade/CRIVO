#!/usr/bin/env bash
set -uo pipefail

crivo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
target=${1:-}
profile=${2:-complete-international-benchmark}
[[ -n "$target" ]] || { printf 'Uso: project-qualify.sh <diretorio-alvo> [perfil]\n' >&2; exit 2; }
[[ "$target" = /* ]] || target=$(realpath "$target") || exit 2
target=$(realpath "$target") || exit 2
[[ -d "$target" ]] || { printf 'CRIVO QUALIFY PREFLIGHT FAILED: alvo nao e diretorio\n' >&2; exit 2; }

for tool in python3 git cmake; do command -v "$tool" >/dev/null || { printf 'CRIVO QUALIFY PREFLIGHT FAILED: %s ausente\n' "$tool" >&2; exit 2; }; done

crivo_bin="$crivo_root/${CRIVO_BUILD_DIR:-build}/crivo"
[[ -x "$crivo_bin" ]] || { printf 'CRIVO QUALIFY PREFLIGHT FAILED: binario CRIVO ausente\n' >&2; exit 2; }

project_id=$(basename "$target" | sed 's/[^A-Za-z0-9._-]/-/g')
revision=$(git -C "$target" rev-parse HEAD 2>/dev/null || printf unknown)
if git -C "$target" diff --quiet --ignore-submodules HEAD -- 2>/dev/null &&
   [[ -z "$(git -C "$target" ls-files --others --exclude-standard 2>/dev/null)" ]]; then
  worktree=clean
else
  worktree=dirty
fi

run_id=$(date -u +%Y%m%dT%H%M%SZ)-$$
run_root="$crivo_root/.run/qualification/$project_id/$run_id"
build_dir="$crivo_root/.run/qualification/builds/$project_id"
evidence_dir="$run_root/evidence"
db=${CRIVO_DB:-"$crivo_root/.run/crivo.db"}
mkdir -p "$run_root" "$evidence_dir"

adapter_json=null
adapter_rc=0
if [[ -f "$target/CMakeLists.txt" ]]; then
  cmake -S "$target" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug >"$run_root/configure.log" 2>&1 || adapter_rc=$?
  if [[ $adapter_rc -eq 0 ]]; then cmake --build "$build_dir" -j"${CRIVO_JOBS:-2}" >"$run_root/build.log" 2>&1 || adapter_rc=$?; fi
  if [[ $adapter_rc -eq 0 ]]; then
    adapter_json=$($crivo_bin adapter ctest run --build "$build_dir" --workspace-root "$crivo_root/.run/qualification/builds" \
      --evidence-dir "$evidence_dir/ctest" --project "$project_id" --record-db "$db" \
      --source-revision "$revision" --source-worktree "$worktree" 2>"$run_root/adapter.err") || adapter_rc=$?
  fi
fi

check_json=$($crivo_bin check --target "$target" --profile "$profile" --catalog "$crivo_root/catalog" \
  --evidence-dir "$evidence_dir/check" --db "$db" --json 2>"$run_root/check.err")
check_rc=$?
[[ -n "$check_json" ]] || check_json='{"status":"BLOCKED","error":"check produced no JSON"}'
printf '%s\n' "$adapter_json" >"$run_root/adapter.json"
printf '%s\n' "$check_json" >"$run_root/check.json"
python3 "$crivo_root/scripts/qualification-summary.py" \
  "$project_id" "$target" "$revision" "$worktree" "$profile" "$adapter_rc" "$check_rc" \
  "$run_root/adapter.json" "$run_root/check.json" "$crivo_root/catalog" "$run_root/qualification-summary.json"
cat "$run_root/qualification-summary.json"
overall=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["status"])' "$run_root/qualification-summary.json")

printf 'CRIVO qualification artifacts: %s\n' "$run_root" >&2
[[ "$overall" == PASS ]] && exit 0
[[ "$overall" == BLOCKED || "$overall" == NO_TESTS ]] && exit 3
exit 2
