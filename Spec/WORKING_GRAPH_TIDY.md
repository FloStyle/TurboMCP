# Working spec: `P/E-GRAPH-TIDY` with nested comment boxes

**Status: a specification to implement from, not a record of work done.** Nothing here has been
compiled or run. Every engine symbol named below was read from the file and line given; nothing is
inferred. Items I could not settle from the source are listed under **OPEN** rather than invented.

**Reference colour for the implementer:** `T-LAY-5`, `T-LAY-7` and `T-LAY-8` in
`Spec/06_ACCEPTANCE.md` are the acceptance tests, and the fixture they name (`TA-1`, `TA-2`, `TA-3`)
**does not exist yet** — see "What the fixture can and cannot support" below, which is the single
biggest risk to this capability being verifiable.

---

## 1. What the spec requires, verbatim

From `Spec/03_TOOLS.md:420`, the section `### P/E-GRAPH-TIDY: lay out one graph`:

- *"Arranges the nodes within each comment box the user made, then fits that box to its new contents.
  Never moves a user's box as a whole."*
- *"Nested boxes: treat each box as the child of the tightest box around it. Arrange the inner boxes
  first, then arrange the outer box with each inner box as one block."*
- *"Groups with no box are ordered by execution order and placed left to right."*
- *"Column of a node: the length of the longest chain of links (exec and data together) leading to it.
  Then shift each pure node rightward until it sits next to the node that uses it."*
- *"A variable getter used by several consumers is split into one getter per consumer, placed next to
  it. Only variable getters get copied, because a getter takes no input and changes nothing. Merging
  getters is an option, off by default, and when on it only merges getters that feed the same
  consumer."*
- *"Adds reroute knots where they make a wire straight or stop it crossing a node (same rules as
  `P/E-GRAPH-KNOTS`), and counts them in the measure."*
- *"Disconnected groups are not moved into the layout."*
- *"Compares total wire length from before to after. Rolls back if worse, unless `Force` is true.
  Prints both numbers on the first line."*

And from `Spec/08_ROADMAP.md:5`, the reason for the phase's ordering:

> `| 6. Layout | P/E-GRAPH-FIX-BACKWARD, P/E-GRAPH-DEAD, P/E-COMMENT, P/E-GRAPH-TIDY with nested boxes, P/E-GRAPH-KNOTS, P/E-LOCAL-VARS | Local repair first: it helps mostly good graphs, where full layout tends to refuse |`

That row is a **design instruction for tidy**: full layout is expected to **refuse** on graphs that
are not mostly good. Section 8 below turns that into refusal conditions.

---

## 2. Naming and files

Matching the existing `FTurboMCPComment` / `FTurboMCPGraphDead` shape exactly:

| File | Holds |
|---|---|
| `Source/TurboMCP/Public/TurboMCPGraphTidy.h` | `FTurboMCPGraphTidy` — `FPlan`, `Plan`, `RenderPreview`, `Apply`, and the layer/box structures |
| `Source/TurboMCP/TurboMCPGraphTidy.cpp` | the reading, the layer computation, the measure, the rollback |
| `Source/TurboMCP/TurboMCPGraphTidyTools.cpp` | the two `UFUNCTION` bodies, which call `Plan` then either `RenderPreview` or `Apply` |

The class is **not** a `UObject` and is **not** a `UCLASS`. `FTurboMCPComment` at
`Public/TurboMCPComment.h:32` is a plain `class` with `static` members, and the tools in
`TurboMCPCommentTools.cpp` call it from the two `UTurboMCPPreviewToolset` /
`UTurboMCPEditToolset` methods. Tidy follows that — there is no engine reason to register state.

---

## 3. The `Plan` / `RenderPreview` / `Apply` split

Exact signatures, matching `FTurboMCPComment` (`Public/TurboMCPComment.h:106-115`):

```cpp
static bool Plan(const FString& InAsset, const FString& InGraphName,
    bool bInForce, FPlan& OutPlan, FString& OutError);

static FString RenderPreview(const FPlan& InPlan);

static FString Apply(const FPlan& InPlan);
```

`bInForce` is on `Plan`, not `Apply`, so the **Preview** can say "this would be worse and would roll
back, but Force is set so it will be applied". A preview that omitted that would show a rollback the
edit does not perform — the exact disagreement `D-02` exists to prevent, and the project has been
caught by it twice already (see `TestRecord.md` sections 4b and 4f).

### `FPlan`

