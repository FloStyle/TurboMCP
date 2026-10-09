# Working spec: `P/E-GRAPH-FIX-BACKWARD`

**Status:** specification, not implemented. Written against UE 5.8.3 at `C:\UnrealEngine`.
**Sibling documents:** `Spec/WORKING_GRAPH_TIDY.md` (the closest relative), `Spec/WORKING_GRAPH_KNOTS.md`
(the other measure-and-roll-back capability), `Spec/AUDIT_LOCAL_VARS_PROMOTION.md`.

This document settles every clause of the capability with a concrete, checkable rule, cites the file and
line of every engine symbol it names, and lists what it could **not** settle as `OPEN` rather than
inventing an answer.

---

## 0. The finding this spec leads with

**THE ACCEPTANCE ROW FOR THIS CAPABILITY CANNOT BE RUN, AND THAT IS THE FIRST THING TO FIX.**

`Spec/06_ACCEPTANCE.md:100` defines the acceptance test:

> `| T-LAY-9 | Fix backward on TA-4 | The two exec nodes share a row, getters left of consumers, no box moved, shorter wiring |`

`TA-4` is defined at `Spec/06_ACCEPTANCE.md:18`:

> `| TA-4 | Same Blueprint, function "stacked" | Two exec nodes in sequence with the second placed directly under the first, a getter placed under the setter it feeds, a second copy of it under a branch, one node at the origin linked to nodes far away |`

**`TA-4` DOES NOT EXIST.** Measured: the project's content folder holds exactly
`BP_NodeTest.uasset`, `TA11_TestEnum.uasset`, `TA18_ColourSuite.uasset`, `TurboMCPAcceptance.uasset` and
`TurboMCPMaterialFixture.uasset`. There is no `TA-4`, no `TA-1`, no `TA-2`, no `TA-3`. Of the thirteen
kit assets the acceptance document defines, **none is built**.

This is the same defect both sibling specs found independently, and it is worse here than for them:

| Capability | Its acceptance asset | Exists? | Can the committed asset exercise it? |
|---|---|---|---|
| `P/E-GRAPH-TIDY` | `TA-1`, `TA-2`, `TA-3` | no | partly — no comment boxes in `TurboMCPAcceptance` |
| `P/E-GRAPH-KNOTS` | `TA-1` / `T-LAY-10` | no | no — `KnotProbe`'s straight route crosses no node |
| **`P/E-GRAPH-FIX-BACKWARD`** | **`TA-4`** | **no** | **no — and nothing in the committed asset is close** |

**Why nothing committed is close.** The committed asset `TurboMCPAcceptance` holds four probe graphs.
`SaneProbe` (`TurboMCPAcceptanceAssetCommandlet.cpp:994` — **line numbers in that file move; another
workstream is adding a `LayoutProbe` graph to it, so re-read the line rather than trusting this one**) is
`Entry → Call → Result`, one row, left to
right, **no backward wire, no getter and no origin node** — deliberately, because it is the graph a
measure-and-roll-back pass must leave ALONE. `KnotProbe`, `DeadProbe` and `LocalVarProbe` contain no
backward exec wire, no misplaced getter and no node at the origin. So `T-LAY-9` is untestable today.

**§15 gives the fixture this capability needs.** It should be a probe graph rather than `TA-4`
itself, for the reason the sibling specs give: a probe graph is built by a commandlet from C++, so a
reader can see exactly what was made, whereas `TA-4` is specified as a hand-made asset in the kit.

**A FIFTH PROBE GRAPH `LayoutProbe` ALREADY EXISTS AND IS NOT ENOUGH — CHECK BEFORE BUILDING A SIXTH.**
`TurboMCPAcceptanceAssetCommandlet.cpp:775` adds `BuildLayoutProbe`, whose own comment says it *"carries a
CROSSING, NESTING, a wire between NEIGHBOURING COLUMNS that a knot pass must SKIP, and a MESSY RUN for a
tidy pass to improve"* — it serves `P/E-GRAPH-KNOTS` and `P/E-GRAPH-TIDY`. **It does NOT carry any of this
capability's four shapes**: no stacked `backward-exec` pair, no getter under its consumer, no lone node at
the origin, and no pure chain. So `LayoutProbe` is a precedent for HOW to build the fixture and for the
discipline its comment states — *"the graph is measured back after it is built and the function FAILS if
any of the three structures is absent; a build whose own report says 0 crossing is a FAILED build, not a
saved asset"* — **but it is not the fixture itself.** Either extend it or add a sixth graph named
`FixBackwardProbe`; extending is cheaper and keeps one graph per concern, which is what the existing four
already do.

**The same self-measuring discipline applies here**: the commandlet must MEASURE BACK that a wire really
runs backward, that the getter really is under its consumer, and that the stray really is beyond 1200
units, and FAIL the build if not. **Hand-chosen coordinates are exactly the kind of claim this project has
been wrong about**, and the sibling agent wrote that sentence after finding that `KnotProbe`'s
hand-chosen geometry did not produce the crossing it was built for.

**AND THE ACCEPTANCE DOCUMENT HAS A SECOND, PRE-EXISTING ROW THAT DESCRIBES THIS CAPABILITY'S INPUT.**
`Spec/06_ACCEPTANCE.md:96`:

> `| T-LAY-2 | Report on TA-4 | Finds the stacked exec wire, both backward getters and the node at the origin |`

`T-LAY-2` tests `L-LAYOUT-REPORT` on the SAME missing asset. So **two acceptance rows are blocked on one
fixture**, and building it unblocks both. That makes the fixture the cheapest correct first step, before
any of the code below is written.

---

## 1. What the spec requires, verbatim

`Spec/03_TOOLS.md`, section `P/E-GRAPH-FIX-BACKWARD: repair bad spots only`:

> - A local pass for graphs that are mostly fine. Inside one box, for an exec wire that
>   runs backward: place the next node, with any pure nodes used only by it,
>   on the row of the node before it and to its right, pushing later nodes
>   along. A getter pointing backwards goes directly to the left of the node
>   that uses it. Move a lone node at the origin next to what it links to.
> - Never moves a box. Measures and rolls back like tidy.
> - Test: T-LAY-9.

Three sentences, and every clause in them is load-bearing. Sections 4 to 12 settle each one.

---

## 2. Naming and files

Matching the existing capability modules (`TurboMCPComment.*`, `TurboMCPGraphDead.*`,
`TurboMCPGraphKnots.*`):

| File | Contents |
|---|---|
| `Source/TurboMCP/Public/TurboMCPGraphFixBackward.h` | `FTurboMCPGraphFixBackward` — `FPlan`, `FNode`, `FBox`, `FMove`, the constants, `Plan`, `RenderPreview`, `Apply` |
| `Source/TurboMCP/TurboMCPGraphFixBackward.cpp` | the pass |
| `Source/TurboMCP/TurboMCPGraphFixBackwardTools.cpp` | `PreviewTurboMCPGraphFixBackward` and `EditTurboMCPGraphFixBackward`, on `UTurboMCPPreviewToolset` and `UTurboMCPEditToolset` |

