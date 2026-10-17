---
id: "019cf7af-dfe5-4cda-91b9-1ca706ace4e5"
title: "T-AUDIT-22 (P1, medium) — AUDITED BY HAND 22/49: TurboMCPCallCostTools.cpp read to the end — a substituted cap parameter, and reporting discipline the three material writers lack"
status: backlog
priority: "P1"
tags: ["P1", "medium", "audited-by-hand", "pattern-C-P2", "pattern-C-P1", "callcost"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:25:49Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-22 (P1, medium) — AUDITED BY HAND 22/49: TurboMCPCallCostTools.cpp read to the end — a substituted cap parameter, and reporting discipline the three material writers lack

| Field | Value |
|---|---|
| Card ID | `019cf7af-dfe5-4cda-91b9-1ca706ace4e5` |
| Status | backlog |
| Priority | P1 |
| Tags | `P1` `medium` `audited-by-hand` `pattern-C-P2` `pattern-C-P1` `callcost` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:25:49Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPCallCostTools.cpp` (403 lines) was read to the end. The first finding held; the second was too strong and is narrowed here after reading the rest of the file. The same file is also the best reporting example in the whole audit.

What was found
    1. An unreadable numeric parameter is silently replaced — the second instance of this pattern in two files. In `TurboMCPTypes.cpp:395-404` and again at `TurboMCPCallCostTools.cpp:114-122`, a value that fails to parse falls back to the default: `banana`, `0` and `-5` all produce the answer for 50 (types) or 10 (this file). A refused value and an accepted value print the same line. Fix: refuse the bad value by name, the way `TurboMCPTypes.cpp:407` now does.
    2. Narrowed: the earlier note said `FMath::Min(Wanted, 50)` "bounds silently". That was too strong. The ceiling itself is not announced (`:123`), but every truncation it causes is: `:349` and `:382` print "... and %d more. RAISE CAP TO SEE THEM.", `:344` and `:377` put the row count in the heading, `:177` says how many log entries did not match the filter, and every row carries tokens, characters and milliseconds together. So the repair is one sentence — say the cap is bounded at 50 — which is what `TurboMCPTypes.cpp:351-355` already does with ", showing the first %d".

What this file gets right (the model to copy)
    `:87-93` states the three kinds of emptiness before computing any number, and the action each implies: logging is off means turn it on, on-but-empty means call something, nothing matched the filter means change the filter — reporting zeros for all three would tell an agent its tools are cheap when in fact nothing was recorded. `:100-104` says no cost can be reported and the reason is not that anything was cheap. `:19-32` records a defect found by running it: the qualified toolset was prepended to an already-qualified tool, printing `TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog | 1 call(s)`. `:35-44` records a second one in the same function: an unresolved hash written as "(engine tool ending 866fe74ba22c)" — human text with dots in it — so slicing at the last dot produced `866fe74ba22c)`. `:66-69` keeps two different problems apart: a call that returns a huge payload is not the same as a call that blocks the game thread. `:71-73` hands over structs the log itself parsed, so no second JSON reader can drift from the first. `:75-77` gives every total its denominator. `:111-113` bounds the answer rather than the analysis, so a total is never wrong because a list was short. `:125-127` counts every rejected entry. `:164-165` prints the log path and whether recording is on, on every answer.

Why this card is worth more than its findings
    The reporting discipline in this file — a count, its denominator, every truncated list naming what to do about it, every empty case naming the action it implies — is exactly what the three material writers lack. `TurboMCPMaterialTidy.cpp` computes `MovedBefore` and never prints it; this file prints "(%d row(s), each with its call count)". The cheapest repair in this audit is to move that discipline to the three writers, using this file as the model.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
