# WORKING SPEC — `P/E-GRAPH-KNOTS` (T-LAY-10)

Status: spec for implementation. Not implemented.
Author: reconnaissance pass, round 32. Every engine symbol below carries the file and line it was read
from, because five guessed API names in a row have turned out wrong in this project.

---

## 1. What the spec asks for, and what each clause has to become

`Spec/03_TOOLS.md:455-462`, verbatim:

> Only where a knot makes a wire one straight run plus one bend, or keeps it from passing over a node.
> Skip wires between neighbouring columns. Place each knot level with either the source or the target
> pin, in free space. Never stack two knots. When one pin feeds several distant nodes, share a single
> knot.
>
> Counts wires crossing nodes before and after. Rolls back if not better.

`Spec/06_ACCEPTANCE.md:104` gives the observable:

> | T-LAY-10 | Knots | Preview lists only wires that cross nodes; after Edit those wires are straight |

**THE ACCEPTANCE LINE IS NARROWER THAN THE PROSE, AND THAT IS THE DESIGN CONSTRAINT.** T-LAY-10 asks
for exactly two things: the Preview lists only wires that CROSS A NODE, and after the Edit those wires
are STRAIGHT. The prose adds qualifiers (one bend, neighbouring columns, level with a pin, free space,
never stacked, sharing). Each qualifier is a REFUSAL or a PLACEMENT RULE, and each must be a concrete
predicate, because the agent-facing failure mode of this capability is a knot placed where it makes
the graph worse.

### 1.1 "wires crossing nodes" — the primary predicate

The project already has the geometry it needs and does NOT have the predicate. Measured:

- `FTurboMCPLayoutReport` has `FRect` (`TurboMCPLayoutReport.h:35-47`) with `MinX/MinY/MaxX/MaxY` and
  `Width()/Height()`, and `RectOf(UEdGraphNode*)` (`TurboMCPLayoutReport.cpp:86-93`) which fills it
  from `NodePosX`, `NodePosY`, `NodePosX + NodeWidth`, `NodePosY + NodeHeight`.
- `FTurboMCPLayoutReport::PinPosition(const UEdGraphPin*)` (`TurboMCPLayoutReport.cpp:95-136`) computes
  a pin's position from the node rectangle and the pin's index among same-direction pins, using a
  documented fixed row spacing. It is the ONLY pin-position convention in this project and this
  capability MUST reuse it, or two tools would disagree about the same wire.
- **The findings a layout report can produce are `backward-exec`, `backward-data`, `overlap`,
  `getter-place`, `stray`, `disconnected`** (`TurboMCPLayoutReport.cpp:284,321,333,379,448,526`, and
  the `Kind` doc comment at `TurboMCPLayoutReport.h:52`). **There is NO crossing finding.** Confirmed
  by the summary line the report prints when clean — `TurboMCPLayout.cpp:136`:
  `"findings: none. No backward wire, no overlap, no misplaced ..."` — which names four of the six and
  no crossing.

So the crossing test is NEW CODE, and it lives in the knots module rather than in the report, because
adding it to the report would change the output of `L-LAYOUT-REPORT` and therefore the baseline that
`T-LAY-1` asserts.

**RULE C1 (a wire crosses a node).** For each wire between a source pin and a target pin, taken from
the pin pair itself and NOT through knots:

1. `A = ResolveThroughKnots(source owning node)`, `B = ResolveThroughKnots(target owning node)`.
   ResolveThroughKnots already exists and walks a knot's single output pin
   (`TurboMCPGraphText.cpp:47-70`). An existing knot therefore does not hide a crossing.
2. If `A == B`, the wire is ignored. A self-wire of a knot is not a routing problem.
3. `S = PinPosition(sourcePin)`, `T = PinPosition(targetPin)`.
4. For every FLOW node `N` in the graph that is neither `A` nor `B`, and is not a knot: let
   `R = RectOf(N)`. The wire crosses `N` when the closed segment `S->T` intersects the OPEN
   rectangle `(R.MinX, R.MinY, R.MaxX, R.MaxY)`. Open means touching an edge exactly does not count,
   for the same reason `RectsOverlap` treats touching edges as not an overlap
   (`TurboMCPLayoutReport.cpp:41-48`): two nodes side by side share a border.

