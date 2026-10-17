---
id: "a4eed2e1-88a7-4c0a-b9de-7443e492f54b"
title: "T25 (P3, low) — Data table create, with nested rows"
status: todo
priority: "P3"
tags: ["P3", "low", "datatable", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:46Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T25 (P3, low) — Data table create, with nested rows

| Field | Value |
|---|---|
| Card ID | `a4eed2e1-88a7-4c0a-b9de-7443e492f54b` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `low` `datatable` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:46Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:291-296`, group P3. SOURCE IDs: FT-12 (S4). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** a TurboMCP tool CREATES A TABLE FROM A ROW STRUCT.***
        - *** rows can be written WITH DISPLAY FIELD NAMES, INCLUDING ARRAYS OF NESTED STRUCTS.***

    *** AND THE SECOND CLAUSE IS THE ONE WITH THE MEASURED PRECEDENT: `T22` REQUIRES DISPLAY NAMES FIRST AND INTERNAL
    NAMES SECOND FOR THE SAME TOOLS, SO THIS CARD IS THE DEEPEST CASE OF THAT RULE - A NESTED STRUCT ARRAY IS WHERE A
    NAME PATH HAS MORE THAN ONE SEGMENT, AND A PARSER THAT HANDLES `Field` BUT NOT `Field.Sub` WILL FAIL ON THE FIRST
    REAL TABLE. ***
    *** AND THE FIRST CLAUSE HAS A `C-P10` OBLIGATION THAT THE LIST DOES NOT SPELL OUT: A TOOL THAT CREATES AN ASSET
    CAN FAIL PART-WAY, AND THE PROJECT'S OWN RECORD IS THAT `UPackage::SavePackage` CAN REFUSE A RE-RUN WHEN THE
    PACKAGE IS NOT MARKED FULLY LOADED (`06f6868d`, `SavePackage2.cpp:239`). A CREATE TOOL MUST BE RE-RUNNABLE, WHICH
    MEANS THE SECOND RUN MUST EITHER REFUSE BY NAME OR TAKE THE SAME PATH AS THE FIRST. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
