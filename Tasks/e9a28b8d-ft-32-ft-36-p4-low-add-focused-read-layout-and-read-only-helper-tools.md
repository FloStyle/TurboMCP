---
id: "e9a28b8d-b558-4054-a5ae-3ff7b39bea2e"
title: "FT-32–FT-36 (P4, low) — add focused read, layout and read-only helper tools"
status: todo
priority: "P4"
tags: ["P4", "low", "feedback-2026-10-15", "feature", "read-tools", "safety"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-15T19:08:24Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# FT-32–FT-36 (P4, low) — add focused read, layout and read-only helper tools

| Field | Value |
|---|---|
| Card ID | `e9a28b8d-b558-4054-a5ae-3ff7b39bea2e` |
| Status | todo |
| Priority | P4 |
| Tags | `P4` `low` `feedback-2026-10-15` `feature` `read-tools` `safety` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-15T19:08:24Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Sources: 2026-10-15 S2 and S3. Requested helpers: one-call layout done check for selected graphs, node count by asset/graph, source text read within approved roots, detailed named-child widget instance values, and safe cleanup of read-only dirty flags. Triage these as separate small designs; do not bundle implementation. Source-text access must define allowed roots and limits before code.

## Execution prompt

```text
Break this card into focused subtasks or decisions before implementation. For each helper, define inputs, bounds, output, safety guard and a focused test. Treat source-text access and dirty-flag cleanup as safety-sensitive; validate allowed paths and preservation of pre-existing dirty state. Do not run a complete suite by default.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
