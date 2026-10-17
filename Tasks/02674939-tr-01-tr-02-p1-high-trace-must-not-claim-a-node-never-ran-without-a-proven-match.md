---
id: "02674939-2883-4cec-b2eb-24588dcfeac6"
title: "TR-01 / TR-02 (P1, high) — trace must not claim a node never ran without a proven match"
status: todo
priority: "P1"
tags: ["P1", "high", "feedback-2026-10-16", "trace", "TR-01", "TR-02"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# TR-01 / TR-02 (P1, high) — trace must not claim a node never ran without a proven match

| Field | Value |
|---|---|
| Card ID | `02674939-2883-4cec-b2eb-24588dcfeac6` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `feedback-2026-10-16` `trace` `TR-01` `TR-02` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Source: Feedbacks/2026-10-16 S1. Trace saw 187k events yet reported all armed nodes had not run. It also cleared armed points across play. Per point, distinguish no graph events from failed node matching; retain arms through play lifecycle or state why they were cleared.

## Execution prompt

```text
Create a focused trace fixture/control point. Test status wording for graph events with no node match, no graph events, and true hits; verify arming survives play start/end. Preserve current useful empty-state reporting. Commit and push only verified work.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
