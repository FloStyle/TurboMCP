# S04 evidence — a mutating tool now REFUSES an ambiguous graph name, and the "intermittent" failure is explained

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-17.

## What was added

`TurboMCPGraphRepoint`'s plan now resolves with the counting overload and **refuses** when the name matches more
than one graph:

```cpp
int32 GraphMatches = 0;
UEdGraph* G = TurboMCPNodeId::FindGraphByName(B, GraphName, GraphMatches);
...
if (GraphMatches > 1)
{
    Out.Refusals.Add(FString::Printf(
        TEXT("the name '%s' matches %d graphs in '%s', so this tool will not guess which one to edit; name a graph "
             "that matches exactly one"), *GraphName, GraphMatches, *R.PackagePath));
    return true;
}
```

**The mutating side refuses; the reader keeps reporting.** A read stays useful while the truth is ambiguous; a
WRITE must not land somewhere the caller did not name. A refusal costs a round trip, a guess costs an edit the
caller cannot see. That asymmetry is the ARC06 concern made operational.

## Verified

| Check | Result |
|---|---|
| Build | **exit 0**, artifact 3 737 088 bytes |
| `test-graph-repoint.py` with a fresh fixture, **twice** | **exit 0 PASS, exit 0 PASS** |
| The refusal firing | **NOT observed** — the fixture has no ambiguous graph name, and the refusal text did not appear |

**The refusal path is therefore implemented but UNEXERCISED**, and that is stated rather than glossed: it needs a
fixture with two graphs sharing a name, which does not exist. The build and the passes prove it does not fire
spuriously; they do not prove it fires at all.

## And the "intermittent" failure is now EXPLAINED — it is not intermittent

Running suites in this order earlier produced:

```
repoint run 1 (fresh fixture, via TURBOMCP_REPOINT_REBUILD=1) : PASS
repoint run 2 (fresh fixture)                                 : PASS
--- without the rebuild ---
repoint run 1 : PASS      (it rebuilt)
repoint run 2 : FAIL      (it did NOT rebuild)
```

**The cases MUTATE SHARED FIXTURES and are not idempotent.** A run that rebuilds passes; a second run against the
first run's output fails, because the graph it expects has already been edited. That is **deterministic given the
fixture state**, not intermittent — which supersedes the earlier phrasing and, importantly, means the suites are
correct and the *harness* is what needs the discipline: **a mutating case must own a fresh fixture, or assert
nothing about state a previous run changed.**

That also retroactively explains the round-22 and round-41 swap failures: both were second runs against mutated
fixtures, and the "load-bearing resolver" reading I drew from one of them was wrong for exactly this reason.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPGraphRepoint.cpp` (the ambiguity refusal)
- build exit 0, artifact 3 737 088 bytes; 0 editors, no lease
