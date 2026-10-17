# Defer curve authoring until a workflow gap is shown

ID: 4ec9e1cc-763d-46f6-aa31-181e8547e2fa
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md).

A curve fixture generator does not establish a shipped curve-editing contract. This proposal lacks operation-level acceptance.

### Next action and acceptance

Compare read/write curve workflows with pinned native capabilities. Specify key times, interpolation/tangents, duplicate-time policy, undo and persistence only if a missing workflow justifies a separate design. Do not implement from a table-row title.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPKitCommandlet.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPKitCommandlet.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h](../Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

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

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:368`, group P6 (a table row). ***
>
>     CONTENT, IN THE LIST'S OWN WORDS
>         *** "Curve assets (row 7.3)." ***
>     NOTE, IN THE LIST'S OWN WORDS
>         *** "THE TEST ASSET EXISTS." ***
>
>     *** AND THE NOTE IS A PRECONDITION ALREADY SATISFIED, WHICH IS WORTH VERIFYING RATHER THAN ASSUMING: the kit
>     commandlet builds curve fixtures, and this session read its save paths - `TurboMCPKitCommandlet.cpp:2010` IS "the
>     curve-float half of TA-17" AND `:2082` IS "the curve-table half of TA-17", BOTH WITH THEIR `SaveContainer` RESULTS
>     DISCARDED AT THE TIME AND BOTH NOW ON THE `C-P10` WORK (that card is in `backlog`). SO THE FIXTURE PATH EXISTS AND
>     THE QUESTION IS WHETHER THE ASSET IS ON DISK AND LOADED. ***
>     *** AND THE CARD IS THE THINNEST IN THE LIST BECAUSE THE LIST GIVES NO `done when` FOR IT - so THE FIRST ACTION IS
>     TO WRITE ONE, and the honest starting point is the project's own `E-`/`R-` pair convention: a reader that prints a
>     curve's key count with its denominator, and a writer that refuses by name. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
