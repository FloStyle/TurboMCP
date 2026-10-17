---
id: "4ec9e1cc-763d-46f6-aa31-181e8547e2fa"
title: "T39 (P6) — Curve assets (row 7.3)"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "curves", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T39 (P6) — Curve assets (row 7.3)

| Field | Value |
|---|---|
| Card ID | `4ec9e1cc-763d-46f6-aa31-181e8547e2fa` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `curves` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:368`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Curve assets (row 7.3)." ***
    NOTE, IN THE LIST'S OWN WORDS
        *** "THE TEST ASSET EXISTS." ***

    *** AND THE NOTE IS A PRECONDITION ALREADY SATISFIED, WHICH IS WORTH VERIFYING RATHER THAN ASSUMING: the kit
    commandlet builds curve fixtures, and this session read its save paths - `TurboMCPKitCommandlet.cpp:2010` IS "the
    curve-float half of TA-17" AND `:2082` IS "the curve-table half of TA-17", BOTH WITH THEIR `SaveContainer` RESULTS
    DISCARDED AT THE TIME AND BOTH NOW ON THE `C-P10` WORK (that card is in `backlog`). SO THE FIXTURE PATH EXISTS AND
    THE QUESTION IS WHETHER THE ASSET IS ON DISK AND LOADED. ***
    *** AND THE CARD IS THE THINNEST IN THE LIST BECAUSE THE LIST GIVES NO `done when` FOR IT - so THE FIRST ACTION IS
    TO WRITE ONE, and the honest starting point is the project's own `E-`/`R-` pair convention: a reader that prints a
    curve's key count with its denominator, and a writer that refuses by name. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
