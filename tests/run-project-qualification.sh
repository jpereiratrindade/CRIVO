#!/usr/bin/env bash
set -euo pipefail
root=$1
fixture=$2
bin=$3

rm -rf "$root/.run/qualification/e12_all_oracles"
rm -f "$fixture/app.db" "$fixture/app.db-wal" "$fixture/app.db-shm"
"$bin" init --db "$fixture/app.db" --file "$root/catalog/tests.json" >/dev/null
set +e
CRIVO_BUILD_DIR=build "$root/scripts/project-qualify.sh" "$fixture" complete-international-benchmark >/dev/null
qualify_rc=$?
set -e
test "$qualify_rc" -eq 3
summary=$(find "$root/.run/qualification/e12_all_oracles" -name qualification-summary.json -type f | sort | tail -1)
test -n "$summary"
python3 - "$summary" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    value = json.load(stream)
assert value["status"] == "BLOCKED"
assert value["learning_status"] == "INSUFFICIENT_EVIDENCE"
assert value["adapter_result"] is None
assert value["target_tests_role"] == "OPTIONAL_DIAGNOSTIC"
assert value["findings"]["failed"] == 0
assert value["findings"]["blocked"] == 6
assert value["findings"]["not_applicable"] == 1
assert value["check_result"]["passed"] == 4
assert value["check_result"]["skipped"] == 1
assert len(value["check_result"]["results"]) == 11
assert value["next_action"]
assert len(value["test_contracts"]) == 11
assert all(item["stimulus"]["implementation"] for item in value["test_contracts"])
assert all(item["expected_behavior"]["description"] for item in value["test_contracts"])
PY
