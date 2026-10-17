# S04 evidence — Lead 1 is DISPROVED: the probe node never touches the real graph

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** This record removes a suspect. The cause is still not established.

## Lead 1, disproved by reading `BuildProbeNode`

I had written, as a lead: *"`BuildProbeNode` runs in the PLAN ... if it registers that node in `G->Nodes` and
nothing removes it, then previewing a created-target repoint adds a node to the graph — a read-only tool with a
write effect."*

`TurboMCPNodeType.cpp:267-313` says otherwise, in its own words:

```cpp
UEdGraphNode* FTurboMCPNodeType::BuildProbeNode(UBlueprint* InBlueprint, UEdGraph* InGraph,
                                                const FString& InTypeId, FString& OutNote)
...
    UEdGraph* ProbeGraph = NewObject<UEdGraph>(InBlueprint, NAME_None, RF_Transient);
    ...
    ProbeGraph->Schema = InGraph->Schema;
    FBlueprintActionInfo ActionInfo(Entry.Owner, Entry.Spawner);
    UEdGraphNode* Spawned = Entry.Spawner->Invoke(ProbeGraph, ActionInfo.GetBindings(), FVector2D::ZeroVector);
```

**The probe node is spawned into a NEW `RF_Transient` graph, not into `InGraph`.** It is never added to the graph
the user is editing, and it is transient by construction.

So:

- **the preview does not add a node to the real graph** — the read-only tool is not writing;
- **the transient graph produces no orphan** in the edited graph;
- and Lead 1 is **withdrawn**.

This is the **third** of my own findings in three rounds that a single look at the code or a single measurement
disproved. It is recorded as a correction rather than quietly dropped, because the pattern matters more than the
individual item: I have been writing plausible mechanisms into evidence records faster than I have been testing
them, and each one costs a round to retract.

## What that leaves

The P0's **verified facts** are unchanged:

| Claim | State |
|---|---|
| A `K2Node_CallFunction` is genuinely lost (3 → 2, `554569` gone) | **verified by class count** |
| The created target `3c85d3` is in the graph neither before nor after | **verified** |
| The report says `changed`, `compile: passed`, `old source retained` | **verified** |
| The source survives / `"old source retained"` is accurate | **verified** |
| The identity is unstable across the operation | **withdrawn** |
| `BuildProbeNode` leaves a node in the real graph | **WITHDRAWN — it uses an `RF_Transient` graph** |
| Why `554569` went | **not established** |
| Why the created node does not persist | **not established** |

## The remaining lead, narrowed

`SpawnNodeOfType` (L315 onward) is the **only** path that puts a node into the real graph, and it is the one
function not yet read. Two questions to answer by reading it, and they are one look:

1. Does it call **`InGraph->AddNode`** (or `Modify` + `AddNode`) before returning `Spawned`?
2. Does it return a node that is **owned by `InGraph`**, or does it hand back a pointer without registering it?

If the answer to (1) is no, then the created node was never in the graph — which fits the measurement exactly —
and the link the tool made pointed at an unregistered node, which the engine then discarded along with the link's
reader, which would account for the lost `554569` **without** any `RemoveNode` call.

That is a **hypothesis**, and it is the last one this record states. It is one function read, and then it is
either confirmed or withdrawn like the others.

## The guards, unchanged and still cheap

1. The plan's **`NodeCountBefore` (L45) is recorded and never used** — the tool must refuse on a count mismatch
   and report both counts.
2. A preview must be **proven** not to write: a case that reads the graph's node count before and after a
   `PreviewTurboMCPGraphRepoint` with `targetTypeId` and asserts it is unchanged. Lead 1's reading says it will
   pass; the case is still owed, because a reading is not a test.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPNodeType.cpp:267-313`
- `Release/EVIDENCE/S04/p0-confirmed.md` (the class-count measurement), `p0-two-paths.md` (the retracted lead)
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Read `FTurboMCPNodeType::SpawnNodeOfType` (`TurboMCPNodeType.cpp:315`) and answer the two questions above. Then,
whether or not it is the cause, add the `NodeCountBefore` guard so this class of defect cannot report a clean
success again.
