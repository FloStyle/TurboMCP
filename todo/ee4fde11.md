# Retire the parallel-fix batch verifier

ID: ee4fde11-5d9a-43f2-a017-ad6993605e30
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** superseded. **Follow-up owner:** [R11](roadmap-r11.md).

The ten parallel workers and shared-checkout assumptions are obsolete. Current engine execution is serialized and tied to a candidate digest.

### Next action and acceptance

R11 owns combined regression evidence after reviewed integration. Preserve named failure classes in acceptance, but do not wait for nonexistent workers or start the historical parallel run.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [ROADMAP.md](../ROADMAP.md)
- [Tools/execute-tests.py](../Tools/execute-tests.py)
- [Tests/acceptance.json](../Tests/acceptance.json)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Use ROADMAP.md and the ordered queue. Obsolete branch counts, parallel workstream instructions and old release steps are not executable instructions.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> Run only after the ten parallel fix cards have settled. Scope: X-1; F3/F4/X-3; DT-06/SV-07; X-20/TD-04/CP-05; GC-02/GC-03/GC-04; SF-04/VA-02/FA-04; GC-06/GC-07; X-18/X-21/X-22; MI-02/FT-27; FT-30/FT-31/TD-05. First reconcile the shared worktree and list every commit or conflict. Then build once from the final worktree and run the selected regression suites for every changed area. Record each card's pass, fail, blocked or not-covered result. Do not close source cards solely because this batch starts.

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