```cpp
struct FPlan
{
    FString Asset;                  // resolved object path
    FString GraphName;              // the graph examined
    bool    bForce = false;

    // Every node in the graph, read once. Tidy works on these VALUES (see section 9).
    struct FNode
    {
        FString Key;                // TurboMCPNodeId::MakeKey(NodeGuid), 6 lowercase hex
        FString ClassName;
        double  X = 0.0, Y = 0.0, Width = 0.0, Height = 0.0;
        int32   Column = 0;         // section 4
        bool    bIsComment = false;
        bool    bIsKnot = false;
        bool    bIsPure = false;
        bool    bIsGetter = false;
        bool    bUnreachable = false;   // section 8, refusal 3
    };

    // One comment box, with its nesting parent resolved.
    struct FBox
    {
        FString Key;
        FString Text;
        int32   Depth = 0;              // CommentDepth, section 5
        int32   ParentIndex = INDEX_NONE;   // the TIGHTEST box around it (section 5)
        TArray<int32> ChildBoxIndices;  // directly nested boxes
        TArray<FString> MemberKeys;     // non-comment nodes geometrically inside
    };

    TArray<FNode> Nodes;
    TArray<FBox>  Boxes;

    // The measure, section 7. WireLengthBefore is computed from the graph as read.
    double WireLengthBefore = 0.0;
    double WireLengthAfter = 0.0;       // computed on the PLANNED positions, before writing
    int32  KnotsPlanned = 0;

    // section 7: false when the planned layout is worse and Force is not set.
    bool bWouldApply = false;
    FString RollbackReason;
};
```

---

## 4. Column of a node, and the pure-node shift

The spec's rule, made arithmetic:

> *"Column of a node: the length of the longest chain of links (exec and data together) leading to it."*

**Rule.** Build a directed graph over the graph's nodes. An edge `A -> B` exists when **any** pin of
`A` links to **any** pin of `B`, in either direction of the pin (an output of `A` to an input of `B`).
Then:

```
Column(entry)  = 0                     for every node with no incoming edge
Column(N)      = 1 + max(Column(P))    over every P with an edge P -> N
```

computed by topological order. **A cycle is a refusal**, not a silent `0`: section 8, refusal 1.

### Why exec and data together, and why that is the right reading

The spec says *"exec and data together"*, and there is a second reason the project has already
measured. A pure node has **no exec wire of its own** — it is reachable only because a live node reads
its value. `P/E-GRAPH-DEAD`'s reachability walk had to follow data wires for exactly this reason, and
`TestRecord.md` section 6b records the conclusion: *"judging liveness on exec alone would delete pure
nodes that Live code reads"*. A column computation on exec alone would put every pure node in column 0
and stack them all against the entry, which is the opposite of a tidy layout. **So data edges must
count for column, and they do.**

### The pure-node shift

> *"Then shift each pure node rightward until it sits next to the node that uses it."*

**Rule.** After columns are assigned, for each pure node `P` with exactly one consumer `C`:

```
TargetX = C.X - P.Width - HorizontalNodeGap
P.X     = max(P.X, TargetX)
```

`max` and not assignment, because a node may already be further right than its consumer's left edge,
and moving it **leftward** is not what the spec asks for. The consumer's `X` is read **after** its own
column placement, so a chain of pure nodes settles right-to-left; iterate consumers in **decreasing
column order** so one pass is enough.

`HorizontalNodeGap` and `VerticalNodeGap` are new constants; section 10 gives their values and says
where the existing ones come from.

---

## 5. Nesting: the tightest box around a box

> *"Nested boxes: treat each box as the child of the tightest box around it. Arrange the inner boxes
> first, then arrange the outer box with each inner box as one block."*

### Membership is geometric, and the engine's cache cannot be used

This is **already solved in this codebase and the solver must be reused, not rewritten.**
`FTurboMCPComment::ComputeMembers` at `Source/TurboMCP/TurboMCPComment.cpp:112` computes it, and the
comment above it at lines 94-111 states the finding:

> `UEdGraphNode_Comment::GetNodesUnderComment` returns a **CACHED** set that
> `SGraphNodeComment::HandleSelection` fills from the **RENDERED** widgets […] Measured consequence: an
> asset read without its graph editor open has an **EMPTY** cache for every box […] The engine exposes
> no function that rebuilds it. So the RULE is reproduced rather than the cache read, taken from
> `SGraphNodeComment::IsNodeUnderComment`, which is
> `FSlateRect::IsRectangleContained(CommentRect, NodeRect)` in graph space.

The two constants that rule depends on are `FTurboMCPComment::FitPadding = 40.0` and
`FTurboMCPComment::TitleBarHeight = 42.0` (`Public/TurboMCPComment.h:118` and `:121`).

**A box's rectangle is `(NodePosX, NodePosY + TitleBarHeight)` to
`(NodePosX + NodeWidth, NodePosY + NodeHeight)`** — see `TurboMCPComment.cpp:121-124`. Tidy must use
the same rectangle, or it will disagree with `P/E-COMMENT` about what is inside a box, and the
disagreement will be invisible.

