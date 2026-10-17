---
id: "66d97dc8-2aa7-41a2-a8ae-7b289c89cbdb"
title: "T37 (P6) — Asset operations (row 7.6): create, rename, move, duplicate, with redirectors fixed"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "assets", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T37 (P6) — Asset operations (row 7.6): create, rename, move, duplicate, with redirectors fixed

| Field | Value |
|---|---|
| Card ID | `66d97dc8-2aa7-41a2-a8ae-7b289c89cbdb` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `assets` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:366`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Asset operations (row 7.6): CREATE, RENAME, MOVE, DUPLICATE, WITH REDIRECTORS FIXED." ***
    NOTE, IN THE LIST'S OWN WORDS
        *** "AFTER A RENAME OR MOVE, LIST THE REMAINING REDIRECTORS AND THE REFERENCERS." ***

    *** AND THE NOTE IS THE `P1` OBLIGATION ON A DESTRUCTIVE OPERATION: A MOVE THAT REPORTS SUCCESS AND LEAVES A
    REDIRECTOR BEHIND HAS LEFT THE PROJECT IN A STATE THE CALLER CANNOT SEE. LISTING BOTH THE REMAINING REDIRECTORS
    **AND** THE REFERENCERS IS THE DIFFERENCE BETWEEN "the move worked" AND "the move worked AND HERE IS WHAT STILL
    POINTS AT THE OLD PATH". ***
    *** AND THIS REPOSITORY ALREADY HAS THE FAILURE MODE ON RECORD, IN A DIFFERENT FILE: `TurboMCPAcceptanceAsset
    Commandlet.cpp:1138` "THE EXISTING ASSET FILE IS DELETED BEFORE THE SAVE and no undo restores it, so when
    `UPackage::SavePackage` fails the commandlet returns 3 WITH THE ORIGINAL FILE ALREADY GONE", AND `:1060` "AN
    EXISTING BLUEPRINT IS RENAMED OUT OF ITS PACKAGE WITH NO UNDO AND THE `Rename` BOOL DISCARDED". A RENAME TOOL MUST
    READ THE BOOL, AND MUST NOT DELETE BEFORE IT KNOWS THE SAVE WILL SUCCEED. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
