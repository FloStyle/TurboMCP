# R12: Validate installed-engine packages on the exact target

ID: R12
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: investigation
Review: source-reviewed

## Problem
Verify dependency headers/libraries/manifests before proposing module extraction. Source 5.8.3 and installed 5.6 do not prove installed 5.8.3 support. Extract fixture-only dependencies only when production callers are accounted for; package against the exact target.

## Scope
Plugin descriptor/Build.cs, development commandlets, exact installed 5.8.3 distribution and clean consumer project.

## Dependencies
R16, R11

## Acceptance
- Dependency availability is recorded for the exact distribution.
- BuildPlugin output and Blueprint-only/C++ clean-consumer tests have binary hashes.
- Unavailable engine distribution is a blocker, never a support claim.

## Verification
- Inspect the installed target identity directly; Tools/doctor.py validates the source baseline and cannot certify a Launcher distribution.
- `Run the target distribution RunUAT BuildPlugin with explicit plugin and package paths`
- `Run clean-consumer discovery and mutation/refusal checks`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [foundation-engine-matrix](foundation-engine-matrix.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R12](roadmap-r12.md).

Source 5.8.3 plugin compilation is established; installed 5.8.3 package availability is not. Module extraction remains a proposal, not a proven prerequisite.

### Next action and acceptance

After R16 and baseline acceptance, inspect exact target distribution then package and test Blueprint-only and C++ consumers. If unavailable, record blocked; no support claim from source build or installed 5.6.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/TurboMCP.uplugin](../Plugins/TurboMCP/TurboMCP.uplugin)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs](../Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs)
- [docs/installed-engine.md](../docs/installed-engine.md)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