Members are declared on the toolset classes in `Public/Tools/TurboMCPToolsets.h`. **A second writer on
that header is how a merge loses a tool silently**, so the declarations are handed over as a block
(§13) rather than patched in by two people.

---

## 3. The `Plan` / `RenderPreview` / `Apply` split

Exact signatures, matching `FTurboMCPGraphDead` (`Public/TurboMCPGraphDead.h:90-97`):

```cpp
static bool Plan(const FString& InAsset, const FString& InGraphName,
    bool bInForce, FPlan& OutPlan, FString& OutError);

static FString RenderPreview(const FPlan& InPlan);

static FString Apply(const FPlan& InPlan);
```

**`bInForce` BELONGS ON `Plan`, NOT ON `Apply`,** for the reason `Spec/WORKING_GRAPH_TIDY.md` §3 gives
and this project has been caught by **twice** (`TestRecord.md` sections 4b and 4f): the Preview must be
able to say *"this would be worse and would roll back, but Force is set so it will be applied"*. A
preview that omitted that would describe a rollback the edit does not perform, which is exactly the
`D-02` disagreement.

**WHICH CHECKS LIVE IN `Plan` RATHER THAN `Apply`.** Every rule whose violation would make the Edit
refuse a plan the Preview already showed:

| Check | In `Plan` | Why not `Apply` |
|---|---|---|
| the asset resolves, the graph exists and is one of its graphs | yes | the Preview must refuse the same input with the same words |
| the graph is non-empty | yes | an empty graph has no plan to show |
| **no node has a zero-size rectangle** (§11 refusal 4) | yes | a zero-size node makes the before-number meaningless, and the Preview prints that number |
| force / no-force and the roll-back decision | yes | §3's rule above |
| **the graph is unchanged by `Plan`** | a property, not a check | `RenderPreview` must not write; a suite asserts it by node identity and position |
| the node being moved still exists and still has the planned class | **no — re-checked in `Apply` by KEY** | `Plan`'s pointers can be stale by the time `Apply` runs; this is the D-1 defect class from `Spec/AUDIT_LOCAL_VARS_PROMOTION.md` |
| the box corner guard (§11) | **both** | `Plan` refuses to plan a move that would need one; `Apply` re-checks before each write, because it is the rule whose failure silently breaks the spec's hardest promise |

**`Apply` RE-RESOLVES EVERY NODE BY KEY BEFORE WRITING IT.** `P/E-LOCAL-VARS` was audited before its
first run and had two severe ordering defects; one was that a raw `UEdGraphNode*` was used across a
synchronous compile that can reconstruct nodes (`Spec/AUDIT_LOCAL_VARS_PROMOTION.md`, D-1). This pass
runs no compile, but it does call `Graph->Modify()` and `MarkPackageDirty`, and it iterates
`Graph->Nodes` while reading positions — so the discipline is kept as a rule rather than a coincidence.

### `FPlan`

```cpp
struct FPlan
{
    FString Asset;                  // resolved object path
    FString GraphName;              // the graph examined
    bool    bForce = false;

    /** Every flow node, read once. The pass works on these VALUES. */
    struct FNode
    {
        FString Key;                // TurboMCPNodeId::MakeKey(NodeGuid), 6 lowercase hex
        FString ClassName;
        double  X = 0.0, Y = 0.0, Width = 0.0, Height = 0.0;
        bool    bIsComment = false;
        bool    bIsKnot = false;    // never moved, and never a blocker for the box guard
        bool    bIsPure = false;
        bool    bIsGetter = false;
        bool    bUnreachable = false;
    };

    /** One comment box, with its nesting parent resolved. */
    struct FBox
    {
        FString Key;
        FString Text;
        int32   Depth = 0;                  // CommentDepth
        int32   ParentIndex = INDEX_NONE;   // the TIGHTEST box around it
        TArray<int32> MemberIndices;        // non-comment nodes geometrically inside
    };

    /** One planned move. Plan carries VALUES, so the ordinary rollback is that nothing was written. */
    struct FMove
    {
        FString Key;                // the node to move, BY KEY, not by pointer
        double  ToX = 0.0, ToY = 0.0;
        double  FromX = 0.0, FromY = 0.0;
        FString Reason;             // which rule asked for it, in words
    };

    TArray<FNode> Nodes;
    TArray<FBox>  Boxes;
    TArray<FMove> Moves;            // the whole pass, in the order it will be applied
    TArray<FMove> Pushed;           // the subset that is a cascade, so the report can count them

    // §10. Both measured from the same PinPosition/WireLength the report uses.
    double WireLengthBefore = 0.0;
    double WireLengthAfter  = 0.0;      // computed on the PLANNED positions, before any write

    int32 NodesToMove   = 0;
    int32 GettersToMove = 0;
    int32 StraysToMove  = 0;
    int32 MaxPushDepth  = 0;

    bool    bWouldApply = false;
    FString RollbackReason;
};
```

---

## 4. What "runs backward" means, arithmetically

`FTurboMCPLayoutReport` already emits the finding this pass repairs, at
`Source/TurboMCP/TurboMCPLayoutReport.cpp:281`:

```cpp
if (TargetRect.MinX < SourceRect.MaxX)
{
    Finding.Kind = bIsExec ? TEXT("backward-exec") : TEXT("backward-data");
```

**The rule is therefore fixed by the existing reader, and the reader cannot be changed** — `T-LAY-1`
asserts its output as a baseline. `FIX-BACKWARD` **reuses this predicate verbatim**:

```
A wire runs backward  <=>  RectOf(Target).MinX < RectOf(Source).MaxX
where Source is the pin's owning node and Target is the node owning each pin in LinkedTo.
```

`MinX` and `MaxX` come from `FTurboMCPLayoutReport::RectOf` (`TurboMCPLayoutReport.cpp:80`), which is
`(NodePosX, NodePosY)` to `(NodePosX + NodeWidth, NodePosY + NodeHeight)` (`:86-91`, and the fields at
`EdGraphNode.h:301,305,309,313`).

**THE PASS REPAIRS `backward-exec` ONLY, AND DELIBERATELY LEAVES `backward-data` ALONE.** The spec's
clause is *"for an exec wire that runs backward"*. A backward data wire has two separate remedies in
this capability — the getter rule (§9) and the pure-node rule (§8) — and they apply to specific shapes
rather than to "any backward data wire". Moving a node because a data wire points backwards, with no
getter and no pure consumer involved, would be the pass guessing. So:

- `backward-exec` — repaired by §6;
- `backward-data` to a **getter** — repaired by §9;
- `backward-data` from a **pure node used only by one consumer** — repaired by §8;
- any other `backward-data` — **left alone, and counted**, so the report can say how many were seen and
  not fixed (`P1`: an honest count beats a silent omission).

**This is a deliberate shortfall and it is recorded as such**, because `Spec/03_TOOLS.md` does not say
what to do with the remaining data wires.

---

## 5. "Inside one box": what confines the pass

