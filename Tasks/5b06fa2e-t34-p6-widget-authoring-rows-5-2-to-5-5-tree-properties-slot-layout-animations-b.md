---
id: "5b06fa2e-f0b8-4fa9-be6d-76dc0ae05141"
title: "T34 (P6) — Widget authoring (rows 5.2 to 5.5): tree, properties, slot layout, animations, bindings in bulk"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "widget", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T34 (P6) — Widget authoring (rows 5.2 to 5.5): tree, properties, slot layout, animations, bindings in bulk

| Field | Value |
|---|---|
| Card ID | `5b06fa2e-f0b8-4fa9-be6d-76dc0ae05141` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `widget` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:363`, group P6 (a table row, so the list gives no separate `done when`). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Widget authoring (rows 5.2 to 5.5): TREE, PROPERTIES, SLOT LAYOUT, ANIMATIONS, BINDINGS IN BULK." ***

    *** AND THE LIST ATTACHES A DEPENDENCY THAT THIS BOARD MUST RESPECT: ***
        *** "Needs T03: A SAVE MUST NOT FAIL ON WIDGET DESIGN-TIME WARNINGS." ***
        *** AND `T03` IS **NOT DONE** - `done when` 3 WAS WITHDRAWN IN `bb6467b` BECAUSE THE `WithoutThumbnail` OPTION
        TURNED OUT TO BE A **REQUIRED** SCHEMA PARAMETER, WHICH BROKE EVERY CALLER THAT OMITTED IT. SO **T34 IS
        BLOCKED BY A CARD THAT IS ITSELF UNFINISHED**, AND THAT IS A FACT TO SETTLE BEFORE ANY WORK HERE. ***
        *** AND THE UNDERLYING REPORTED SYMPTOM IS STILL WORTH RECORDING: a save of a widget Blueprint came back
        carrying design-time widget warnings, and the asset showed as unsaved about ten seconds later. `T03`'s card
        carries the identified cause - THE ENGINE'S POST-SAVE THUMBNAIL WRITE RUNS THE WIDGET'S DESIGN-TIME CODE - and
        the fix that was withdrawn. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
