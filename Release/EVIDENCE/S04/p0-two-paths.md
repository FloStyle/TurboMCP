# S04 evidence — the P0's two creation paths, read from the code

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** This record adds code-level facts and two leads. It does **not** claim a cause.

## What the code does, read from the source

| Where | What |
|---|---|
| `TurboMCPGraphRepoint.cpp:51` | the **PLAN** calls `BuildProbeNode(B, G, TargetTypeId, Note)` and sets `Out.bCreateTarget = true` |
| `TurboMCPGraphRepoint.cpp:236` | the **EXECUTOR** calls `SpawnNodeOfType(B, G, P.TargetTypeId, Target, Note)` and **rebinds `Target`** to the spawned node |
| `TurboMCPGraphRepoint.cpp:250` | when *not* creating, `Target = FindNode(G, P.TargetNodeId)` |
| `TurboMCPGraphRepoint.cpp:253` | `FindPin(Target, P.TargetPin, EGPD_Output)` — if the target disappeared, `FindPin` is called on it |
| `TurboMCPGraphRepoint.cpp:413` | the report's `TargetId = MakeForNode(Target, true)` — recomputed from the **executor's live pointer** |
| `TurboMCPGraphSwapCall.cpp:123` | the swap tool's **plan** also calls `BuildProbeNode` |
| `TurboMCPGraphCheck.cpp:477` | the check tool also calls `BuildProbeNode` |
| `TurboMCPGraphAppend.cpp:282, 648` | append calls `SpawnNodeOfType` — the same spawn-then-rebind shape |

## Lead 1 — `BuildProbeNode` runs in the PLAN, and the plan is what the READ-ONLY Preview tool calls

`PreviewTurboMCPGraphRepoint` renders `RenderPreview(P)` from a plan. The plan **builds a node** at L51 to
validate the type and locate its pins. If `BuildProbeNode` registers that node in `G->Nodes` and nothing removes
it, then:

- **previewing a created-target repoint adds a node to the graph** — a read-only tool with a write effect; and
- each preview leaves an orphan, which is consistent with the graph's node population drifting between runs.

Whether `BuildProbeNode` cleans up after itself is **not established** and is the first thing to read.

## Lead 2 — `Target` is rebound at L236, so the plan's probe pointer and the executor's node are different objects

The plan decides (`bCreateTarget`) from a **probe** node; the executor links a **spawned** node; the report names
the **spawned** node's freshly computed id (L413). MEASURED: that reported id (`3c85d3`) is in the graph neither
before nor after, while the `K2Node_CallFunction` class count **fell** from 3 to 2.

So the sequence the report describes — "created a node, repointed a reader to it" — is **not** the sequence the
graph ended up in. **Neither the created node nor the linking survived**, and the arithmetic of the counts says
the edit also **lost** a node (`554569`).

## What is verified and what is not

| Claim | State |
|---|---|
| A `K2Node_CallFunction` is genuinely lost (3 → 2, `554569` gone) | **verified by class count** |
| The created target `3c85d3` is in the graph neither before nor after | **verified** |
| The report says `changed`, `compile: passed`, `old source retained` | **verified** |
| The source survives | **verified** — my earlier claim is withdrawn |
| The identity is unstable across the operation | **withdrawn — four of five ids reused** |
| `BuildProbeNode` leaves a node behind | **not established — Lead 1** |
| The probe node and the spawned node diverge | **read from the code; consequence not established — Lead 2** |
| Why `554569` went | **not established** |

## The two guards that would have caught this, both cheap

1. **The plan's `NodeCountBefore` (L45) is recorded and never used.** The tool must compare the graph's node count
   after the edit against `NodeCountBefore` plus planned creations and removals, **refuse** on a mismatch, and
   **report both counts**. A destructive edit must not be able to report a clean success.
2. **The read-only tools must not write.** `PreviewTurboMCPGraphRepoint` plans with a function that builds a
   node; if that is not cleaned up, the preview is a mutation. It needs a case that reads the graph's node count
   before and after a preview and asserts it is unchanged.

## Next action, in one step

Read `FTurboMCPNodeType::BuildProbeNode` and `SpawnNodeOfType` themselves — the two functions this file calls —
and establish whether `BuildProbeNode` (a) registers its node in `G->Nodes`, and (b) removes it before returning.
That single answer decides Lead 1, and it is the only thing standing between this record and the cause.
