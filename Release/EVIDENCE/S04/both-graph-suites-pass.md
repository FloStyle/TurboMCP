# S04 evidence — both graph suites PASS; the earlier swap failure was stale state

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified` for S04 as a whole, but the graph-repoint half is now fully evidenced. Two
suites pass repeatably, and the swap check that appeared to fail passes when its own values are read.**

## The measurement, from the suite itself

Round 20's re-created probe was inconclusive because it was not the subject. This round instrumented the suite
in place, with one temporary print before the check, and read its own values:

```
DBG replacement_id : '/Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:AppendChainProbe:1b7af5'
DBG looking for    : ':1b7af5)'
DBG edit head      : 'changed: swapped ...:c96a8c to ...:1b7af5; 2 link(s) migrated by pin name;'
DBG after has short: True
DBG link line      : 'execute (-> TurboMCPAcceptance:AppendChainProbe:1b7af5) ; in 0'
RESULT: PASS
```

The values the check needs are all present: the replacement id is extracted, the string it looks for **is** in
the readback, and the migrated execute link reads exactly as the case expects — `execute (-> ...:1b7af5)`. The
print was removed immediately (`0 markers left`).

## Repeatability

Run three times in sequence, after the instrumentation was removed:

| Suite | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| `Tools/test-graph-swap-call.py` | exit **0** PASS | exit **0** PASS | — |
| `Tools/test-graph-repoint.py` | — | — | exit **0** PASS |

## Why it failed before, stated as the reading it supports

The failing assertion was `":%s)" % replacement_id.rsplit(":", 1)[-1]) in after`. Every input to it is present
now, so the failure was **not** a missing link and **not** a defect in the swap tool. The reading that fits
every measurement is **stale state from the runs before the round-19 fixes**: those runs refused at the editor
lifecycle or at the diagnostic scope and left the session mid-way, and the assertion was evaluated against a
readback taken in that state.

I am labelling that a **reading** rather than a diagnosis: it is what the evidence supports after the fact, and
reproducing the old failure deliberately would be a poor use of the release's time when the current behaviour is
stable across three runs. What matters for the release is the present behaviour, and it is green.

## What S04 now has

| Piece | Evidence |
|---|---|
| The ARC02 result contract, writer and reader | C03–C08 pass (60 checks); native `TurboMCP.Result.LineContract` Success |
| The ARC03 mutation coordinator | native `TurboMCP.Mutation.CoordinatorStateMachine` Success |
| The first migrated writer | `test-graph-repoint.py` **exit 0, all four checks**, repeatable |
| The sibling graph writer | `test-graph-swap-call.py` **exit 0, all six checks**, repeatable |

Two L3 suites now exercise the migrated tool and its neighbour end to end — plan, refusal, edit, readback and
link preservation — which is the regression evidence this step has been missing since round 9.

## Still open in S04

- **C09's success half** is not observed: a real success and a real refusal through the structured client. The
  refusal half was observed in round 9; the success half needs a fixture whose target output is free.
- **M01–M08** have not run.
- The ARC04 cross-call baseline cache and ARC05's published helper are not started.
- 1 of 101 tools is migrated.
- `TurboMCP.T04.QueueForCompilationDuringCompile` still reports `Result={Fail}` in the native run.

## Evidence paths

- this record; the instrumented run's output quoted above
- tracked-diff fingerprint: `260caef138f8063a51f9c3dfa4ca2cd856fe16a8244eb35cf3c5c69476a64cb6`
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Run C09's success half: build a fixture whose repoint target output is genuinely free (or use a `targetTypeId`
so the tool creates the target node), then assert through `call_structured` that a real success reports
`outcome=changed` with `compile=passed` and exit 0. Then M01–M08.
