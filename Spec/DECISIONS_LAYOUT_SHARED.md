# SHARED DECISIONS: the layout passes must answer the same way

`Spec/WORKING_GRAPH_TIDY.md` and `Spec/WORKING_GRAPH_FIX_BACKWARD.md` were written independently, and the
FIX-BACKWARD workstream found that **three of their open items are THE SAME QUESTION ASKED TWICE**:

| Tidy's OPEN | Shared | The question |
|---|---|---|
| 3 | **YES** | does the pass apply to event graphs, function graphs, or both? |
| 4 | **YES** | when a node sits in NESTED boxes, which box lays it out - the tightest or the outermost? |
| 5 | **YES**, weakly | is a box's parent resolved by `NodeDepth` or geometrically? |

**TWO INDEPENDENT ANSWERS AGREE UNTIL ONE OF THEM IS CHANGED**, which is this project's most-recorded
failure mode. So these are settled ONCE, HERE, and both specs cite this file rather than stating a rule of
their own.

Two of tidy's items are deliberately NOT shared, and the FIX-BACKWARD workstream established why:
- **OPEN 1** (do added knots count toward the measure) does not apply, because FIX-BACKWARD **adds no
  knots**;
- **OPEN 2** (merged getters) does not apply, because FIX-BACKWARD **moves** getters and never copies or
  merges one.

---

## D-LAY-1: both passes apply to ANY graph that has nodes

**DECISION: any `UEdGraph` the asset resolver returns, with no filtering by graph kind.**

WHY. Both passes MOVE nodes and CREATE NOTHING. A `UEdGraph` either has node positions or it does not, and
both passes already refuse when a rectangle is degenerate, so no graph-kind test is needed to stay safe.

A KIND TEST WOULD BE WORSE THAN USELESS HERE. It would have to name the kinds, and a kind it failed to name
would silently exclude a graph from a pass the user asked for - a whole graph quietly skipped, with the
report's own counts still looking right. **A REFUSAL THAT NAMES A REASON beats an exclusion nobody sees.**

WHAT IT COSTS: nothing measurable. `SaneProbe` - the graph a measure-and-roll-back pass must leave ALONE -
is a FUNCTION graph, and it is already the rollback fixture for both.

**WHERE EACH PASS ENFORCES IT:** in `Plan`, by refusing when the resolved graph has no nodes, with the
graph named in the refusal.

---

## D-LAY-2: a node inside nested boxes belongs to its TIGHTEST box

**DECISION: the INNERMOST box that contains the node lays it out.**

WHY, AND THIS IS THE ONE THAT MATTERS. The engine already decides membership GEOMETRICALLY -
`FSlateRect::IsRectangleContained` in graph space, with a box's rectangle starting `TitleBarHeight` BELOW
its position, because the title bar's own strip sits above it. That test does NOT pick a winner: a node
inside `INNER` is inside `OUTER` too, so **geometry alone says a nested node belongs to two boxes at once.**

`FTurboMCPComment::MembersOf` is the ONE copy of that rule in this plugin, promoted for exactly this
reason. It answers "which nodes does THIS box enclose", and both passes call it per box.

THE CHOICE IS THE INNERMOST, and the reason is that a box exists to say something about the nodes inside
it. An `INNER` box that changed nothing, because every node it contains was laid out by the `OUTER` box
around it, would be a box that cannot do the one job a box has. **THE INNERMOST BOX IS THE MOST SPECIFIC
STATEMENT, and the most specific statement is the one that should win.**

IT ALSO MAKES THE TWO PASSES AGREE FOR FREE, which is the point of this file: the FIX-BACKWARD spec's
"inside one box" and tidy's nesting rule are now the same rule rather than two that happen to coincide.

**WHERE EACH PASS ENFORCES IT:** computed from the box rectangles, taking the box with the SMALLEST area
among those that contain the node, with ties broken by the LATER box in graph order so the result is
deterministic. Never from `UEdGraphNode_Comment::GetNodesUnderComment`, which is a cache filled from
RENDERED widgets and is therefore EMPTY headlessly.

---

## D-LAY-3: a box's parent is resolved GEOMETRICALLY, not from `NodeDepth`

**DECISION: geometric containment, the SAME rule as `D-LAY-2`. `NodeDepth` is REPORTED, never used to
decide.**

WHY. Resolving a parent one way while deciding membership another way gives two answers that agree until a
graph is edited in a way that changes one and not the other. `Spec/04_FORMATS.md` and `R-COMMENT-LIST`
already REPORT `CommentDepth`, so the information is available to a caller that wants it.

`NodeDepth` is a stored field, and a stored field can disagree with geometry when a box is resized by hand
or created by a tool that did not maintain it. Geometry cannot disagree with itself.

**THIS IS ALSO WHY IT IS A WEAK SHARE, as the FIX-BACKWARD workstream put it:** both passes need the
innermost box, and both get it from `D-LAY-2`. The parent chain is only needed to REPORT nesting, so the
choice costs nothing either way - but it is written down so that neither pass invents a second rule later.

---

## WHAT IS STILL OPEN IN EACH SPEC, and is genuinely open

- tidy OPEN 1: do added knots count toward the measured wire length. **Bears on `P/E-GRAPH-KNOTS` as well**,
  because a knot adds length while removing a crossing, and the two passes could fight: knots make a wire
  longer, then a measure that counts length rolls the knots back. **THIS ONE STILL NEEDS AN ANSWER AND IS
  THE SHARED RISK WORTH SETTLING NEXT.**
- tidy OPEN 2: implement getter merging or refuse it by name this phase.
- fix-backward's five: see its own section.
