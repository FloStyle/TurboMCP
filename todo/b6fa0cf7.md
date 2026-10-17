# Defer broad material authoring after reader verification

ID: b6fa0cf7-c92e-41e4-a5f1-0c1c86636283
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md), [R08](roadmap-r08.md).

Material readers and specialized writers already exist. A six-operation authoring roadmap is not a bounded defect repair, and historic rollback counts are stale.

### Next action and acceptance

Require f434ffba constant/read-completeness verification, R05 lifecycle evidence and native overlap before any append/replace/patch/function-argument expansion. Split a justified gap into individual operation designs.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterial.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterial.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidy.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidy.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterialBlocks.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterialBlocks.cpp)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-material --execute`

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

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:364`, group P6 (a table row). ***
>
>     CONTENT, IN THE LIST'S OWN WORDS
>         *** "Material writing (rows 3.2 to 3.6): FULL READER, THEN CHECK, APPEND, REPLACE, PATCH, THEN FUNCTION
>         ARGUMENTS." ***
>
>     *** AND THE LIST GIVES AN ORDER AND A GATE: *** *** "Run test T38 first." ***
>         *** SO `T38` - "the material reader keeps constants on unconnected inputs" - IS THE THING THIS CARD DEPENDS ON,
>         AND THE LIST SAYS TO RUN IT BEFORE STARTING. THAT IS A MEASURABLE FIRST STEP RATHER THAN A CONVENTION. ***
>         *** AND THE MATERIAL TOOLS ARE THE MOST-MEASURED FAMILY IN THIS REPOSITORY, WHICH IS WHY THE GATE MATTERS: the
>         three writers carried **28 unchecked `Transaction.Cancel()`** CALLS AND **three dead counters**
>         (`TurboMCPMaterialTidy.cpp`, `TurboMCPMaterialLabelsPromote.cpp`, `TurboMCPMaterialBlocks.cpp`), ALL NOW FIXED
>         - `a62e94f` REPLACED ALL 37 SITES WITH ONE SHARED HELPER AFTER `ade82641` MEASURED THAT `Cancel()` REVERTED
>         NOTHING AT ALL, AND `add84fad` REMOVED THE DEAD COUNTERS. THE SIX SEQUENTIAL TOOLS THIS CARD ASKS FOR WOULD
>         EACH HAVE BEEN BUILT ON THE BROKEN ROLLBACK. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
