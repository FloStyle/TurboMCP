# Local append placement and explicit fixed anchors

OCT27-05 approved repair contract, 2026-10-28.

## Source review

GraphAppendPlacement already resolves an execution predecessor (or pure consumer), and tries a gap for one inserted call. General placement starts below the attachment and repeatedly shifts downward around collisions; it never compares a nearby alternative above the attachment. Distant obstacles can therefore push a connected block far away. Preserve the existing single-call, branch and pure-input behavior while fixing this fallback. GraphTidy rejects every MainPath node outside the selection, including a fixed first predecessor that should remain immovable.

## Required behavior

For connected blocks, try measured collision-free positions near all relevant fixed endpoints, including above and below obstructions, before accepting a distant fallback. Respect native geometry, comment ownership, coordinate limits and intended links. Prefer the execution lane when feasible. Refuse with concrete placement guidance when no bounded local candidate fits; do not silently move existing nodes or claim that a distant location is local. Detached blocks retain an explicit separate placement policy.

Allow exactly the first explicit MainPath node to be a fixed execution anchor outside the selected scope. It must connect to the next selected node; all later path nodes must be selected and distinct. The anchor remains unchanged. Auto path semantics remain unchanged. Unsupported or ambiguous input is refused before mutation.

Repair the full test-graph-append wrapper to use current reader schemas and connected positive fixtures. Keep real identity/default/link and saved-byte preservation checks. Ignore only explicitly identified observational metadata when comparing preview/edit refusal reasons; never hide behavioral differences. Reconcile native placement assumptions with measured geometry and current preflight orphan safety. Retain the failed d9f1bd67c21747adb8f78a725cad4627 evidence and document why each assertion changed.

## Verification

Run sparse/dense local insertions, short chain, explicit fixed anchor outside selection, occupied-space refusal or safe local alternative, unchanged existing coordinates/links, and native screenshots. Run the full append suite, not only its literal subset. No gameplay, persistence or arbitrary project guarantee follows. Stock engine; DSH paused.

## Ownership

Assignment oct27-append-local owns append placement, tidy MainPath handling and their scoped native fixtures plus the existing full append wrapper. Shared tool help, catalog and tracking are integrator-owned. Do not widen to unrelated parser or graph edits without a new source-reviewed scope.
