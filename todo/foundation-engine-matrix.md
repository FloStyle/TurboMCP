# Track installed-engine verification through R16 and R12

ID: foundation-engine-matrix
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** duplicate. **Follow-up owner:** [R16](roadmap-r16.md), [R12](roadmap-r12.md).

The source-engine incremental plugin build is verified. Installed 5.8.3 availability and ABI compatibility are not. This foundation task duplicates the newer two-stage plan.

### Next action and acceptance

R16 owns dependency/build-boundary investigation; R12 owns actual installed-distribution packaging and clean consumer validation. Do not create a third packaging workstream or treat installed 5.6 as equivalent.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/TurboMCP.uplugin](../Plugins/TurboMCP/TurboMCP.uplugin)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs](../Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Inspect exact installed 5.8.3 dependencies and toolchain when available; keep source 5.8.3 pinned. Read docs/installed-engine.md. Do not claim 5.6 is a substitute.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Document descriptor/header/library availability; BuildPlugin result and clean-project install for the exact target. If required engine distribution is unavailable, report blocked instead of support.

## Verification
Run python Tools/doctor.py for the baseline. Record exact RunUAT command, engine Build.version, package hash and clean-project runtime logs for the installed candidate.

## Handoff
Record results and remaining blockers before changing status.

## Roadmap disposition

Workstream: installed-engine. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

</details>
