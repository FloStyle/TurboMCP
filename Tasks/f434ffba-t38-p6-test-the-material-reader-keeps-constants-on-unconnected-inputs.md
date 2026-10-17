---
id: "f434ffba-f4cc-49a1-bb51-21b51941c8b7"
title: "T38 (P6) — Test: the material reader keeps constants on unconnected inputs"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "test", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T38 (P6) — Test: the material reader keeps constants on unconnected inputs

| Field | Value |
|---|---|
| Card ID | `f434ffba-f4cc-49a1-bb51-21b51941c8b7` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `test` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:367`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Test: the material reader keeps constants on unconnected inputs." ***
    PASS WHEN, IN THE LIST'S OWN WORDS
        *** "EVERY UNCONNECTED INPUT PRINTS ITS CONSTANT VALUE, OR 'default'." ***

    *** AND THIS CARD IS A GATE FOR ANOTHER: `T35` SAYS "Run test T38 first", SO THIS IS THE CHEAPEST CARD IN THE LIST
    THAT UNBLOCKS THE MOST WORK. ***
    *** AND THE PASS CONDITION IS ALREADY THE PROJECT'S HOUSE SHAPE, WHICH MAKES IT EASY TO CHECK RATHER THAN EASY TO
    FAKE: `TurboMCPLayout.cpp:141-142` PRINTS "findings: none. No backward wire, no overlap, no misplaced getter, no
    stray node and no disconnected group" - *** AN EMPTY RESULT THAT ENUMERATES WHAT WAS CHECKED, SO A ZERO IS A
    MEASUREMENT RATHER THAN AN ABSENCE. "EVERY UNCONNECTED INPUT PRINTS ITS CONSTANT VALUE, OR 'default'" IS THE SAME
    REQUIREMENT AT THE ROW LEVEL, AND THE WORD "default" IS THE HONEST FALLBACK THE LIST ITSELF NAMES. ***
    *** AND ONE MEASUREMENT PRECEDES THE TEST: whether the material reader already HAS the constants. `TurboMCPMaterial
    Instance.cpp:359-365` PRINTS A CHAIN WITH ITS DENOMINATOR, SO THE REPORTING HABIT EXISTS IN THIS FAMILY. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
