---
id: "9db83209-5824-4dd4-a08d-fdfffce29f2c"
title: "T28 (P4, medium) — Node-type lookup accuracy"
status: todo
priority: "P4"
tags: ["P4", "medium", "nodetype", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T28 (P4, medium) — Node-type lookup accuracy

| Field | Value |
|---|---|
| Card ID | `9db83209-5824-4dd4-a08d-fdfffce29f2c` |
| Status | todo |
| Priority | P4 |
| Tags | `P4` `medium` `nodetype` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:314-319`, group P4. SOURCE IDs: NT-01 (S2), NT-05 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** THE PURE FLAG MATCHES THE NODE THAT THE ENGINE CREATES.***
        - *** A VARIABLE'S DECLARED NAME (for example "bFlag") IS ACCEPTED AND MAPPED TO ITS ID ("Flag"). ***

    *** AND THE SECOND CLAUSE IS A `T05`-SHAPED REQUIREMENT: THE FINDER AND THE SPAWNER MUST AGREE ABOUT WHICH ID A
    CALLER MAY ASK FOR. `T05` ALREADY ESTABLISHED THAT `AvailableTypeIds` AND `SpawnNodeOfType` SHARE ONE CLASS, SO A
    SUBSET IS IMPOSSIBLE BY CONSTRUCTION (`15f6f52`) - BUT THAT IS ABOUT THE IDS THE GRAPH ACCEPTS, NOT ABOUT THE
    NAME A CALLER TYPES. "bFlag" -> "Flag" IS A TRANSLATION, AND A TRANSLATION THAT LIVES IN THE CALLER IS NOT A
    CONTRACT. ***
    *** AND THE FIRST CLAUSE HAS A RECORDED NEAR-MISS ON THE SAME FILE: `TurboMCPNodeType.cpp`'s `GetNodeTypeId`
    SPECIAL-CASES THE CUSTOM-EVENT FORM BECAUSE IT IS "not derivable from category and title ... Without this an agent
    would be handed a type ID the writer cannot build". THE PURE FLAG IS THE SAME KIND OF PROPERTY: IT COMES FROM THE
    ENGINE'S CREATION PATH AND NOT FROM THE TITLE. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
