---
id: "63d5c9e2-ab8b-4a85-a6f2-2f644e0c3e1c"
title: "T11 (P1, medium) — Function from text, with its signature"
status: todo
priority: "P1"
tags: ["P1", "medium", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:03:13Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T11 (P1, medium) — Function from text, with its signature

| Field | Value |
|---|---|
| Card ID | `63d5c9e2-ab8b-4a85-a6f2-2f644e0c3e1c` |
| Status | todo |
| Priority | P1 |
| Tags | `P1` `medium` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:03:13Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:166-173`, group P1. SOURCE IDs: FT-15 (S4), FA-03 (S4), FT-06 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** function add accepts inputs, outputs and local variables OF ANY TYPE IN THE SHARED FIELD SYNTAX (`T22`),
          AND OPTIONALLY A BODY IN GRAPH TEXT (`T05`). ***
        - *** the function RESULT NODE IS CREATED AND WIRED. ***

    *** AND THE LIST ATTACHES ITS OWN MEASURED NOTE, WHICH IS THE REASON THIS CARD EXISTS: ***
        *** "in these sessions, THE ENGINE GRAPH WRITER COULD NOT CREATE A RETURN NODE. It also ADDED THE NEW BODY NEXT
        TO THE OLD ONE INSTEAD OF REPLACING IT. The TurboMCP tool must avoid both problems." ***
        *** TWO ENGINE BEHAVIOURS, BOTH OBSERVED, BOTH SILENT - AND THE SECOND IS THE DANGEROUS ONE: a body that is
        added beside the old one leaves TWO bodies where the caller asked for a replacement, and the tool reports
        success. `86cb04de` IS THE SAME SHAPE IN THE FIXTURE BUILDER: `SaneProbe` IS A VOID FUNCTION, SO IT HAS NO
        RESULT NODE, AND ITS OLD GUARD MADE "no result node" INDISTINGUISHABLE FROM "the link failed" (`86cb04de`,
        `BlueprintEditorUtils.cpp:2301`: only a function with out parameters gets a result node). ***

    *** SO "THE RESULT NODE IS CREATED AND WIRED" NEEDS AN ANSWER FOR THE VOID CASE, TAKEN BEFORE THE TOOL IS WRITTEN:
    a function with no outputs has NO result node BY ENGINE RULE, and a tool that insists on one will either fail on
    every void function or invent a node the engine would not have made. *** *** THE `T22` DEPENDENCY IS REAL AND THE
    LIST STATES IT: this card uses the SHARED FIELD SYNTAX, so it lands after that one. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
