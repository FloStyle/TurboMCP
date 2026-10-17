# S04 evidence — the P0, narrowed by reading the code: an unstable identity, a probe node, and a lost source

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0.** Still open. This record adds code-level findings and does **not** claim a root cause.

## What the code does, read from `TurboMCPGraphRepoint.cpp`

| Line | What it does | Why it matters here |
|---|---|---|
| **L41** | `Out.bKeepSource = BoolText(KeepSource) == TEXT("true")` | the plan's own record of `keepSource` |
| **L45** | `Out.NodeCountBefore = G->Nodes.Num()` | the plan already records the before-count |
| **L46** | `FindNode(G, SourceNodeId)` → `MakeForNode(N, true) == Id` | **identity is a generated id, not a lookup by key** |
| **L51** | `Target = FTurboMCPNodeType::BuildProbeNode(B, G, TargetTypeId, Note)` | **the PLAN builds a node** — to validate the type and find its pins |
| **L236** | `FTurboMCPNodeType::SpawnNodeOfType(B, G, P.TargetTypeId, Target, Note)` | the EXECUTOR builds **another** node, and rebinds `Target` |
| **L309-315** | `if (!P.bKeepSource) { if (Source->Pins.Num() > 0) { G->RemoveNode(Source); } }` | the source is removed **only** when `keepSource` is false |
| **L413-417** | `TargetId = MakeForNode(Target, true)`; report `old source %s` from `P.bKeepSource` | the reported id is **computed from the live pointer**, and the word "retained" comes from the plan flag |

## Finding 1 — the reported node identity is unstable across a recompile

`FindNode` matches by `TurboMCPNodeId::MakeForNode(N, true)`, and the earlier migration record states that node
ids are `...:GraphName:<6hex>`. A 6-hex suffix is a **hash**. If that hash is derived from the node's **GUID**,
then a node that is destroyed and respawned — which is exactly what a blueprint recompile does to a node — gets
a **different id**.

That single property explains a symptom that has cost several rounds and that I had recorded as "my probe's
pattern was wrong":

- `PreviewTurboMCPGraphRepoint` with `targetTypeId` reports **`planned`**;
- the immediate `EditTurboMCPGraphRepoint` with the *same* arguments reports **`refused`**;
- a second run of the case **succeeded** where the first refused, and the reverse;
- the swap tool once answered `node 'c96a8c' is not in graph 'AppendChainProbe'` for an id the reader had just
  printed.

**A caller cannot reliably name a node across two calls.** That is a contract-level defect in the same family as
ARC06 ("no raw UObject pointers across calls") — the id is stable within one call and not across a recompile.
**This is stated as the reading the code supports, not as a verified cause.**

## Finding 2 — the plan creates a node, and the executor creates another

`BuildProbeNode` (L51) builds a node **in the plan**, and `SpawnNodeOfType` (L236) builds **a second one** in the
executor. The plan is a *preview* path used by the read-only `Preview` tool — so **previewing a created-target
repoint may already have added a node to the graph.** The report's `TargetId` is recomputed from the executor's
live `Target`, which is why the reported id is a node the reader then cannot find.

## Finding 3 — the source node vanished while `keepSource` was true

Measured: the disappeared node is the **source**, and the report says **"old source retained"**. With `L309`
guarding on `P.bKeepSource`, `BoolText("true")` returns `"true"` and removal should not run — so either the
removal did not run and the node was **lost another way** (recompile), or `bKeepSource` was not true on the
executor's plan.

**The plan already records `NodeCountBefore` (L45) and never uses it in the report.** The report should state the
graph's node count before and after, and the tool should refuse when they disagree with the plan — that is the
missing guard that let a data-losing edit be reported as a clean success.

## What is NOT established

- **The root cause of the P0.** Not `RemoveNode`, not the recompile, not the probe node — none is verified.
- Whether the created node exists in the graph under a **different id** (Finding 1 would produce exactly that).
- Whether `BuildProbeNode` leaves a node behind in the **preview** path.

## The discriminating next run, and it is cheap

**Count nodes by CLASS, not by id.** Ids are unstable; a `K2Node_CallFunction` is not. Read the graph before
and after one created-target edit and print the count **per class**. If the number of `K2Node_CallFunction` nodes
is unchanged while the ids all changed, the node was never lost and the whole id-based reading — including mine —
was measuring a recompile's churn. If the class count really falls, the node is genuinely gone.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPGraphRepoint.cpp` L41, L45, L46, L51, L236, L309-315, L413-417
- `Release/EVIDENCE/S04/created-node-lost.md` (the measurement that opened the P0)
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Run the class-count discriminator above. Then, independently of its result, make the tool **refuse** when the
graph's post-edit node count disagrees with the plan's `NodeCountBefore` plus the planned creations and removals,
and **report both counts** — a destructive edit must not be able to report a clean success.
