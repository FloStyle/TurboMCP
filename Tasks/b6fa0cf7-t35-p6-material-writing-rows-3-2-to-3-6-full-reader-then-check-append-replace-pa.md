---
id: "b6fa0cf7-c92e-41e4-a5f1-0c1c86636283"
title: "T35 (P6) — Material writing (rows 3.2 to 3.6): full reader, then check, append, replace, patch, then function arguments"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "material", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T35 (P6) — Material writing (rows 3.2 to 3.6): full reader, then check, append, replace, patch, then function arguments

| Field | Value |
|---|---|
| Card ID | `b6fa0cf7-c92e-41e4-a5f1-0c1c86636283` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `material` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:364`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Material writing (rows 3.2 to 3.6): FULL READER, THEN CHECK, APPEND, REPLACE, PATCH, THEN FUNCTION
        ARGUMENTS." ***

    *** AND THE LIST GIVES AN ORDER AND A GATE: *** *** "Run test T38 first." ***
        *** SO `T38` - "the material reader keeps constants on unconnected inputs" - IS THE THING THIS CARD DEPENDS ON,
        AND THE LIST SAYS TO RUN IT BEFORE STARTING. THAT IS A MEASURABLE FIRST STEP RATHER THAN A CONVENTION. ***
        *** AND THE MATERIAL TOOLS ARE THE MOST-MEASURED FAMILY IN THIS REPOSITORY, WHICH IS WHY THE GATE MATTERS: the
        three writers carried **28 unchecked `Transaction.Cancel()`** CALLS AND **three dead counters**
        (`TurboMCPMaterialTidy.cpp`, `TurboMCPMaterialLabelsPromote.cpp`, `TurboMCPMaterialBlocks.cpp`), ALL NOW FIXED
        - `a62e94f` REPLACED ALL 37 SITES WITH ONE SHARED HELPER AFTER `ade82641` MEASURED THAT `Cancel()` REVERTED
        NOTHING AT ALL, AND `add84fad` REMOVED THE DEAD COUNTERS. THE SIX SEQUENTIAL TOOLS THIS CARD ASKS FOR WOULD
        EACH HAVE BEEN BUILT ON THE BROKEN ROLLBACK. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
