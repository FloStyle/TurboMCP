---
id: "4de12720-cdc0-473e-8cb0-d9b4f5431a7b"
title: "GC-10–GC-12 / X-33 (P2, medium) — graph writers must accept canonical reader text"
status: todo
priority: "P2"
tags: ["P2", "medium", "feedback-2026-10-16", "graph-append", "parser", "GC-10", "GC-11", "GC-12"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:15Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# GC-10–GC-12 / X-33 (P2, medium) — graph writers must accept canonical reader text

| Field | Value |
|---|---|
| Card ID | `4de12720-cdc0-473e-8cb0-d9b4f5431a7b` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `medium` `feedback-2026-10-16` `graph-append` `parser` `GC-10` `GC-11` `GC-12` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 0 |

## Description

Source: Feedbacks/2026-10-16 S3. Append rejects inline getters, misattributes nested pure-node pins, rejects empty labelled forms, and treats case-only type-ID differences as distinct. Reader and writers need one grammar/canonical type-ID behavior.

## Execution prompt

```text
Add parser and graph-append focused cases for case-only type IDs, inline getters, nested pure calls and empty labelled forms. Make accepted results canonical and preserve label/link semantics. Do not combine with GC-09 integrity work; commit/push separately.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
