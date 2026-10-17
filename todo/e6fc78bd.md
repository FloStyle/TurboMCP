# Verify Tidy cycle reports with real wire graphs

ID: e6fc78bd-754d-43c1-bdad-78d7abaeaaee
Status: todo
Priority: P2
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/graphs.md
Review: source-reviewed

## Current review

**Decision:** partial. **Follow-up owner:** [R07](roadmap-r07.md).

Current adjacency is built from output-to-input LinkedTo edges with parallel-edge deduplication and a cycle path. The alleged variable-name dependency is not present in that code.

### Next action and acceptance

Retain set/get plus by-reference-call as a regression alongside a real directed wire cycle and parallel exec/data links. Confirm full cycle titles and no mutation on refusal. Do not rewrite the cycle detector from the historical diagnosis.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPGraphTidy.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPGraphTidy.cpp#L587)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-graph-tidy --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: layout-capture. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

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

</details>
