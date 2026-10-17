---
id: "02ba0f24-4c9d-41cf-8a02-b75644a13b0c"
title: "T18 (P2, high) — One getter rule for the report and the fixer"
status: todo
priority: "P2"
tags: ["P2", "high", "layout", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:45Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T18 (P2, high) — One getter rule for the report and the fixer

| Field | Value |
|---|---|
| Card ID | `02ba0f24-4c9d-41cf-8a02-b75644a13b0c` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `high` `layout` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:45Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:236-241`, group P2. Source IDs: LR-01 and LR-02 (S1, S2). This card came from the list. No one measured it on the tree.

done when
    1. A getter is flagged only when it overlaps a node, or sits right of the pin it feeds, or is too far from that pin vertically.
    2. After a fix, the report checks again. It lists each finding that was not cleared, with the reason.

Why clause 2 has the most evidence behind it
    `TurboMCPWidgetRetarget.cpp:355-384` re-counts from the Blueprint after the write and says "the removal did not take. Report this rather than trusting the count above". `TurboMCPDataTableRows.cpp:574-583` calls its read-back "a measurement of the result rather than a restatement of the request". `TurboMCPGraphKnots.cpp:1376-1405` prints "the rollback did not restore the graph". A fixer that does not re-read reports its own intention.

Why clause 1 must live in one place
    T17's six rules and this card's three are the same kind of property. The list split them into two cards because the getter rule is the one that drifted.

First measurement
    Find which of the two holds the rule today, the report or the fixer, and whether they agree. A screen for both sites is cheaper than reading either.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