**RECOMMENDED ACTION FOR THE IMPLEMENTER:** move `ComputeMembers` and the two constants out of the
anonymous namespace in `TurboMCPComment.cpp` into a shared helper — for example
`FTurboMCPComment::MembersOf(UEdGraphNode_Comment*, TArray<FString>&)` declared in
`Public/TurboMCPComment.h` — and have tidy call it. Two copies of a geometric rule is one copy too
many, and this project's recurring failure is precisely two places that agree until they do not.

### Parent resolution

> *"the tightest box around it"*

**Rule.** For box `B`, consider every other box `A` whose rectangle wholly contains `B`'s rectangle
using the same rule as membership. Among those, the parent is the one with the **smallest area**.
Ties broken by `NodeDepth` then by node key, so the result is deterministic.

`UEdGraphNode_Comment::CommentDepth` exists — read at
`Engine/Source/Editor/UnrealEd/Public/EdGraphNode_Comment.h:79` — and the comment reader already
reports it (`ReadTurboMCPCommentList` prints `nested` counts). It is used here for **tie-breaking and
for the report only**, never as the primary rule, because it is data the editor maintains and tidy
must not become dependent on it being correct.

### Order of operations — this is the part most likely to be wrong

The spec's instruction is an **inside-out** pass. Spelled out:

1. Read the graph. Build `Nodes`, `Boxes`, membership and parents.
2. Compute columns and the pure-node shift **for all nodes** (section 4). This is layout, not boxing,
   and it is the same for a boxed node and a free one.
3. Collect the **depth of each box** from the parent chain: a box with no parent has depth 0, a child
   has parent's depth + 1.
4. Process boxes in **DECREASING depth**. For a box at depth `d`, lay out the **non-comment nodes it
   owns** — those in its `MemberKeys` that are **not** themselves inside a deeper box, so a node is
   laid out by exactly one box, the deepest one that contains it — left to right in column order.
5. **Then fit that box to its contents** (the same computation `P/E-COMMENT`'s `fit` does, at
   `TurboMCPComment.cpp:192`: grow to enclose the members plus `FitPadding`, allowing
   `TitleBarHeight`). Because step 4 runs before step 5 for every box, and deepest-first, an outer box
   sees its child boxes **already fitted** and treats each as **one block**.
6. After a box is fitted, treat the **whole box rectangle** as a single unit for its parent's step 4.
7. Finally, the **boxless group** (section 6).

**The failure this ordering prevents:** if an outer box were fitted before its children were arranged,
the outer rectangle would be computed from stale child sizes, and the second pass would have to move
the outer box — which the spec forbids (*"Never moves a user's box as a whole"*).

**What "never moves a user's box as a whole" means precisely.** The box's **top-left corner is
preserved** and only `NodeWidth`/`NodeHeight` change. That is what `fit` already does:
`TurboMCPComment.cpp:277-283` sets `OutX = InComment->NodePosX; OutY = InComment->NodePosY;` and only
grows the size. Tidy must do the same, and the suite must assert the corner is unchanged — that is the
checkable form of "never moves a box".

---

## 6. Boxless groups, and execution order

> *"Groups with no box are ordered by execution order and placed left to right."*

**Rule.** A node is *boxless* when no box's membership contains it (comments themselves are neither
boxed nor boxless; they are the boxes). Boxless nodes are placed on a single row:

```
FreeStartX = max over all boxes of box.Right + BoxGap        (or GraphOriginX when there are no boxes)
FreeY      = 0
X          = FreeStartX
for each boxless node in (Column asc, then exec order, then key):
    node.X = X
    node.Y = FreeY
    X      = X + node.Width + HorizontalNodeGap
```

*"Execution order"* is the column already computed (section 4) — a node whose longest incoming chain
is short comes first. Ties are broken by the order the node appears in `Graph->Nodes`, then by key,
so the result is deterministic and a re-run of tidy on an already-tidied graph changes nothing.

---

## 7. The measure and the rollback

> *"Compares total wire length from before to after. Rolls back if worse, unless `Force` is true.
> Prints both numbers on the first line."*

### Wire length is defined by the existing reader, not invented

`FTurboMCPLayoutReport::WireLength` and `PinPosition` already exist at
`Source/TurboMCP/TurboMCPLayoutReport.cpp:138` and `:95`, and the reader's own output documents its
convention:

> *"wire length is the straight distance between linked pins. Pin positions are computed from each
> node's rectangle and pin order, because the engine exposes no pin position to a tool call, so the
> number is consistent and comparable rather than pixel exact."*

The convention, read from the code:

- `RectOf` (`:80`) is `(NodePosX, NodePosY)` to `(NodePosX + NodeWidth, NodePosY + NodeHeight)`.
- `PinPosition` (`:95`) puts an input on the node's **left** edge and an output on its **right** edge
  (`:126-128`), and the pin's row at `NodePosY + PinRowSpacing * (Index + 1)` where `Index` counts
  only pins of the same direction (`:112-124`) and `PinRowSpacing = 24.0` (`TurboMCPLayoutReport.cpp:26`).

