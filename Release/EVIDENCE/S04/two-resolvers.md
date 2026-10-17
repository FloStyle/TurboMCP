# S04 evidence — the divergence is concrete: two graph resolvers search DIFFERENT sets of arrays

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0.** The cause identified in the previous record is now localised to a specific, readable difference.

## The two resolvers, side by side

**The reader's** — `TurboMCPNodeId::FindGraphByName`, `TurboMCPNodeId.cpp:322`, used by the graph reader
(`TurboMCPGraphLoad.cpp:123`) and by layout, capture, comments, dead-node and node-resolve:

```cpp
// Every graph kind is searched, because an ID names a graph and not a kind of graph.
Collect(InBlueprint->UbergraphPages);
Collect(InBlueprint->FunctionGraphs);
Collect(InBlueprint->MacroGraphs);
Collect(InBlueprint->EventGraphs);
```

**The executor's** — a file-local `FindGraph` in `TurboMCPGraphRepoint.cpp`, used by the plan (L44) **and** the
executor (L164):

```cpp
TArray<UEdGraph*> Graphs;
Graphs.Append(B->UbergraphPages);
Graphs.Append(B->FunctionGraphs);
Graphs.Append(B->MacroGraphs);
for (UEdGraph* G : Graphs) if (G && (Name.IsEmpty() || G->GetName().Equals(Name, ...))) return G;
```

**The reader searches FOUR arrays. The executor searches THREE.** `EventGraphs` is missing, and — as the reader's
own comment says — *"an ID names a graph and not a kind of graph"*.

## Why that is sufficient to produce everything measured

- The two functions **agree on the name** but can **disagree on the object**, because they build their candidate
  lists differently and return the **first** match. Nothing anywhere compares the two results.
- A node id is `...:GraphName:<hash>` — it carries the **name**. So a caller and the tool can agree perfectly on
  the id while operating on **different `UEdGraph` objects with the same name**, which is exactly the state the
  instrumented counts showed: the executor at **6 nodes / 3 call** where the reader saw **5 / 3**, on the same
  named graph.
- And it explains the guard: the guard read the executor's object, where the arithmetic **was** right.

**This is the concrete defect, and it is a resolution defect, not a mutation defect.** The mutation code — which
I spent six rounds reading — was never wrong.

## What is NOT yet established, and it is one run

The instrumented counts came from **one** graph object each. I have **not** yet printed the two `UEdGraph`
pointers, their names and their full paths together, nor how many graphs in the asset share the name
`AppendChainProbe`. So:

- **established by reading:** the two resolvers search different arrays, so they can return different objects;
- **established by measurement:** the executor and the reader count different node populations on the same named
  graph;
- **NOT established:** whether this particular fixture has the name in more than one array, and which two objects
  are involved.

That distinction is deliberate. Six hypotheses in this P0 were stated as findings and had to be withdrawn; this
record separates the part that is read from the part that is measured, and names the one run that closes the gap.

## The fix direction, unchanged and now specific

1. **One resolver.** `TurboMCPGraphRepoint.cpp`'s local `FindGraph` must be deleted and replaced by
   `TurboMCPNodeId::FindGraphByName`, so the tool and the reader resolve a name the same way. That the repository
   has **two** resolvers for one concept is the defect's root, and it is the same class as ARC06.
2. **Ambiguity must refuse, not pick.** `FindGraphByName` must be able to report that a name matched more than one
   graph, and the tools must **refuse** rather than take the first. This is cheap and it is the guard that would
   have turned this P0 into a refusal on day one.
3. **A guard must verify the object the report describes.** The `NodeCountBefore` guard is correct and stays, but
   it verified the executor's object — which is exactly why it passed. It must read the graph the **reader**
   resolves, or the tool must first prove the two agree.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPNodeId.cpp:322-347` and the local `FindGraph` in
  `Source/TurboMCP/TurboMCPGraphRepoint.cpp`
- `Release/EVIDENCE/S04/cause-different-graph.md` (the instrumented counts)
- editor: 0 running at the end, no lease

## Next action, in one step

Print, in one run, **both** resolutions of `AppendChainProbe`: each `UEdGraph*` pointer, `GetName()`, its full
path, and the number of graphs in the blueprint whose name matches. That closes the gap between "they can differ"
and "these two objects are the ones". Then delete the local `FindGraph` in favour of the shared resolver, make the
shared resolver report a multiple match, and re-run `.agent/tmp/three-readings.py` as this P0's acceptance check.
