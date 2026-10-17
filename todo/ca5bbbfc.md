# T29 (P4, low) — Node find covers every graph

ID: ca5bbbfc-9ffe-4ae4-b92c-32c18d688b1a
Status: todo
Priority: P4
Readiness: needs_reconciliation
Owner: unassigned

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. Old documents are recoverable from the private backup commit recorded in docs/migration.md.

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