**Tidy must use this same function.** It is the only way the two tools' numbers can be compared, and
the suite can check that `L-LAYOUT-REPORT`'s before-number matches tidy's before-number — a
cross-check between two tools, which is the project's standing preference over a tool's own report.

**RECOMMENDED ACTION:** expose `PinPosition` and `WireLength` (or a `TotalWireLength(UEdGraph*)`) from
`FTurboMCPLayoutReport` as public statics rather than duplicating them, for the same reason as
`ComputeMembers` in section 5.

### "Better"

```
WireLengthAfter < WireLengthBefore   -> apply
WireLengthAfter >= WireLengthBefore  -> roll back, UNLESS bForce
```

Strictly-less, not "not worse". `T-LAY-5` in `Spec/06_ACCEPTANCE.md:150` says
*"wire length not worse, or rolled back with numbers"* — which is the acceptance criterion for the
**outcome**: after tidy runs, the graph's wire length must not be worse. A pass that changes nothing
and rolls back satisfies that, and so does a pass that improves. A pass that makes it equal but moves
every node has changed the user's graph for no gain, so it rolls back.

### Rollback — what is stored and how it is restored

**Tidy never writes until the measure is in.** The design is: `Plan` computes the **planned positions
into a second array** (`TArray<FNode> PlannedNodes` alongside `Nodes`), `WireLengthAfter` is measured
on `PlannedNodes`, and `Apply` writes only when `bWouldApply`. **So the normal rollback is that no
write ever happened** — the strongest possible form, and the reason the plan carries values rather
than pointers.

`Apply` still needs a rollback for the two things it must do before it can know the outcome:

| What is written | When | How it is undone |
|---|---|---|
| node `NodePosX`/`NodePosY` | only after `bWouldApply` is true | nothing to undo — the write is the point |
| comment `NodeWidth`/`NodeHeight` | only after `bWouldApply` is true, and never `NodePosX`/`NodePosY` | nothing to undo |
| **copied variable getters** (section 11) | before the measure, because a copy changes the graph's node set | recorded in `PlannedGetters`; on rollback each is `DestroyNode`d and the pin that was re-pointed is linked back to the original |
| **reroute knots** (section 12) | before the measure, same reason | recorded in `PlannedKnots`; on rollback each is `DestroyNode`d and the wire it carried is re-made between the original two pins |

**The two creations are the only irreversible steps and they are the reason the rollback exists as
code rather than as a design property.** The spec's *"Rolls back if worse"* is therefore implemented
as: create the getters and knots, measure, and if the measure is not better, destroy exactly the
objects recorded in `PlannedGetters` and `PlannedKnots` and restore the links recorded beside them.
`SaneProbe` in the acceptance asset exists to prove a pass leaves an already-good graph alone.

**Do not use `FScopedTransaction` as the rollback mechanism.** A transaction is an undo entry, not a
guarantee, and `P4` in `Spec/02_PRINCIPLES.md` already makes every Edit one transaction — a second
transaction that is conditionally cancelled would be a second undo entry for one call.

---

## 8. Refusals — the spec's own instruction

`Spec/08_ROADMAP.md:5` says local repair comes first *"because it helps mostly good graphs, where full
layout tends to refuse"*. That is a requirement to **refuse**, and these are the conditions, all
evaluated in `Plan` so the Preview states them:

1. **A cycle among the nodes.** Column is defined by a longest path (section 4), which a cycle has no
   answer for. Refused by naming one node in the cycle. (A Blueprint should not contain one; if it
   does, tidy is the wrong tool and guessing an order would move nodes on no basis.)
2. **A comment box whose parent chain contains a cycle.** Two boxes each containing the other is
   geometrically impossible but reachable by a corrupted asset, and the depth-first pass of section 5
   would recurse forever. Detected while building the chains and refused by naming both boxes.
3. **An unreachable node.** `P/E-GRAPH-DEAD` owns those, and the spec here says *"Disconnected groups
   are not moved into the layout"*. They are excluded from the layout, **not** refused — but if the
   graph contains any, the result says how many were left where they are, so a caller is not
   surprised by nodes that did not move.
4. **A node whose `NodeWidth` or `NodeHeight` is zero.** Pins cannot be positioned on a zero-size
   rectangle, so the before-number would be meaningless and the comparison would be against nothing.
   This is **the state a node is in before the editor has ever rendered it** — measured and recorded
   in `TestRecord.md` section 6d: a commandlet-created node has no pins and no size until
   `AllocateDefaultPins` and a reconstruction. Refused by naming the nodes, with the advice to open
   the Blueprint once or run the asset's commandlet.
5. **A graph with no nodes.**

Each refusal opens with `error:` and names what stopped it, per the project's convention that an
error names its reason and a no-op opens with `unchanged:`.

---

## 9. `Apply`'s result format

`P4` requires an Edit to open with `changed:` or `unchanged:`, and the spec requires **both wire
length numbers on the first line**:

