# S04 evidence — the match count is now OBSERVABLE, with no caller changed

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

## What was added

`TurboMCPNodeId::FindGraphByName` gained an **overload** that reports how many graphs matched, while the existing
signature is kept and every one of the six callers is untouched:

```cpp
UEdGraph* FindGraphByName(const UBlueprint*, const FString&);                        // unchanged behaviour
UEdGraph* FindGraphByName(const UBlueprint*, const FString&, int32& OutMatchCount);  // NEW
```

The counting version keeps the **FIRST** match as its answer - so nothing behaves differently - and the count is
the new information. The original entry point is now a two-line forwarder.

## Why an overload and not a behaviour change

Turning "ambiguous" into a **refusal** is the next step, and it must happen in the **mutating tools** only: a
reader should still be able to read, while a writer must not guess. Splitting the two is exactly what the overload
makes possible, and it means this step could not break anything even if the next one is deferred.

## Verified

| Check | Result |
|---|---|
| Build | **exit 0**, artifact 3 736 576 bytes |
| `test-graph-swap-call.py` | **exit 0 PASS, twice in sequence** |
| `test-graph-repoint.py` | **exit 0 PASS** |

The repeat run on the swap suite is deliberate: this session lost a round to attributing a cause from a single run
of a suite with an intermittent first-run failure.

## What remains for this defect, and it is one step plus a fixture

1. The **mutating** tools refuse when `OutMatchCount > 1`, naming the candidates.
2. The **reader** reports the count instead of refusing.
3. A **fixture with two graphs sharing a name** - which does not exist, and is why the defect has never fired.
4. Then D-008 (the empty-name contract) can be answered and `GraphPatch`/`GraphReplace` unified.

## Evidence paths

- this record; `Source/TurboMCP/Public/TurboMCPNodeId.h`, `Source/TurboMCP/TurboMCPNodeId.cpp`
- `Release/add_matchcount_overload.py`; build exit 0; 0 editors, no lease
