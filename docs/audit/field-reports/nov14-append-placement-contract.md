# Bounded append placement repair

Integration owner: Codex. Design approved for NOV09-02/03; implementation and runtime evidence pending.

## Current gap

TurboMCPGraphAppendPlacement.cpp chooses the first execution attachment and plans all created forms as one block. FPlan::PlannedPositions contains only created forms. TurboMCPGraphAppend.cpp repeats placement after measurement and applies created positions in the append transaction. Existing-node movement must therefore be represented in the plan, validated again after measurement, and applied in the same transaction; a post-append tidy call is not equivalent.

## Explicit insertion

Keep existing local and outside modes unchanged. Add an opt-in insertion mode with an explicit list of movable existing node IDs and an optional containing comment ID. The list is an upper bound on movement, not permission to move other nodes reached by traversal. Empty movement scope must never imply the whole graph.

For an inserted execution chain, reserve measured space between the selected attachment and its existing successor. Shift only the necessary explicitly allowed downstream nodes horizontally. Preserve their relative positions. Treat shared consumers, branch joins, loops and linked nodes outside the list as fixed constraints; refuse an infeasible plan with the conflicting node IDs. Do not silently expand the movement list or fall back to outside placement.

For boxed insertion, retain the comment's top-left, existing membership and nested contents. Rightward growth is allowed only for the explicitly named containing comment and only when it clears fixed neighbors. Moving a nested comment requires all its contained nodes to be explicitly included; otherwise it is fixed. No automatic enlargement of ancestors or unrelated comments.

Preview must list every existing node's old/new coordinates, every comment size change, created positions, estimated dimensions and refusal reasons. Apply must validate that node identities, links, geometry and membership used by the preview still match. A stale plan refuses before mutation. Failed apply restores all changes; one undo restores geometry, comment sizes, nodes and links. Compilation and saving remain separate.

## Independent groups

Add a separate per-anchor mode. Partition newly created nodes by connectivity, including shared pure dependencies. Each independent component uses its own fixed execution source or data consumer as anchor. A shared dependency joins groups rather than being duplicated. Multiple incompatible fixed anchors must be reported, not resolved by iteration order.

Plan every component against fixed geometry and previously planned components in a deterministic order. Keep pure producers left of their consumers. This mode does not move existing nodes. If local space is unavailable, report the blocked component and anchor; outside placement remains an explicit caller choice. Explicit insertion and per-anchor placement may share measurement and collision helpers but must not blur movement authorization.

## Focused acceptance

Use one synthetic boxed graph with an insertion chain and an unrelated neighboring lane, plus four disconnected groups with distinct anchors. Verify preview/apply equality, exact allowed movement, fixed neighbors, links, stale-plan refusal and undo. Add a shared-consumer conflict and a blocked-growth refusal. Inspect one final editor capture per visual case after batching changes; do not rerun the whole tool inventory. Retain existing append-suite failures until individually resolved.

## Implementation boundary

Expected files: Public/TurboMCPGraphAppend.h, TurboMCPGraphAppend.cpp, TurboMCPGraphAppendPlacement.cpp, Public/Tools/TurboMCPToolsets.h, the append tool entry point and its existing native placement fixture. Locate and record exact targets in worker-scopes.json before claiming implementation. Catalog and status updates remain integrator-owned. No implementation completion is claimed by this design document.
