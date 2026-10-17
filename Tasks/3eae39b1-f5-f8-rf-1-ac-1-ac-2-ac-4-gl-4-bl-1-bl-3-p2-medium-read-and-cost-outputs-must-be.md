---
id: "3eae39b1-82f6-480c-bf6e-e20d3bef5fe6"
title: "F5-F8/RF-1/AC-1/AC-2/AC-4/GL-4/BL-1/BL-3 (P2, medium) — read and cost outputs must be truthful and bounded"
status: todo
priority: "P2"
tags: ["P2", "medium", "read-tools", "cost-tools", "feedback-2026-10-14"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
permission: "workspace-write"
created: "2026-10-14T14:57:30Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# F5-F8/RF-1/AC-1/AC-2/AC-4/GL-4/BL-1/BL-3 (P2, medium) — read and cost outputs must be truthful and bounded

| Field | Value |
|---|---|
| Card ID | `3eae39b1-82f6-480c-bf6e-e20d3bef5fe6` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `medium` `read-tools` `cost-tools` `feedback-2026-10-14` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Permission | `workspace-write` |
| Created (UTC) | 2026-10-14T14:57:30Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

New feedback from S1/S2 groups several medium correctness issues: DataTable field-name and empty-value contradictions; claims about unsearched graphs; wrong generated-class or called-function statements; missing reference edge kind; dependency-kind and size semantics; missing holder properties; missing graph entry filters; and token budgets not being respected.

## Execution prompt

```text
Fix the listed truthful/bounded read and cost outputs with focused regression tests per affected tool. Re-run only those focused tests after each change. Do not run a complete suite unless a shared cross-tool integration cannot be proven otherwise; record why if required.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
