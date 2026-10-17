# S04 evidence — the reader is NOT defective either: it INLINES value nodes by design

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **the reader-completeness defect is WITHDRAWN too.** What remains is one latent identity defect.

## What the code does

`TurboMCPGraphLoad.cpp:123-152` resolves the graph, then:

```cpp
TArray<FString> Notes;
const FTurboMCPGraphText Writer;
const FString Text = Writer.Render(Graph, DetailLevel, Resolved.ObjectPath, Notes);
...
FString Result = FString::Printf(TEXT("read: %s graph %s, format read, detail %s, %d node(s)"),
    *Resolved.ObjectPath, *Graph->GetName(), *DetailLabel, Graph->Nodes.Num());
```

**The header already prints the TRUE node count** — `Graph->Nodes.Num()` — which on the measured fixture is
**6**. And `Notes` exists precisely to declare anything that could not be printed in full:

```cpp
// Anything that could not be printed is already an inline `; error:` line, and it
// is repeated here so the caller cannot miss that the text is incomplete
if (Notes.Num() > 0)
{
    Result += FString::Printf(TEXT("\n\n; %d node(s) could not be printed in full:"), Notes.Num());
```

**So the reader is not silent and not incomplete.** It reports the total, and it declares what it could not print.

## The mechanism I mistook for omission

`FTurboMCPGraphText::RenderPinValue` (`TurboMCPGraphText.cpp:303`) inlines **value nodes** into the node that
consumes them:

```cpp
if (IsValueNode(RealTarget))
{
    if (InOutInlineNodes.Contains(RealTarget))
    ...
```

**A pure value node has no line of its own — it appears inside its consumer's pin text.** That is a documented,
deliberate rendering decision, and it is why three of six nodes in my readings had no
`...:GraphName:<id> ; class ...` line.

And that explains the "omission set changes after an edit": `fdd181` was **inlined before** the edit and became a
**top-level line after** it, because the edit changed what consumes it. **Nothing was omitted and nothing
changed about the reporting policy — the rendering SHAPE of one node changed because the graph changed.**

## Withdrawn, third time in this record's subject

| Claim | State |
|---|---|
| A node is lost from the graph | **WITHDRAWN** — `fdd181` present before and after |
| The reader omits nodes silently | **WITHDRAWN** — the header prints the true count and `Notes` declares gaps |
| The reader's omission set changes | **WITHDRAWN** — a value node moved between inlined and top-level, by design |
| The executor's arithmetic | **correct all along** (6 → 7, one created, none removed) |

**There was no P0.** Nine things I asserted in this sequence are now disproved, and every one of them came from
the same instrument: a regex over the reader's rendered text, treated as the graph.

## What genuinely remains open, and it is one item

**Two graph resolvers search different arrays** — the reader's `TurboMCPNodeId::FindGraphByName` collects four
(`UbergraphPages`, `FunctionGraphs`, `MacroGraphs`, `EventGraphs`) while the executor's file-local `FindGraph`
collects three, and **nothing compares them**. Measured on this fixture they return the **same object** with one
match each, so it is not causing the current behaviour — but it is a latent identity defect of the **ARC06**
family: a name that matches two graphs in different arrays would make the tool edit one and report on another.
**That is worth fixing on its own merits, and it is the only substantive finding this sequence produced.**

## The method record, which is the actual product of these rounds

**A regex over a tool's human-readable output is not a measurement of the tool's effect.** I used one as ground
truth for roughly twelve rounds and it produced: an invented data-loss P0, nine disproved mechanisms, and the
`NodeCountBefore` guard — which is a *correct* addition, and the only thing that survived.

**The instrument that worked** was reading the state from the **executor's own objects** (`G->Nodes`), which
settled in one round what twelve rounds of text-parsing could not.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPGraphLoad.cpp:123-152`; `TurboMCPGraphText.cpp:303-337`
- the executor's id lists from `TestProject\Saved\Logs\TurboMCPTest.log`
- editor: 0 running at the end, no lease

## Next action, in one step

Remove or cvar-gate `LogNodeIds`, `LogGraphResolution` and `LogNodeCount` — the instrumentation has done its job —
then fix the two-resolver divergence: delete the file-local `FindGraph` in `TurboMCPGraphRepoint.cpp` in favour of
`TurboMCPNodeId::FindGraphByName`, and make the shared resolver report a **multiple match** so an ambiguous graph
name is refused instead of silently resolved.
