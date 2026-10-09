#!/usr/bin/env bash
set -euo pipefail
root=$1
fixture=$2
bin=$3

rm -rf "$root/.run/qualification/e12_all_oracles"
rm -f "$fixture/app.db" "$fixture/app.db-wal" "$fixture/app.db-shm"
"$bin" init --db "$fixture/app.db" --file "$root/catalog/tests.json" >/dev/null
CRIVO_BUILD_DIR=build "$root/scripts/project-qualify.sh" "$fixture" complete-international-benchmark >/dev/null
summary=$(find "$root/.run/qualification/e12_all_oracles" -name qualification-summary.json -type f | sort | tail -1)
test -n "$summary"
python3 - "$summary" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    value = json.load(stream)
assert value["status"] == "PASS"
assert value["learning_status"] == "REVIEW_REQUIRED"
assert value["adapter_result"] is None
assert value["target_tests_role"] == "OPTIONAL_DIAGNOSTIC"
assert value["findings"]["failed"] == 0
assert value["next_action"]
assert len(value["test_contracts"]) == 11
assert all(item["stimulus"]["implementation"] for item in value["test_contracts"])
assert all(item["expected_behavior"]["description"] for item in value["test_contracts"])
PY
