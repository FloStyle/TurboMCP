# T25 (P3, low) — Data table create, with nested rows

ID: a4eed2e1-88a7-4c0a-b9de-7443e492f54b
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md

## Roadmap disposition

Do not expand this capability before a pinned native-tool comparison demonstrates a concrete missing workflow and an accepted design. Existing safety defects remain eligible for fixes.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:291-296`, group P3. SOURCE IDs: FT-12 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***
>
>     DONE WHEN
>         - *** a TurboMCP tool CREATES A TABLE FROM A ROW STRUCT.***
>         - *** rows can be written WITH DISPLAY FIELD NAMES, INCLUDING ARRAYS OF NESTED STRUCTS.***
>
>     *** AND THE SECOND CLAUSE IS THE ONE WITH THE MEASURED PRECEDENT: `T22` REQUIRES DISPLAY NAMES FIRST AND INTERNAL
>     NAMES SECOND FOR THE SAME TOOLS, SO THIS CARD IS THE DEEPEST CASE OF THAT RULE - A NESTED STRUCT ARRAY IS WHERE A
>     NAME PATH HAS MORE THAN ONE SEGMENT, AND A PARSER THAT HANDLES `Field` BUT NOT `Field.Sub` WILL FAIL ON THE FIRST
>     REAL TABLE. ***
>     *** AND THE FIRST CLAUSE HAS A `C-P10` OBLIGATION THAT THE LIST DOES NOT SPELL OUT: A TOOL THAT CREATES AN ASSET
>     CAN FAIL PART-WAY, AND THE PROJECT'S OWN RECORD IS THAT `UPackage::SavePackage` CAN REFUSE A RE-RUN WHEN THE
>     PACKAGE IS NOT MARKED FULLY LOADED (`06f6868d`, `SavePackage2.cpp:239`). A CREATE TOOL MUST BE RE-RUNNABLE, WHICH
>     MEANS THE SECOND RUN MUST EITHER REFUSE BY NAME OR TAKE THE SAME PATH AS THE FIRST. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
