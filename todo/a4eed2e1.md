# Separate table creation from nested row correctness

ID: a4eed2e1-88a7-4c0a-b9de-7443e492f54b
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md), [R08](roadmap-r08.md).

Existing row operations and fixture creation are distinct. A kit commandlet does not establish a public table-create tool. Nested field aliases are correctness work only where already supported.

### Next action and acceptance

Compare native table creation before adding a tool. Keep nested-array/display-name cases under abdd2c08 and row read-back under ba6df3c8. Any new create design must refuse collisions and preserve existing files on failure.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L409)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPTypeParser.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPTypeParser.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPKitCommandlet.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPKitCommandlet.cpp)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-datatable-rows --execute`

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

</details>
