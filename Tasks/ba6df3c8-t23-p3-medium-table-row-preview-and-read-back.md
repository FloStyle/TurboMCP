---
id: "ba6df3c8-12e8-4678-a020-a89881e3e0d7"
title: "T23 (P3, medium) — Table row preview and read-back"
status: todo
priority: "P3"
tags: ["P3", "medium", "datatable", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:45Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T23 (P3, medium) — Table row preview and read-back

| Field | Value |
|---|---|
| Card ID | `ba6df3c8-12e8-4678-a020-a89881e3e0d7` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `medium` `datatable` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:45Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:279-284`, group P3. SOURCE IDs: DT-02, DT-03 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** A REFUSED LINE IS NEVER LISTED AS A PLANNED CHANGE.***
        - *** the read-back SHOWS EVERY FIELD, OR SAYS THAT DEFAULT VALUES ARE LEFT OUT.***

    *** AND THE FIRST CLAUSE IS THE `P8` SHAPE IN ITS PUREST FORM: A PREVIEW THAT LISTS A LINE IT WILL REFUSE IS A
    PREVIEW THAT LIES, AND THE CALLER WHO APPROVED THE PLAN HAS APPROVED SOMETHING THAT WILL NOT HAPPEN. THIS SESSION
    MEASURED THE SAME DEFECT IN `test-struct` - a save that reported success while writing nothing - AND IN `T16`'s
    "0 overlaps" FROM A DEFAULT SQUARE. ***
    *** AND THE SECOND CLAUSE IS A CHOICE WITH TWO HONEST ANSWERS, AND THE CARD ALLOWS EITHER: SHOW EVERY FIELD, OR SAY
    THAT DEFAULTS ARE OMITTED. WHAT IT FORBIDS IS THE THIRD - OMITTING THEM SILENTLY, WHICH MAKES A READ-BACK THAT
    LOOKS SHORT INDISTINGUISHABLE FROM ONE THAT LOST DATA. `TurboMCPDataTableRows.cpp:574-583` ALREADY PRINTS "MISSING
    AFTER WRITE" for a row that did not survive, WHICH IS THE SAME ARGUMENT ONE LEVEL DOWN. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
