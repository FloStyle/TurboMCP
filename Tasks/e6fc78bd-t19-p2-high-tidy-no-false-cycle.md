---
id: "e6fc78bd-754d-43c1-bdad-78d7abaeaaee"
title: "T19 (P2, high) — Tidy: no false cycle"
status: todo
priority: "P2"
tags: ["P2", "high", "graph", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:45Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T19 (P2, high) — Tidy: no false cycle

| Field | Value |
|---|---|
| Card ID | `e6fc78bd-754d-43c1-bdad-78d7abaeaaee` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `high` `graph` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:45Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:243-250`, group P2. SOURCE IDs: TD-01, TD-02 (S1). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***

    DONE WHEN
        - *** A CYCLE MEANS A WIRE LOOP. A WRITE AND A READ OF THE SAME VARIABLE ARE NOT A LINK. ***
        - *** a refusal PRINTS THE FULL CYCLE AS A LIST OF NODE TITLES. ***

    VERIFY
        *** run tidy on a function that sets a variable and later reads it through a by-reference call. Tidy does not
        refuse. ***

    *** AND THE FIRST CLAUSE IS A DEFINITION, NOT AN OPTIMISATION, WHICH IS WHY THE CARD STATES IT AS ONE: a variable
    SET and a variable GET are TWO NODES with NO WIRE BETWEEN THEM. A cycle detector that walks a "variable" edge
    invents a loop the graph does not have, and the VERIFY IS CHOSEN TO CATCH EXACTLY THAT - a by-reference call is
    the case where a naive walker finds a path from the setter to the getter. ***
    *** AND THE SECOND IS `C-P2`/`C-P7`: A REFUSAL THAT NAMES A CYCLE WITHOUT PRINTING IT LEAVES THE CALLER UNABLE TO
    ACT. `TurboMCPNodeResolve.cpp:336-337` IS THE PATTERN TO COPY - "a missing graph is refused WITH EVERY GRAPH NAME
    LISTED, so the answer contains the correction" - AND ITS OWN COMMENT SAYS WHY: "saying which class it is PREVENTS A
    SECOND IDENTICAL ATTEMPT." ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
