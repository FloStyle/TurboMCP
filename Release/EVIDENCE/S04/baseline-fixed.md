# S04 evidence — the baseline mechanism, CONFIRMED by measurement, and the fix

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The ARC04 baseline defect is DIAGNOSED, FIXED AND MEASURED. The remaining
blocker is now exactly one thing: the fixture does not compile.**

## The hypothesis, and the measurement that confirms it

Round 16 stated a hypothesis and refused to act on it: *`CompileBlueprint` recompiles what is dirty and reports
only that, so a baseline taken on a freshly loaded blueprint reads a no-op compile and returns nothing.*

The instrumented run is decisive:

```
LogTemp: Display: [TurboMCP] baseline diagnostics for TurboMCPAcceptance: 7 message(s), 7 error(s) -
                  read after forcing the compile; without the force this read returned none
```

**7 messages, 7 errors** — the same asset the baseline previously reported as having none, and the same count the
independent `EditTurboMCPCompile` tool reports. The hypothesis is confirmed, and the earlier round's reason for
not acting on it is discharged.

## The fix, in two parts

**1. The baseline is forced.** `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B)` runs before the
baseline compile, which is what makes the compile do the work the diagnostics describe. It is safe at that point:
the transaction has not opened and the call sets a dirty flag rather than altering a graph.

**2. The message no longer blames the caller.** It was:

```
error: the edit introduced 6 new compile diagnostic(s): error: Can't connect pins ...
```

It is now:

```
the edit would add N compile diagnostic(s) to an asset that already had M.
Pre-existing: ...  New from this edit: ...
```

MEASURED before the fix: six errors the asset already had were reported as "introduced" by the edit, and a caller
acting on that wording would have searched their own change for a defect that was not there. Both counts are now
named, so "the asset was already failing" is distinguishable from "this edit broke it" — and it is the first
thing to fix.

**3. The count is logged rather than discarded.** The two numbers are the evidence for the mechanism; a silent
skip was invisible precisely because nothing printed it.

## What the suite reports now, and it is the honest reading

`test-graph-repoint.py`: three of four checks pass, `checks[2]` still fails — and now for the **right, measured
reason**: the asset has 7 pre-existing compile errors, so the edit correctly refuses and rolls back. The failure
is no longer a misattributed message; it is a real fixture defect.

## The one remaining blocker, unchanged and now alone

**`TurboMCPAcceptance` does not compile** — 7 errors in its saved state, from a node whose pins `A` and
`Return Value` are connected to each other, creating a loop. Until it is repaired or regenerated:

- the repoint suite's success path is unreachable;
- the graph repoint migration's regression status stays **UNKNOWN**;
- and `checks[2]` will keep failing for a reason that is not the migration.

## Verified / not verified

| Item | State |
|---|---|
| The baseline was a skipped no-op compile | **CONFIRMED — 7 messages, 7 errors when forced** |
| The baseline is now forced and reads 7 | **verified by the same measurement** |
| The refusal message names pre-existing and new counts separately | implemented |
| The six previously-misattributed errors | now correctly attributed to the asset |
| The fixture compiles | **NO — 7 pre-existing errors** |
| The graph repoint migration preserved behaviour | **UNKNOWN** |
| `test-graph-swap-call.py` | not run |

## Evidence paths

- this record; the log line above from
  `TestProject\Saved\Logs\TurboMCPTest.log`
- `Source/TurboMCP/TurboMCPGraphRepoint.cpp` — the forced baseline, the corrected message, and the log line
- build: exit 0, artifact 3 738 624 bytes

## Next action, in one step

Repair or regenerate `TurboMCPAcceptance` so it compiles cleanly, and make the repoint case assert the pre-edit
compile is clean — so a broken fixture fails loudly instead of disguising itself as a product defect. Then run
`test-graph-repoint.py` and `test-graph-swap-call.py` to a pass; that is the graph repoint migration's missing
regression evidence.
