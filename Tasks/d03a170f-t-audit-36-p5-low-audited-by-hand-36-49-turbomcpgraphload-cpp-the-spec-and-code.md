---
id: "d03a170f-8ba2-4131-9258-c02644e22eb9"
title: "T-AUDIT-36 (P5, low) — AUDITED BY HAND 36/49: TurboMCPGraphLoad.cpp — the spec and code are both honest about format 'write'; the plan's count is the only thing that omits it"
status: backlog
priority: "P5"
tags: ["P5", "low", "audited-by-hand", "C6", "spec-gap", "graphload"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:43:04Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-36 (P5, low) — AUDITED BY HAND 36/49: TurboMCPGraphLoad.cpp — the spec and code are both honest about format 'write'; the plan's count is the only thing that omits it

| Field | Value |
|---|---|
| Card ID | `d03a170f-8ba2-4131-9258-c02644e22eb9` |
| Status | backlog |
| Priority | P5 |
| Tags | `P5` `low` `audited-by-hand` `C6` `spec-gap` `graphload` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:43:04Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPGraphLoad.cpp`, 170 lines, a pure reader. The first version of this card called it "a capability the spec promises and the code refuses". Checking the spec showed the spec is honest, so the finding is narrower and exact.

What the spec actually says
    `Spec/04_FORMATS.md:118-120`, repeated at `Spec/03_TOOLS.md:221`: format `write` prints a graph in the form the engine's own graph write tool accepts, because agents will keep using that tool until the plugin's writer is done. So the spec says the writer is not finished, and the code's refusal is correct and honest — `:97-103` names the spec section, says it is the next step of phase 2, and offers the fallback: use format `read`, the default, to read the graph now. There is no gap between the spec and the code.

What remains, and it is one thing
    The plan's count does not include it. `Tools/checks/count-plan.py` reads `30 todo, 16 done, 2 open` of 48 rows, and the graph write dialect is not among the two open rows; no `PLAN.md` row names it either, and no suite calls `write`, so the refusal is never exercised. A known-incomplete capability is therefore missing from the count, and the number a reader checks first reads as more complete than the project is.
    Fix: add the graph write dialect to `Spec/WORKING_PLAN.md` as an open item, or say in the plan why it is not one. Then `count-plan.py` counts it and the number is true.

How the first version got it wrong
    One file was read and a claim was made about a second without opening it. The measurement was right and the claim built on it was wrong — the same shape as the withdrawn finding on `TurboMCPTokenEstimate`.

What the file gets right
    No `Modify()`, no `Transaction`, no `MarkPackageDirty`. Every refusal names its accepted set (`:87` "is not a detail level. Accepted: %s", `:105` "is not a format. Accepted: read") or its consequence (`:119` "is not a Blueprint, so it has no graphs to read", `:129` "has no graph named '%s'. Its graphs are: %s").

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
