---
id: "d3897f07-b45f-4726-91e9-9c59829985cd"
title: "GL-10–GL-13 / GL-11 (P1, high) — graph reads must preserve every node and every pin link"
status: todo
priority: "P1"
tags: ["P1", "high", "feedback-2026-10-16", "graph-reader", "GL-10", "GL-11", "GL-13", "release-1.0"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# GL-10–GL-13 / GL-11 (P1, high) — graph reads must preserve every node and every pin link

| Field | Value |
|---|---|
| Card ID | `d3897f07-b45f-4726-91e9-9c59829985cd` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `feedback-2026-10-16` `graph-reader` `GL-10` `GL-11` `GL-13` `release-1.0` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Sources: 2026-10-16 S1–S3. Graph reader still prints own-function/delegate/struct nodes as errors and drops all but one source on multi-linked pins. Print every node in a read-only form where needed, and render every link source without presenting one as complete.

## Execution prompt

```text
Add a focused graph-reader fixture with own-function calls, delegate/struct nodes and a pin with multiple links. Ensure output contains all nodes, targets, arguments and all endpoints, then run graph checker. Commit/push only after focused proof; leave Todo if editor runtime is unavailable.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
