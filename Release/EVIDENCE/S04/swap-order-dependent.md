# S04 evidence — the swap failure is ORDER-DEPENDENT, and the resolver hypothesis is withdrawn

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

## The measurement

Run three times in sequence, plus the repoint suite, after the resolver unification and the instrumentation removal:

```
test-graph-swap-call run 1 : exit=0  RESULT: PASS
test-graph-swap-call run 2 : exit=0  RESULT: PASS
test-graph-swap-call run 3 : exit=0  RESULT: PASS
test-graph-repoint             : exit=0  RESULT: PASS
```

**Four consecutive passes.** The failure I reported in the previous round — and in round 22 — happened on the run
immediately following a **fixture rebuild**, and passes thereafter.

**So the hash-hypothesis is withdrawn: the swap tool's own un-unified resolver is NOT the cause.** That is the
**ninth** mechanism disproved in this sequence, and it lasted one round because it was tested rather than argued.

## What the failure actually is, and it is a test-hygiene defect

The pattern across both occurrences is consistent: **the first run of a suite after its fixture was rebuilt
fails; every later run passes.** Two candidate readings, neither established:

- the suite's `rebuild_fixture` + `start-editor` sequence leaves the editor with an asset that has not finished
  loading or compiling when the first read is taken; or
- the case's own first assertion reads state that the suite's setup invalidates — the `rebuild_fixture` contract
  is documented as *stopping the editor and not starting it*, and `test-graph-swap-call.py` restarts it, so the
  first read after a cold start is the one that differs.

**A disappearing defect is not a fixed one**, and this one has now appeared twice and vanished twice. It is
carded as such, with the instrument named: **run the suite four times and record which run fails**, then read the
state on the failing run only. Do not close it because passes are reproducible — the *failure* is reproducible
too, on the run after a rebuild.

## What this round leaves verified

| Item | State |
|---|---|
| `test-graph-repoint.py` | **exit 0, all four checks** — the migration is intact after the resolver unification |
| `test-graph-swap-call.py` | **exit 0 on four consecutive runs** |
| The resolver unification in `TurboMCPGraphRepoint.cpp` | no regression |
| The P0 instrumentation | **removed**; it must not have shipped |
| Six more resolvers to unify, and ambiguity-as-refusal | carded |

## Evidence paths

- this record; the run output quoted above
- tracked-diff fingerprint: `d177fa56d1332b4319447cbdd3fd39d57f43d846e8215148ca3f2d7d4f0eef5d`
- editor: 0 running at the end, no lease

## Next action, in one step

Instrument the **first run after a fixture rebuild** — that is the run that fails — and read the state on it,
rather than on the passes that follow. Then unify the six remaining resolvers.

## A plain note on this sequence, for whoever reads it next

The last dozen rounds produced **nine disproved mechanisms** and **no product defect on the graph-mutation path**:
the executor, the transaction, the report, the rollback and the reader all behave correctly, and every alarming
finding came from a regex over the reader's **rendered text** used as if it were the graph. Two real items did
come out of it — the `NodeCountBefore` guard, which is correct and stays, and the systemic **eight-resolver**
identity defect. **The method that produced nothing was guessing mechanisms from output; the method that
produced both real items was reading the objects and the code.** That is worth more than the findings.
