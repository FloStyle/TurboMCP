---
id: "ff18311b-9e72-49c5-8a65-5a96ccf5e028"
title: "T13 (P1, medium) — Pin-level link dump"
status: todo
priority: "P1"
tags: ["P1", "medium", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:03:13Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T13 (P1, medium) — Pin-level link dump

| Field | Value |
|---|---|
| Card ID | `ff18311b-9e72-49c5-8a65-5a96ccf5e028` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `medium` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:03:13Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:186-189`, group P1. SOURCE IDs: FT-07 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        *** one read lists every node with its TITLE, KEY AND POSITION. Each link is printed as
        "node title . pin -> node title . pin". THE OUTPUT IS SAFE TO PLAN A REWIRE FROM. ***

    *** AND THE LAST CLAUSE IS THE SPECIFICATION, NOT A WISH: A DUMP THAT IS "SAFE TO PLAN A REWIRE FROM" MUST BE
    COMPLETE AND UNAMBIGUOUS - every node present, every link named on BOTH ends, and every pin identified well enough
    that a later write can FIND it again. ***
    *** THAT IS EXACTLY WHERE `T06` BIT: "plain call nodes on other widgets printed as 'no node type ID, cannot be
    written back'", AND THE READER LISTED GETTERS AS "not reachable" BECAUSE THEY FED NODES IT COULD NOT PRINT. A DUMP
    IS ONLY AS COMPLETE AS THE POOREST NODE IT CAN NAME. ***
    *** AND THE KEY IS THE PROJECT'S OWN SINGLE AUTHORITY, `TurboMCPNodeId` - WHOSE AUDIT CARD IS IN `backlog` AND
    RECORDS TWO FINDINGS THAT LAND HERE: a graph name containing a COLON ROUND-TRIPS TO A DIFFERENT NAME, and
    `FindNodesByKey` DOES NOT NORMALISE ITS KEY while two of its four callers pass it raw. A DUMP THAT PRINTS A KEY THE
    READER CANNOT RESOLVE IS NOT SAFE TO PLAN FROM. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