```
changed: tidied <Graph> in <Asset> : wire length 12345.6 -> 8901.2
```

When the measure is not better and `Force` is not set:

```
unchanged: <Graph> would be worse (wire length 8901.2 -> 12345.6), so it was rolled back
```

When `Force` is set and the measure is worse:

```
changed: tidied <Graph> in <Asset> : wire length 8901.2 -> 12345.6 (WORSE, applied because Force was set)
```

Then, on following lines: how many nodes moved, how many boxes were resized (and that **no box
corner moved**), how many getters were copied, how many knots were added, how many unreachable nodes
were left alone, and the standing `; the change is in memory and NOT saved. Call E-SAVE.`

---

## 10. Constants

| Constant | Value | Where it comes from |
|---|---|---|
| `FitPadding` | `40.0` | `FTurboMCPComment::FitPadding`, `Public/TurboMCPComment.h:118` |
| `TitleBarHeight` | `42.0` | `FTurboMCPComment::TitleBarHeight`, `Public/TurboMCPComment.h:121` |
| `PinRowSpacing` | `24.0` | `TurboMCPLayoutReport.cpp:26` |
| `HorizontalNodeGap` | `60.0` | **new**, chosen to be larger than `FitPadding` so a fitted box's edge does not sit on a neighbouring node |
| `VerticalNodeGap` | `80.0` | **new**, chosen to be larger than `TitleBarHeight` so a stacked row does not fall under the box above its title |
| `BoxGap` | `40.0` | **new**, the clearance between two sibling boxes, set equal to `FitPadding` so a box's own margin is not doubled |

The three new ones must be `static constexpr` members of `FTurboMCPGraphTidy` so the suite can cite
them rather than hard-coding numbers.

---

## 11. Getter splitting

> *"A variable getter used by several consumers is split into one getter per consumer, placed next to
> it. Only variable getters get copied, because a getter takes no input and changes nothing."*

**Rule.** For each `UK2Node_VariableGet` whose value pin has **more than one** linked consumer pin:

```
count = number of consumer pins
if count < 2: leave alone
keep the ORIGINAL getter for the FIRST consumer (in the node order of the consumers)
for each remaining consumer:
    make a copy: NewObject<UK2Node_VariableGet>(Graph)
    copy the VariableReference EXACTLY (MemberScope, MemberName, MemberGuid)
    CreateNewGuid(); AllocateDefaultPins()
    place at (consumer.NodePosX - copy.Width - HorizontalNodeGap, consumer.NodePosY)
    break the original's link to this consumer; link the copy's value pin to it
```

Three details that are not optional:

1. **`AllocateDefaultPins` must be called.** Measured in `TestRecord.md` section 6d: `NewObject` does
   not allocate a node's pins, and `UEdGraph::AddNode` does not either — the editor allocates them
   when the node is reconstructed for display, which does not happen headlessly.
   `UK2Node_Knot::GetOutputPin` is `return Pins[1];` **unguarded** and crashed on an empty array.
2. **`MemberGuid` is copied**, not left default. `UK2Node_VariableGet::VariableReference` is a
   `FMemberReference`; a local-variable reference resolves through its GUID, and a copy with an empty
   GUID would point at nothing. `FBlueprintEditorUtils::FindLocalVariableGuidByName` exists for the
   reverse lookup (`Engine/Source/Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h:1036`).
3. **The copies are recorded for rollback.** See section 7.

**Merging is off by default and is a separate input.** The spec says *"Merging getters is an option,
off by default, and when on it only merges getters that feed the same consumer"*. This requires a
second boolean. **Recommendation: do NOT implement merging in the first pass** — implement the input
and refuse with a named reason when it is set, so the capability is honest about what it does rather
than silently ignoring a caller's request. Record the decision in the tool's doc comment.

---

## 12. Reroute knots

> *"Adds reroute knots where they make a wire straight or stop it crossing a node (same rules as
> `P/E-GRAPH-KNOTS`), and counts them in the measure."*

**This is the one requirement that must not be implemented twice.** `P/E-GRAPH-KNOTS` is a separate
capability with its own spec (`Spec/WORKING_GRAPH_KNOTS.md`, written by a parallel workstream) and its
own rules. Tidy must **call the same placement routine** it produces, not re-derive the rules, for the
same reason section 5 says not to copy `ComputeMembers`.

`KnotsPlanned` is counted in the report. Whether knot placement contributes to `WireLengthAfter` is
**OPEN** — see below.

---

## 13. The `UFUNCTION` declarations

Following the existing style exactly — the doc comment is the agent-facing description, and
`Spec/02_PRINCIPLES.md` `P10` requires it to start with its kind (`P-` for a preview, `E-` for an
edit). Placement: `PreviewTurboMCPGraphTidy` in `UTurboMCPPreviewToolset` beside
`PreviewTurboMCPComment`, `EditTurboMCPGraphTidy` in `UTurboMCPEditToolset` beside
`EditTurboMCPComment`.

