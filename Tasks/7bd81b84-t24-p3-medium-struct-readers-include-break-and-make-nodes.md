---
id: "7bd81b84-da3f-4132-bb87-de189eea69ac"
title: "T24 (P3, medium) — Struct readers include break and make nodes"
status: todo
priority: "P3"
tags: ["P3", "medium", "struct", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:46Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T24 (P3, medium) — Struct readers include break and make nodes

| Field | Value |
|---|---|
| Card ID | `7bd81b84-da3f-4132-bb87-de189eea69ac` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `medium` `struct` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:46Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:286-289`, group P3. SOURCE IDs: SF-02 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        *** a BREAK or MAKE node that uses a struct COUNTS AS A READER OF EACH FIELD IT EXPOSES. THE "FORCE EQUALS
        READER COUNT" RULE THEN APPLIES TO FIELD REMOVALS. ***

    *** AND THE SECOND HALF IS WHY THE FIRST MATTERS: A FIELD-REMOVAL TOOL THAT UNDERCOUNTS READERS WILL REMOVE A FIELD
    SOMETHING STILL READS, AND THE COUNT IT PRINTS WILL AGREE WITH ITS OWN UNDERCOUNT. A `BreakStruct` NODE EXPOSES
    EVERY FIELD OF THE STRUCT WITH NO PER-FIELD PIN ACCESS IN THE REPORT, SO A READER SCAN THAT WALKS PINS WILL MISS
    ALL OF THEM AT ONCE - WHICH IS THE SHAPE OF THE DEFECT RATHER THAN A MISSING CASE. ***
    *** AND THIS IS `T05`'S LESSON ONE LAYER OVER: `T05` FOUND THAT THE TYPE FINDER AND THE SPAWNER SHARED ONE SOURCE,
    SO A SUBSET WAS IMPOSSIBLE BY CONSTRUCTION. HERE THE READER COUNT AND THE REMOVAL DECISION MUST SHARE ONE SOURCE
    FOR THE SAME REASON - AND THE LIST'S WORD "THEN" SAYS THE ORDER: FIX THE COUNT FIRST, BECAUSE THE RULE IS ONLY AS
    GOOD AS THE COUNT IT READS. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
