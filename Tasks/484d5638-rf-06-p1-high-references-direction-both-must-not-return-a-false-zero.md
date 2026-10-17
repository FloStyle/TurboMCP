---
id: "484d5638-06b1-44ea-a597-97097644699d"
title: "RF-06 (P1, high) — references direction both must not return a false zero"
status: todo
priority: "P1"
tags: ["P1", "high", "feedback-2026-10-16", "references", "RF-06", "release-1.0"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:28Z"
updated: "2026-10-16T19:58:15Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# RF-06 (P1, high) — references direction both must not return a false zero

| Field | Value |
|---|---|
| Card ID | `484d5638-06b1-44ea-a597-97097644699d` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `feedback-2026-10-16` `references` `RF-06` `release-1.0` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:28Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 0 |

## Description

Source: Feedbacks/2026-10-16 S1. Direction=both returned 0/0 for a short-name asset while the same asset/path/direction=in had a user. Normalize resolution and combine in/out edges consistently across accepted asset forms.

## Execution prompt

```text
Extend the focused references fixture to compare short-name and package-path resolution for in/out/both. Assert both equals the union and cannot silently return zero when either side has edges. Run focused test-refs, then commit/push.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
