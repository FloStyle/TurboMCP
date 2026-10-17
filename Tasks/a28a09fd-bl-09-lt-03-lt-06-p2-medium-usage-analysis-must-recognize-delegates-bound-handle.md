---
id: "a28a09fd-cb1a-4b58-baee-085beff7ce10"
title: "BL-09 / LT-03 / LT-06 (P2, medium) — usage analysis must recognize delegates, bound handlers and implicit self"
status: todo
priority: "P2"
tags: ["P2", "medium", "feedback-2026-10-16", "blueprint-reader", "lint", "BL-09", "LT-03", "LT-06"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:29Z"
updated: "2026-10-16T19:58:15Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# BL-09 / LT-03 / LT-06 (P2, medium) — usage analysis must recognize delegates, bound handlers and implicit self

| Field | Value |
|---|---|
| Card ID | `a28a09fd-cb1a-4b58-baee-085beff7ce10` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `medium` `feedback-2026-10-16` `blueprint-reader` `lint` `BL-09` `LT-03` `LT-06` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:29Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 0 |

## Description

Sources: 2026-10-16 S1/S3. Dispatcher calls/binds are marked unused, create-event-bound handlers are reported uncalled, and implicit self pins dominate unconnected-pin findings. Correct the shared usage rules and report use kind.

## Execution prompt

```text
Create focused fixtures for dispatcher call/bind/unbind, create-event handler binding and self calls to owning/parent versus foreign classes. Assert true uses are named by kind, owning-class self is ignored, and foreign self remains inspectable. Commit/push task-scoped changes.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