```cpp
	/**
	 * Work out how a graph would be tidied, without moving anything.
	 *
	 * Arranges the nodes inside each comment box the user made, then fits that box to its new
	 * contents. **A box's top-left corner is never moved** — only its size changes — because a box
	 * is a label the user placed, not a container.
	 *
	 * NESTED BOXES are handled inside-out: each box is the child of the TIGHTEST box around it, the
	 * innermost boxes are arranged first, and an outer box then treats each child box as ONE BLOCK.
	 * Nodes with no box are ordered by execution order and placed left to right.
	 *
	 * WHAT IS INSIDE A BOX IS DECIDED GEOMETRICALLY, by the same rule `P-COMMENT` uses, because the
	 * engine's own `GetNodesUnderComment` is a cache filled from RENDERED widgets and is therefore
	 * EMPTY in a headless editor.
	 *
	 * The result prints the TOTAL WIRE LENGTH before and after on its first line, and says whether
	 * the pass would be applied or rolled back. A pass that does not make the wire length SHORTER is
	 * rolled back unless `force` is set.
	 *
	 * Refused, with the reason, when: the graph has a cycle among its nodes; two comment boxes each
	 * contain the other; a node has zero width or height, which is the state a node is in before the
	 * editor has ever rendered it and which makes the measurement meaningless; or the graph is empty.
	 * Disconnected groups are NOT moved and are reported as a count.
	 *
	 * The wire length is the straight distance between linked pins, computed the same way
	 * L-LAYOUT-REPORT computes it, so the two numbers can be compared.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);
```

```cpp
	/**
	 * Tidy a graph: arrange the nodes and fit the comment boxes.
	 *
	 * The same plan as `P-GRAPH-TIDY`, because the two share one code path up to the write.
	 *
	 * **Nothing is written until the measurement is in.** The planned positions are computed in
	 * memory, the wire length is measured on the PLAN, and the graph is only touched when the result
	 * is shorter — so the ordinary rollback is that no write ever happened. The two exceptions are
	 * copied variable getters and added reroute knots, which exist before the measurement because
	 * they change the graph's node set; both are recorded and both are DESTROYED again, with their
	 * links restored, when the measurement is not better.
	 *
	 * A BOX'S TOP-LEFT CORNER IS NEVER MOVED. Only its size changes. The result says so and the
	 * suite checks it, because "never moves a user's box as a whole" is otherwise a claim nobody can
	 * falsify.
	 *
	 * `force` applies the layout even when it is worse, and the first line says WORSE when it does,
	 * so a caller cannot mistake a forced regression for an improvement.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);
```

