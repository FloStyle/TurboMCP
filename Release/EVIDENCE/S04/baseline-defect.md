# S04 evidence — the repoint failure is decided: a broken fixture, and a baseline read that saw none of it

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The cause of `test-graph-repoint.py`'s failing check is now DECIDED by
measurement, and it exposes a real defect in my own ARC04 baseline read.**

## The measurement that decided it

The migrated Edit refuses with a rollback and names diagnostics:

```
error: the edit introduced 6 new compile diagnostic(s): error: Can't connect pins  A  and  Return Value :
Both are on the same node | error: Dependency cycle detected, preventing node  int + int  from being scheduled | ...
```

The discriminator between "a migration defect" and "a broken fixture" is a compile with **no edit at all**,
through the plugin's own Compile tool, on the same asset:

```
changed: compiled 1 of 1 asset(s)
0 clean, 0 with warnings, 1 with errors
/Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance  [error (7 errors, 0 warnings)]  not saved yet
    error: Pins  A  and  Return Value  on the same node  int + int  are connected to each other, creating a loop.
    error: Can't connect pins  A  and  Return Value : Both are on the same node
    error: Dependency cycle detected, preventing node  int + int  from being scheduled
    ...
```

**All six of the "new" diagnostics are PRE-EXISTING.** `TurboMCPAcceptance` does not compile — 7 errors in its
saved state, from a node connected to itself — so the edit did not introduce them.

So the failing check is **not a migration defect**, and the migration's new-compile-diagnostic rule behaved
exactly as ARC04 requires: it detected a set difference and rolled back rather than reporting a change on an
asset that does not compile.

## But it exposes a real defect in my own code

**My baseline read returned NONE of those diagnostics.** The edit reported six as "new", and they were present
before the edit — which means the `Before` list my `ApplyTransacted` builds is empty or wrong while the
`After` list is right. The two are produced by different code:

| | How |
|---|---|
| the Compile tool's list | `EditTurboMCPCompile`, 7 errors, names and details |
| my `Before` | `FKismetEditorUtilities::CompileBlueprint(B, ..., &Log)` then `NormalizeDiagnostics(Log)` |
| my `After` | the same call, 6 found |

The likeliest explanation is that **the baseline compile does not populate the results log the same way the
second one does** — the first compile of an asset in a session runs the full compiler and may report through a
different path, or `Log.Messages` is reset by the engine between the two. I have not established which, and I
am **not** going to assert one: this is the same trap the round-14 record is about, and it has been the most
repeated mistake of this run.

**What matters is that it is a real defect with a real consequence**: ARC04's rule only works if the baseline
captures the diagnostics. Here it did not, so on a fixture with pre-existing errors every edit is refused for a
reason the tool misreports as "the edit introduced" them. That message is **wrong**, and a caller acting on it
would look for a defect in their change.

## Verified / not verified

| Item | State |
|---|---|
| The reclaim rule, the reuse rule | **verified** (rounds 12, 13) |
| The migrated tool's refusal path through the repoint suite | verified (checks[1] passes) |
| The migrated tool's new-diagnostic rule fires and rolls back | **verified by this measurement** |
| Whether the fixture compiles | **NO — 7 pre-existing errors, measured** |
| The ARC04 baseline read captures pre-existing diagnostics | **NO — it returned none of six** |
| The graph repoint migration preserved behaviour | **still UNKNOWN** — the fixture cannot exercise it |
| Round 13's "selects nodes by position" | **disproved** (round 14) |

## Two defects, both now named and neither fixed

1. **The fixture `TurboMCPAcceptance` does not compile** — 7 errors from a self-connected node in its saved
   state. Any acceptance case run against it is measuring an asset that is already invalid, and the repoint
   suite cannot exercise its success path at all.
2. **The ARC04 baseline read does not see pre-existing diagnostics**, so the tool reports them as introduced.
   The rule is right; the measurement it depends on is not.

## Evidence paths

- this record; `%TEMP%\decide.py` output and `decide-baseline.py` output are quoted above
- editor at the end of the round: 0 running, no lease

## Next action, in one step

Find out why `FKismetEditorUtilities::CompileBlueprint(B, ..., &Log)` followed by `NormalizeDiagnostics(Log)`
returns an empty list where the Compile tool reports 7 errors — read `Log.Messages` immediately after the call
and print its size, rather than assuming. Fix the baseline, and make the refusal say "the asset already had N
errors and the edit added M" instead of "the edit introduced" them. Then repair or regenerate the fixture, and
only then is the graph repoint migration testable.
