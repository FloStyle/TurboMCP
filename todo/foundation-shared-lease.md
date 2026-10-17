# Retain shared-ownership completion evidence

ID: foundation-shared-lease
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** superseded. **Follow-up owner:** [R02](roadmap-r02.md).

Shared kernel resource locks now supplement per-checkout leases. R02 carries completion evidence. The old statement that ownership is only check-then-write is stale for supervised entry points.

### Next action and acceptance

Retain this as the original requirement. Regress contention, owner death and foreign-process preservation through R02 tests; no second lock manager. Port 8000 still serializes engine sessions.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tools/execution_runtime.py](../Tools/execution_runtime.py)
- [Tools/execute-tests.py](../Tools/execute-tests.py)
- [Tools/test-execution-runtime.py](../Tools/test-execution-runtime.py)

Starting checks for the assigned follow-up:

- `python Tools/test-execution-runtime.py`
- `python Tools/test-process-cleanup.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Audit Tools/ownership.ps1, build.ps1, start-editor.ps1 and run-suites.ps1. Existing leases are per checkout and acquisition is check-then-write. Design a shared atomic engine/port resource lock without killing foreign processes.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Two simultaneous contenders from different worktrees cannot acquire one resource. A crashed owner has an explicit recoverable state. Nested same-owner operations work. Foreign editor and fixture access is refused.

## Verification
Add concurrent-process tests for contention, stale state and cleanup. Run python Tools/validate.py; then prove serialized build/editor behavior with two disposable worktrees.

## Handoff
Record results and remaining blockers before changing status.

## Roadmap disposition

Workstream: engine-ownership. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

</details>
