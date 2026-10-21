# State graph search coverage explicitly

ID: ca5bbbfc-9ffe-4ae4-b92c-32c18d688b1a
Status: todo
Priority: P4
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/graphs.md
Review: source-reviewed

## Current review

Partial repair c6e173a (Untested): unscoped NodeFind now uses UBlueprint::GetAllGraphs plus editor event graphs and deduplicates graph pointers. Nested graphs are included. Follow-up 2b16d12 adds recursive named-graph resolution and refuses ambiguous names for callers without a match count. BlueprintRead coverage is also implemented in 7f85a91, including pointer deduplication and stable ordering for equal names. Independent coverage verification remains deferred; this broad issue remains open. No tests/build/editor run.

**Decision:** investigate. **Follow-up owner:** [R06](roadmap-r06.md).

Several call sites enumerate UbergraphPages, FunctionGraphs and MacroGraphs themselves. Shared graph lookup and graph enumeration are different contracts; recursive/subgraph coverage remains to be proved.

### Next action and acceptance

Use a Blueprint with function, macro, event and nested graphs. Compare against an independent engine graph enumeration; list skipped graphs and reasons. Do not infer whole-asset coverage from a successful named-graph resolver.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPNodeResolve.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPNodeResolve.cpp#L481)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPBlueprintRead.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPBlueprintRead.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-batch-tools --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: graph-fidelity. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:321-324`, group P4. SOURCE IDs: NF-01 (S3). *** *** CARDED FROM THE LIST, NEVER MEASURED ON THE TREE. ***
>
>     DONE WHEN
>         *** the search covers EVERY GRAPH of the Blueprint, OR NAMES EACH SKIPPED GRAPH WITH THE REASON. ***
>
>     *** AND THE "OR" IS THE WHOLE CARD: A SEARCH THAT SILENTLY SKIPS A GRAPH REPORTS A NEGATIVE THAT IS FALSE, AND A
>     CALLER CANNOT TELL "no match anywhere" FROM "no match in the three graphs I happened to look at". THE PROJECT'S OWN
>     `P1` IS EXACTLY THIS - AN ABSENT MEASUREMENT MUST NOT BE REPORTED AS A MEASURED ABSENCE. ***
>     *** AND THE PATTERN IS ALREADY IN THE TREE, ON THE FILE THAT OWNS GRAPH LOOKUP: `TurboMCPNodeResolve.cpp:336-337`
>     REFUSES A MISSING GRAPH "WITH EVERY GRAPH NAME LISTED, so the answer contains the correction", AND
>     `TurboMCPNodeId.cpp:292-294` HAS `FindGraphByName` SEARCH EVERY ARRAY BECAUSE "an ID names a graph and not a kind
>     of graph. A caller asking for `EventGraph` should find it whichever array the Blueprint happens to keep it in." ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
