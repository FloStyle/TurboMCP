# NOV09-01 Tidy output repair

## Current review

The 9 November field call returned about 451,000 characters for a scoped 65-node preview. The current `RenderPreview` emits complete before/after `CompactGeometryEvidence` before its verdict. The measured quality scores and geometry are useful, but the default response hides the decision and can exceed client limits. `Apply` likewise emits complete geometry after a successful edit. This is a presentation defect; it is not evidence that planning or layout arithmetic is wrong.

## Approved repair

Put the preview decision and moved-node count first. Keep the seven compact scores and safety/refusal explanation in the default result. Omit per-rectangle, per-edge and per-segment evidence by default. Add optional, bounded `GeometryOffset` and `GeometryLimit` parameters to preview. An explicit request returns a numbered slice of the same scoring evidence with total row count and next offset. Reject invalid offsets or limits. Each preview replans current editor state; a page is evidence for that current plan only. The edit result retains the seven measured scores and omits full geometry; requesting another edit solely to fetch a page would risk a second mutation. The edit does not save or compile.

Keep native preview image paths and existing non-geometry diagnostics. Do not change scoring, layout positions, graph mutation or safety guards. Do not claim that a paged output is a stable snapshot across intervening editor changes.

## Exact implementation scope

`nov09-tidy-output` at the approved base is limited to the Graph Tidy implementation, wrapper, public header and tool help declaration named in `roadmap/worker-scopes.json`. Integrator owns the field ledger, tests, help catalog and status updates.

## Focused verification

Build the plugin under the stock 5.8.4 supervisor. On a disposable graph, check that default preview starts with the verdict, remains under a bounded response size, retains the seven scores and skips `score rect` rows; request consecutive evidence pages and check their row numbering and total. Confirm invalid page arguments refuse without graph changes. Use one focused Tidy workflow and preserve any existing unrelated suite failure. The reported 450-node field graph is unavailable locally, so its exact response size remains a field follow-up.
