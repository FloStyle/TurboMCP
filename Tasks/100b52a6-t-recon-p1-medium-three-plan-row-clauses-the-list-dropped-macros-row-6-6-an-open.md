---
id: "100b52a6-19fa-4437-982e-34485a9695ea"
title: "T-RECON (P1, medium) — Three `plan row` clauses the list dropped: macros (row 6.6), an open tab refusing (row 7.6), E-SESSION/R-EDIT-SESSION bounded (row 9.2)"
status: todo
priority: "P1"
tags: ["P1", "medium", "roadmap", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T23:34:00Z"
updated: "2026-10-16T19:58:04Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-RECON (P1, medium) — Three `plan row` clauses the list dropped: macros (row 6.6), an open tab refusing (row 7.6), E-SESSION/R-EDIT-SESSION bounded (row 9.2)

| Field | Value |
|---|---|
| Card ID | `100b52a6-19fa-4437-982e-34485a9695ea` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `medium` `roadmap` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T23:34:00Z |
| Updated (UTC) | 2026-10-16T19:58:04Z |
| Executions recorded | 0 |

## Description

Summary
    Three clauses from the roadmap rows that the list cites. They reached neither the list nor any live card. I found them by resolving every `plan row` citation in `Feedbacks/TurboMCP_TaskList_2026-10-13.md` against `Spec/WORKING_PLAN.md`. All seven citations resolve, and that is what makes the omissions visible.

Clause 1 — "macros", from row 6.6, which T05 cites
    Roadmap, `Spec/WORKING_PLAN.md:140`: "Enumerate and test every node family of `03_TOOLS.md` section 8's table, one at a time: promotable operators, split struct pins, delegates, macros, timelines, latent nodes."
    The list, `:112`: "... promotable operators, split struct pins, delegates and CreateEvent, timelines and latent nodes."
    "Macros" is in the roadmap row and in no task. The row's test column also asks for "one recorded row per family". The list asks only that every offered type is built or refused by name.

Clause 2 — "an open tab refusing", from row 7.6, which T37 cites
    Roadmap, `:151`: "`E-ASSET-NEW`, `E-ASSET-RENAME`, `E-ASSET-MOVE`, `E-ASSET-DUPLICATE`, with redirectors fixed and an open tab refusing."
    The list, `:366`: "Asset operations (row 7.6): create, rename, move, duplicate, with redirectors fixed."
    The refusal that applies when the asset is open in a tab is in the roadmap row and in no task. That refusal is a `P8` condition, not a detail. A rename or a move of an asset that the editor holds open is the destructive case that the session's own card names.

Clause 3 — `E-SESSION` and `R-EDIT-SESSION`, from row 9.2, which T12 cites
    Roadmap, `:168`: "`E-SESSION` and `R-EDIT-SESSION`, bounded, and saying when it ended itself."
    Roadmap, `:169`: "`P/E-ASSET-BATCH` as `E-SESSION` plus a list, one transaction."
    The list, `:175-184`: T12 covers the second row. T12 asks for one batch, several assets, one undo, one compile, one save, a preview, and a failure report that names the step and what rolled back.
    The `E-SESSION` and `R-EDIT-SESSION` half is in no task. That half is the session itself, bounded, and saying when it ended itself. It is also on no live card. The only card that ever named those three tools is `ce026a9f` ("Build 11 specified capabilities with no tool: graph rename/collapse, 4 asset ops, 2 curve ops, 5 level/actor ops, session+batch"). That card is archived.

The measurement behind all three
    A case-insensitive sweep of every live card returns zero cards that carry `macros` beside a node-family clause. It returns zero that carry `open tab`, and zero that carry `E-SESSION`, `R-EDIT-SESSION` or `E-ASSET-BATCH`. The omission is not a wording difference. The clauses are absent.

done when
    1. Each of the three clauses has a decision on this card. Either the file's task gains the clause, and that task's card follows, or the clause is dropped with the reason.
    2. No decision is silent. A reader can check the reason against `Spec/WORKING_PLAN.md`.
    3. Clause 3 also settles `ce026a9f`. Restoring that card wholesale would put step 6.1/6.2 and all of step 8 back on the board, and the list marks those out of scope (`:371`). The honest options are to reopen the session rows only, or to leave the archived card as the record and carry row 9.2's session half here.

## Execution prompt

```text
Reconcile the three omitted roadmap clauses against the cited plan, task list and implementation, then record an adopt/drop decision and exact downstream edits. Use direct document/tree validation only; do not run a complete suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
