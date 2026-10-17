---
id: "ba518a2a-c4fa-4260-9947-613f8568a84b"
title: "T41 (P6) — Input test loop for play sessions: mouse, input action injection, simulated gamepad, menu solver"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "play", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T41 (P6) — Input test loop for play sessions: mouse, input action injection, simulated gamepad, menu solver

| Field | Value |
|---|---|
| Card ID | `ba518a2a-c4fa-4260-9947-613f8568a84b` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `play` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:370`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Input test loop for play sessions: MOUSE, INPUT ACTION INJECTION, SIMULATED GAMEPAD, MENU SOLVER." ***
    NOTE, IN THE LIST'S OWN WORDS
        *** "FULL DESIGN IN THE PROPOSAL FILE, PHASES P1 TO P7. RUN ITS PROBE FIRST." ***

    *** AND THE NOTE NAMES A MEASURABLE FIRST STEP, WHICH IS THE WHOLE REASON THIS CARD IS WRITTEN THE WAY IT IS: the
    design exists elsewhere and is READ RATHER THAN INVENTED, and THE PROBE IS RUN BEFORE ANY OF THE FOUR PARTS IS
    BUILT. ***
    *** AND THIS IS THE ONE CARD IN THE LIST WHOSE SUBJECT TOUCHES THE **LIVE** EDITOR AT RUNTIME, WHICH PUTS IT IN THE
    SAME FAMILY AS `TurboMCPPlayCheck` - 757 LINES THIS SESSION'S AUDIT READ AS ZERO DEFECTS, WITH A COUNTER SET THAT
    INCLUDES `DroppedCount` BECAUSE "a play check that dropped log lines must say so, or a clean report of `0 error(s)`
    cannot be told from a report over a log that lost its errors" (that card is in `backlog`). A PLAY-SESSION INPUT
    LOOP NEEDS THE SAME DISCIPLINE AND THE SAME COUNTERS. ***
    *** AND "PHASES P1 TO P7" MEANS THIS CARD SHOULD PROBABLY BECOME SEVEN WHEN IT STARTS. THAT IS A DECISION FOR THEN,
    NOT NOW, AND IT IS RECORDED SO IT IS NOT DISCOVERED LATE. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
