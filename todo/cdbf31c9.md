# T36 (P6) — Source control next to the editor hooks

ID: cdbf31c9-caa6-4233-8314-1f960538ea8c
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md

## Roadmap disposition

Do not expand this capability before a pinned native-tool comparison demonstrates a concrete missing workflow and an accepted design. Existing safety defects remain eligible for fixes.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:365`, group P6 (a table row). ***
>
>     CONTENT, IN THE LIST'S OWN WORDS
>         *** "Source control next to the editor hooks." ***
>     NOTE, IN THE LIST'S OWN WORDS
>         *** "REPORT 'ALREADY CHECKED OUT BY THE HOOK' AS SUCCESS. THE SAVE REPORT SAYS WHO DID THE CHECKOUT." ***
>
>     *** AND THE NOTE IS A STATUS-CODE REQUIREMENT WITH A MEASURED PRECEDENT IN THIS TREE: `T31`'S CARD QUOTES THE
>     CURRENT CHECKOUT BLOCK, WHICH ALREADY SAYS "the checkout answer NEVER gates the write ... so a provider that cannot
>     take a lock cannot turn a working write into a failed one - which is the outcome `D-57` exists to prevent". ***
>     *** SO THE WORK IS TO EXTEND THAT DISTINCTION TO A SECOND, LEGITIMATE REASON A CHECKOUT DID NOT HAPPEN: SOMEONE
>     ELSE - A HOOK - ALREADY DID IT. TODAY THE TOOL WOULD REPORT A STATE, NOT A VERDICT, AND THE CALLER CANNOT TELL AN
>     ALREADY-CHECKED-OUT FILE FROM A FILE NOBODY COULD CHECK OUT. ***
>     *** AND "ALREADY CHECKED OUT" IS ALREADY A MODELLED CASE: `TurboMCPSourceControl.cpp` HANDLES A ROW ALREADY UNDER
>     CHECKOUT (`:165-171`), AND `GitSourceControlModule.cpp:21` IS CITED FOR GIT'S ALREADY-CHECKED-OUT STATE. THE
>     MEASUREMENT IS WHETHER THE TWO PATHS PRODUCE THE SAME SENTENCE TODAY. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
