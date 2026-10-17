---
id: "37d3d49e-9a9d-4b7d-b5d6-88652b0dfe43"
title: "T-AUDIT-27-29 (P6, low) — AUDITED BY HAND 27-29/49: TurboMCPPlayCheck (.cpp + .h + Tools) — 757 lines, zero defects, and the correct form of what TraceTools does wrong"
status: backlog
priority: "P6"
tags: ["P6", "low", "audited-by-hand", "clean", "playcheck"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:27:10Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-27-29 (P6, low) — AUDITED BY HAND 27-29/49: TurboMCPPlayCheck (.cpp + .h + Tools) — 757 lines, zero defects, and the correct form of what TraceTools does wrong

| Field | Value |
|---|---|
| Card ID | `37d3d49e-9a9d-4b7d-b5d6-88652b0dfe43` |
| Status | backlog |
| Priority | P6 |
| Tags | `P6` `low` `audited-by-hand` `clean` `playcheck` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:27:10Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPPlayCheck` — the .cpp (570 lines), the .h (124) and the tools file (63), 757 lines in all. Zero defects found, and this set settles the `TraceTools.cpp` finding by contrast: it does the same job and does it correctly.

The refusal-prefix path is right here
    All thirteen `Answer.Reason` assignments in `PlayCheck.cpp` (`:351`, `:370`, `:377`, `:390`, `:407`, `:428`, `:445`, `:456`, `:468`, `:477`, `:489`, `:513`, `:562`) are plain sentences with no prefix. The wrapper adds the prefix once, in a switch over the outcome (`PlayCheckTools.cpp:26-50`): refused gives "error: R-PLAY-CHECK was refused. %s", already running gives "already playing: %s", pending gives "pending: %s". Each outcome gets its own form of words, added in one place, by the layer that knows which outcome it has. `:41-43` explains why `pending:` is an answer rather than an error: the session is running and the caller is being told to ask again, which is the only honest thing a single call can say.
    That is what makes `TurboMCPTraceTools.cpp:39` a defect rather than a convention — it wraps an error string with `"error: %s"` without testing it, which is the one shape that can produce `error: error: ...`.

Two more things it does right
    `PlayCheckTools.cpp:20-25` says both parameters are strings because the engine's tool registry marks an `FString` parameter required whatever its default, so "not given" is expressed by an empty string rather than by omitting the argument — a limitation named, traced to the note `R-TOOL-SCHEMA` already carries, and worked around in the signature. `PlayCheck.h:27` states that `UEditorEngine::RequestPlaySession` starts nothing but stores the request, which is the distinction that makes `pending` a real outcome.

All four counters are read
    `PlayCheck.cpp:152-155` sets `WarningCount`, `ErrorCount`, `DroppedCount` and `OwnerThreadId`, and all four appear in the answer. `DroppedCount` matters most: a play check that dropped log lines has to say so, or a clean report of "0 error(s)" cannot be told from a report over a log that lost its errors. This is the third file in the audit that carries its own denominator without being told to.

Tally
    After twenty-nine files: fourteen with no defect found, fifteen with one to three, and the defects are concentrated in the three material writers and in refusal/convention divergences across files written in many rounds. No file that only reads has shown a defect that changes behaviour.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
