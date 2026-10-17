# T23 (P3, medium) — Table row preview and read-back

ID: ba6df3c8-12e8-4678-a020-a89881e3e0d7
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md

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
