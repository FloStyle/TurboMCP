---
id: "c421299d-8275-4cc5-85d1-bc85cbb49276"
title: "T20 (P2, high) — Capture shows what it framed"
status: todo
priority: "P2"
tags: ["P2", "high", "capture", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:45Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T20 (P2, high) — Capture shows what it framed

| Field | Value |
|---|---|
| Card ID | `c421299d-8275-4cc5-85d1-bc85cbb49276` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `high` `capture` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:45Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:252-259`, group P2. SOURCE IDs: CP-01 to CP-04 (S1, S2). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** the whole-graph capture SHOWS EVERY NODE IT REPORTS AS FRAMED.***
        - *** a frame that is TOO SMALL IS REFUSED, WITH THE REASON.***
        - *** THE RESULT TEXT HAS NO CONTRADICTIONS.***
        - *** the image is returned, OR A PATH THE AGENT CAN OPEN DIRECTLY.***

    *** AND THE THIRD CLAUSE IS A PROPERTY NO SINGLE CHECK CAN PROVE, WHICH IS WHY IT IS WORTH CARDING SEPARATELY: A
    READER HAS TO COMPARE THE SENTENCES AGAINST EACH OTHER. THIS SESSION HAS FOUND THE SHAPE FOUR TIMES IN OTHER
    FILES - a caption naming one thing while the text names another (`TurboMCPCapture.cpp:906`, verified true), a
    count that is a sum of three buckets labelled as one (`TurboMCPGraphReplace.cpp:192`, fixed), and a
    "described by the text" figure that double-counted the reused nodes (`:222-224`, fixed). ***
    *** AND THE FOURTH CLAUSE IS ABOUT A DIFFERENT LAYER ENTIRELY: AN ANSWER THAT CONTAINS AN IMAGE IS NOT THE SAME AS
    ONE THAT CONTAINS A PATH, AND `LookTurboMCPToolset`'S OWN DOC SAYS IT "lists the live Slate windows a capture
    could frame" rather than capturing one. SO THE FIRST MEASUREMENT IS WHICH OF THE TWO THE TOOL ALREADY DOES. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
