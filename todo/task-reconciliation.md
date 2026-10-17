# Keep one owner for task review

ID: task-reconciliation
Status: backlog
Priority: P2
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** duplicate. **Follow-up owner:** [R04](roadmap-r04.md).

The original 85 imported IDs remain intact. This review supplies a current disposition for all 110 records; that is separate from proving all product outcomes.

### Next action and acceptance

foundation-task-triage owns review maintenance. Preserve original evidence in folded history and the current decisions above it. Future repairs must cite the original IDs, not create competing copies of their scope.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [roadmap/task-dispositions.json](../roadmap/task-dispositions.json)
- [Tools/update-todo.py](../Tools/update-todo.py)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Use ROADMAP.md and the ordered queue. Obsolete branch counts, parallel workstream instructions and old release steps are not executable instructions.

## Scope and acceptance

All 85 original open task IDs are retained. Review duplicated or stale claims one task at a time; map acceptance cases and preserve deferred scopes. Do not recreate the external board or mark inherited done claims verified.

## Verification

Pending.

Implementation entry point: foundation-task-triage. Keep this imported follow-up as scope evidence; do not execute a duplicate reconciliation workstream.

</details>
