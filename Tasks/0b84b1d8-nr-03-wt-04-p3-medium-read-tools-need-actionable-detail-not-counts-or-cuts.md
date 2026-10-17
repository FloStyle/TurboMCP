---
id: "0b84b1d8-ac34-4ea6-b2a7-f196ad74e81f"
title: "NR-03/WT-04 (P3, medium) — read tools need actionable detail, not counts or cuts"
status: todo
priority: "P3"
tags: ["P3", "medium", "feedback-2026-10-15", "read-tools", "widget", "node-resolve"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-15T19:07:48Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# NR-03/WT-04 (P3, medium) — read tools need actionable detail, not counts or cuts

| Field | Value |
|---|---|
| Card ID | `0b84b1d8-ac34-4ea6-b2a7-f196ad74e81f` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `medium` `feedback-2026-10-15` `read-tools` `widget` `node-resolve` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-15T19:07:48Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Sources: 2026-10-15 S1 and S3. Node resolve reports only a link count, so callers cannot see destinations. Widget-tree reads omit changed child-widget instance values or cut them. Add an explicit detail level: node resolve lists each link endpoint; widget tree lists every changed instance property without truncation when requested.

## Execution prompt

```text
Use focused node-resolve and widget-tree fixtures. Assert complete named link endpoints and all changed child-widget instance properties in detailed mode. Keep default output compact. Do not run a complete suite by default.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
