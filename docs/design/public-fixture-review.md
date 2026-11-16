# Public fixture inventory

Readiness: ready. The integration owner approved this bounded investigation
on 2026-11-16. Assignment: `nov16-public-fixture-inventory` under R04.
This inventory supports the requested public development preview. It does
not certify release acceptance or grant publication permission.

## Scope and source

Inspect the exact 15 tracked Unreal packages in TestProject/Content at
source commit `0df188fc56156e8b1bdcdb2d48885f4ff60cc573`. Record their relative
paths, Git blobs and SHA-256 hashes in Tests/fixture-privacy-inventory.json.
Derive this allowlist from Git and the files; no private scratch dependency.
Do not regenerate, modify or delete fixtures. Historical package versions
require a separate disposition; a current-tree result cannot clear history.

## Implementation contract

Add Tools/test-fixture-privacy-inventory.py using existing supervised engine
reader conventions. Require execution_runtime.require_context before MCP
access and the existing owned TestProject health/module identity checks.
Use the pinned workspace roots. Reject foreign contexts, linked fixture
paths, mismatched bytes, incomplete file sets and missing ownership evidence.
Do not add a direct endpoint override or bypass the supervisor.

Use advertised reader schemas for AssetDescribe and AssetCost, then the
applicable BlueprintLoad/GraphLoad, WidgetTree or MaterialLoad/MaterialTrace
readers. Read only exact allowlisted packages and their enumerated graphs.
Do not load dependency packages or maps to widen coverage. Record explicit
coverage gaps for enums, World actors, import metadata, nested content or
other fields that the existing readers cannot inspect completely.

No save, compile, mutation, script execution, PIE, fixture generation or
default capture. Verify file hashes and available dirty-state evidence
before and after. A restored write is not a successful read-only audit.
Unknown dirty state must be recorded as unknown, not clean.

Write per-package evidence under the supervisor-owned ignored run output:
source/engine revisions, available module identity, package identity,
reader scopes, completion/refusal, hashes, dirty evidence and gaps. Keep
raw responses local and ignored. Terminal output and tracked files contain
only reviewed locators, counts and bounded reason codes, not asset content.
Report partial coverage honestly. Timeout, fixture mismatch, side effects
or missing expected objects must fail their corresponding claims.

## Worker boundary and verification

The worker may change only the two new files in the approved base manifest.
Run minimal offline syntax and data checks. Do not start the engine or MCP.
Return source evidence, candidate SHA, claim ID and the proposed catalog row.
The integrator reviews code before executing it, registers the engine-tier
suite with timeout_seconds 900 and acceptance not_mapped, then schedules one
focused supervised run. No general infrastructure refactor is authorized.
Report a missing reader or supervision contract rather than inventing one.

## Completion

Implementation is reviewable when the exact allowlist and bounded reader
are submitted. Publication remains blocked on current and historical
content disposition; this tool alone does not establish redistribution rights.
