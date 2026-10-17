---
id: "4f1bfd61-2e79-4f4f-ab66-644a6d1acc76"
title: "GC-09 / FT-47 (P1, high) — graph append must detect invalid created nodes and roll back"
status: todo
priority: "P1"
tags: ["P1", "high", "feedback-2026-10-16", "graph-append", "GC-09", "release-1.0"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# GC-09 / FT-47 (P1, high) — graph append must detect invalid created nodes and roll back

| Field | Value |
|---|---|
| Card ID | `4f1bfd61-2e79-4f4f-ab66-644a6d1acc76` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `feedback-2026-10-16` `graph-append` `GC-09` `release-1.0` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Source: Feedbacks/2026-10-16 S3. Graph append falsely reported success after creating nodes with duplicated pins and unresolved wildcard types; compile then failed. After every append, compare each created node's pin list against a fresh equivalent node, resolve wildcard pin types, compile for new errors, and roll back on failure.

## Execution prompt

```text
Build a focused empty-function fixture covering loop wildcard inference, interface targets and pin-list integrity. Verify preview vs edit, pin parity with an editor-created node, compile verdict and rollback on a new error. Commit and push only this task's work; leave the card Todo if runtime proof is blocked.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
