# S04 evidence — the resolver divergence is LOAD-BEARING, proven by unifying it

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**This record disproves my own round-38 claim.** I wrote that the two resolvers "returned the same pointer with one
match each, so they agreed by luck of the fixture" and that this was "measured harmless". Unifying them **changed
the swap tool's result**, which is the opposite of harmless.

## The measurement

Four resolvers were unified onto `TurboMCPNodeId::FindGraphByName`, keeping each local name and signature so no
call site changed: `TurboMCPComment.cpp`, `TurboMCPGraphAppend.cpp`, `TurboMCPGraphStructure.cpp`, and (from the
previous round) `TurboMCPGraphRepoint.cpp`. Build: **exit 0**, artifact 3 736 576 bytes.

A fifth — `TurboMCPGraphSwapCall.cpp:23` — was unified in the same pass. Then:

```
test-graph-repoint   exit 0  RESULT: PASS
test-graph-swap-call exit 1  RESULT: FAIL (readback preserves the migrated execute link)
```

**That suite had passed three consecutive times with its own resolver in place**, measured the round before. So:

| Swap resolver | `test-graph-swap-call.py` |
|---|---|
| its own (three arrays, first match) | **PASS**, three consecutive runs |
| delegating to the shared (four arrays) | **FAIL** — `readback preserves the migrated execute link` |

## What that proves, and it is the real finding

**The two resolvers are NOT equivalent for this tool.** Unifying them silently changed its behaviour. So the
divergence is **load-bearing**, and one of these must be true:

- the shared resolver returns a **different graph** for `AppendChainProbe`, and the swap tool's edit therefore
  lands in a graph the check does not read; or
- `TurboMCPNodeId::FindGraphByName` does something the local one did not, beyond the extra `EventGraphs` array.

**I have not established which, and I am not guessing** — this sequence has nine disproved mechanisms behind it.
What is established is that the difference is **behavioural, not cosmetic**, which is exactly the ARC06-family
concern: **two resolvers for one name produce different results.**

## The reversion, and why I reverted rather than pushed on

The swap resolver's own body is **restored**, with a comment naming the measurement, the suite and the three
passes that preceded it. Both suites now pass:

```
test-graph-swap-call exit 0  RESULT: PASS
test-graph-repoint   exit 0  RESULT: PASS
```

**I reverted because I had introduced a regression I did not understand.** Leaving it in would have meant shipping
a tool change whose only measured effect was to break its own suite, justified by a theory. The defect the reverted
code represents is real and stays carded.

## A build failure worth recording, because it looked like a code error

The first build after the reversion failed:

```
Link [x64] UnrealEditor-TurboMCP.dll: Exited with error code 9001. This action will retry without UBA
Result: Failed (OtherCompilationError)
```

The cause was **an editor still running and holding the DLL**, not the edit: `Get-CimInstance` reported 1 editor,
`start-editor.ps1 -Stop` released it, and the same build then **succeeded**. A link failure with exit 9001 is a
**lock**, and the first thing to check is whether an editor holds the artifact — not the diff.

## State of the unification

| Resolver | State |
|---|---|
| `TurboMCPGraphRepoint.cpp` | **unified**, no regression |
| `TurboMCPComment.cpp` | **unified**, no regression |
| `TurboMCPGraphAppend.cpp` | **unified**, no regression |
| `TurboMCPGraphStructure.cpp` | **unified**, no regression |
| `TurboMCPGraphSwapCall.cpp` | **REVERTED — divergence is load-bearing here** |
| `TurboMCPGraphPatch.cpp`, `TurboMCPGraphReplace.cpp` | inline candidate lists, not yet examined |

## Evidence paths

- this record; the two suite results quoted above
- `Source/TurboMCP/TurboMCPGraphSwapCall.cpp` (the restored body and its comment)
- build exit 0, artifact 3 737 600 bytes; tracked-diff fingerprint
  `e32ef2788eae16df830ca44c713264e5cd5b3b48b3b6556b5b4f07092f1d26fc`
- editor: 0 running at the end, no lease

## Next action, in one step

Establish **what differs** between the two resolvers for `AppendChainProbe` by measurement, not reading: have the
swap tool log the `UEdGraph*` it resolves **and** the one `TurboMCPNodeId::FindGraphByName` returns, for the same
name, exactly as the repoint tool did — that comparison printed `SAME OBJECT: YES` there in one run. If it prints
`NO` for the swap tool, the divergence is confirmed as the cause and the fix is to resolve once and carry the
graph. Only then re-attempt the unification.
