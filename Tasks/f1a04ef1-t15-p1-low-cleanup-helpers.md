---
id: "f1a04ef1-cd1d-4129-a972-53646838d9ce"
title: "T15 (P1, low) — Cleanup helpers"
status: todo
priority: "P1"
tags: ["P1", "low", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:03:13Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T15 (P1, low) — Cleanup helpers

| Field | Value |
|---|---|
| Card ID | `f1a04ef1-cd1d-4129-a972-53646838d9ce` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `low` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:03:13Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:196-202`, group P1. SOURCE IDs: FT-10 (S3), FT-05 (S3), DD-03 (S1). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** "Remove debug prints" FINDS PRINT NODES BY TEXT, BRIDGES THE EXEC LINK AROUND EACH ONE, AND REMOVES EACH
          PRINT WITH ITS STRING INPUTS.***
        - *** an optional "park" mode MOVES UNPLUGGED NODES INTO ONE LABELLED COMMENT BOX.***
        - *** the DEFAULT IS DELETE: "Users who keep work clean want unplugged nodes removed." ***

    *** AND THE FIRST CLAUSE IS THE SAME BREAK-THEN-MAKE THAT `T08` AND `T10` CARRY, WITH A DESTRUCTION AT THE END OF
    IT: BRIDGING MEANS BREAKING THE ENTRY-TO-PRINT LINK AND THE PRINT-TO-NEXT LINK, THEN JOINING ENTRY TO NEXT. IF THE
    JOIN IS REFUSED THE CHAIN IS SEVERED AND THE PRINT IS STILL THERE - AND `TurboMCPGraphTidy.cpp:1306` IS THE
    RECORDED INSTANCE OF EXACTLY THAT. ***
    *** AND "REMOVES EACH PRINT WITH ITS STRING INPUTS" NEEDS A MEASUREMENT: A STRING INPUT CAN BE SHARED WITH A NODE
    THE CALLER DID NOT ASK TO TOUCH. A `PrintString`'S `InString` IS OFTEN ITS OWN LITERAL, BUT IT CAN BE A VARIABLE
    GETTER OR ANOTHER CALL'S OUTPUT - SO THE TOOL MUST COUNT WHAT IT REMOVED AND REFUSE A SHARED INPUT RATHER THAN
    DELETING A NODE SOMETHING ELSE STILL READS. ***
    *** AND "PARK" HAS THE SAME RISK IN THE OTHER DIRECTION: MOVING NODES IS A WRITE, AND A MOVED NODE THAT WAS NOT
    UNPLUGGED IS A CHANGE THE CALLER DID NOT ASK FOR. THE CARD'S OWN "MOVES UNPLUGGED NODES" IS THE CHECK. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
