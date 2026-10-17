---
id: "0e4658ad-625d-4f86-b81b-ee38cb298011"
title: "AS-01/BL-07/TH-02/TH-03 (P3, low) — filters, focused output and usable help"
status: todo
priority: "P3"
tags: ["P3", "low", "feedback-2026-10-15", "help", "asset-search", "output"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-15T19:07:48Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# AS-01/BL-07/TH-02/TH-03 (P3, low) — filters, focused output and usable help

| Field | Value |
|---|---|
| Card ID | `0e4658ad-625d-4f86-b81b-ee38cb298011` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `low` `feedback-2026-10-15` `help` `asset-search` `output` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-15T19:07:48Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Sources: 2026-10-15 S1 and S3. Asset search ignores a class filter placed after words. Blueprint graph-filter calls repeat a large widget tree. Help omits parameter type, required status, format and examples, including engine-tool forms. Make filters order-independent, omit widget tree by default when a graph filter is used, and give each parameter one valid concise example. Add relevant trap notes.

## Execution prompt

```text
Use focused static/output tests for search argument order, graph-filter output shape and tool-help examples. Verify the examples execute or parse. This is documentation/output work; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
