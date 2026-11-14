# NOV05-02 scoped loop placement repair

Readiness: ready
Owner: integration owner
Current review: 2026-11-14

The existing L3B scoped cascade case is a reproducible remaining limitation:
the proposed LoopBody vertical separation increases from 3513 to 3696 pixels.
The current guard prevents an unconfirmed edit; that refusal is not a placement
fix. Preserve the guard for genuinely infeasible cases.

Source review: GraphTidy.cpp's selected-component re-anchoring pass explicitly
excludes nodes with an OwnerOfNode comment membership. Its candidate scoring
also runs before the final LoopBody regression guard. This is a candidate
repair location, not yet a confirmed complete explanation of L3B's result.

Implement a bounded placement correction for selected compact loop bodies.
Keep all unselected node and comment rectangles fixed. Preserve selected
internal order, graph links, defaults and comment membership. Evaluate proposed
translations against fixed obstacles, containment and every boundary wire;
do not improve one handoff by introducing another boundary regression. Reuse
the existing overlap and boundary limits. Where no feasible candidate exists,
retain an accurate refusal rather than silently widening the scope.

Evidence: run the existing focused L3B case, record original and proposed
separation, preview immutability, exact unselected geometry, graph semantics,
and undo restoration. Add a constrained-obstacle control that still refuses.
Change the old test's required refusal only when the same input demonstrably
has a valid corrected placement. Capture one overview/detail of the repaired
case. Do not rerun the entire cascade after every edit or claim full cascade
acceptance from this case.

Exact implementation scope: TurboMCPGraphTidy.cpp,
TurboMCPTidyFeedbackTest.cpp and Tools/tidy_loop_cascade.py. Tracking and
catalog edits remain integrator-owned. NOV05-02 stays open until the observed
placement failure is repaired and evidenced.
