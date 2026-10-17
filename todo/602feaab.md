# Defer a session change ledger pending semantics

ID: 602feaab-6a94-4004-9c1b-ada71d983f53
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md).

Call logs record calls and timeline merges; they are not an authoritative semantic change ledger. A startup snapshot would miss create-then-undo history.

### Next action and acceptance

Choose net state difference versus chronological edits, retention, privacy and external edits before designing anything. Compare native session tooling. Do not add instrumentation to every writer under this vague card.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallTimeline.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallTimeline.cpp)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-call-log --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Do not expand this capability before a pinned native-tool comparison demonstrates a concrete missing workflow and an accepted design. Existing safety defects remain eligible for fixes.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:191-194`, group P1. SOURCE IDs: FT-14 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***
>
>     DONE WHEN
>         *** one read lists EVERY NODE, VARIABLE AND FUNCTION that TurboMCP created, unplugged or deleted IN THE CURRENT
>         EDITOR SESSION, BY ASSET AND BY GRAPH. ***
>
>     *** AND THE CARD CARRIES A DESIGN QUESTION THE LIST DOES NOT ANSWER, WHICH IS WHY IT IS WORTH MEASURING FIRST:
>     NOTHING IN THE PLUGIN KEEPS A SESSION LEDGER TODAY. `TurboMCPCallLog` RECORDS CALLS, NOT CHANGES - it has the tool
>     name and the arguments but it does not know WHICH NODES MOVED, and it is cleared when the editor exits. ***
>     *** SO THE CHOICE IS BETWEEN TWO HONEST SHAPES: READ THE ASSETS AND DIFF THEM AGAINST A SNAPSHOT TAKEN AT MODULE
>     STARTUP (which is measurable today and needs no new bookkeeping), OR HAVE EVERY WRITER APPEND TO A LEDGER (which is
>     `T02`'S 68-CALL-SITE PROBLEM AGAIN, AND `T02` ALREADY MEASURED THAT PRICE AT 59 DISTINCT FIRST-STATEMENT SHAPES). ***
>     *** THE SNAPSHOT SHAPE HAS A KNOWN LIMIT WORTH STATING UP FRONT: IT REPORTS WHAT IS DIFFERENT NOW, SO AN EDIT THAT
>     WAS MADE AND THEN UNDONE IN THE SAME SESSION IS INVISIBLE TO IT - WHICH MAY OR MAY NOT BE WHAT "CHANGED" MEANS TO
>     THE CALLER. THAT IS THE FIRST THING TO SETTLE. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
