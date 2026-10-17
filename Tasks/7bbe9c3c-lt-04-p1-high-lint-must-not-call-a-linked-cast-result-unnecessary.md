---
id: "7bbe9c3c-19c2-4b62-aad2-9cd098d0fedf"
title: "LT-04 (P1, high) — lint must not call a linked cast result unnecessary"
status: todo
priority: "P1"
tags: ["P1", "high", "feedback-2026-10-16", "lint", "LT-04", "safety"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:15Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# LT-04 (P1, high) — lint must not call a linked cast result unnecessary

| Field | Value |
|---|---|
| Card ID | `7bbe9c3c-19c2-4b62-aad2-9cd098d0fedf` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `feedback-2026-10-16` `lint` `LT-04` `safety` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 0 |

## Description

Source: Feedbacks/2026-10-16 S3. The unnecessary-cast check marked casts whose typed output is linked to a variable set, creating a dangerous false deletion recommendation.

## Execution prompt

```text
Add a focused lint fixture with both a genuinely unused cast and a cast whose typed output is linked and consumed. Correct the reachability/use rule; validate with focused lint output. Commit and push only task-scoped changes.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
