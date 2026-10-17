# S04 evidence — Route A works: the graph repoint suite PASSES, and its regression evidence now exists

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The graph repoint migration's regression evidence now EXISTS and PASSES.
One real failure remains in the sibling suite, recorded with its detail.**

## The scope conflict, resolved by Route A

The new-diagnostic comparison is now **scoped to the graph the edit touched**, and the scoping is **explicit in
the message** rather than a silent weakening:

- a diagnostic naming the edited graph is in scope and decides the edit;
- a diagnostic naming another graph is reported as **out of scope** with its count, never dropped — because a
  reader must be able to see what was set aside and why;
- the success report names the scope and the out-of-scope count too.

The limit is stated in the code: a diagnostic whose text names **no** graph cannot be attributed to the edit and
is therefore treated as out of scope, which is why the out-of-scope count is surfaced rather than hidden.

This is what the fixture's design required. `TurboMCPAcceptance` deliberately contains `ValueCycleProbe` and
`SelfLinkProbe` — "pure-value cycles" built so the reader can be tested against a cyclic input — so a
package-scoped rule refused every edit to every graph for reasons belonging to graphs the edit never touched.

## The result

| Command | Exit | Result |
|---|---|---|
| `python Tools\test-graph-repoint.py` | **0** | **RESULT: PASS**, 16.9 s — **all four checks pass** |
| `python Tools\test-graph-swap-call.py` | 1 | `RESULT: FAIL (readback preserves the migrated execute link)` |

**The graph repoint migration now has regression evidence, and it passes.** That was the open question this
sequence of rounds kept failing to answer, and it is answered:

```
=== editor lifecycle ===   PASS (an existing healthy editor was reused)
  ok
  ok
  ok
  ok
RESULT: PASS
```

Together with round 17's measured baseline fix, this is the first evidence in this run that the migrated writer
preserved the behaviour it replaced — the plan, the refusal, the edit, the readback, and the undo path all
exercised at L3.

## The remaining failure, with its detail, and NOT diagnosed

```
  ok preview is unchanged
  ok preview names migration
  ok preview uses safe keepOld default
  ok edit reports a changed swap
  ok readback contains the replacement node
  FAIL readback preserves the migrated execute link
```

Five of six checks pass; the sixth fails. **I have not diagnosed it, and I am not going to assert a cause.**
What is known and worth stating:

- it is **not** on the migration path this round touched — the swap tool is a different file and a different
  code path from the repoint tool;
- it is **not** the fixture-rebuild problem (the suite reached its checks);
- it could be a pre-existing defect in the swap implementation — which is exactly what finding `G06` ("Safe call
  swap resets unlinked arguments") and the audit's "swap migrates links but not unlinked defaults" describe — or
  it could be a defect the case has always had.

Deciding it needs the readback output and the case's own link comparison, which is one focused run.

## Verified / not verified

| Item | State |
|---|---|
| The ARC04 baseline mechanism and its fix | **verified** (round 17: 7 messages, 7 errors when forced) |
| The diagnostic scope conflict and Route A | **verified — the repoint suite passes** |
| **The graph repoint migration preserved behaviour** | **VERIFIED — `test-graph-repoint.py` exit 0, all four checks** |
| `test-graph-swap-call.py` | **FAILS one of six checks, cause not established** |
| C09's success half | not observed |
| M01–M08 | not run |
| Tools migrated | 1 of 101 |

## Evidence paths

- this record
- build: exit 0, artifact 3 740 672 bytes
- the two suite outputs quoted above
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Diagnose `check("readback preserves the migrated execute link")` in `Tools/test-graph-swap-call.py`: print the
readback and the exact link the case expects, then decide whether it is finding G06 (a real swap defect) or a
case defect. Then C09's success half, then M01–M08 — S04's remaining acceptance work.
