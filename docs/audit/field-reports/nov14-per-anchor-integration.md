# Append per-anchor integration review — 2026-11-14

Reviewed candidate: 96a26b439ae126c1ed39473e813c6d5ba4d030b8. Combined source reviewed against 6c8bfac4; latest scope gate against c89d17cf passed.

## Accepted implementation slice

Opt-in Placement=per-anchor groups connected new nodes, including data dependencies, and places independent components near fixed connections. Existing nodes remain fixed. Earlier component rectangles reserve space. Missing anchors, multiple execution sources and unavailable local placement refuse; there is no silent outside fallback. Existing local/outside modes remain available.

## Evidence and limits

- Supervised stock-engine build and focused TurboMCP.GraphAppend.Placement passed at the candidate. Execution e29fc29dce2748c2b6058f92e013518d; 16 fixture files restored.
- Four independent planner anchors, repeatability, fixed existing coordinates and conflicting-source refusal passed.
- Earlier full append execution 5d19da114b70490daa7c791ea388ec70 failed an outdated placement-mode expectation. It was corrected, then only the focused case was rerun. This is not a full-suite pass. Previously recorded append failures remain retained.
- Public per-anchor preview/apply, shared-producer and close-component collision scenarios still need focused evidence. The captured public fixture uses local mode, not per-anchor. No per-anchor visual acceptance is claimed.
- Multiple execution sources are conservatively refused, including potentially compatible sources.
- NOV09-03 remains open as partial implementation. NOV09-02 explicit movable-tail insertion is not implemented.

## Takeover

Finish per-anchor public workflow and geometry coverage in one batch, then implement explicitly scoped tail movement under the approved placement contract. Reuse valid evidence; run only focused checks for changed behavior. Continue remaining backlog by safety and impact, preserving unknown causes and partial capabilities. Keep DSH paused, stock Unreal, private main and dashboard current. Testing remains separate from implementation status.
