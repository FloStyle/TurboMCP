# S04 evidence — it is SEVEN graph resolvers, not two

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P1, systemic.** One is now unified; six remain.

## What the grep found

```
TurboMCPComment.cpp:38       UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName, FString& OutError)
TurboMCPGraphAppend.cpp:29   UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)
TurboMCPGraphAppend.cpp:37       Graphs.Append(InBlueprint->UbergraphPages);
TurboMCPGraphPatch.cpp:35        Candidates.Append(InBlueprint->UbergraphPages);
TurboMCPGraphReplace.cpp:39      Candidates.Append(InBlueprint->UbergraphPages);
TurboMCPGraphRepoint.cpp:31  UEdGraph* FindGraph(UBlueprint* B, const FString& Name)     <-- now delegates
TurboMCPGraphStructure.cpp:42 UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)
TurboMCPGraphStructure.cpp:50     Graphs.Append(InBlueprint->UbergraphPages);
TurboMCPGraphSwapCall.cpp:23 UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& Name)
```

**Seven file-local graph resolvers**, each assembling its candidate list its own way, while the reader uses
`TurboMCPNodeId::FindGraphByName`. My previous record said "two"; that was the count in the one file I had opened.
It is **seven**, and the reader's is the eighth.

## Why this is the substantive finding of the sequence

A node id is `...:GraphName:<hash>` — it carries the graph **name and nothing else**. So when a blueprint holds two
graphs of the same name in different arrays, each of these eight resolvers may pick a different object, and **no
caller can tell which one was edited**. A tool can edit graph A, the reader can show graph B, and the report names
`...:<name>:<hash>` for both.

**That is the ARC06 family** — identity that cannot be verified across calls — and it is now **systemic** rather
than local.

## What was done this round, and it is verified

1. **`TurboMCPGraphRepoint.cpp`'s resolver now DELEGATES** to `TurboMCPNodeId::FindGraphByName`, so the tool and
   the reader resolve a name identically. The local name is kept so no call site changed.
2. **The P0 instrumentation is REMOVED** — `LogNodeCount`, `LogGraphResolution` and `LogNodeIds` and their six call
   sites are gone, along with the `K2Node_CallFunction.h` include they needed. It had logged on every edit and
   must not have shipped.
3. **Build: exit 0**, artifact 3 742 208 bytes.

## Regression check, and one honest caveat

| Suite | Result |
|---|---|
| `test-graph-repoint.py` | **exit 0, RESULT: PASS** — the migration is intact |
| `test-graph-swap-call.py` | **exit 1**, `FAIL (readback preserves the migrated execute link)` |

**The repoint suite passing is the regression evidence for this round's change**, since that is the file I edited.

**The swap failure is not explained and I am not attributing it.** Two things are known and they point in opposite
directions: the same check failed in round 22 and then passed twice in a row when instrumented in place, which
looked like order-dependent state; and `TurboMCPGraphSwapCall.cpp:23` has **its own resolver** which this round did
**not** unify. **If the swap tool resolves a different graph than the reader, the link its report describes would
not be in the graph the case reads** — which is the same signature as this check's failure. That is a
**hypothesis**; the sequence's record of eight disproved mechanisms says plainly that a hypothesis here is worth
little until it is a reading.

## The next step, one command

Run the same id-diff that settled the phantom P0, on the **swap** tool: have it print every id in its `G->Nodes`
after the edit, and print the ids the reader emits, then compare. That is the measurement that took one round to
end twelve, and it has not been taken for the swap path.

## Evidence paths

- this record; the grep output above
- `Source/TurboMCP/TurboMCPGraphRepoint.cpp` (the unified resolver, instrumentation removed)
- build exit 0, artifact 3 742 208 bytes; editor: 0 running, no lease
