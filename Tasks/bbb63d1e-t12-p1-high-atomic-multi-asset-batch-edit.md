---
id: "bbb63d1e-1503-4094-9376-9ae2e2578222"
title: "T12 (P1, high) — Atomic multi-asset batch edit"
status: todo
priority: "P1"
tags: ["P1", "high", "batch", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:03:13Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T12 (P1, high) — Atomic multi-asset batch edit

| Field | Value |
|---|---|
| Card ID | `bbb63d1e-1503-4094-9376-9ae2e2578222` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `high` `batch` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:03:13Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Architectural decision
    Add a dedicated Preview/Edit batch capability. Do not call existing Edit tools sequentially: each existing writer owns its own transaction, compile, and save, so composition would create multiple undo records and cannot guarantee rollback. The batch owns the only transaction and invokes reusable plan/apply primitives without nested transactions.

Public interface
    Add `PreviewTurboMCPBatchEdit` and `EditTurboMCPBatchEdit` with one `Operations` UTF-8 JSON string. Each operation has an ordered `kind` and the arguments required by that kind (for example `graphRepoint`, `graphSwapCall`, `functionAdd`, or an asset/table operation), plus a stable `id` supplied by the caller. The preview returns the complete ordered plan, affected asset/package set, compile/save phases, and every operation's predicted change.

Required design
    1. Parse and validate the complete operation list before opening a transaction. Reject duplicate IDs, unknown kinds, malformed arguments, and any operation whose plan refuses. Preview performs no writes.
    2. Apply all accepted operations under one `FScopedTransaction` spanning every touched package. Existing writer cores must expose transaction-free plan/apply primitives; wrappers must not call public Edit tools or create nested transactions.
    3. Defer compile and save until every operation has applied. Compile each affected Blueprint once; save each affected package once. Report the ordered compile/save set.
    4. On failure, identify the operation ID and kind, call `FTurboMCPRollback::EndAndUndoActiveTransaction`, and report every operation that had already applied and was rolled back. No partial success response.
    5. After apply, independently read back every operation's target state and compare it with the plan. A failed comparison rolls back and names the mismatch.
    6. The first line is `unchanged` for refusal/rollback and `changed` only after all operations, compiles, saves, and readback checks pass. The result names one undo, one compile phase, one save phase, and the exact rollback scope.

Verification
    First measure whether one transaction can capture modifications in two Blueprint packages and an asset package. Then add a focused four-step batch whose third operation refuses: assert independent reads show no first/second/fourth change, the response names the third operation, and the rollback list is exact. Add a successful multi-asset preview/edit case and assert one compile/save per affected asset.

Rationale
    A batch is a transaction coordinator, not another graph dialect. It must reuse the existing plan cores while moving transaction/compile/save ownership to one outer layer; otherwise the API would claim atomicity it cannot provide.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