The clause is *"Inside one box"*. Read as: **the pass works one comment box at a time, and a move may not
carry a node out of the box it is in.**

Concrete rule:

1. **Membership is geometric, by the editor's own rule.** Reuse `FTurboMCPComment::MembersOf`, which is
   already public for exactly this reason (`Public/TurboMCPComment.h`, promoted in `TestRecord.md`
   section 6e). **Do not re-derive it.** The engine's `UEdGraphNode_Comment::GetNodesUnderComment`
   (`EdGraphNode_Comment.h:115`) returns a CACHE filled from RENDERED widgets and is **EMPTY
   headlessly** — measured, and recorded in `TestRecord.md` section 6e. Two copies of a membership rule
   agree until one is changed, which is this project's recurring failure.
2. **A node in NO box is still processed**, because the spec's rules are about wires, not boxes, and
   `TA-4`'s stacked pair is not required to be boxed. A boxless node is `bUnreachable == false` and has
   no `FBox` member index; §6 to §9 apply to it unchanged.
3. **A move is confined to the node's own innermost box.** A node inside a box may be moved only to a
   position that keeps its whole rectangle inside that box's rectangle. If the correct position would
   leave the box, the move is **refused for that node with the box named**, not clamped — clamping
   would place a node at a position the rule did not ask for and the measure would be comparing
   something else.
4. **A node in no box may be moved anywhere**, subject to §11's overlap guard.
5. **Boxes themselves are never moved, resized or created.** §12.

---

## 6. "The row of the node before it": what a row is

The clause is *"place the next node, with any pure nodes used only by it, on the row of the node before it
and to its right, pushing later nodes along."*

**A ROW IS EXACT Y EQUALITY OF THE TOP EDGE, IN A SHARED BAND.** Concretely, two nodes share a row when

```
|Node1.NodePosY - Node2.NodePosY| <= RowTolerance      // 8.0
```

and the moved node is placed at **exactly** the source node's `NodePosY`:

```
Target.NodePosY = Source.NodePosY
```

`RowTolerance` is an 8-unit band rather than `==` because hand-laid graphs (`TA-1`) and the engine's own
arrange tool both leave rows a few units out, and `T-LAY-9` asks that the two nodes *"share a row"* —
exact equality of the planned `Y` is what satisfies it, and the tolerance is only how a row is
**recognised** so an already-aligned pair is not needlessly rewritten.

**THE HORIZONTAL PLACEMENT, AND THE CLEARANCE.**

```
Target.NodePosX = Source.NodePosX + Source.NodeWidth + HorizontalNodeGap
```

`HorizontalNodeGap = 60.0`, taken from `Spec/WORKING_GRAPH_TIDY.md` §10 rather than chosen again, so the
two passes place a node at the same distance from its predecessor. `60.0` is deliberately larger than
`FTurboMCPComment::FitPadding = 40.0` (`Public/TurboMCPComment.h:141`) so a box fitted around the source
does not put its edge on the moved node.

**WHY "THE NODE BEFORE IT" IS WELL DEFINED.** The wire that runs backward identifies the pair exactly:
`Source` is the pin's owning node, `Target` is the node fed. There is no ambiguity and no need to
topologically sort the graph for this clause — which is the one place this pass is substantially simpler
than tidy.

**THE OVERLAP GUARD APPLIES TO EVERY MOVE.** Before a planned position is accepted, the moved node's
planned rectangle must not overlap any other flow node's rectangle. `FTurboMCPLayoutReport` has the
overlap predicate (`RectsOverlap`, used at `TurboMCPLayoutReport.cpp:309`); reuse its rule. A move whose
planned rectangle overlaps is **refused for that node, naming the node it would hit**.

---

## 7. "Pushing later nodes along": which nodes, by how much, in what order

The clause is *"pushing later nodes along."* This is the clause most likely to cascade forever, so the
rule is stated as an algorithm with a termination proof.

**DEFINITION — the pushed set.** A node `N` is pushed by a move of node `M` when **all** of:

1. `N` is in the same row as `M`'s planned position (within `RowTolerance`);
2. `N.NodePosX >= M`'s ORIGINAL `NodePosX` (it is at or to the right of where `M` was);
3. `N`'s rectangle would overlap `M`'s PLANNED rectangle;
4. `N` is not a comment, not a knot, and not unreachable.

**HOW FAR.** Each pushed node moves by exactly the smallest amount that clears the moved node:

```
N.NodePosX = M.PlannedMaxX + HorizontalNodeGap
```

**IN WHAT ORDER, AND WHY IT TERMINATES.** Process the row **right to left, starting from the rightmost
pushed node**, applying each push once:

1. build the pushed set from the ORIGINAL positions;
2. sort it by `NodePosX` **descending**;
3. for each node in that order, compute its new `X` from the **already-planned** position of the node to
   its left in the same row, which is fixed by the time the loop reaches it;
4. **never revisit a node.** A node is pushed at most once per pass.

**This terminates by construction**, and the proof is the reason for the order: every node's new `X` is
computed from a position that is already final, and the sequence of planned `X` values in a row is
strictly increasing left to right. A node cannot push a node that has already been placed, so there is no
cycle. **The cascade is therefore bounded by the number of nodes in the row**, and the pass asserts that
bound rather than trusting it:

```
check(NodesMoved <= Nodes in the row)   // and MaxPushDepth is reported
```

**A PUSH THAT WOULD LEAVE A BOX IS REFUSED** by §5 rule 3, and refusing a push **cancels the move that
caused it**, in reverse order — because leaving the first node moved and the pushed node unmoved is the
overlapping state the push existed to prevent. The cancellation walks the pass's `Moves` backwards to the
first non-push move and removes the whole run.

**A PUSH MAY CROSS A BOX BOUNDARY ONLY IF BOTH NODES ARE IN NO BOX.** If the pushed node is inside a box
and the moved node is not, the push is refused; the moved node's own move is then re-checked against §7's
overlap guard and refused too if it would still overlap.

---

## 8. "With any pure nodes used only by it"

The clause is *"place the next node, **with any pure nodes used only by it**, on the row..."*.

**THE TEST FOR "USED ONLY BY IT", PRECISELY.** A node `P` is used only by node `T` when **every** pin in
`P->Pins` (`EdGraphNode.h:293`) has **every** entry of its `LinkedTo` array (`EdGraphPin.h:398`) owned by
`T`, ignoring nulls and self-links:

```
for each Pin in P->Pins:
    for each Other in Pin->LinkedTo:
        if Other == nullptr: continue
        Owner = Other->GetOwningNodeUnchecked()      // EdGraphPin.h:469
        if Owner == nullptr || Owner == P: continue
        if Owner != T: return false                  // used by something else
return true                                          // used only by T
```

**A node with NO links at all is NOT "used only by it"** — it is used by nothing, which is a different
state, and §11's lone-node rule owns it.

