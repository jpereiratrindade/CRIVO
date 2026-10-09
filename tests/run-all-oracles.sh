#!/usr/bin/env bash
set -euo pipefail
bin=$1
catalog=$2
fixture=$3
evidence=$4
db=$fixture/app.db
rm -f "$db" "$db-wal" "$db-shm"
"$bin" init --db "$db" --file "$catalog/tests.json" >/dev/null
out=$("$bin" check --target "$fixture" --profile complete-international-benchmark --catalog "$catalog" --evidence-dir "$evidence" --db "$evidence/crivo-memory.db")
grep -q 'CRIVO CHECK PASS: total=11, passed=11, failed=0, skipped=0, blocked=0' <<<"$out"
