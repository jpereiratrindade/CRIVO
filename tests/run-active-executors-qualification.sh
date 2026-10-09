#!/usr/bin/env bash
set -euo pipefail
root=$1
fixture=$2
bin=$3

rm -rf "$root/.run/qualification/e12_active_clean"
rm -rf "$fixture"
mkdir -p "$fixture"

# Target with sources and DB but ZERO pre-existing artifacts (no SARIF, no TSan report, no SBOM, no crivo-analysis.json, no evidence.json)
cat >"$fixture/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(active_clean_pilot CXX)
find_package(Threads REQUIRED)
add_executable(app main.cpp)
EOF

cat >"$fixture/main.cpp" <<'EOF'
#include <iostream>
int main() { return 0; }
EOF

cat >"$fixture/index.html" <<'EOF'
<!DOCTYPE html><html><body>Active Probe</body></html>
EOF

"$bin" init --db "$fixture/app.db" --file "$root/catalog/tests.json" >/dev/null

CRIVO_BUILD_DIR=build "$root/scripts/project-qualify.sh" "$fixture" complete-international-benchmark >/dev/null
summary=$(find "$root/.run/qualification/e12_active_clean" -name qualification-summary.json -type f | sort | tail -1)
test -n "$summary"
python3 - "$summary" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    value = json.load(stream)
assert value["status"] == "PASS", f"Expected PASS, got {value['status']}"
assert value["learning_status"] == "REVIEW_REQUIRED"
assert value["adapter_result"] is None
assert value["findings"]["failed"] == 0
assert value["findings"]["blocked"] == 0
assert value["check_result"]["passed"] == 11
assert len(value["test_contracts"]) == 11
PY
