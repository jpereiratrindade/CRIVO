#!/usr/bin/env bash
set -euo pipefail
root=$1
bin=$2
work=$3
rm -rf "$work"
mkdir -p "$work"

cat >"$work/evidence.json" <<'JSON'
{"schema_version":"crivo.evidence/1.0.0","evidence_id":"req-integrity","project_id":"promotion-fixture","status":"PASS"}
JSON
evidence_sha=$(sha256sum "$work/evidence.json" | cut -d' ' -f1)
cat >"$work/qualification-summary.json" <<JSON
{"schema_version":"crivo.qualification-summary/1.0.0","project_id":"promotion-fixture","target_path":"$work","source_revision":"0123456789abcdef","source_worktree":"clean","profile":"integrity-test","status":"PASS","adapter_exit_code":0,"check_exit_code":0,"adapter_result":null,"target_tests_role":"OPTIONAL_DIAGNOSTIC","check_result":{"request_id":"req-integrity","evidence_path":"$work/evidence.json","evidence_sha256":"$evidence_sha"},"test_contracts":[],"findings":{"failed":0,"blocked":0,"not_applicable":0},"learning_status":"REVIEW_REQUIRED","next_action":"review"}
JSON
summary_sha=$(sha256sum "$work/qualification-summary.json" | cut -d' ' -f1)
"$bin" memory promote --file "$work/qualification-summary.json" --db "$work/memory.db" >/dev/null
test "$evidence_sha" = "$(sha256sum "$work/evidence.json" | cut -d' ' -f1)"
test "$summary_sha" = "$(sha256sum "$work/qualification-summary.json" | cut -d' ' -f1)"
"$bin" memory query --db "$work/memory.db" --project promotion-fixture | python3 -c 'import json,sys; x=json.load(sys.stdin); assert len(x)==1; assert x[0]["source_revision"]=="0123456789abcdef"; assert x[0]["evidence_sha256"]'

printf '\n' >>"$work/evidence.json"
if "$bin" memory promote --file "$work/qualification-summary.json" --db "$work/tampered.db" >/dev/null 2>&1; then
  echo "tampered evidence was promoted" >&2
  exit 1
fi

python3 - "$root/schemas/qualification-summary/1.0.0.schema.json" <<'PY'
import json, sys
from jsonschema import Draft202012Validator
schema=json.load(open(sys.argv[1], encoding="utf-8"))
assert "PARTIAL" in schema["properties"]["status"]["enum"]
assert "PROMOTED" in schema["properties"]["learning_status"]["enum"]
assert schema["properties"]["promoted_experience_id"]["minLength"] == 1
base = {
    "schema_version": "crivo.qualification-summary/1.0.0",
    "project_id": "schema-fixture", "target_path": ".",
    "source_revision": "abc", "source_worktree": "clean", "profile": "test",
    "adapter_exit_code": 0, "check_exit_code": 0, "adapter_result": None,
    "target_tests_role": "OPTIONAL_DIAGNOSTIC", "check_result": {},
    "test_contracts": [], "findings": {"failed": 0, "blocked": 0, "not_applicable": 0},
    "next_action": "review",
}
validator = Draft202012Validator(schema)
for status in ("PASS", "PARTIAL", "FAIL", "BLOCKED", "NO_TESTS"):
    for learning in ("REVIEW_REQUIRED", "INSUFFICIENT_EVIDENCE"):
        validator.validate({**base, "status": status, "learning_status": learning})
validator.validate({**base, "status": "PASS", "learning_status": "PROMOTED",
                    "promoted_experience_id": "exp-schema-fixture"})
assert list(validator.iter_errors({**base, "status": "PASS", "learning_status": "PROMOTED"}))
PY
