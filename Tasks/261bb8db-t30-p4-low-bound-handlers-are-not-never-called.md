---
id: "261bb8db-950c-49f3-b7f2-fff304a3ec8a"
title: "T30 (P4, low) — Bound handlers are not \"never called\""
status: todo
priority: "P4"
tags: ["P4", "low", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T30 (P4, low) — Bound handlers are not "never called"

| Field | Value |
|---|---|
| Card ID | `261bb8db-950c-49f3-b7f2-fff304a3ec8a` |
| Status | todo |
| Priority | P4 |
| Tags | `P4` `low` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:326-329`, group P4. SOURCE IDs: BL-01 (S2). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        *** handlers BOUND TO EVENTS AT RUNTIME are listed as "CALLED BY A BOUND EVENT". ***

    *** AND THIS IS A FALSE NEGATIVE WITH A REAL COST: A DEAD-CODE REPORT THAT SAYS "NEVER CALLED" ABOUT A HANDLER THE
    GAME CALLS WILL LEAD A CALLER TO DELETE LIVE CODE. THAT IS THE `P8` LOSS THE PROJECT FORBIDS, ARRIVING THROUGH A
    REPORT RATHER THAN AN EDIT - AND IT IS THE SAME SHAPE AS `T07`'S "the preview missed nodes linked to the entry only
    through a data pin", WHERE THE MISS WAS A FALSE NEGATIVE ABOUT REACHABILITY. ***
    *** SO THIS CARD AND `T07` SHOULD SHARE ONE REACHABILITY RULE THE WAY `T06` AND `T07` ARE REQUIRED TO. THE
    MECHANISM TO MEASURE FIRST IS WHERE A BINDING LIVES: `WidgetBlueprint->Bindings` IS A LIST THIS PROJECT ALREADY
    READS (`TurboMCPWidgetBindAdd.cpp:110-167`, WHICH WALKS IT BACKWARDS AND REPORTS ITS COUNT), AND A BLUEPRINT'S
    `Delegate` BINDINGS ARE A DIFFERENT ARRAY IN A DIFFERENT PLACE. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
