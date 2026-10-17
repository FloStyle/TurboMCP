# S04 evidence — the "load-bearing" claim is WITHDRAWN, and ALL FIVE resolvers are now unified

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**This record corrects my own previous round.** I recorded that unifying the swap resolver made its suite fail and
that the divergence was therefore "LOAD-BEARING". That conclusion rested on a **single run**. Tested with repeats,
it is **wrong**.

## The repeat test

The swap resolver's delegation was restored — deliberately, to test the attribution — and built (exit 0, artifact
3 736 576 bytes). Then the suite was run **four consecutive times**:

```
swap run 1 : exit=0  RESULT: PASS
swap run 2 : exit=0  RESULT: PASS
swap run 3 : exit=0  RESULT: PASS
swap run 4 : exit=0  RESULT: PASS
repoint    : exit=0  RESULT: PASS
```

**Four passes on the unified resolver.** So the single failure in the previous round was the **order-dependence**
already identified in round 40 — the first run after a fixture rebuild — and **not** the unification.

## Withdrawn, and the reasoning error named

**"The swap resolver's divergence is load-bearing" is WITHDRAWN.** The evidence was one observation. This is the
**tenth** disproved mechanism in the sequence, and it is the same error as the nine before it: **acting on a single
measurement as though it were a repeated one.**

The specific mistake is worth stating exactly, because it is the one that keeps recurring: I ran the swap suite
**once** after the change, saw a failure, and attributed the failure to the change — when the *same suite on
identical code* had failed once and passed three times in earlier rounds. **A suite with a known intermittent
first-run failure cannot be used to attribute a cause from one run.**

## What is now done and verified

**All five named resolvers delegate to `TurboMCPNodeId::FindGraphByName`**, each keeping its local name and
signature so no call site changed:

| Resolver | State |
|---|---|
| `TurboMCPGraphRepoint.cpp` | unified |
| `TurboMCPComment.cpp` | unified |
| `TurboMCPGraphAppend.cpp` | unified |
| `TurboMCPGraphStructure.cpp` | unified |
| `TurboMCPGraphSwapCall.cpp` | **unified and verified with four consecutive passes** |

## The two remaining resolvers, and a REAL behavioural difference found by reading them

`TurboMCPGraphPatch.cpp` and `TurboMCPGraphReplace.cpp` assemble inline candidate lists. Both are the same shape,
and reading them turned up **two** differences from the shared resolver, not one:

1. **They omit `EventGraphs`** — the difference already known and named for the other six.
2. **They accept an EMPTY name** — `if (Wanted.IsEmpty() || Graph->GetName().Equals(Wanted, ...))` returns the
   **first graph**. `TurboMCPNodeId::FindGraphByName` **refuses an empty name** (`if (... || InGraphName.IsEmpty())
   return nullptr;`).

**Difference 2 is a behaviour change, not a bug fix.** An empty graph name in those tools currently means "take the
first graph"; after unification it would mean "resolve nothing". **So they must NOT be unified blindly** — the
question of what an empty graph name should mean is a contract decision, and it is the reason these two were left
alone rather than swept up with the other five. This is a case where reading the code first produced the answer,
which is the method that has actually worked in this sequence.

## Evidence paths

- this record; the five-pass output quoted above
- `Source/TurboMCP/TurboMCPGraphSwapCall.cpp` (the restored delegation and its note)
- `Source/TurboMCP/TurboMCPGraphPatch.cpp`, `TurboMCPGraphReplace.cpp` (the empty-name branch)
- build exit 0, artifact 3 736 576 bytes; tracked-diff fingerprint
  `69cab495de36e54fcf6b64d0f92623359525a48d15cfcb12b822272dc26d6fd4`
- editor: 0 running at the end, no lease

## Next action, in one step

Decide what an **empty graph name** means — today `GraphPatch` and `GraphReplace` treat it as "the first graph",
while the shared resolver refuses it — then unify those two accordingly, and make the shared resolver able to
report a **MULTIPLE MATCH** so an ambiguous name is refused rather than silently resolved. Then move out of this
defect family and resume the roadmap: **M01–M08, then S05.**
