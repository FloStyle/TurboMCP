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

## Candidate checkpoint

Execution a974edd563d34946886b854c87de7e62 built successfully. The focused
J5 run stopped at its historical refusal assertion: the same input now reports
WOULD APPLY, Body row drift 3513 -> 3113, and overlaps 0 -> 0. Independent
readback confirms preview geometry and graph semantics are unchanged. This is
preview evidence only, not application/undo acceptance. The failed run remains
recorded. Updated assertions require successful application, unchanged external
geometry and semantics, and exact undo. A constrained-obstacle refusal control
and final applied screenshots remain required. No task closure or merge yet.

## Focused application result

Execution 55bdea70983a4ac2b6aae7696d1e8a1d passed the stock-engine build and
boundary-only workflow (472 seconds total). Candidate tree digest:
29bbb5af61bdf083a041ea140848545a86826234bc766e5ba6e75948739d5233.
Evidence: .local/tidy-cascade/2ed27fc5d51a44cea795047244108a3e/evidence.json.
The same 26-node selection now applies without Force. Preview is immutable,
unselected rectangles and graph semantics remain identical, and undo restores
all original positions and semantics. Body drift improves 3513 -> 3113 pixels.
The existing native TidyFeedback fixed-obstacle/containment controls passed
(1 test, no errors or warnings). Applied overview inspected: separated blocks,
with long boundary wires retained. This is not full-cascade visual acceptance.
Completed-above-body remains 3 -> 3; NOV05-02 remains partial/open for that
remaining layout limitation. The earlier failed assertion run is retained.

## Integration review

Candidate 083abfc263d6b9f64b359d44ffded84ca0389923 passed the approved
exact-file scope gate. Offline gate 3a8d58f5a7164c7982deb2ace3c71825
passed all checks. Independent saved-evidence comparison matched all 83
preview coordinates to applied readback, with zero mismatches. Native overview
and detail were inspected; long boundary wires remain visible, and this is
not a claim of full visual acceptance. The default suite subset is restored
to all; the recorded execution used boundary only. Accepted for private main
as a partial scoped-placement repair, with NOV05-02 still open.

## Scope reconciliation after integration

Saved-evidence inspection of 2ed27fc5d51a44cea795047244108a3e confirms that
all three loop controllers and their direct Completed targets are outside J5's
26-node L3B selection: N12 -> N23, N24 -> N71 and N27 -> N56. Their continuation
rectangles are identical before/after: (3101,165,201,82), (5372,257,175,77), and
(7542,1847,135,100), respectively. No new editor execution was needed.

GraphTidy's CompletedAboveBody diagnostic iterates every controller in the graph,
not only selected controllers. Thus its unchanged 3 -> 3 count is not isolated
evidence of a regression in the repaired scoped placement. Moving those fixed
continuations to improve that count would violate the selection contract. This
does not establish that every selected body arrangement is optimal, nor close
NOV05-02. The next acceptance case must include complete loop controllers,
bodies and continuations, and compare the accepted cascade reference. Preserve
J5's outside-geometry assertions; do not widen its scope or relabel its passing
boundary evidence as full loop-layout acceptance.

## Complete-loop evidence reconciled

Located execution 1400dc7bcf324bbb859108b8343d9239 in the nov09-graph-reader
worktree, evidence e2df6704e2d64467b51b5704d2d85170. Its compact/cascade
case records Body drift 3513 -> 0, Completed-above-body 3 -> 0, no score
regressions and restored undo. Acceptance remains false in that evidence.
The L3B detail 9A3154B14ED5BC6E7EC74988D19E0F25 was inspected: nodes are
separated and the body runs horizontally, but the curved Completed wire runs
through the lower expression area. The routing overview
17AA998A4E325BE2325866B2AF428C85 is too reduced to prove local wire clearance.
Do not treat that overview or numeric success as complete visual acceptance.
Next inspect the corresponding routed detail and accepted reference; improve
only confirmed remaining routing behavior. The previous next-step text about
Completed-above-body remaining unresolved was stale for complete-loop scope.

## Approved follow-up: native spline clearance

Readiness: ready. Integrator-approved scope remains the same three files.

The routed after-block-4-row-0-col-0 image from preview
081D441A4A2BD31C228B90B6CB6DBF29 shows the white execution spline crossing
the lower integer-add node. This is the routed variant, not the earlier
unrouted capture. PlanOrthogonalRoutes checks obstacle clearance using straight
segments while sampling native splines only for wire-to-wire ranking.

Repair candidate clearance against native spline geometry, including generated
knots and unrelated comment obstacles. Preserve source/destination endpoint
exceptions and scope. Do not call an accepted route collision-free merely
because its straight chord is clear. Where candidate routes fail, retain the
original connection and report the skipped route honestly. A skipped route
alone does not resolve the requested layout.

Add a focused regression with a node beside a vertical chord but inside the
native spline bulge. Verify a clear alternate channel or an explicit skipped
route, unchanged logical connections and undo. Build once after the batch and
inspect one focused native image; do not repeat the full cascade. Completion
requires the actual routing issue to be resolved, not just a stronger refusal.