**Note for the implementer:** the existing tools take their optional inputs as `FString` and parse
them (see `FTurboMCPComment::Plan`'s `InSize` at `TurboMCPComment.h:109`). A `bool` is cleaner here
because `force` has exactly two states and the tool-call schema carries the type. Check how
`EditTurboMCPWidgetsAsVars` declares its `bool bIsVariable`
(`Public/Tools/TurboMCPToolsets.h`, the `EditTurboMCPWidgetsAsVars` declaration) — it is the existing
precedent for a `bool` argument and should be followed so the schema is consistent.

---

## 14. Includes — every symbol cited

The project has had **five guessed API names in a row turn out not to exist**
(`GetEdGraphPinTypeFromSimpleType`, `GetEnumType`, `CPF_Private`, `CPF_BlueprintReadWrite`,
`SetBlueprintVariableMetaData` with four arguments — `TestRecord.md` section 4b). So:

| Symbol | Header | Cited from |
|---|---|---|
| `UEdGraphNode_Comment`, `CommentDepth` | `EdGraphNode_Comment.h` | `Engine/Source/Editor/UnrealEd/Public/EdGraphNode_Comment.h:79` |
| `UEdGraphNode::NodePosX/NodePosY/NodeWidth/NodeHeight`, `Pins`, `GetGraph` | `EdGraph/EdGraphNode.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphNode.h:301,309,313` |
| `UEdGraphPin::LinkedTo`, `Direction`, `PinName`, `BreakLinkTo`, `BreakAllPinLinks` | `EdGraph/EdGraphPin.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphPin.h:398,312,306,434,437` |
| `UK2Node_VariableGet`, `GetValuePin` | `K2Node_VariableGet.h` | `Engine/Source/Editor/BlueprintGraph/Classes/K2Node_VariableGet.h`; `GetValuePin` is used already at `Source/TurboMCP/TurboMCPLocalVars.cpp` in the promotion path |
| `UK2Node_Knot`, `GetInputPin`, `GetOutputPin` | `K2Node_Knot.h` | `Engine/Source/Editor/BlueprintGraph/Classes/K2Node_Knot.h:54` — **`GetOutputPin` is `Pins[1]`, unguarded** |
| `UEdGraphNode::AllocateDefaultPins` | `EdGraph/EdGraphNode.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphNode.h`; the crash it prevents is in `TestRecord.md` section 6d |
| `UEdGraph::AddNode`, `Modify`, `MarkPackageDirty` | `EdGraph/EdGraph.h` | used at `Source/TurboMCP/TurboMCPComment.cpp` in `Apply`'s create path |
| `EGPD_Input`, `EGPD_Output` | `EdGraph/EdGraphPin.h` | `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphPin.h:312` |
| `UEdGraphSchema_K2::PC_Exec` | `EdGraphSchema_K2.h` | used at `Source/TurboMCP/TurboMCPLocalVars.cpp` in `IsDataPin` |
| `UBlueprint`, `FunctionGraphs`, `UbergraphPages` | `Engine/Blueprint.h` | used throughout `Source/TurboMCP/TurboMCPGraphDead.cpp` |
| `FTurboMCPAssetResolver::Resolve` | `TurboMCPAssetResolver.h` | `Source/TurboMCP/TurboMCPAssetResolver.h` |
| `TurboMCPNodeId::MakeKey` | `TurboMCPNodeId.h` | `Source/TurboMCP/Public/TurboMCPNodeId.h:60` |
| `FTurboMCPComment::FitPadding`, `TitleBarHeight` | `TurboMCPComment.h` | `Source/TurboMCP/Public/TurboMCPComment.h:118,121` |
| `FTurboMCPLayoutReport::PinPosition`, `WireLength`, `RectOf` | `TurboMCPLayoutReport.h` | `Source/TurboMCP/TurboMCPLayoutReport.cpp:95,138,80` |

The implementer must **open each of these and confirm the symbol** before writing against it. A
citation here is where to look, not a substitute for looking.

---

## 15. Test plan: `Tools/test-graph-tidy.py`

The project's recurring failure, recorded four times in `TestRecord.md`, is **a check that passes while
the thing it verifies is broken**. So every check below names **how it could fail**.

| # | Check | What it proves | **How it could fail** |
|---|---|---|---|
| 1 | the preview prints TWO wire-length numbers on its first line | the measure ran at all | break the print; the check goes red |
| 2 | **the graph is unchanged by a preview**, compared BY NODE IDENTITY AND POSITION | a preview is read-only | move a node in `Plan`; positions differ |
| 3 | the edit opens with `changed:` or `unchanged:` | `P4` | return a bare sentence |
| 4 | **no comment box's top-left corner moved** | the spec's hardest promise | change `Apply` to set `NodePosX` from the plan; the corner moves |
| 5 | **the wire length after is <= the before**, read back through `L-LAYOUT-REPORT` | the measure is real and the rollback works | make `Apply` write unconditionally on a graph it makes worse; the after-number is higher |
| 6 | when the measure would be worse and `force` is absent, the result says `unchanged:` **and the numbers got worse** | the rollback actually fires | disable the rollback; the result says `changed:` |
| 7 | with `force`, the result says `WORSE` | a forced regression is not disguised | drop the marker |
| 8 | **on `SaneProbe`, an already-good graph, tidy is `unchanged:` or improves** | a pass does not damage a good graph | break the comparison; a still graph gets moved |
| 9 | the result counts unreachable nodes when the graph has any, and does not move them | the spec's exclusion | include them in the layout; their positions change |
| 10 | refusals: an empty graph, a zero-size node, a cycle | `P2`, refuse rather than guess | remove a refusal; the tool answers something instead of `error:` |
| 11 | **the preview refuses the same inputs the edit refuses, with the SAME reason** | `D-02` | put a check in `Apply` only; the preview accepts |

**Check 5 is the one that matters and is the hardest to fake**, because it reads the after-number
through a DIFFERENT tool (`L-LAYOUT-REPORT`) rather than trusting tidy's own report. If only one check
can be written, write that one.

**Check 4 is the second.** "Never moves a user's box as a whole" is otherwise a claim in a comment.

### What the fixture can and cannot support

**The acceptance kit `TA-1`, `TA-2`, `TA-3` REQUIRED BY `T-LAY-5`, `T-LAY-7` AND `T-LAY-8` DOES NOT
EXIST.** I checked the task board and the acceptance document: `Spec/06_ACCEPTANCE.md:16-18` defines
them, and none is built.

What exists is `TurboMCPAcceptance`, built by
`Source/TurboMCP/TurboMCPAcceptanceAssetCommandlet.cpp` and committed. It has four function graphs:

| Graph | Contains | Usable for tidy? |
|---|---|---|
| `KnotProbe` | 6 nodes, three knots between two distant calls | **yes** — a graph tidy should improve |
| `DeadProbe` | 4 nodes, two disconnected groups with every link broken | **yes, for check 9** |
| `LocalVarProbe` | 8 nodes, two values read twice, one safe and one unsafe | **yes, for check 11's context** |
| `SaneProbe` | 2 nodes, an ordinary chain | **yes, for check 8** |

**It has NO COMMENT BOXES AT ALL** — measured: `ReadTurboMCPCommentList` reports
`0 box(es), 0 nested`. So **checks 4, 5 and 8 cannot be tested against any graph that exists**, and
nesting cannot be tested at all.

**That is the key recommendation of this spec: tidy needs a FIFTH probe graph added to
`TurboMCPAcceptanceAssetCommandlet`.** Specifically a `TidyProbe` function graph with:

1. **a clean chain of 3 nodes** — proves check 8's "already good stays good" in a graph where tidy
   should be able to improve something else;
2. **a deliberately messy run of 4 nodes at wrong Y values and out of column order** — proves checks 1,
   3 and 5, because there is wire length to reduce;
3. **two comment boxes, one geometrically inside the other** — proves check 4 (no corner moves) and
   exercises the nesting order of section 5. This is the only structure in the whole capability that
   is not otherwise testable;
4. **a node placed far from its chain** so a wire crosses another node's rectangle — proves check 9's
   exclusion is not confused with a real move.

`DeadProbe` and `LocalVarProbe` already cover checks 9 and 11's context, so `TidyProbe` need not
duplicate them.

### Cross-tool checks the project prefers

- `L-LAYOUT-REPORT`'s `wire length` for the same graph must equal tidy's **before** number. Two tools,
  one number: a disagreement is a bug in one of them and the check says so.
- `L-LAYOUT-REPORT`'s `counts:` node total must match `R-BP-LOAD`'s graph node count. This pattern is
  already used in `Tools/test-capture.py` for the same reason.

---

## 16. What will remain unverified

Stated plainly, because the project's rule is that an unverified claim is recorded rather than
implied:

1. **Nothing has been compiled or run.** This is a specification.
2. **Nesting cannot be tested until `TidyProbe` exists.** Until then, the inside-out order of section 5
   and the "child box as one block" rule rest on code review, which is exactly the position
   `P/E-GRAPH-DEAD`'s removal and `P/E-LOCAL-VARS`'s promotion were in for five rounds before the
   acceptance asset found a live bug in one of them on its first use (`TestRecord.md` section 6d).
3. **The wire-length conventions agree only if `PinPosition` is shared.** If the implementer copies it
   instead, the cross-tool check will fail for a reason unrelated to tidy, and the temptation will be
   to weaken the check rather than to share the function.
4. **`P/E-GRAPH-KNOTS` does not exist yet.** Knot placement in tidy depends on it. Until it exists,
   tidy's knot half cannot be implemented without duplicating rules, which section 12 says not to do.
5. **Getter merging is deliberately not implemented** and must be refused by name.
6. **Whether knot placement contributes to the measured wire length is OPEN** (section 17) — so the
   comparison tidy makes may be incomplete even once it works.

---

## 17. OPEN

Genuinely undecided from the source. Each needs a decision from the project owner or a measurement,
not a guess:

1. **Do added knots count toward `WireLengthAfter`?** A knot has its own rectangle and `PinPosition`
   gives it pins, so a wire through a knot is measured as two segments and can be LONGER in straight
   distance than the single segment it replaced — while crossing fewer nodes, which is the actual
   goal. If knots count, tidy may roll back a layout that is visually better. If they do not count,
   the measure ignores nodes the pass itself added. **The spec says "counts them in the measure"**
   (`03_TOOLS.md:433`) which suggests they count, but then "rolls back if worse" can fight the knot
   pass. This is the riskiest rule in the spec.
2. **Should merged getters be implemented in phase 6, or refused by name?** Section 11 recommends
   refusing. The spec lists merging as an option, so refusing is a deliberate shortfall.
3. **Does tidy apply to an EVENT graph, or only a FUNCTION graph?** The spec does not say. Local
   variables are function-only (`FBlueprintEditorUtils::DoesSupportLocalVariables`), but tidy moves
   nodes rather than adding locals, so it plausibly applies to both. The getter-splitting of section 11
   does not care. **Recommendation: apply to any graph**, and note that `P/E-LOCAL-VARS` is the one
   that must refuse an event graph.
4. **What happens to nodes that are inside a box but whose box is inside another box?** Section 5 step
   4 says the DEEPEST box lays them out. If the project prefers the OUTERMOST to own them, the
   ordering changes. The spec's *"the tightest box around it"* is about boxes, not nodes, so the
   reading taken here is the deepest box. Flagged because it is an interpretation.
5. **`NodeDepth` versus geometry for parent resolution.** Section 5 uses geometry with `CommentDepth`
   only as a tie-break. If the editor ever maintains `CommentDepth` more reliably than geometry, that
   ordering should be reversed — checkable only by finding an asset where the two disagree.
