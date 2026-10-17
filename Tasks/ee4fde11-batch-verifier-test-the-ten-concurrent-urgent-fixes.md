---
id: "ee4fde11-5d9a-43f2-a017-ad6993605e30"
title: "Batch verifier — test the ten concurrent urgent fixes"
status: todo
priority: "P1"
tags: ["batch-verification", "urgent-batch", "release-1.0", "P1"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-15T19:21:03Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# Batch verifier — test the ten concurrent urgent fixes

| Field | Value |
|---|---|
| Card ID | `ee4fde11-5d9a-43f2-a017-ad6993605e30` |
| Status | todo |
| Priority | P1 |
| Tags | `batch-verification` `urgent-batch` `release-1.0` `P1` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-15T19:21:03Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Run only after the ten parallel fix cards have settled. Scope: X-1; F3/F4/X-3; DT-06/SV-07; X-20/TD-04/CP-05; GC-02/GC-03/GC-04; SF-04/VA-02/FA-04; GC-06/GC-07; X-18/X-21/X-22; MI-02/FT-27; FT-30/FT-31/TD-05. First reconcile the shared worktree and list every commit or conflict. Then build once from the final worktree and run the selected regression suites for every changed area. Record each card's pass, fail, blocked or not-covered result. Do not close source cards solely because this batch starts.

## Execution prompt

```text
Wait until all ten source fix tasks settle. Confirm a clean, reconciled worktree before testing. Build once, then run a combined targeted suite covering references, widget/traps, data tables and save, graph text/reader/append, typed fields/functions, dirty-list, material reads, layout and capture. If a test cannot run, state the concrete environment block and do not call the affected item verified.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
