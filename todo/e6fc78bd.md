# T19 (P2, high) — Tidy: no false cycle

ID: e6fc78bd-754d-43c1-bdad-78d7abaeaaee
Status: todo
Priority: P2

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. Old documents are recoverable from the private backup commit recorded in docs/migration.md.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:243-250`, group P2. SOURCE IDs: TD-01, TD-02 (S1). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***
>
>     DONE WHEN
>         - *** A CYCLE MEANS A WIRE LOOP. A WRITE AND A READ OF THE SAME VARIABLE ARE NOT A LINK. ***
>         - *** a refusal PRINTS THE FULL CYCLE AS A LIST OF NODE TITLES. ***
>
>     VERIFY
>         *** run tidy on a function that sets a variable and later reads it through a by-reference call. Tidy does not
>         refuse. ***
>
>     *** AND THE FIRST CLAUSE IS A DEFINITION, NOT AN OPTIMISATION, WHICH IS WHY THE CARD STATES IT AS ONE: a variable
>     SET and a variable GET are TWO NODES with NO WIRE BETWEEN THEM. A cycle detector that walks a "variable" edge
>     invents a loop the graph does not have, and the VERIFY IS CHOSEN TO CATCH EXACTLY THAT - a by-reference call is
>     the case where a naive walker finds a path from the setter to the getter. ***
>     *** AND THE SECOND IS `C-P2`/`C-P7`: A REFUSAL THAT NAMES A CYCLE WITHOUT PRINTING IT LEAVES THE CALLER UNABLE TO
>     ACT. `TurboMCPNodeResolve.cpp:336-337` IS THE PATTERN TO COPY - "a missing graph is refused WITH EVERY GRAPH NAME
>     LISTED, so the answer contains the correction" - AND ITS OWN COMMENT SAYS WHY: "saying which class it is PREVENTS A
>     SECOND IDENTICAL ATTEMPT." ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