**WHY ONLY PURE NODES COME ALONG.** A pure node has no exec pins, so it cannot be on the exec path this
pass is repairing; moving it cannot reorder execution. `UK2Node::IsNodePure` is
`BlueprintGraph/Classes/K2Node.h:244` (`virtual bool IsNodePure() const { return false; }`, overridden by
the pure classes). Moving an IMPURE node that happens to feed `T` would move a node with an exec pin and
therefore **could** change the graph's order — so the pass refuses it, and counts it.

**THE PURE NODES MOVE AS A UNIT, TO THE LEFT OF THEIR CONSUMER.** For each pure node `P` that is used
only by `T`:

```
P.NodePosX = T.PlannedMinX - P.NodeWidth - HorizontalNodeGap
P.NodePosY = T.PlannedNodePosY
```

**ORDER OF OPERATIONS, WHICH MATTERS.** Compute `T`'s planned position FIRST (§6), then place the pure
nodes to ITS left, then run §7's push for the row. Placing the pure nodes first would compute their
position from `T`'s old position and then move `T`, leaving them stranded.

**A CHAIN OF PURE NODES IS MOVED AS A CHAIN.** If `P` is itself fed only by another pure node `Q` that is
used only by `P`, `Q` moves too, recursively, each one a further `HorizontalNodeGap` to the left. The
recursion is bounded by a visited set and a depth cap of `MaxPureChain = 8`, and exceeding the cap
**refuses the whole move for that node** rather than truncating the chain — a half-moved chain is worse
than an unmoved one.

**THIS IS WHERE `TA-1`'s "pure math chains four nodes long" (`06_ACCEPTANCE.md:16`) is exercised**, and it
is a second reason the kit matters.

---

## 9. "A getter pointing backwards goes directly to the left of the node that uses it"

The clause is exact. There is already a getter finding to be consistent with, at
`TurboMCPLayoutReport.cpp:373-379`:

```cpp
const bool bToTheRight = GetterRect.MinX >= ConsumerRect.MaxX;
const bool bUnderneath = GetterRect.MinY >= ConsumerRect.MaxY;

if (bToTheRight || bUnderneath)
{
    Finding.Kind = TEXT("getter-place");
```

**THE REPORT'S PREDICATE IS `bToTheRight || bUnderneath`; THE PASS REPAIRS ONLY `bUnderneath`.** A getter
placed to the RIGHT of its consumer is also a `getter-place` finding, but the spec's wording is *"a getter
pointing BACKWARDS"*, and "backwards" is the layout report's own word for
`TargetRect.MinX < SourceRect.MaxX` (`:281`) — a strictly horizontal notion. A getter to the right on the
SAME row is not backwards by that predicate. So:

```
A getter "points backwards"  <=>  GetterRect.MinY >= ConsumerRect.MaxY   (it is UNDER its consumer)
```

and the pass repairs exactly that, reusing `:374`'s comparison so the two tools agree.

**"DIRECTLY TO THE LEFT" MEANS:**

```
Getter.NodePosX = Consumer.NodePosX - Getter.NodeWidth - GetterGap
Getter.NodePosY = Consumer.NodePosY
```

- `GetterGap = 40.0`, equal to `FTurboMCPComment::FitPadding` (`Public/TurboMCPComment.h:141`), so a
  fitted box's left edge does not sit on the getter.
- **"directly"** is the `NodePosY` equality: the getter ends on the consumer's row, not merely somewhere to
  its left.
- **When more than one getter feeds the same consumer** they are stacked in the same column, ordered by
  the consumer pin's index in `Pins` so the order is stable between runs, each one `RowSpacing` below the
  previous. `RowSpacing = PinRowSpacing = 24.0` (`TurboMCPLayoutReport.cpp:26`), so the getters line up
  with the consumer's pin rows as computed by `PinPosition` (`:132`).

**HOW THIS DIFFERS FROM THE REPORT'S FINDING.** The report says *"the getter is under its consumer"*. The
pass moves it to a position where the report would no longer emit `getter-place` **for that getter**, and
the suite checks that by re-reading the report rather than by trusting the pass — a cross-tool check.

**A GETTER BEING MOVED IS NOT ALSO SUBJECT TO §6.** A getter has no exec pins
(`UK2Node_VariableGet : UK2Node_Variable`, `K2Node_VariableGet.h:39`), so it can never be one end of a
`backward-exec` wire, and §6 will never select it. §9 is the only rule that moves a getter.

**THE CONSUMER IS IDENTIFIED BY PIN, NOT BY CLASS.** `Consumer` is the node owning the pin that the
getter's value pin links to — read from `LinkedTo` on the getter's value pin, whose name comes from
`UK2Node_Variable::GetVarName` (`K2Node_Variable.h:135`, `return VariableReference.GetMemberName();`).
**`UK2Node_VariableGet::GetValuePin` IS USED ALREADY** at `Source/TurboMCP/TurboMCPLocalVars.cpp` in the
promotion path; read that call site for the working spelling rather than guessing one. If the consumer
cannot be identified, the getter is **left alone and counted**, because there is no target position.

**A COPY OF A GETTER IS NOT MADE.** `Spec/02_PRINCIPLES.md` and `Spec/WORKING_GRAPH_TIDY.md` §11 treat
getter SPLITTING as tidy's job and recommend refusing it in phase 6. This pass only MOVES the existing
getter. Creating a node would make the pass irreversible, which §10 depends on not being the case.

---

## 10. THE MEASURE AND THE ROLLBACK — taken from tidy, not re-derived

`Spec/03_TOOLS.md` says *"Measures and rolls back like tidy."* **So this section says which parts are taken
from `Spec/WORKING_GRAPH_TIDY.md` §7 rather than repeating them**, because two copies of a measure are two
measures.

**TAKEN VERBATIM FROM THE TIDY SPEC:**

| What | Tidy spec §7 | Why it is the same here |
|---|---|---|
| the wire-length definition | `FTurboMCPLayoutReport::WireLength` / `PinPosition` | two tools' numbers must be comparable |
| **"better" is strictly less** | `After < Before` applies; `After >= Before` rolls back unless Force | a pass that moves every node for no gain has changed the user's graph for nothing |
| the roll-back trigger | `bWouldApply` computed in `Plan` | the Preview must state it |
| the result format | both numbers on the first line | `T-LAY-5`/`T-LAY-9` read them |

**`FTurboMCPLayoutReport::RectOf`, `PinPosition` and `WireLength` ARE ALREADY PUBLIC STATICS** —
`Public/TurboMCPLayoutReport.h:91,100,103`. Measured. So the tidy spec's recommended action for that
capability is **already done for this one**, and this pass calls them directly. The `PinRowSpacing = 24.0`
convention (`TurboMCPLayoutReport.cpp:26`) and the pin-row formula (`:132`) come along with them.

**WHAT IS DIFFERENT HERE, AND IT IS THE GOOD NEWS: THIS PASS CREATES NOTHING.**

`Spec/WORKING_GRAPH_TIDY.md` §7 needs a rollback as **code** because tidy creates getters and knots before
it can measure the outcome, and those creations are the only irreversible steps. **`FIX-BACKWARD` moves
nodes and creates nothing**: no node is created, none is destroyed, no link is made or broken, no box is
touched.

