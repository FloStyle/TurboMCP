# Maintain the source-backed task review

ID: foundation-task-triage
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** plan. **Follow-up owner:** [R04](roadmap-r04.md).

This pass reviews all task records, separating current source findings from missing runtime evidence. Imported status does not authorize implementation.

### Next action and acceptance

Use docs/audit/task-review.md and each reviewed action. Keep confirmed repair specifications separate from investigation outcomes; update evidence when implementation changes source. Do not repeat a blanket backlog rewrite or claim all acceptance cases passed.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [roadmap/task-dispositions.json](../roadmap/task-dispositions.json)
- [Tests/acceptance.json](../Tests/acceptance.json)
- [docs/architecture.md](../docs/architecture.md)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Inspect imported P0/P1 findings, docs/architecture.md, Tests/acceptance.json and associated source/tests. Preserve every imported ID. Group duplicates under a primary task; explain resolved or withdrawn findings. Do not implement unrelated product changes.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Each reviewed task has a current reproduction or explicit source finding, exact scope, dependencies, acceptance and commands. Only confirmed bounded tasks become ready; withdrawn findings cite evidence.

## Verification
Run python Tools/update-todo.py and python Tools/validate.py. Review source and focused tests for each selected task; report runtime checks that remain blocked.

## Handoff
Record results and remaining blockers before changing status.

## Roadmap disposition

Use ROADMAP.md and the ordered queue. Obsolete branch counts, parallel workstream instructions and old release steps are not executable instructions.

</details>
