---
id: "602feaab-6a94-4004-9c1b-ada71d983f53"
title: "T14 (P1, low) — Session change list"
status: todo
priority: "P1"
tags: ["P1", "low", "session", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:03:13Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T14 (P1, low) — Session change list

| Field | Value |
|---|---|
| Card ID | `602feaab-6a94-4004-9c1b-ada71d983f53` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `low` `session` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:03:13Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:191-194`, group P1. SOURCE IDs: FT-14 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        *** one read lists EVERY NODE, VARIABLE AND FUNCTION that TurboMCP created, unplugged or deleted IN THE CURRENT
        EDITOR SESSION, BY ASSET AND BY GRAPH. ***

    *** AND THE CARD CARRIES A DESIGN QUESTION THE LIST DOES NOT ANSWER, WHICH IS WHY IT IS WORTH MEASURING FIRST:
    NOTHING IN THE PLUGIN KEEPS A SESSION LEDGER TODAY. `TurboMCPCallLog` RECORDS CALLS, NOT CHANGES - it has the tool
    name and the arguments but it does not know WHICH NODES MOVED, and it is cleared when the editor exits. ***
    *** SO THE CHOICE IS BETWEEN TWO HONEST SHAPES: READ THE ASSETS AND DIFF THEM AGAINST A SNAPSHOT TAKEN AT MODULE
    STARTUP (which is measurable today and needs no new bookkeeping), OR HAVE EVERY WRITER APPEND TO A LEDGER (which is
    `T02`'S 68-CALL-SITE PROBLEM AGAIN, AND `T02` ALREADY MEASURED THAT PRICE AT 59 DISTINCT FIRST-STATEMENT SHAPES). ***
    *** THE SNAPSHOT SHAPE HAS A KNOWN LIMIT WORTH STATING UP FRONT: IT REPORTS WHAT IS DIFFERENT NOW, SO AN EDIT THAT
    WAS MADE AND THEN UNDONE IN THE SAME SESSION IS INVISIBLE TO IT - WHICH MAY OR MAY NOT BE WHAT "CHANGED" MEANS TO
    THE CALLER. THAT IS THE FIRST THING TO SETTLE. ***

## Execution prompt

```text
Testing policy: Use the fastest relevant focused/static validation first. Avoid complete suites unless the change crosses integrations or focused validation is inadequate; record the reason if one is needed.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