**SO THE ROLLBACK IS A DESIGN PROPERTY RATHER THAN A CODE PATH, AND THE PLAN CARRIES VALUES FOR THAT
REASON:**

1. `Plan` reads every node's position into `FNode`, computes every `FMove` against those VALUES, writes the
   planned positions into `FNode.X/Y` **in the plan**, and measures `WireLengthAfter` **on the planned
   values**;
2. `bWouldApply = (WireLengthAfter < WireLengthBefore) || bForce`;
3. `Apply` writes the planned values **only when `bWouldApply`**;
4. if `!bWouldApply`, `Apply` writes **nothing at all** and returns `unchanged:` with both numbers.

**THE EXCEPTION, AND IT IS THE ONLY ONE: A NODE WHOSE POSITION IS ALREADY CORRECT IS NOT RE-WRITTEN**, so
`Apply` reports `unchanged:` rather than `changed:` when the pass computed a plan that matches the graph it
read. Without this, a pass on `SaneProbe` would write identical numbers and honestly report `changed:`,
which reads as work done for nothing.

**THE RE-CHECK AFTER WRITING.** `Apply` re-measures the live graph after writing and reports that number
beside the planned one. **A measure whose result is not read back is a measure nobody knows is right** —
the rule the knot spec states as *"a rollback whose result is not checked is a rollback nobody knows
works"*, applied here to the write instead of the rollback. A mismatch means a planned position did not
survive the write, and it is reported as a `; WARNING` line rather than hidden.

**`Apply` DOES NOT USE `FScopedTransaction` AS THE ROLLBACK**, for the tidy spec's reason: a transaction is
an undo entry, not a guarantee, and `P4` already makes every Edit one transaction. `Apply` wraps its
writes in exactly one `Graph->Modify()` under the existing convention
(`TurboMCPComment.cpp:695`, `TurboMCPGraphDead.cpp:620`).

**THE MEASURE IS `TOTAL WIRE LENGTH`, AND §11's GUARDS ARE NOT IN IT.** A pass that avoids every overlap
and stays inside every box but lengthens the wiring is rolled back, because the spec's word is *"Measures"*
and the measurable property the project already agreed on is wire length. The guards are **preconditions**
that determine WHICH moves are planned; they are not terms in the comparison. **Recorded because it is an
interpretation** and because mixing them would make the number incomparable with `L-LAYOUT-REPORT`'s.

---

## 11. "A lone node at the origin", "never moves a box", and the guards

### "A lone node at the origin"

The report already has this rule and the pass **reuses its constants and its shape**
(`TurboMCPLayoutReport.cpp:394-406`):

```cpp
constexpr double OriginBand = 64.0;
constexpr double StrayLinkDistance = 1200.0;

const bool bAtOrigin = FMath::Abs(Rect.MinX) <= OriginBand
    && FMath::Abs(Rect.MinY) <= OriginBand;
```

- **"at the origin"** means the rectangle's top-left is within **64.0 units** of `(0, 0)` in BOTH axes.
  The band rather than exact equality is the report's own decision, and the report's comment gives the
  reason: *"a node at the origin is only a finding when everything it links to is far away, because a
  small graph legitimately lives near the origin."*
- **"lone"** means the node **HAS at least one link** and the **nearest** linked node is farther than
  **1200.0 units**, measured with `FVector2D::Distance` between `PinPosition` values
  (`TurboMCPLayoutReport.cpp:427`). A node with **no** links is a disconnected node, which
  `P/E-GRAPH-DEAD` owns, and is **not** moved by this pass.
- **the move**: the stray is placed immediately **left of its nearest linked node**:

```
Stray.NodePosX = Nearest.NodePosX - Stray.NodeWidth - HorizontalNodeGap
Stray.NodePosY = Nearest.NodePosY
```

  which is the same placement §9 uses for a getter and §6 uses for a following node, so a caller sees one
  convention rather than three.

**A NODE AT EXACTLY `(0,0)` THAT IS ALREADY NEAR ITS NEIGHBOURS IS LEFT ALONE**, because the 1200-unit
test is what makes it a finding. `SaneProbe`'s entry node sits near the origin and is fine; the pass must
not touch it, and §15 check 8 is that.

### "Never moves a box": guaranteed, not intended

The hard promise is enforced in **four** places rather than by care:

1. **`FPlan` HAS NO BOX POSITION FIELD AT ALL.** `FBox` carries `Key`, `Text`, `Depth`, `ParentIndex` and
   `MemberIndices` — **no `X`, `Y`, `Width` or `Height`.** There is nothing to write a box position from,
   so the promise is a property of the type rather than a rule a later edit could break. This mirrors the
   tidy spec's recommendation and goes further: tidy needs box SIZES for its nesting, so it carries them;
   this pass needs neither.
2. **`FMove` NAMES A NODE TO MOVE, AND COMMENTS ARE NEVER ENTERED INTO `Moves`.** The planner skips any
   node with `bIsComment`, and `Apply` skips any resolved node that `IsA<UEdGraphNode_Comment>()`
   (`EdGraphNode_Comment.h`; the `IsIgnored` filter at `TurboMCPLayoutReport.cpp:176-179` is the same
   shape).
3. **`Apply` RE-CHECKS BEFORE EVERY WRITE** that the resolved node is not a comment, by class, not by the
   plan's flag. This is the belt-and-braces the tidy spec asks for on the check whose failure breaks the
   spec's hardest promise.
4. **THE SUITE COMPARES EVERY BOX'S FULL RECTANGLE BEFORE AND AFTER**, by key, and `T-LAY-9` names exactly
   this. A pass that resized a box would be caught even though the plan cannot express it.

### The remaining guards, and the refusals

Every refusal opens with `error:` and names what stopped it; a no-op opens with `unchanged:`.

1. **the asset or graph cannot be resolved** — refused by the shared resolver's own words;
2. **the graph has no flow nodes** (`P1`: say so in words, not by returning nothing);
3. **a node whose `NodeWidth` or `NodeHeight` is zero** — refused by naming the nodes. Pins cannot be
   positioned on a zero-size rectangle, so the before-number is meaningless. **This is the state a node
   is in before the editor has ever rendered it**, and `Spec/WORKING_GRAPH_TIDY.md` §8 refusal 4 records
   it from `TestRecord.md` section 6d. **IT IS NOT A DEFENSIVE BRANCH ABOUT HEADLESS RENDERING IN
   GENERAL:** node rectangles ARE real headless, measured — `L-LAYOUT-REPORT` reported *"longest single
   wire: 400"* for a wire between nodes at x=400 and x=1600, and `PinPosition` uses `NodePosX +
   NodeWidth`, so a zero width would have measured 1200. The refusal is about a node that was created
   and never allocated, not about the editor's rendering;
4. **no wire in the graph runs backward, no getter points backwards and no stray sits at the origin** —
   **`unchanged:`**, with the counts of each rule's findings at zero, so a caller knows all three rules
   were evaluated. `SaneProbe` is this case;
5. **a planned move that would leave its box, overlap another node, or push past a box boundary** —
   refused for that node, naming the blocker, and the rest of the pass proceeds;
6. **the measure is not better and Force is absent** — `unchanged:` with both numbers (§10).

---

## 12. `Apply`'s result format

`P4` requires `changed:` or `unchanged:` and the spec requires the numbers. Following
`FTurboMCPGraphDead`'s shape (`TurboMCPGraphDead.cpp:631`) and the tidy spec §9:

```
changed: fixed backward wires in <Graph> of <Asset> : wire length <Before> -> <After>
    moved <N> node(s), of which <K> were pushed along
    getters moved left of their consumer: <G>
    lone nodes brought next to what they link to: <S>
    boxes: <B> seen, 0 moved, 0 resized
    left alone: <D> backward DATA wire(s) that no rule covers, <U> unreachable node(s)
    ; the change is in memory and NOT saved. Call E-SAVE.
