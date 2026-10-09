#!/usr/bin/env bash
set -euo pipefail

crivo_bin=$1
database=$2
worktree_a=$3
worktree_b=$4

extract_target_id() {
  "$crivo_bin" context --target "$1" --db "$database" --json |
    grep -o '"target_id":"[^"]*"' |
    cut -d '"' -f 4
}

id_a=$(extract_target_id "$worktree_a")
id_b=$(extract_target_id "$worktree_b")
[[ -n "$id_a" && "$id_a" == "$id_b" ]]
