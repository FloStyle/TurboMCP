# S04 evidence — C09's SUCCESS half is OBSERVED

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: C09's success half is now observed end to end through the structured client.** The case as written is
**not repeatable on the same fixture**, and that is recorded rather than papered over.

## The success, measured

Probe: `Tools/test-c09-success.py`, which passes an empty `targetNodeId` with a
`targetTypeId = "Development|PrintString"` so the tool **creates** the target — the documented path for a new
target, which makes its output free by construction. Every earlier attempt had been refused by the plan because
the chosen target output was already occupied, which was the plan working.

```
--- preview ---
outcome=planned code=ok changed=False exit=0
  ok   the preview of a creatable target is planned, not refused
  ok   the preview reports compile=not_run

--- edit ---
changed: repointed 1 reader(s) in AppendChainProbe; target
         /Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:AppendChainProbe:9d6ca9.then; old source retained
compile: passed (compared against the pre-edit diagnostics, scoped to graph 'AppendChainProbe'; no new
         diagnostic naming it, 6 other diagnostic(s) in this asset out of scope)
outcome=changed code=ok changed=True compile=passed rollback=not_needed save=not_requested exit=0

  ok   A REAL SUCCESS reports outcome=changed
  ok   a real success reports changed=true
  ok   a real success reports compile=passed
  ok   a real success reports rollback=not_needed
  ok   a real success reports save=not_requested
  ok   a real success exits 0
```

**Every part of the ARC02 contract is observed on a real success**, not just on a refusal: `outcome=changed`,
`changed=true`, `compile=passed`, `rollback=not_needed`, `save=not_requested`, **exit 0**. And the migrated
tool's own report names the reader it moved and the target it created, while the compile line states the scope
and the six out-of-scope diagnostics — so a caller can see both what changed and what was set aside.

**C09 is therefore PASSED: a real success and a real refusal through the structured client.** The refusal half
was observed in round 9.

## Two probe defects I made, and the standing state they expose

**1. The target extraction missed the pin suffix.** The report writes `...:9d6ca9.then` — node **and** pin — and
my regex expected the id alone. Fixed. This is the same class as the round-20 probe defect: my pattern was not
the subject's format.

**2. The case is not repeatable on the same fixture, and that is the important finding.** The second run
reported:

```
outcome=refused ... compile=not_run ... exit 1
```

and it was **right**: the first run's edit persisted, `AppendChainProbe`'s links had already moved, and there
was nothing left to repoint — so the plan refused with nothing to do. The success assertion is therefore
**order-dependent**: it is true of a fixture in its built state and false of the same fixture after the case has
run.

That is a defect in the CASE, not the product. Its consequence is real: a suite that runs twice without
rebuilding the fixture would report a spurious failure, exactly as round 22's swap check did. The fix is for the
case to **rebuild the fixture, or assert against a freshly created target every time** — and until it does, a
green run must not be followed by a second run treated as a regression.

## Verified / not verified

| Item | State |
|---|---|
| **C09 — a real success through the structured client** | **OBSERVED** (the run quoted above) |
| C09 — a real refusal through the structured client | observed (round 9) |
| The preview of a creatable target is `planned`, `compile=not_run` | **verified** |
| The compile line names the scope and the out-of-scope count | **verified on a success** |
| The case is repeatable without rebuilding the fixture | **NO — it is order-dependent, and the case needs fixing** |
| M01–M08 | not run |
| Tools migrated | 1 of 101 |

## Evidence paths

- this record; `Tools/test-c09-success.py`
- the two runs quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Make `Tools/test-c09-success.py` repeatable — rebuild the fixture (or create a fresh target each run) so the
assertion holds in any order — then run it twice in sequence to prove it. Then M01–M08, and the two unstarted
helpers.
