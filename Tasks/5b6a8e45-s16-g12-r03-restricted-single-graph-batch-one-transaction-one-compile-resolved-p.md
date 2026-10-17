---
id: "5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae"
title: "S16 / G12+R03 — Restricted single-graph batch: one transaction, one compile, resolved per operation"
status: todo
priority: "P2"
tags: ["release-1.0", "P2", "batch", "ARC09", "R03", "G12"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
parent: "5f2cee78-ed31-487e-bfa6-6452eaaf738f"
created: "2026-10-16T20:14:50Z"
updated: "2026-10-16T20:14:50Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# S16 / G12+R03 — Restricted single-graph batch: one transaction, one compile, resolved per operation

| Field | Value |
|---|---|
| Card ID | `5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae` |
| Status | todo |
| Priority | P2 |
| Tags | `release-1.0` `P2` `batch` `ARC09` `R03` `G12` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Parent card | `5f2cee78-ed31-487e-bfa6-6452eaaf738f` |
| Created (UTC) | 2026-10-16T20:14:50Z |
| Updated (UTC) | 2026-10-16T20:14:50Z |
| Executions recorded | 0 |

## Description

Finding IDs: R03 (unfinished batch edit is publicly reflected), G12 (batch rollback wording and plan staleness). Original board card: T12 (bbb63d1e-1503-4094-9376-9ae2e2578222) — that broad card stays in backlog; this is the release subset card ARC09 requires.
Architecture: ARC09 (batch release scope), ARC03 (mutation coordination), ARC04 (compile baseline), ARC05 (rollback vs persistence).

EXACT BEHAVIOR TO CHANGE, from reading Source/TurboMCP/TurboMCPBatchEditTools.cpp in this run:
  1. `ParseBatch` computes EVERY plan up front (line 46), so a later operation is never re-resolved against the state its predecessors produced. ARC09 requires re-resolving and revalidating each operation against the preceding state.
  2. It does not refuse a cross-asset or cross-graph batch. Two operations may name different `asset`/`graph` values and both are planned and applied.
  3. It does not refuse unknown FIELDS. `graphRepoint` arguments are read by name and anything else is ignored, so a misspelled field silently changes the meaning of the operation.
  4. `EditTurboMCPBatchEdit` returns `changed` with `compile=deferred`; ARC09 requires ONE final compile for the batch.
  5. On failure it prints `unchanged` (line 77) even when its own rollback reports `failed`, and the reported rolled-back list omits the failing operation's possible partial mutation. R03 recorded this verbatim.
  6. Ownership: it opens `FScopedTransaction` and then calls `FTurboMCPRollback::EndAndUndoActiveTransaction()` on the failure path, which is a second end of the same transaction. ARC03 requires ending a transaction exactly once.

NON-GOALS: no new operation kinds (ARC09 allows only the existing `graphRepoint`), no cross-asset editing, no save from the batch, no general multi-asset atomic persistence. Do not mark broad T12 done.

SOURCE: Source/TurboMCP/TurboMCPBatchEditTools.cpp; Source/TurboMCP/TurboMCPGraphRepoint.cpp (ApplyCore); Public/Tools/TurboMCPToolsets.h (PreviewTurboMCPBatchEdit, EditTurboMCPBatchEdit).

ACCEPTANCE CASES: B01, B02, B03, B04, B05 (Release/ACCEPTANCE_CASES.md).

EVIDENCE DESTINATION: Release/EVIDENCE/S16/, and Tools/test-batch-edit.py.

ALSO OWED: Tools/audit-tool-coverage.py currently reports both batch tools as uncalled by every suite, which makes that blocking audit fail. Tools/test-batch-edit.py exists with BASELINE assertions and the ARC09 refusal shapes; S16 replaces the BASELINE block with the full B01-B05 contract so the file stops asserting the defective behaviour and starts asserting the contract.

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