**USE `FMath::LineBoxIntersection`, OR WRITE THE 4-EDGE TEST.** The segment-rectangle test is four
segment-segment tests plus the containment case. `FMath::LineBoxIntersection` is in
`Runtime/Core/Public/Math/UnrealMathUtility.h` — **verify its exact overload before use**, because
this spec has not read that line. Writing the four-edge test with `FMath::SegmentIntersection2D` is
also acceptable and needs no new API surface. Either way the test must be shown to fail on a wire that
does cross and pass on one that does not; see §7.

**RULE C2 (the whole wire, not the first crossing).** A wire may cross several nodes. The unit the
tool reports and fixes is the WIRE, once, naming every node it crosses. Reporting once per crossing
would make a Preview that lists the same wire three times and make "before" and "after" counts
incomparable.

### 1.2 "one straight run plus one bend"

`Spec/03_TOOLS.md:457` says a knot is placed "only where a knot makes a wire one straight run plus one
bend, or keeps it from passing over a node." The "or" means the two conditions are alternatives, and
the second is RULE C1. So this clause only CONSTRAINS WHERE, given a crossing wire.

**RULE C3 (the knot's position is level with a pin).** `Spec/03_TOOLS.md:459-460`: "Place each knot
level with either the source or the target pin". So a knot's position is one of exactly two
candidates, not a free choice:

- `K_source = (T.X, S.Y)` — level with the target pin's X, on the source pin's row. The route is then
  one horizontal run from `S` to `K_source` and one vertical run from `K_source` to `T`.
- `K_target = (S.X, T.Y)` — level with the source pin's X, on the target pin's row. The route is one
  vertical run then one horizontal run.

**"one straight run plus one bend" is exactly this shape**: the route through the knot consists of one
axis-aligned run and one axis-aligned run meeting at a right angle. A knot placed anywhere else makes a
route with two bends, which is what the clause forbids.

**THE TWO BEND COUNTS, MEASURED.** Before the knot, the wire `S->T` is a single unbroken run with zero
bends. After the knot there is one bend. So this clause does NOT mean "fewer bends than before" — it
means **the route is still only one bend**, and it exists to keep the tool from introducing a third
segment. State it that way in the code comment, or a reader will try to minimise a number that starts
at zero.

### 1.3 "skip wires between neighbouring columns"

**THERE IS NO COLUMN NOTION IN THIS PROJECT.** Measured: a search for `column`/`Column` across
`TurboMCPLayoutReport.cpp` and its header returns nothing. So the columns must be DEFINED here, and the
definition must be stated in the code because it is an invention of this capability.

**RULE C4 (columns and neighbours).**

- A node's COLUMN is its `NodePosX`. Nodes do not have a discrete column index anywhere in the engine
  or the plugin, so a band has to be chosen.
- Two nodes are in the SAME column when `|A.NodePosX - B.NodePosX| <= ColumnBand`.
- Two columns are NEIGHBOURING when the horizontal gap between their rectangles is
  `<= NeighbourGap`.

**Values.** `ColumnBand = 64.0` and `NeighbourGap = 400.0`. Neither is in the spec; both are this
capability's choice, and both must be named constants with the comment that they are choices. A
starting point for the numbers, which the implementer should re-derive from `KnotProbe` rather than
copy blind: in `KnotProbe` the two calls are 1200 apart horizontally and the three probes are 300
apart, so `NeighbourGap = 400.0` treats the probes as one column and the calls as two columns that are
NOT neighbours.

**Why skip neighbours.** A wire between two nodes 300 apart cannot be worth a knot: the knot's own
width is a large fraction of the distance, and the route had no room to bend badly. The clause is a
REFUSAL, and it is cheap to get wrong in the direction of placing knots everywhere.

### 1.4 "in free space"

**RULE C5 (free space).** A candidate knot position `K` plus the knot's own footprint must not
intersect any of:

- every FLOW node's rectangle, other than the two the wire joins;
- every COMMENT box's rectangle;
- every EXISTING knot's footprint, inflated by `StackClearance`.

Using the knot's footprint rather than its centre is what makes "never stack two knots" a consequence
of the same test rather than a separate rule, which is the shape to prefer: one predicate, two clauses
satisfied.

**Values.** The knot's footprint is its own rectangle, `NodeWidth x NodeHeight`. `StackClearance =
32.0`. Both are choices and must be constants with that comment.

**MEASURED WARNING THAT APPLIES TO ALL OF §1.4.** `UK2Node_Knot` sets only `bCanRenameNode = true` in
its constructor (`K2Node_Knot.cpp:25-29`) and its `AllocateDefaultPins` (`:31-40`) creates exactly two
pins and does not touch `NodeWidth`/`NodeHeight`. `NodeWidth` and `NodeHeight` are plain `int32` fields
on `UEdGraphNode` (`EdGraphNode.h:309,313`) whose sizes the SLATE widget supplies at render time. **In a
headless editor a node can therefore have width and height zero.** Any predicate built on rectangles
must handle a zero-size node explicitly, and the honest handling is to say so rather than to pretend
the rectangle is meaningful. See §6 OPEN-1.

### 1.5 "never stack two knots"

Satisfied by RULE C5 with `StackClearance` (see above). Also enforced by construction if only ONE knot
is ever placed per wire — see RULE C7.

### 1.6 "when one pin feeds several distant nodes, share a single knot"

**RULE C6 (sharing).** Group the crossing wires by their SOURCE PIN, comparing `UEdGraphPin*` identity
and not the owning node, because two different output pins of one node are different sources. Within a
group:

- if the group has one wire, it gets its own knot;
- if the group has more than one wire, ONE knot is placed, and its position is the free-space candidate
  closest to the source pin among the candidates acceptable to EVERY wire in the group.

Grouping by source pin means the wires from one pin fan out through one knot, which is what the clause
describes. A group whose members have no position acceptable to all of them is REFUSED for sharing and
falls back to one knot per wire, and the Preview must say which wires were refused sharing and why.

### 1.7 "never more than one knot per wire"

**RULE C7.** At most one knot is inserted per wire. The spec's "one straight run plus one bend" makes
a second knot on the same wire pointless: it would add a second bend. So a wire that still crosses a
node after its single knot is REPORTED as not fixed, not fixed twice. Stating this prevents an
implementation that loops until clean, which would place knots without bound on a graph where the
route cannot avoid the obstacle.

---

## 2. Files to create, and the class

Match `FTurboMCPComment` and `FTurboMCPGraphDead` exactly in shape.

| File | Contents |
|---|---|
| `Source/TurboMCP/Public/TurboMCPGraphKnots.h` | `class FTurboMCPGraphKnots` |
| `Source/TurboMCP/TurboMCPGraphKnots.cpp` | `Plan`, `RenderPreview`, `Apply`, and the geometry helpers in an anonymous namespace |
| `Source/TurboMCP/TurboMCPGraphKnotsTools.cpp` | the two `UTurboMCP*Toolset` methods, four lines each |

The header's doc comment must carry, in this order: what the two capabilities are; the T-LAY-10
acceptance line verbatim, because it is narrower than the prose and is the contract; the RULE C
predicates summarised with the constants named; and the rollback statement.

---

## 3. The `Plan`/`RenderPreview`/`Apply` split

```cpp
class FTurboMCPGraphKnots
{
public:
    /** One crossing wire, with the knot that would fix it. */
    struct FWire
    {
        /** The source pin's owning node, in node-id form. */
        FString SourceId;
        /** The target pin's owning node, in node-id form. */
        FString TargetId;
        /** The source pin's name, so a pin is identified rather than a node. */
        FString SourcePin;
        /** The target pin's name. */
        FString TargetPin;
        /** Every flow node's id the wire crosses, sorted. */
        TArray<FString> CrossedIds;
        /** True when the wire was refused before a position was even computed. */
        bool bRefused = false;
        /** Why, when it was. Empty otherwise. */
        FString RefusalReason;
        /** The position the knot would take, valid only when bRefused is false. */
        double KnotX = 0.0;
        double KnotY = 0.0;
        /** True when this wire shares a knot with another wire from the same source pin. */
        bool bShared = false;
    };

    /** The parsed and checked request. */
    struct FPlan
    {
        FString Asset;
        FString GraphName;
        TArray<FWire> Wires;
        int32 NodeCountBefore = 0;
        int32 KnotCountBefore = 0;
        int32 CrossingsBefore = 0;
        int32 CrossingWiresBefore = 0;
    };

    static bool Plan(const FString& InAsset, const FString& InGraphName,
        FPlan& OutPlan, FString& OutError);

    static FString RenderPreview(const FPlan& InPlan);

    static FString Apply(const FPlan& InPlan);
};
```

### 3.1 What lives in `Plan`, and why

**EVERY REFUSAL LIVES IN `Plan`.** The project has learned this twice, in `P/E-VARS-ADD` and in
`P/E-OVERRIDE-ADD`: a check placed in `Apply` lets the Preview offer a plan the Edit then refuses, and
the two disagree. So all of these are decided while planning and are VISIBLE IN THE PREVIEW:

- asset and graph resolution;
- RULE C1's crossings;
- RULE C4's neighbouring-column refusal;
- RULE C5's free-space test, per candidate position;
- RULE C6's sharing and its refusal;
- RULE C7's single-knot limit.

`Apply` re-resolves the graph and the WIRES BY PIN IDENTITY before touching anything, because a plan is
made and applied in two calls and the graph can change between them. A wire whose pins no longer
connect is REPORTED and skipped, never silently dropped: a partly applied edit is what `P8` forbids.

### 3.2 `Apply`'s transaction order, per wire

For each accepted wire, in this order. The order is not arbitrary; each step depends on the one before.

1. Re-find the source and target pins on the live graph. If either is gone, or no longer linked, record
   a skip and continue.
2. `Graph->Modify()`.
3. **Create the knot**: `NewObject<UK2Node_Knot>(Graph)`, then `CreateNewGuid()`, then set
   `NodePosX`/`NodePosY` from the plan, then `Graph->AddNode(Knot, false, false)`, then
   **`Knot->AllocateDefaultPins()`**.
   **`AllocateDefaultPins` IS MANDATORY AND ITS ABSENCE CRASHES.** Measured in this project's own
   commandlet: `UK2Node_Knot::GetOutputPin` is `return Pins[1];` with no bounds check
   (`K2Node_Knot.h:54-62`), `NewObject` does not allocate pins, `UEdGraph::AddNode`
   (`EdGraph.h:148`) does not either, and the editor allocates them only when the node is
   RECONSTRUCTED for display — which never happens headless. The observed failure was
   `Array index out of bounds: 1 into an array of size 0` in `BuildKnotProbe`. `Plan` must therefore
   never call `GetInputPin`/`GetOutputPin` on a newly made knot before allocating.
4. **Break the original link** between source and target:
   `Schema->BreakSinglePinLink(SourcePin, TargetPin)`, where `Schema` is
   `Graph->GetSchema()` and the method is
   `UEdGraphSchema::BreakSinglePinLink(UEdGraphPin*, UEdGraphPin*)`
   (`EdGraphSchema.h:1057`).
5. **Connect source to the knot's input and the knot's output to target**, through
   `Schema->TryCreateConnection(A, B)` (`EdGraphSchema.h:828`), checking EACH return value. A failed
   connection leaves a wire broken in two, which is worse than the crossing it was fixing — so a
   failure at this step must trigger the per-wire ROLLBACK in §4, not be ignored.
6. `Graph->MarkPackageDirty()`.

Do NOT use `MakeLinkTo`. It is `UEdGraphPin::MakeLinkTo(UEdGraphPin*, bool)` (`EdGraphPin.h:420`) and
it links unconditionally, skipping the schema's compatibility and notification work. `BreakLinkTo` and
`BreakAllPinLinks` (`EdGraphPin.h:434,437`) are likewise lower level than
`BreakSinglePinLink`. For a WRITE, the schema's methods are the ones the editor itself uses.

---

## 4. The rollback

`Spec/03_TOOLS.md:462`: "Counts wires crossing nodes before and after. Rolls back if not better."

**THE ROLLBACK IS PER-PASS, NOT PER-WIRE, AND THAT IS THE DESIGN CHOICE.** A per-wire rollback cannot
answer "is the graph better", because it has no view of the whole. So:

1. `Plan` records `CrossingsBefore` — the total count of (wire, crossed node) pairs — and
   `CrossingWiresBefore`. It also snapshots, per accepted wire, the two pin names and the two node ids
   it needs to undo step 3-5. It does NOT need to snapshot positions, because this pass **moves
   nothing**: it only creates knots and relinks wires.
2. `Apply` performs every accepted wire, collecting the `UK2Node_Knot*` it created in a local array.
3. `Apply` then RE-MEASURES the crossings with the same RULE C1 code, over the live graph.
4. **If `CrossingsAfter < CrossingsBefore`, keep.** Report both numbers.
5. **If `CrossingsAfter >= CrossingsBefore`, ROLL BACK EVERYTHING**: for each created knot, break its
   two links and `Knot->DestroyNode()`, and re-link each original pair with `TryCreateConnection`.
   Then re-measure to CONFIRM the rollback restored the original count, and report that confirmation.
   A rollback whose result is not checked is a rollback nobody knows works.

**WHY `>=` AND NOT `>`.** A pass that leaves the count unchanged has added knots for nothing, so it
must be rolled back. "Not better" in the spec means exactly this.

**WHY THIS IS SAFE TO GET WRONG ONCE.** If the rollback fails, the graph is left with knots and relinked
wires carrying the same crossing count as before, which is a cosmetic loss and not a behavioural one:
`UK2Node_Knot::IsCompilerRelevant()` returns `false` (`K2Node_Knot.h:45`) and `ExpandNode` collapses the
knot (`K2Node_Knot.cpp:64-73`), so a knot cannot change what a Blueprint does. Say this in the code,
because it is the reason the risk is acceptable.

---

## 5. The two tools

Declare in `Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`: the Preview in
`UTurboMCPPreviewToolset`, the Edit in `UTurboMCPEditToolset`. Both take `(Asset, GraphName)`.

**A HEADER TRAP THAT HAS ALREADY BITTEN TWICE IN THIS PROJECT:** a doc comment for a generated header
must not contain a backtick followed later by an apostrophe, because UHT reads the pair as an
unterminated character constant. Measured failure: `Error: Unterminated character constant` on a line
reading `` `fit` ... the box's top-left ``. Write `the box top-left`, not `the box's top-left`, inside
any doc comment in this file. Also do not leave a stray `};` — a duplicated closing brace produced
`Error: Invalid use of keyword 'UFUNCTION'` in the same session.

```cpp
    /**
     * Work out which wires cross a node, and where a knot would fix them, without placing any.
     *
     * T-LAY-10's contract, and it is narrower than the prose: the preview lists ONLY wires that CROSS
     * A NODE, and after the edit those wires are straight.
     *
     * A wire is reported once, naming every node it crosses, even when it crosses several: counting
     * once per crossing would list one wire three times and make the before and after counts
     * incomparable.
     *
     * A knot is only ever offered where the route through it is ONE run plus ONE bend. The position
     * is one of exactly two: level with the source pin, or level with the target pin. Anything else
     * makes a route with two bends, which the spec forbids.
     *
     * REFUSED, and the preview says which and why: a wire between neighbouring columns, a wire with no
     * free space at either candidate position, and a wire already carrying a knot. Sharing is offered
     * when one pin feeds several distant nodes, and refused with the reason when no single position
     * suits them all.
     *
     * The edit MEASURES the crossing count, applies, measures again, and ROLLS BACK EVERYTHING when
     * the result is not better. A knot cannot change what a Blueprint does - it is not compiler
     * relevant - so this pass can be undone without affecting behaviour.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);

    /**
     * Place the knots the preview listed.
     *
     * The same inputs, found and judged the same way, as `P-GRAPH-KNOTS`, because the two share one
     * code path up to the write.
     *
     * Each wire is re-resolved by PIN IDENTITY first, because a plan is made and applied in two calls
     * and the graph can change between them. A wire whose pins no longer connect is REPORTED and
     * skipped rather than silently dropped.
     *
     * The crossing count is measured before and after, and the whole pass is ROLLED BACK when the
     * result is not better. The rollback is then re-measured to confirm it restored the original
     * count, because a rollback whose result is not checked is a rollback nobody knows works.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);
```

---

## 6. Includes, each with the file and line it was read from

Every symbol below was READ, not assumed.

| Include | Symbol used | Where it was read |
|---|---|---|
| `K2Node_Knot.h` | `UK2Node_Knot`, `GetInputPin`, `GetOutputPin`, `IsCompilerRelevant` | `Engine/Source/Editor/BlueprintGraph/Classes/K2Node_Knot.h:25,54,58,45` |
| `EdGraph/EdGraphPin.h` | `Direction`, `PinName`, `LinkedTo`, `GetOwningNode`, `GetOwningNodeUnchecked`, `MakeLinkTo`, `BreakLinkTo`, `BreakAllPinLinks` | `Runtime/Engine/Classes/EdGraph/EdGraphPin.h:312,306,398,468,469,420,434,437` |
| `EdGraph/EdGraphNode.h` | `NodePosX`, `NodePosY`, `NodeWidth`, `NodeHeight`, `NodeGuid`, `Pins`, `AllocateDefaultPins`, `DestroyNode`, `CreateNewGuid` | `Runtime/Engine/Classes/EdGraph/EdGraphNode.h:301,305,309,313,406,(Pins array),704,707,889` |
| `EdGraph/EdGraph.h` | `Nodes`, `AddNode`, `RemoveNode` | `Runtime/Engine/Classes/EdGraph/EdGraph.h:79,148,167` |
| `EdGraph/EdGraphSchema.h` | `TryCreateConnection`, `BreakSinglePinLink`, `BreakPinLinks` | `Runtime/Engine/Classes/EdGraph/EdGraphSchema.h:828,1057,1049` |
| `EdGraphSchema_K2.h` | `UEdGraphSchema_K2::PC_Wildcard`, used only if a type must be named | `Engine/Source/Editor/BlueprintGraph/Classes/EdGraphSchema_K2.h` (path confirmed; the exact `PC_Wildcard` line was NOT read — see OPEN-5) |
| `EdGraphNode_Comment.h` | `UEdGraphNode_Comment`, for excluding boxes from free space | `Engine/Source/Editor/UnrealEd/Public/EdGraphNode_Comment.h` |
| `TurboMCPLayoutReport.h` | `FTurboMCPLayoutReport::FRect` | `Source/TurboMCP/Public/TurboMCPLayoutReport.h:35-47` |
| `TurboMCPNodeId.h` | `MakeForNode`, `MakeKey` | `Source/TurboMCP/Public/TurboMCPNodeId.h` |
| `TurboMCPAssetResolver.h` | `Resolve`, `SplitList` | `Source/TurboMCP/Public/TurboMCPAssetResolver.h` |

**`PinPosition` MUST BE REUSED, NOT REIMPLEMENTED.** It is `FTurboMCPLayoutReport::PinPosition` at
`TurboMCPLayoutReport.cpp:95-136`, it is a static-or-public member (verify its access before calling),
and its convention is: inputs index down the LEFT edge at `NodePosX`, outputs down the RIGHT edge at
`NodePosX + NodeWidth`, each counted among pins of the same direction, at
`NodePosY + PinRowSpacing * (Index + 1)` with `PinRowSpacing = 24.0`
(`TurboMCPLayoutReport.cpp:26,126-133`). The first-row offset is documented as arbitrary and as
cancelling out when two wires are compared. Two tools disagreeing about where a wire's ends are would
be an invisible inconsistency, so if the access is wrong, WIDEN IT rather than copying the arithmetic.

### OPEN items

- **OPEN-1. ZERO-SIZE NODES.** A headless editor never renders a node, so `NodeWidth`/`NodeHeight` may
  be zero, making every rectangle a point and RULE C1, C4 and C5 degenerate. **Measure this first**, by
  reading `NodeWidth`/`NodeHeight` for `KnotProbe`'s nodes through `L-LAYOUT-REPORT` or a diagnostic.
  If they are zero, this capability's geometry has no meaning without a size source, and the honest
  answer is to REFUSE with that as the reason rather than to place knots from points. `P/E-COMMENT` hit
  the same family of problem and solved it by reproducing the engine's geometry from positions the
  engine does store.
- **OPEN-2. `FMath::LineBoxIntersection`'s exact overload was not read.** Either read it or write the
  four-edge test with `FMath::SegmentIntersection2D`.
- **OPEN-3. THE CONSTANTS ARE UNJUSTIFIED.** `ColumnBand = 64.0`, `NeighbourGap = 400.0`,
  `StackClearance = 32.0` are choices made here. They must be named constants with a comment saying so,
  and the implementer should re-derive them from `KnotProbe` (calls 1200 apart, probes 300 apart).
- **OPEN-4. THE ACCEPTANCE ASSET HAS NO WIRE THAT CROSSES A NODE.** See §7.
- **OPEN-5. `PC_Wildcard` was not read at a line.** Only needed if a type must be named; the plan above
  never names one, because the knot's own `AllocateDefaultPins` sets its pins to wildcard.

---

## 7. Test plan — `Tools/test-graph-knots.py`

### 7.1 The fixture problem, which must be solved before any check is written

**MEASURED: `TurboMCPAcceptance:KnotProbe` CONTAINS NO WIRE THAT CROSSES A NODE.** Read from
`TurboMCPAcceptanceAssetCommandlet.cpp`'s `BuildKnotProbe`: `First` at `(400, 0)`, `Second` at
`(1600, 600)`, three knots at `(700, 200)`, `(1000, 330)`, `(1300, 460)`, and wires
`Entry -> First -> Knot0 -> Knot1 -> Knot2 -> Second`. The straight segment from `First`'s then-pin to
`Second`'s exec pin passes over no node, and the three knots are already there, so the crossing
predicate finds nothing. **T-LAY-10's "Preview lists only wires that cross nodes" therefore cannot be
tested against the committed asset as it stands.**

**So the commandlet must gain a fifth probe graph**, `KnotNeeded`, with a deliberately crossed wire:

| Node | Position | Note |
|---|---|---|
| Entry | engine-placed | the function entry |
| `CallA` (`PrintString`) | `(400, 0)` | the wire starts here |
| `Obstacle` (`PrintString`) | `(900, 260)` | sits ON the straight route from A to B |
| `CallB` (`PrintString`) | `(1500, 0)` | the wire ends here |

with `Entry -> CallA -> CallB` as exec. The straight route from `CallA` at `(400, 0)` to `CallB` at
`(1500, 0)` is the horizontal band `y` near 24-48, while `Obstacle` at `(900, 260)` is well below it —
**so those coordinates do NOT produce a crossing.** The obstacle must sit ON the A-to-B line:

- `CallA` at `(400, 0)`, `CallB` at `(1500, 400)`, `Obstacle` at `(950, 200)`.
  The straight segment from `(400, ~24)` to `(1500, ~424)` passes through `y = 200` at about
  `x = 890`, which is inside `Obstacle`'s rectangle if `Obstacle` is ~200 wide and ~100 tall starting at
  `(950, 200)` — **the implementer must verify this numerically rather than trusting the arithmetic
  here**, and adjust until `L-LAYOUT-REPORT` names the wire or until a direct probe shows the crossing.

`Tools/make-acceptance-asset.ps1` can REGENERATE the asset, so adding the fifth graph is a change to
the commandlet followed by a regeneration, and the asset is then COMMITTED again as a build artifact.

### 7.2 The checks, and how each can fail

Every check must be falsifiable, because this project's recurring failure is a check that passes while
the thing it verifies is broken. For each: what it proves, and the break that turns it red.

| # | Check | Proves | How to make it fail |
|---|---|---|---|
| 1 | `KnotProbe` preview says **zero** crossing wires | the predicate does not fire on a clean graph | break C1's segment test so it returns true for any pair — the count becomes non-zero |
| 2 | `KnotNeeded` preview names **exactly one** crossing wire, and names `Obstacle` among the nodes crossed | the predicate fires on a real crossing and identifies WHICH node | move `Obstacle` off the route in the commandlet; the preview must then say zero, so a predicate that always fires is caught |
| 3 | the preview lists each crossed wire **once**, however many nodes it crosses | RULE C2 | give `KnotNeeded` two obstacles on the route; a per-crossing implementation lists the wire twice |
| 4 | the preview names the two candidate positions and refuses one when it is not free | RULE C5 | put a node exactly at one candidate; the preview must refuse that position with a reason |
| 5 | the preview REFUSES a wire between neighbouring columns | RULE C4 | in `KnotNeeded`, move `Obstacle` so `CallA` and `CallB` are 300 apart; the wire must then be refused by the column rule, not silently accepted |
| 6 | the edit REPORTS `crossings before` and `crossings after` as numbers | the measurement exists | assert the two numbers appear; a report with only one turns it red |
| 7 | on `KnotNeeded` the edit leaves `crossings after < crossings before` and does NOT roll back | the pass improves the graph | this is the positive case; it fails if the segment test is wrong in either direction |
| 8 | on `SaneProbe` — a graph with nothing to fix — the edit says `unchanged:` and creates NO knot | the pass does nothing when there is nothing to do | make C1 always fire; knots appear and the check goes red |
| 9 | **THE ROLLBACK.** With a wire whose two candidate positions are both occupied, the edit must ROLL BACK and say so, and the graph must be left byte-identical in node count and knot count | the rollback runs AND is confirmed | disable the rollback branch; the graph keeps knots and the counts differ |
| 10 | after the edit, every wire the preview listed is now **straight**: re-run the preview and it reports zero crossings | T-LAY-10's second half, verbatim | place the knot at a position that is not level with a pin; the route keeps crossing and the second preview is non-zero |
| 11 | a knot the pass placed is **not compiler relevant**, so the Blueprint still compiles | the change cannot alter behaviour | assert `EditTurboMCPCompile` reports the same result before and after; a knot that became compiler relevant would change the compile output |
| 12 | the request is validated: no asset, a non-Blueprint, a graph that does not exist | the refusals name themselves | each is a refusal with its own wording |
| 13 | the EDIT refuses the same inputs the PREVIEW refuses, with the same reason | `D-02` | move one refusal from `Plan` into `Apply`; the preview stops refusing and the check goes red |

**CHECKS 1, 5 AND 8 ARE THE ONES THAT MATTER MOST**, because the failure that hurts is placing knots
where they make the graph worse, and each of those three fails in that direction.

**THE SUITE MUST RESET THE ASSET FIRST.** The edit mutates the fixture, so the suite calls
`pwsh -File Tools/make-acceptance-asset.ps1` in a setup step, or documents that it requires a fresh
asset. Regeneration is EXACT, which is why it was chosen over duplicating the asset for this purpose.

### 7.3 What will remain unverified

- **Whether the constants in OPEN-3 are right for any graph other than `KnotProbe` and `KnotNeeded`.**
  They are two fixtures, and a constant tuned to two graphs is a constant tuned to nothing.
- **Whether real Blueprint graphs have the shape these predicates assume.** Every fixture here is built
  by the commandlet, so a graph made by a human in the editor may have pin positions, node sizes or
  column spacings that break RULE C1's segment test.
- **Whether the sharing rule helps or hurts in practice.** RULE C6 is implemented and exercised for
  refusal, but "one knot for several wires" is a claim about readability that only a human looking at a
  graph can judge.
- **The `max_pages`-style budget behaviour.** `P6` wants capped lists. This capability's wire list is
  bounded by the number of wires, which for a large graph is not a cap. Decide whether the preview
  needs a limit and record the decision.
