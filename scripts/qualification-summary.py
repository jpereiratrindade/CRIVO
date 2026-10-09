#!/usr/bin/env python3
import json
import sys

(
    _, project_id, target, revision, worktree, profile, adapter_rc, check_rc,
    adapter_path, check_path, catalog_path, output_path,
) = sys.argv

with open(adapter_path, encoding="utf-8") as stream:
    adapter = json.load(stream)
with open(check_path, encoding="utf-8") as stream:
    check = json.load(stream)

status = check.get("status", "BLOCKED")
# A suíte do alvo é diagnóstico opcional; nunca governa o veredito CRIVO.
results = check.get("results", [])
with open(f"{catalog_path}/profiles/{profile}.json", encoding="utf-8") as stream:
    profile_document = json.load(stream)
contracts = []
for reference in profile_document.get("selection", {}).get("include_specs", []):
    with open(f"{catalog_path}/specifications/{reference}.json", encoding="utf-8") as stream:
        specification = json.load(stream)
    implementation = specification.get("implementation_refs", [None])[0]
    contracts.append({
        "specification": reference,
        "stimulus": {
            "purpose": specification.get("description", ""),
            "preconditions": specification.get("preconditions", []),
            "implementation": implementation,
        },
        "expected_behavior": specification.get("oracle", {}),
    })
findings = {
    "failed": sum(item.get("status") == "FAIL" for item in results) if results else check.get("failed", 0),
    "blocked": sum(item.get("status") == "BLOCKED" for item in results) if results else check.get("blocked", 0),
    "not_applicable": sum(item.get("status") == "NOT_APPLICABLE" for item in results),
}
if status == "PASS":
    learning_status = "REVIEW_REQUIRED"
    next_action = "revisar evidencias antes de promover aprendizado"
elif status in ("BLOCKED", "NO_TESTS"):
    learning_status = "INSUFFICIENT_EVIDENCE"
    next_action = "fornecer capacidades ou artefatos ausentes e repetir a qualificacao"
else:
    learning_status = "INSUFFICIENT_EVIDENCE"
    next_action = "corrigir falhas e repetir a qualificacao"

summary = {
    "schema_version": "crivo.qualification-summary/1.0.0",
    "project_id": project_id,
    "target_path": target,
    "source_revision": revision,
    "source_worktree": worktree,
    "profile": profile,
    "status": status,
    "adapter_exit_code": int(adapter_rc),
    "check_exit_code": int(check_rc),
    "adapter_result": adapter,
    "target_tests_role": "OPTIONAL_DIAGNOSTIC",
    "check_result": check,
    "test_contracts": contracts,
    "findings": findings,
    "learning_status": learning_status,
    "next_action": next_action,
}
with open(output_path, "w", encoding="utf-8") as stream:
    json.dump(summary, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
