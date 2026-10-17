# S04 evidence — `SpawnNodeOfType` REGISTERS the node, so the cause is NOT there either

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** This record removes the last structural suspect and states what remains.

## The read, as planned

`FTurboMCPNodeType::SpawnNodeOfType` (`TurboMCPNodeType.cpp:315-358`):

```cpp
FBlueprintActionInfo ActionInfo(Entry.Owner, Entry.Spawner);
UEdGraphNode* Spawned = Entry.Spawner->Invoke(InGraph, ActionInfo.GetBindings(), FVector2D::ZeroVector);
if (Spawned == nullptr) { ...; return ESpawnResult::SpawnerDeclined; }
Spawned->AllocateDefaultPins();
OutNode = Spawned;
return ESpawnResult::Spawned;
```

**It passes `InGraph` to the spawner** — the real graph — and that is the engine's action spawner, which
registers the node in the graph it is given. So:

**The hypothesis is withdrawn.** The created node **is** registered into the real graph. The "created node was
never in the graph, therefore the link pointed at an unregistered node" explanation **does not hold**.

This is the **fourth** of my own mechanisms disproved by one look, in four rounds. It is recorded plainly.

## One observation, offered as an observation and NOT as a cause

`Spawned->AllocateDefaultPins()` at L351 is called on a node the spawner has just **returned**, while
`BuildProbeNode` (L302-303) does **not** call it — it returns the spawner's node as-is. So the two paths differ in
whether they re-allocate pins. Whether a second `AllocateDefaultPins()` on a registered node is harmless, or
resets pins and breaks links, is **not established**, and I am deliberately not claiming it. It is the only
visible difference between the probe path (which works) and the spawn path (which does not), so it is the next
thing to look at — with a test, not a reading.

## What the transaction end looks like

`FScopedTransaction Tx` is declared at L215. There is **no `Commit()` call anywhere** in the file — only
`Tx.Cancel()` on the failure paths (L224, L239, L256, L271, L282, L300, L390). That matches the engine finding
recorded in round 9: on 5.8.3, **destroying the transaction without `Cancel()` IS the commit**. Nothing there is
wrong.

**And the report is built at L390-420, which is BEFORE `Tx` is destroyed** — so the node id is computed while the
transaction is still open. That is not itself a defect (the id is computed from the live pointer), but it means
**the report cannot verify what the graph contains after the commit.** The plan's `NodeCountBefore` (L45) is
recorded and never used, so **nothing in this function checks the graph's final state at all**.

## Where the P0 stands

| Claim | State |
|---|---|
| A `K2Node_CallFunction` is genuinely lost (3 → 2, `554569` gone) | **verified by class count** |
| The created target `3c85d3` is in the graph neither before nor after | **verified** |
| The report says `changed`, `compile: passed`, `old source retained` | **verified** |
| The source survives; `"old source retained"` is accurate | **verified** |
| The created node is registered into `InGraph` | **verified — this record** |
| The node is absent AFTER, while created and registered BEFORE | **both verified — this is the contradiction** |
| Identity unstable across the operation | **withdrawn** |
| The source was lost | **withdrawn** |
| `BuildProbeNode` leaves an orphan | **withdrawn** |
| The node is never registered | **withdrawn — this record** |
| The mechanism | **NOT ESTABLISHED** |

## What that contradiction now narrows to

The node **is** created and registered, and the report **names** it; the graph **does not have it** afterwards.
Between those two facts lie exactly two operations that can undo a registered node:

1. **the transaction commit** (`Tx` destroyed without `Cancel()` at end of scope), and
2. **the blueprint recompile** the tool performs itself, which respawns nodes and is the only other thing that
   touches this graph inside the same call.

Both are already inside the measured window, and the class-count method — which is immune to id churn — says the
**node count** falls, so this is not merely a recompile renaming nodes.

## The two guards, which are now the actionable part of this card

1. **`NodeCountBefore` must be used.** After the commit, the tool must re-read the graph's node count, compare it
   with `NodeCountBefore` plus planned creations and removals, **refuse** on a mismatch, and **report both
   counts**. This one guard turns "the tool confidently reported a clean success while the graph disagreed" into a
   refusal, whatever the underlying cause turns out to be. **It is worth doing before the cause is known.**
2. **A preview must be proven not to write** — a case asserting the node count is unchanged across a
   `PreviewTurboMCPGraphRepoint` with `targetTypeId`. My reading says it will pass; a reading is not a test.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPNodeType.cpp:315-358`, `TurboMCPGraphRepoint.cpp:215, 390-420`
- `p0-confirmed.md` (class-count measurement), `p0-lead1-disproved.md` (previous retraction)
- editor: 0 running, no lease

## Next action, in one step

**Add the `NodeCountBefore` guard first**, because it is correct regardless of the cause: capture the count after
the commit, refuse on a mismatch with the plan's expectation, and report both counts. Then re-run the
class-count discriminator — if the tool now **refuses** instead of reporting success, the guard works and the P0
stops being able to hide. Finding the underlying cause comes after the tool can no longer lie about the outcome.
