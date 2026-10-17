---
id: "d0f909a3-e329-45da-9b20-1c47a5980317"
title: "T40 (P6) — Preview of the ordered steps to add a node: create, then link these pins in this order"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T40 (P6) — Preview of the ordered steps to add a node: create, then link these pins in this order

| Field | Value |
|---|---|
| Card ID | `d0f909a3-e329-45da-9b20-1c47a5980317` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:369`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Preview of the ordered steps to add a node: CREATE, THEN LINK THESE PINS IN THIS ORDER." ***
    NOTE, IN THE LIST'S OWN WORDS
        *** "PARTLY A GAP TODAY. SEE GC-04 AND T05." ***

    *** AND THE ORDER IS THE POINT, NOT A DETAIL: A GRAPH LINK CAN REFUSE WHEN THE PIN IT LINKS FROM DOES NOT EXIST
    YET, OR WHEN THE NODE IS NOT FULLY CONSTRUCTED. SO "CREATE THEN LINK IN THIS ORDER" IS A CONTRACT ABOUT WHAT THE
    TOOL WILL DO, AND A PREVIEW THAT PRINTS IT LETS A CALLER SEE THE SEQUENCE BEFORE IT RUNS. ***
    *** AND `C-P4` - THE PROJECT'S OWN RULE THAT THE PREVIEW AND THE EDIT MUST NOT BE ABLE TO REACH DIFFERENT
    DECISIONS - IS EXACTLY WHAT THIS CARD ASKS FOR, AND `T40`'S NOTE POINTS AT `T05`, WHICH THIS SESSION CLOSED:
    `AvailableTypeIds` AND `SpawnNodeOfType` SHARE ONE CLASS SO A SUBSET IS IMPOSSIBLE BY CONSTRUCTION (`15f6f52`), AND
    THE FINDER NOW LISTS WHAT A GRAPH ACCEPTS WHEN GIVEN A `limit` (`0c2a8da`). SO THE CAPABILITY THIS CARD NEEDS
    ALREADY EXISTS AND WHAT REMAINS IS THE ORDERED PREVIEW. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
