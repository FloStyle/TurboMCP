---
id: "e866ba0d-4f6e-408e-9df7-d90a6467d410"
title: "T27 (P4, medium) — One node ID format, readable by a person"
status: todo
priority: "P4"
tags: ["P4", "medium", "nodeid", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T27 (P4, medium) — One node ID format, readable by a person

| Field | Value |
|---|---|
| Card ID | `e866ba0d-4f6e-408e-9df7-d90a6467d410` |
| Status | todo |
| Priority | P4 |
| Tags | `P4` `medium` `nodeid` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:307-312`, group P4. Source IDs: X-02 and X-03 (S1), DD-02 (S1). This card came from the list. No one measured it on the tree.

done when
    1. Every tool accepts the short key and the long ID.
    2. Every node reference in a result prints the key, the node title and the graph.

Clause 1 is already known to be half-false
    This session's audit of `TurboMCPNodeId.cpp` found it. That audit card is in `backlog`. `NormaliseKey` exists, and only 2 of its 4 callers use it. `TurboMCPCapture.cpp:675` and `TurboMCPNodeResolve.cpp:343` pass the raw parse result. `FindNodesByKey`'s key test is a prefix test, so a short key does not "match nothing" as its own comment fears. It matches every node whose GUID starts with those characters. The measurement is already taken, so the first action is to read that card, not to derive it again.

Why clause 2 makes clause 1 checkable
    A result that prints a bare key cannot be acted on by a reader who does not already know which graph it came from. That is T13's "safe to plan a rewire from", one layer down.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
