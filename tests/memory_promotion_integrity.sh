#!/usr/bin/env bash
set -euo pipefail
root=$1
bin=$2
work=$3
rm -rf "$work"
mkdir -p "$work"
mkdir -p "$work/target"
printf 'qualified source\n' >"$work/target/source.txt"
revision=unknown

printf '%s\n' "{\"schema_version\":\"crivo.evidence/1.0.0\",\"evidence_id\":\"req-integrity\",\"project_id\":\"promotion-fixture\",\"source_revision\":\"$revision\",\"source_worktree\":\"clean\",\"status\":\"PASS\",\"provenance\":{\"revision_system\":\"none\",\"trust_level\":\"locally_observed\",\"limitations\":\"no version-control revision was observable\"}}" >"$work/evidence.json"
evidence_sha=$(sha256sum "$work/evidence.json" | cut -d' ' -f1)
cat >"$work/qualification-summary.json" <<JSON
{"schema_version":"crivo.qualification-summary/1.0.0","project_id":"promotion-fixture","target_path":"$work/target","source_revision":"$revision","source_worktree":"clean","profile":"integrity-test","status":"PASS","adapter_exit_code":0,"check_exit_code":0,"adapter_result":null,"target_tests_role":"OPTIONAL_DIAGNOSTIC","check_result":{"request_id":"req-integrity","evidence_path":"$work/evidence.json","evidence_sha256":"$evidence_sha"},"test_contracts":[],"findings":{"failed":0,"blocked":0,"not_applicable":0},"learning_status":"REVIEW_REQUIRED","next_action":"review"}
JSON
summary_sha=$(sha256sum "$work/qualification-summary.json" | cut -d' ' -f1)
"$bin" memory promote --file "$work/qualification-summary.json" --db "$work/memory.db" >/dev/null
test "$evidence_sha" = "$(sha256sum "$work/evidence.json" | cut -d' ' -f1)"
test "$summary_sha" = "$(sha256sum "$work/qualification-summary.json" | cut -d' ' -f1)"
"$bin" memory query --db "$work/memory.db" --project promotion-fixture | python3 -c 'import json,sys; x=json.load(sys.stdin); assert len(x)==1; assert x[0]["source_revision"]=="unknown"; assert "unknown" in x[0]["limitations"]; assert x[0]["evidence_sha256"]'

printf '\n' >>"$work/evidence.json"
if "$bin" memory promote --file "$work/qualification-summary.json" --db "$work/tampered.db" >/dev/null 2>&1; then
  echo "tampered evidence was promoted" >&2
  exit 1
fi
sed -i '${/^$/d;}' "$work/evidence.json"
test "$evidence_sha" = "$(sha256sum "$work/evidence.json" | cut -d' ' -f1)"

# Every schema-invalid summary must fail at the promotion boundary itself.
cp "$work/qualification-summary.json" "$work/invalid.json"
python3 - "$work/invalid.json" <<'PY'
import json, sys
p=sys.argv[1]; value=json.load(open(p)); del value["target_path"]
json.dump(value, open(p, "w"))
PY
! "$bin" memory promote --file "$work/invalid.json" --db "$work/invalid.db" >/dev/null 2>&1

python3 - "$work/qualification-summary.json" "$work/invalid.json" <<'PY'
import json, sys
value=json.load(open(sys.argv[1])); value["adapter_exit_code"]="zero"
json.dump(value, open(sys.argv[2], "w"))
PY
! "$bin" memory promote --file "$work/invalid.json" --db "$work/invalid.db" >/dev/null 2>&1

python3 - "$work/qualification-summary.json" "$work/invalid.json" <<'PY'
import json, sys
value=json.load(open(sys.argv[1])); value["findings"]["failed"]=1
json.dump(value, open(sys.argv[2], "w"))
PY
! "$bin" memory promote --file "$work/invalid.json" --db "$work/invalid.db" >/dev/null 2>&1

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
