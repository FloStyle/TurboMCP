# Make table previews and application follow caller order

ID: ba6df3c8-12e8-4678-a020-a89881e3e0d7
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed
Validation: unbuilt_untested

## Current review

2026-10-23 rollback follow-up: failure recovery now compares row identities, exported values, row-struct identity and package dirty state against a pre-mutation baseline. Undo success alone no longer claims complete restoration. External callback effects are explicitly unverified. Implemented slice, unbuilt_untested. Remaining: mixed set/remove caller-order support and guaranteed restoration on failure; this issue stays open. No tests/build/editor runs.

2026-10-23 b872c49: preview now stages the same final merged row text as Apply and imports it into initialized scratch rows; invalid values become refusals and all advertised effects are cleared. Done preview-validation slice, unbuilt_untested. Remaining caller-order issue: mixed set/remove semantics remain refused; sequential mixed-operation support is not implemented. No tests/build/editor runs.

2026-10-23 partial repair 5936f54, unbuilt_untested: refused batches expose no planned additions/removals/changes; duplicate aliases resolve to canonical field identity; repeated field previews use the preceding requested value; row writes follow first-request order with case-insensitive row identity. Remaining: mixed set/remove or remove/set requests still refuse, not sequentially supported; final-state staging/rollback and preview value validation remain to reconcile. Do not close the full caller-order issue. No tests/build/editor runs.

**Decision:** confirmed. **Follow-up owner:** [R08](roadmap-r08.md).

Plan adds changes even after recording an unknown-field refusal. Apply collects edits, removes rows, then writes rows; that grouping can violate set-then-remove order. WholeRowText read-back exists, so absence of any read-back is not the defect.

### Next action and acceptance

Test invalid field plus valid field, set/remove and remove/set on one row, repeated partial sets and default-valued fields. Preview must not present refused lines as accepted; final data must match caller order with atomic restoration on failure.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L409)
- [Tools/test-datatable-rows.py](../Tools/test-datatable-rows.py)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-datatable-rows --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: data-widget. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:279-284`, group P3. SOURCE IDs: DT-02, DT-03 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***
>
>     DONE WHEN
>         - *** A REFUSED LINE IS NEVER LISTED AS A PLANNED CHANGE.***
>         - *** the read-back SHOWS EVERY FIELD, OR SAYS THAT DEFAULT VALUES ARE LEFT OUT.***
>
>     *** AND THE FIRST CLAUSE IS THE `P8` SHAPE IN ITS PUREST FORM: A PREVIEW THAT LISTS A LINE IT WILL REFUSE IS A
>     PREVIEW THAT LIES, AND THE CALLER WHO APPROVED THE PLAN HAS APPROVED SOMETHING THAT WILL NOT HAPPEN. THIS SESSION
>     MEASURED THE SAME DEFECT IN `test-struct` - a save that reported success while writing nothing - AND IN `T16`'s
>     "0 overlaps" FROM A DEFAULT SQUARE. ***
>     *** AND THE SECOND CLAUSE IS A CHOICE WITH TWO HONEST ANSWERS, AND THE CARD ALLOWS EITHER: SHOW EVERY FIELD, OR SAY
>     THAT DEFAULTS ARE OMITTED. WHAT IT FORBIDS IS THE THIRD - OMITTING THEM SILENTLY, WHICH MAKES A READ-BACK THAT
>     LOOKS SHORT INDISTINGUISHABLE FROM ONE THAT LOST DATA. `TurboMCPDataTableRows.cpp:574-583` ALREADY PRINTS "MISSING
>     AFTER WRITE" for a row that did not survive, WHICH IS THE SAME ARGUMENT ONE LEVEL DOWN. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
