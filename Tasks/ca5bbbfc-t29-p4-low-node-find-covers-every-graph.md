---
id: "ca5bbbfc-9ffe-4ae4-b92c-32c18d688b1a"
title: "T29 (P4, low) — Node find covers every graph"
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

# T29 (P4, low) — Node find covers every graph

| Field | Value |
|---|---|
| Card ID | `ca5bbbfc-9ffe-4ae4-b92c-32c18d688b1a` |
| Status | todo |
| Priority | P4 |
| Tags | `P4` `low` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:321-324`, group P4. SOURCE IDs: NF-01 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        *** the search covers EVERY GRAPH of the Blueprint, OR NAMES EACH SKIPPED GRAPH WITH THE REASON. ***

    *** AND THE "OR" IS THE WHOLE CARD: A SEARCH THAT SILENTLY SKIPS A GRAPH REPORTS A NEGATIVE THAT IS FALSE, AND A
    CALLER CANNOT TELL "no match anywhere" FROM "no match in the three graphs I happened to look at". THE PROJECT'S OWN
    `P1` IS EXACTLY THIS - AN ABSENT MEASUREMENT MUST NOT BE REPORTED AS A MEASURED ABSENCE. ***
    *** AND THE PATTERN IS ALREADY IN THE TREE, ON THE FILE THAT OWNS GRAPH LOOKUP: `TurboMCPNodeResolve.cpp:336-337`
    REFUSES A MISSING GRAPH "WITH EVERY GRAPH NAME LISTED, so the answer contains the correction", AND
    `TurboMCPNodeId.cpp:292-294` HAS `FindGraphByName` SEARCH EVERY ARRAY BECAUSE "an ID names a graph and not a kind
    of graph. A caller asking for `EventGraph` should find it whichever array the Blueprint happens to keep it in." ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