```

When the measure is not better and Force is absent:

```
unchanged: <Graph> would be worse (wire length <After> -> <Before>), so it was rolled back and NOTHING was written
```

When Force is set and the measure is worse, the word **`WORSE`** appears in capitals, as tidy's does, so a
forced regression is never disguised.

---

## 13. The `UFUNCTION` declarations

To be pasted as a block; `PreviewTurboMCPGraphFixBackward` into `UTurboMCPPreviewToolset`,
`EditTurboMCPGraphFixBackward` into `UTurboMCPEditToolset`, in
`Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`.

**Do not put a backtick followed later by an apostrophe on one line of these comments** — UHT reads it as
an unterminated character constant and the build fails. That has broken this project's build before.

```cpp
	/**
	 * Repairs the bad spots in a graph that is otherwise fine, WITHOUT moving anything else.
	 *
	 * Three rules, and it repairs only what one of them covers. An EXEC wire that runs backward: the
	 * node it feeds is placed on the row of the node before it and to its right, together with any
	 * PURE nodes used only by it, and later nodes in that row are pushed along. A GETTER placed under
	 * the node that reads it is moved to that node's own row, directly to its left. A LONE node parked
	 * within 64 units of the origin whose nearest neighbour is more than 1200 units away is brought
	 * next to it.
	 *
	 * NOTHING IS CREATED AND NOTHING IS REMOVED. No node, no link, no box. A COMMENT BOX IS NEVER
	 * MOVED, RESIZED OR CREATED, and that is a property of the plan rather than a promise in the code:
	 * the plan carries no box position to write.
	 *
	 * It MEASURES TOTAL WIRE LENGTH and rolls back like P/E-GRAPH-TIDY: the layout is computed in
	 * memory, the length is measured on the PLANNED positions, and the graph is written ONLY if the
	 * result is strictly shorter. Otherwise nothing is written at all and the answer opens with
	 * `unchanged:` with both numbers. A caller can force it with `Force`, and the answer then says
	 * WORSE in capitals.
	 *
	 * A backward DATA wire that no rule covers is LEFT ALONE and COUNTED, so the answer never implies
	 * it fixed more than it did.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool Force);

	/**
	 * Repairs the bad spots in a graph that is otherwise fine.
	 *
	 * The Edit half of the pair above, and it shares the whole pass: the same plan, the same measure,
	 * the same roll-back decision. Anything the Preview would not do, this does not do.
	 *
	 * ONE UNDO ENTRY. This is a single transaction, so one undo puts the graph back.
	 *
	 * It NEVER MOVES A COMMENT BOX, and it never creates or removes a node. When the wiring would not
	 * be shorter, NOTHING is written and the answer opens with `unchanged:` and both numbers. Pass
	 * `Force` true to apply a result that is worse; the answer then says WORSE in capitals.
	 *
	 * The change is IN MEMORY and not saved. Call E-SAVE when it is right.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool Force);
```

---

## 14. Includes — every symbol cited

This project has had **five guessed API names in a row turn out not to exist**, and the last batch produced
ten compile errors, thirteen link errors and a hard engine assertion from guessing. So every symbol below
carries where it was read from. **A citation is where to look, not a substitute for looking.**

| Symbol | Header | Read from |
|---|---|---|
| `UEdGraphNode::NodePosX`, `NodePosY` | `EdGraph/EdGraphNode.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphNode.h:301,305` |
| `UEdGraphNode::NodeWidth`, `NodeHeight` | `EdGraph/EdGraphNode.h` | `EdGraphNode.h:309,313` |
| `UEdGraphNode::Pins` | `EdGraph/EdGraphNode.h` | `EdGraphNode.h:293` — **public** |
| `UEdGraphNode::GetGraph` | `EdGraph/EdGraphNode.h` | `EdGraphNode.h:694` |
| `UEdGraphNode::DestroyNode` | `EdGraph/EdGraphNode.h` | `EdGraphNode.h:707` — **not used by this pass; it creates and destroys nothing** |
| `UEdGraphNode::GetNodeTitle` | `EdGraph/EdGraphNode.h` | `EdGraphNode.h:745` |
| `UEdGraphPin::LinkedTo` | `EdGraph/EdGraphPin.h` | `EdGraphPin.h:398` |
| `UEdGraphPin::PinName` | `EdGraph/EdGraphPin.h` | `EdGraphPin.h:306` |
| `UEdGraphPin::Direction`, `EGPD_Input`, `EGPD_Output` | `EdGraph/EdGraphPin.h` | `EdGraphPin.h:312` |
| `UEdGraphPin::GetOwningNodeUnchecked` | `EdGraph/EdGraphPin.h` | `EdGraphPin.h:469` |
| `UEdGraphPin::GetOwningNode` | `EdGraph/EdGraphPin.h` | `EdGraphPin.h:468` |
| `UEdGraph::Nodes` | `EdGraph/EdGraph.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraph.h:79` — **`TArray<TObjectPtr<UEdGraphNode>>`** |
| `UEdGraph::GetSchema` | `EdGraph/EdGraph.h` | `EdGraph.h:115` |
| `UEdGraphSchema::TryCreateConnection` | `EdGraph/EdGraphSchema.h` | `EdGraphSchema.h:828` |
| `UEdGraphSchema::BreakSinglePinLink` | `EdGraph/EdGraphSchema.h` | `EdGraphSchema.h:1057` |
| `UEdGraphSchema::BreakPinLinks` | `EdGraph/EdGraphSchema.h` | `EdGraphSchema.h:1049` |
| `UEdGraphNode_Comment`, `CommentDepth` | `EdGraphNode_Comment.h` | `Engine/Source/Editor/UnrealEd/Public/EdGraphNode_Comment.h:79` |
| `UEdGraphNode_Comment::GetNodesUnderComment` | `EdGraphNode_Comment.h` | `EdGraphNode_Comment.h:115` — **a cache filled from RENDERED widgets; EMPTY headlessly. DO NOT USE for membership** |
| `UK2Node::IsNodePure` | `K2Node.h` | `Engine/Source/Editor/BlueprintGraph/Classes/K2Node.h:244` |
| `UK2Node_Variable::GetVarName` | `K2Node_Variable.h` | `K2Node_Variable.h:135` — `return VariableReference.GetMemberName();` |
| `UK2Node_VariableGet` | `K2Node_VariableGet.h` | `K2Node_VariableGet.h:39` |
| `UK2Node_VariableGet::GetValuePin` | `K2Node_VariableGet.h` | **used at `Source/TurboMCP/TurboMCPLocalVars.cpp` in the promotion path — read that call site for the spelling** |
| `FTurboMCPLayoutReport::RectOf` | `TurboMCPLayoutReport.h` | `Source/TurboMCP/Public/TurboMCPLayoutReport.h:91` — **already a public static** |
| `FTurboMCPLayoutReport::PinPosition` | `TurboMCPLayoutReport.h` | `Public/TurboMCPLayoutReport.h:100`; the convention at `TurboMCPLayoutReport.cpp:126-133` |
| `FTurboMCPLayoutReport::WireLength` | `TurboMCPLayoutReport.h` | `Public/TurboMCPLayoutReport.h:103`; implementation at `TurboMCPLayoutReport.cpp:138` |
| the backward predicate | — | `TurboMCPLayoutReport.cpp:281` |
| the getter predicate | — | `TurboMCPLayoutReport.cpp:373-379` |
| `OriginBand`, `StrayLinkDistance` | — | `TurboMCPLayoutReport.cpp:35` and `:38`; the test at `:401-402` and `:436` |
| `RectsOverlap` | — | `TurboMCPLayoutReport.cpp:41`; used at `:309` |
| `PinRowSpacing` | — | `TurboMCPLayoutReport.cpp:26` — `24.0` |
| `FTurboMCPComment::MembersOf` | `TurboMCPComment.h` | `Source/TurboMCP/Public/TurboMCPComment.h` — **promoted to public for exactly this reuse; `TestRecord.md` section 6e** |
| `FTurboMCPComment::FitPadding`, `TitleBarHeight` | `TurboMCPComment.h` | `Public/TurboMCPComment.h:141,144` — `40.0`, `42.0` |
| `TurboMCPNodeId::MakeKey`, `MakeForNode` | `TurboMCPNodeId.h` | `Source/TurboMCP/Public/TurboMCPNodeId.h:60,77` |
| `FTurboMCPAssetResolver` | `TurboMCPAssetResolver.h` | used at `TurboMCPGraphDead.cpp` and `TurboMCPComment.cpp:375-393` |
| `FBlueprintEditorUtils::RemoveNode` | `BlueprintEditorUtils.h` | `TurboMCPGraphDead.cpp:622` — **not used here; recorded because it is the destructive call this pass must never need** |

**NOTHING IN THIS PASS CALLS `AllocateDefaultPins`, AND THAT IS A CONSEQUENCE OF CREATING NOTHING.** The
rule still matters and is recorded because the acceptance commandlet reads those pins: `NewObject` plus
`UEdGraph::AddNode` do **not** allocate a node's pins, and the editor only does when it reconstructs the
node for display, which never happens headless. `UK2Node_Knot::GetInputPin` is `Pins[0]` and `GetOutputPin`
is `Pins[1]`, both unguarded, and this crashed the acceptance commandlet. **If a later revision of this
pass ever creates a node, `AllocateDefaultPins` becomes mandatory at that moment.**

---

## 15. Test plan: `Tools/test-graph-fix-backward.py`

The project's recurring failure, recorded six times, is **a check that passes while the thing it verifies
is broken**. So every row names **how it could fail**.

### The fixture this needs — build it BEFORE the code

**A PROBE GRAPH (extend `LayoutProbe`, or add `FixBackwardProbe`),** in
`TurboMCPAcceptanceAssetCommandlet.cpp`, containing exactly `TA-4`'s four shapes:

| Shape | Geometry | Rule it exercises |
|---|---|---|
| a stacked exec pair | `CallA` at `(600, 0)`, `CallB` at `(600, 300)`, `A.then → B.execute` | §6 — `B.TargetRect.MinX = 600 < A.SourceRect.MaxX = 600 + W`, so the wire runs backward |
| a getter under its consumer | a variable getter at `(600, 300)` feeding a node at `(200, 0)` | §9 |
| a second getter copy under a branch | a second getter of the same variable under a branch node | §9's multi-getter stacking |
| a lone node at the origin | a call at `(0, 0)` linked to a node at `(1400, 900)` | §11's stray rule |
| **a pure chain, four long** | four pure math nodes feeding one consumer | §8's chain recursion and `MaxPureChain` |
| **a box containing a node whose fix would leave it** | a small box around one node of the stacked pair | §11 refusal 5, the box guard |

**AND `SaneProbe` IS THE OTHER HALF OF THE SUITE** and already exists
(`TurboMCPAcceptanceAssetCommandlet.cpp:994`, and see the note in §0 about that file's line numbers
moving): `Entry → Call → Result`, one row, no backward wire, no
getter, no origin stray. It is the graph where the pass must compute a plan, find the measure equal, and
write nothing. **`T-LAY-5` and `T-LAY-8` already rely on it for tidy; this capability reuses it rather
than adding a second do-nothing graph.**

### The checks

| # | Check | What it proves | **How it could fail** |
|---|---|---|---|
| 1 | the preview prints TWO wire-length numbers | the measure ran | break the print; the check goes red |
| 2 | **the graph is unchanged by a preview**, compared BY KEY AND POSITION for every node AND the full rectangle of every box | a preview is read-only | move a node, or write a box size, in `Plan`; the comparison differs |
| 3 | **the edit opens with `changed:` or `unchanged:`** | `P4` | return a bare sentence |
| 4 | **the two stacked exec nodes share a row afterwards** — their `NodePosY` are EQUAL | `T-LAY-9`'s first clause | plan `Target.NodePosY` from the wrong source; the Y values differ |
| 5 | **the target is to the RIGHT of the source** — `Target.NodePosX > Source.NodePosX + Source.NodeWidth` | the wire no longer runs backwards | place it at the source's `X`; the predicate is still true |
| 6 | **every backward getter is now `NodePosY == Consumer.NodePosY` and `NodePosX < Consumer.NodePosX`** | §9 | skip the `Y` assignment; the getter is still under |
| 7 | **`L-LAYOUT-REPORT` NO LONGER REPORTS THE `backward-exec` FINDING FOR THAT WIRE** | the repair is real, read back through a DIFFERENT tool | disable the move; the finding is still there |
| 8 | **`L-LAYOUT-REPORT` no longer reports `getter-place` for the moved getters** | same, for §9 | as above |
| 9 | **NO COMMENT BOX'S FULL RECTANGLE MOVED** — compared by key, all four numbers | the spec's hardest promise | let `Apply` write a box position; the rectangle differs |
| 10 | **on `SaneProbe`, the result is `unchanged:` AND the graph is byte-identical in every position** | a good graph is not damaged | break the layout-already-correct short circuit; identical writes become `changed:` |
| 11 | **when the measure would be worse, the result says `unchanged:` and the after-number is higher** | the roll-back fires | make `Apply` write unconditionally; the positions move |
| 12 | **`Force` says `WORSE` in capitals** | a forced regression is not disguised | drop the marker |
| 13 | **the after-number read back through `L-LAYOUT-REPORT` is <= the before-number** | the measure is real, not the pass's own arithmetic | make `Apply`'s re-measure read the PLAN instead of the live graph; the two numbers differ |
| 14 | **a zero-size node is refused BY NAME, and the same refusal wording appears in both the preview and the edit** | `D-02` and `P2` | put the check in `Apply` only; the preview accepts |
| 15 | **a node whose fix would leave its box is refused, naming the box, and the box's rectangle is unchanged** | §11 refusal 5 | clamp instead of refusing; the node lands somewhere the rule did not ask for |
| 16 | **the pure chain moved as a unit**, and `MaxPureChain`'s cap is exercised by a chain of nine | §8 | move only the first pure node; the chain is split |
| 17 | **the stray's nearest neighbour is now within 1200 units** | §11 | place the stray at the origin again |
| 18 | **backward DATA wires that no rule covers are COUNTED and their nodes did not move** | the deliberate shortfall is honest | move them anyway; the count says zero while positions changed |

**CHECK 7 AND CHECK 8 ARE THE ONES THAT MATTER MOST**, because they read the repair back through a
**different tool** rather than through the pass's own report, which is this project's standing preference
and the rule recorded in `TestRecord.md` section 4a.

**CHECK 10 IS THE ONE THAT PROTECTS THE USER**, and `SaneProbe` exists for it.

**EVERY CHECK PARSES A NUMBER RATHER THAN MATCHING A LITERAL.** This project has recorded the pinned-count
mistake five times, and the most recent instance was `test-material.py` asserting the literal `0.375`
until the material workstream flagged it as fixture wording.

---

## 16. What will remain unverified

1. **`T-LAY-9` itself cannot be run until the acceptance kit exists.** The row names `TA-4`, and `TA-4` is
   not built. The suite in §15 runs against a probe graph instead, which is a **different asset from the
   one the acceptance row names**, and the shortfall is recorded rather than papered over.
2. **What a HUMAN sees.** Whether the repaired layout reads better to a person is `Q-11`'s class of
   question, and this plugin has no Slate inspection tool. Only the numbers are checked.
3. **A graph where the repair is correct but the wire length does not fall.** Measuring only distance, a
   repair that removes a long backward detour but adds a short segment can leave the total EQUAL, and §10
   rolls back a layout that a person would call better. **The spec says "Measures and rolls back", so this
   is the specified behaviour**, but it is a real limitation and it is the same one `Spec/WORKING_GRAPH_TIDY.md`
   §17 item 1 records for knots. **It may be worth a second term in the measure later; that is a decision,
   not an implementer's choice.**
4. **Whether `MaxPureChain = 8` is right.** It was chosen as a bound that covers `TA-1`'s stated four-node
   chain with room to spare. No source settles it.
5. **Whether a graph can reach `Plan` with a node already overlapping another.** `TA-4` is specified as
   stacked, not overlapping, and the pass refuses rather than repairs an overlap. Whether an overlap in
   the input should be refused or treated as one more bad spot is **open**.
6. **The interaction with `P/E-GRAPH-KNOTS`.** If a knot was placed on a wire and this pass then moves an
   endpoint, the knot's position is stale. Neither spec settles it. **See §17 item 1.**

---

## 17. OPEN

**1. THE ORDER OF `FIX-BACKWARD` AND `KNOTS` IS UNDECIDED, AND IT IS A SHARED ITEM.**
`P/E-GRAPH-KNOTS` places knots at positions derived from the nodes they join. This pass MOVES nodes
(§7 pushes up to a whole row). So running knots first and fix-backward second leaves every knot stale, and
running fix-backward first means knots are placed on a layout that then stays still. **The natural order is
fix-backward THEN knots**, since knots are the cosmetic final step and fix-backward is the structural one,
**but nothing in either spec says so** and `Spec/08_ROADMAP.md:15` lists them with KNOTS first. This is
the single most important open item for the two capabilities together.

**2. WHICH OF TIDY'S OPEN ITEMS THIS CAPABILITY ALSO DEPENDS ON — a finding, not a gap.**

| Tidy's OPEN item | Does `FIX-BACKWARD` depend on it? | Why |
|---|---|---|
| **1. do added knots count toward the measure** | **NO** | this pass adds no knots, and §10 uses `WireLength`, which counts every pin's `LinkedTo` distance. If knots are later made to count, they count here too, and this pass never creates one |
| **2. merged getters in phase 6 or refused** | **NO** | this pass MOVES getters and never merges or copies one (§9). It inherits tidy's decision to refuse, but does not depend on it |
| **3. event graph or function graph only** | **YES — SHARED AND UNRESOLVED** | `TA-4` is a FUNCTION graph, so `T-LAY-9` is safe either way. But this pass moves nodes and creates nothing, so like tidy it **plausibly applies to any graph**, and the reference to `FBlueprintEditorUtils::DoesSupportLocalVariables` that tidy's item 3 makes is about LOCAL VARIABLES, which this pass never touches. **Recommendation, matching tidy's: apply to any graph.** Flagged because it is shared |
| **4. which box lays out a node inside nested boxes** | **YES — SHARED** | §5 uses the TIGHTEST box for confinement, which is tidy's interpretation. If the project prefers the OUTERMOST, §5 rule 3 changes. **The two caps must agree**, because a node tidy lays out inside one box and this pass confines to another would be moved twice for no reason |
| **5. `NodeDepth` versus geometry for parent resolution** | **YES — SHARED, WEAKLY** | §5 needs a node's innermost box. This pass resolves it the same way tidy does so the two agree; if the ordering is reversed in tidy it must be reversed here |

**THREE OF TIDY'S FIVE OPEN ITEMS ARE SHARED.** That is itself the finding: **the box-nesting decisions
(4 and 5) and the graph-kind decision (3) must be settled ONCE for both capabilities**, because they are
the same question asked twice, and a spec that resolved them independently would produce two answers that
agree until one is changed.

**3. Should a `backward-data` wire that no rule covers cause a refusal rather than a count?** §4 counts it.
`Spec/03_TOOLS.md` does not say. Refusing would be `P2`-friendly but would make the pass refuse most
imperfect graphs, which is the opposite of *"a local pass for graphs that are mostly fine"*.

**4. What happens when a pushed node's row already contains a node whose position is planned by §9?**
A getter and a pushed node can want the same cell. The tie is broken in favour of §9 in this spec, on the
grounds that a getter's position is dictated by its consumer and a push is dictated by an obstacle. This
is a choice, not a derivation.

**5. Whether the stray rule should run at all when the graph is otherwise clean.** A lone node at the
origin may be deliberate — a caller parking something. §11 moves it whenever the 1200-unit test holds,
which is what "*Move a lone node at the origin next to what it links to*" says, but a `Force`-style opt-out
per rule is not specified anywhere.
