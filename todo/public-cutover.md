# Require accepted evidence before fresh-history publication

ID: public-cutover
Status: backlog
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** plan. **Follow-up owner:** [R15](roadmap-r15.md).

The public source artifact is the entire tracked tree, not a hand-selected plugin-only export. release/status remains not_ready. Historical allowlist wording is superseded by the tracked-tree policy.

### Next action and acceptance

This task tracks the stable release and fresh-root route. Require R11 acceptance, R15 dry-run controls, English and privacy review including binary assets, the exact export manifest and explicit owner instruction. The optional development-source cutover has its own [publication procedure](../docs/public-preparation.md). Keep unsanitized private history out of public refs.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tools/export-public.py](../Tools/export-public.py)
- [Tools/release-check.py](../Tools/release-check.py)
- [release/export-policy.json](../release/export-policy.json)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: release. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Keep release/status.json not_ready until required acceptance passes. Review an allowlisted export, isolate publishing credentials, preserve old repository metadata, and obtain owner publication instruction before pushing the public snapshot or replacing repository names.

## Verification

Pending.

</details>
