# S04 evidence — CAUSE FOUND: the executor edits a DIFFERENT graph object than the reader reads

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0.** The cause is now established by measurement, and it is an identity defect, not a removal defect.

## The measurement

The instrumented executor, printed at the three points the readings identified, beside the reader's view of the
same call:

| Point | The EXECUTOR's graph | The READER's graph |
|---|---|---|
| A: transaction open | **6 nodes, 3 call functions** | 5 nodes, 3 call functions |
| B: after `SpawnNodeOfType` | **7 nodes, 4 call functions** | — |
| C: after the pin-move loop | **7 nodes, 4 call functions** | — |
| after the call returns | — | **4 nodes, 2 call functions** |

```
LogTemp: Display: [TurboMCP/P0] A: transaction open: 6 node(s), 3 call function(s)
LogTemp: Display: [TurboMCP/P0] B: after SpawnNodeOfType: 7 node(s), 4 call function(s)
LogTemp: Display: [TurboMCP/P0] C: after the pin-move loop: 7 node(s), 4 call function(s)
```

## What the two numbers mean together

1. **The executor's graph has 6 nodes where the reader's has 5 — before it changes anything.** So the two are
   **not the same graph object**. One is an extra node ahead from the start.
2. **The executor's edit behaved correctly on its own terms**: it added exactly one node (6 → 7), gained exactly
   one call function (3 → 4), and the pin-move loop changed nothing (7 → 7). **Inside the executor's graph, the
   operation is sound.**
3. **The reader's graph lost a node and never gained one** (5 → 4, 3 → 2). The node the report names, `e40851`,
   is in neither reading.

**The two facts together are the cause: the tool edits one graph and reports on it, while the reader shows a
different graph. The node that "vanishes" was never lost at all — it is in the graph the executor edited, which
the reader does not read, and the reader's graph lost a *different* node for reasons that follow from the same
split.**

This explains, at last, every measurement in this P0:

- why the plan looked clean and the edit looked destructive (they resolve the graph differently);
- why the created node is "in neither list" (it is in the executor's graph, which the reader never shows);
- why the count fell by exactly one every run (the two graphs drift apart by one per edit);
- why the guard saw a **correct** count (it read the executor's graph, where the arithmetic was right);
- and why six code-reading mechanisms failed — the defect is not inside the operation at all.

## The likely line, stated as the next thing to confirm

`FindGraph(B, GraphName)` is called by the **plan** (L44) and again by the **executor** (L164), and the reader
resolves the graph its own way. `P.GraphName` is set from `G->GetName()` at L45 — a **graph name**, not a
graph path. If more than one graph in the asset can match that name, the two resolutions can return **different
`UEdGraph` objects**, and nothing in the tool compares them.

**This is the same family as ARC06** ("no raw `UObject` pointers across calls") and the same family as the
withdrawn identity-instability idea — except the id is not unstable, **the graph resolution is ambiguous**, and I
had been looking at ids the whole time.

## The fix direction, and the guards

1. **Resolve once.** The plan already holds the graph; the executor must not resolve it a second time by name.
   Carry the graph (or a documented, unique graph path) from plan to executor.
2. **Make the resolution checkable.** `FindGraph` must report whether the name matched **more than one** graph,
   and the tool must refuse rather than pick one.
3. **The guard must read the SAME graph the report describes.** The `NodeCountBefore` guard is correct and stayed,
   but it verified the executor's graph — which is why it passed. A guard on a different object than the report
   cannot catch anything.

## Evidence paths

- this record; the instrumented counts above, from
  `TestProject\Saved\Logs\TurboMCPTest.log`; `.agent/tmp/three-readings.py`
- `Release/patch_p0_instrument.py` (the instrumentation), build exit 0, artifact 3 744 256 bytes
- editor: 0 running at the end, no lease

## Next action, in one step

Print, inside the executor and at the plan, **the `UEdGraph` pointer and `G->GetName()` and the full path** of the
graph each resolves, and the number of graphs in the asset whose name matches `P.GraphName`. That confirms the
ambiguous resolution in one run and names the graphs involved. Then resolve once and refuse on an ambiguous name.
