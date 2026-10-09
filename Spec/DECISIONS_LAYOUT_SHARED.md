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

## D-LAY-4: the two passes measure DIFFERENT things, and that is not a conflict

**DECISION: `P/E-GRAPH-KNOTS` is judged on WIRES CROSSING NODES. `P/E-GRAPH-TIDY` is judged on TOTAL WIRE
LENGTH. Neither is judged on the other's metric.**

This is written down because the tidy workstream flagged it as its RISKIEST rule: *"whether added reroute
knots count toward the measured wire length. A knot has its own rectangle and `PinPosition` gives it pins,
so a wire through a knot measures as two straight segments and can be LONGER than the single segment it
replaced - while crossing fewer nodes, which is the actual goal."*

THE TWO SPECIFICATIONS ALREADY SETTLE IT, IN THEIR OWN WORDS, AND THEY AGREE:

- `Spec/03_TOOLS.md`, `P/E-GRAPH-KNOTS`: **"Counts wires crossing nodes before and after. Rolls back if not
  better."** The metric is CROSSINGS;
- `Spec/03_TOOLS.md`, `P/E-GRAPH-TIDY`: **"Compares total wire length from before to after. Rolls back if
  worse."** The metric is LENGTH.

SO THERE IS NO CONFLICT TO RESOLVE. The fear that tidy could roll back a visually better layout only holds
if one pass were judged on the other's number, and it is not. **A knot that lengthens a wire while removing
a crossing is a SUCCESS by knots' own metric**, which is why the capability exists: the spec's opening
clause is "only where a knot makes a wire one straight run plus one bend, OR KEEPS IT FROM PASSING OVER A
NODE", and the second of those is worth a longer wire.

**THE ONE REAL CONSEQUENCE IS ABOUT LABELS, NOT CODE.** A wire routed through a knot IS two segments, so
its length really is longer - pretending otherwise would describe a graph that does not exist. Both passes
must therefore print their number AS THE THING THEY MEASURED: `crossings before/after` for knots, `total
wire length before/after` for tidy. **A caller comparing the two across tools would be comparing two
different quantities**, and the labels are what prevent that.

WHAT REMAINS GENUINELY OPEN: **the ORDER to run the two passes in.** The specs do not mandate one, and the
engine's own editor places knots on demand rather than as a pass, so there is nothing to copy. Running
knots then tidy lets tidy shorten the wires a knot lengthened; running tidy then knots lets the knot pass
work on a settled layout. **Neither is wrong.** The honest answer for a caller is to run one, LOOK at its
number, and decide - which is what `P7` asks for.

---

## D-LAY-5: a node's rectangle CAN be zero, and every geometric pass must REFUSE rather than compute

**DECISION: a pass doing arithmetic on node rectangles refuses when a rectangle it needs has zero width or
zero height, and NAMES the node.**

THIS CORRECTS A CLAIM IN `TestRecord.md` SECTION 6g, WHICH WAS WRONG. It states that node rectangles are
real in a headless editor, "proven by `L-LAYOUT-REPORT` reporting longest single wire 400". **The reasoning
was wrong and another workstream proved it by measuring.** Dumped from the commandlet for every node it
built:

```
node K2Node_CallFunction_3 at (1050,90) size (0,0) rect x 1050..1050 y 90..90
```

EVERY node: `size (0,0)`. `NodeWidth` and `NodeHeight` are int32 fields SLATE fills when a node is DRAWN,
and a commandlet draws nothing.

THE EARLIER EVIDENCE WAS NOT WRONG, IT WAS TAKEN IN A DIFFERENT STATE. `L-LAYOUT-REPORT` was run with the
asset OPEN IN A RENDERED EDITOR, where Slate had already filled the sizes in. **Both states are real:**

| State | `NodeWidth` / `NodeHeight` |
|---|---|
| a node in an asset OPENED IN THE EDITOR | real, filled by Slate when it is drawn |
| a node freshly built by a COMMANDLET, never drawn | ZERO |

**CONSEQUENCE FOR EVERY LAYOUT CAPABILITY, and it is why this is a shared decision rather than a note.**
A layout pass runs in the editor and will NORMALLY see real sizes. But "normally" is not a guarantee, and
`P/E-GRAPH-KNOTS`' arithmetic - "a wire is one straight run plus one bend" - operates ON rectangles.

**REFUSING OUTRIGHT WAS TRIED AND IS WRONG, WHICH CORRECTS WHAT THIS DECISION FIRST SAID.** The
implementation refused when a rectangle had no size, and the tool became useless on every graph it exists
for - confirmed live. `P/E-GRAPH-KNOTS` now uses a DOCUMENTED FALLBACK:

| What | Fallback | Where |
|---|---|---|
| a node with no measured size | a 48-unit square centred on its position | `MinimumNodeExtent` |
| a knot with no measured footprint | 32 units | `FallbackKnotExtent` |

**AND EVERY REPORT STATES WHICH GEOMETRY IT USED.** That is the part that makes the fallback honest rather
than a guess dressed as a measurement. The judgement is CONSERVATIVE: it UNDER-REPORTS a crossing rather
than inventing one, so a zero-size graph yields fewer knots than it might deserve and never a wrong one.

THIS IS THE RULE, AND IT REPLACES "refuse": **a pass may not SILENTLY substitute a size.** It may
substitute a documented one, must say so in its output, and must err toward reporting less.

WHY NOT REFUSE: a point is a real answer to "does this wire pass through where this node is", which is
exactly `T-LAY-10`'s question. Refusing answers nothing, and a capability that refuses everything is
indistinguishable from one that is not implemented.

THE FIXTURE PROBLEM IS ALSO SOLVED, and separately: `TurboMCPAcceptanceAssetCommandlet.cpp` sets explicit
fallback sizes on the nodes it builds, so the asset it writes carries geometry a test can measure WITHOUT
opening a graph editor.

**STILL OPEN AND NOW THE MOST VALUABLE FIXTURE WORK IN THE PROJECT:** the fixture needs obstacles with
sizes the ENGINE STORES, not a fallback the commandlet writes, or `T-LAY-10`'s positive case stays
half-tested. `UEdGraphNode_Documentation` is the ONE node type whose size the engine itself persists
(`UEdGraphNode_Documentation::ResizeNode` and `::SetBounds` are the only writes to `NodeWidth` in the whole
engine tree), so it is the candidate - and whether it can be created from a commandlet is NOT YET PROVEN.

---

## WHAT IS STILL OPEN IN EACH SPEC, and is genuinely open

- tidy OPEN 2: implement getter merging or refuse it by name this phase.
- fix-backward's five: see its own section.
- the ORDER of the two layout passes: see `D-LAY-4`.
