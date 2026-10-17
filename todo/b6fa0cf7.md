# T35 (P6) — Material writing (rows 3.2 to 3.6): full reader, then check, append, replace, patch, then function arguments

ID: b6fa0cf7-c92e-41e4-a5f1-0c1c86636283
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. Old documents are recoverable from the private backup commit recorded in docs/migration.md.

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
