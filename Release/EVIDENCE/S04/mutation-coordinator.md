# S04 evidence — the shared mutation coordinator (ARC03), native half

Step: S04 — Implement the fixed shared contracts.
Covers so far: ARC02 (C03–C08) and **ARC03, compiled and state-machine tested**. Not done: the ARC04
diagnostic baselines, the ARC05 transaction/rollback boundaries as published helpers, and every migration.
Date: 2026-10-16.

**State: `implemented_unverified` for the step.** The coordinator compiles and its state machine passes a
native automation test. No writer is migrated to it, so M01–M08 have not been run and no L3 behaviour depends
on it yet.

## What was built

| File | What it is |
|---|---|
| `Source/TurboMCP/Public/TurboMCPMutation.h` (new) | `ETurboMCPMutationOutcome` (Changed/Unchanged/Refused/Failed/**OutcomeUnknown**), `ETurboMCPReadiness` (Ready, PieActive, Simulating, ShuttingDown, CompileInProgress, ForeignTransaction, Busy, TargetUnresolved, CompileBaselineRequired, UndoUnavailable), `FTurboMCPTouchedObject` (canonical identity, never a raw pointer), `FTurboMCPMutationTransaction`. |
| `Source/TurboMCP/TurboMCPMutation.cpp` (new) | The sequence: guard → resolve → refuse an unsafe world → capture pre-state → **one** owned transaction → `Commit()` or `Restore()` → `Release()`. A process-wide guard that REFUSES rather than waits (ARC03: "without blocking another request indefinitely"). |
| `Source/TurboMCP/TurboMCPMutationTest.cpp` (new) | Focused native automation: `TurboMCP.Mutation.CoordinatorStateMachine`. |

The architecture is explicit that S04 compiles the helpers and that each family migrates during its own step
("Compile helpers now. Migrate each writer during its following family step. Do not claim unmigrated writers
satisfy the coordinator contract."), so a shared, compiled, tested coordinator with no migrated writer is the
declared shape of this step rather than a partial delivery.

## Build and test results

| Command | Result |
|---|---|
| `pwsh -File Tools\build.ps1 -Engine C:\UnrealEngine -NoStop` | **exit 0**, 8–12 s, artifact `UnrealEditor-TurboMCP.dll` 3 703 296 bytes |
| `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests TurboMCP.Mutation;Quit" -unattended -nullrhi` | `Found 1 automation tests`, `Test Completed. Result={Success}`, and **no leaked-transaction warning** |

The native cases assert:

- the published vocabulary: every outcome and readiness value maps to the stable snake_case code the result
  line carries, so a writer cannot invent a state no client can classify;
- **finding G04**: a SECOND `Commit()` is refused with `transaction_not_open` rather than ignored, and a
  `Restore()` after a `Commit()` reports `OutcomeUnknown` — it never claims the asset is unchanged;
- **finding G05**: an UNVERIFIED restoration is `OutcomeUnknown` with `rollback_unverified`, while a VERIFIED
  one is `Failed`, so the two answers a caller must distinguish really differ;
- the guard is released on every exit path, including the refusal path, and `ForceReleaseGuard` names the depth
  it found instead of pretending nothing was held.

## Three defects found while building it, all by evidence rather than review

**1. `FScopedTransaction` has no `Commit()`.** MEASURED: engine 5.8.3's `ScopedTransaction.h` exposes only
`Cancel()`, `IsOutstanding()` and the destructor. The first version called `Transaction->Commit()` and did not
compile. The asymmetry is the reason the coordinator owns ONE heap object: destroying it without cancelling
keeps the changes; `Cancel()` is the restore path.

**2. `UWorld::IsSimulating()` does not exist on this engine revision.** The first `CheckReadiness` did not
compile. PIE and SIE are now read from `GEditor->PlayWorld` and `UWorld::WorldType`, which do exist, and the
diagnostic says which state is in the way because the two are recovered differently.

**3. A condition that could not be false, and it was in the guard.** The first `IsForeignTransactionOpen`
tested `GetQueueLength() > 0 && GetUndoCount() >= 0` — and `GetUndoCount() >= 0` is true for every unsigned
count, so it proved nothing while looking like a check. It now asks `GEditor->Trans->IsActive()`, which is the
engine's own answer. This is the fourth time in this run that a check unable to fail was found; the pattern is
consistent enough that it is worth stating plainly.

**And the coordinator's own diagnostic caught a defect in the test that verifies it.** Running the native test
produced:

```
LogTurboMCPMutation: Warning: a mutation transaction opened for 'TurboMCP coordinator test' was never
committed or restored.
```

A case called `Release()` after a successful `Begin()`. `Release()` frees the GUARD; it cannot free the
TRANSACTION, because only `Commit()` or `Restore()` may decide that. The warning was the destructor doing
exactly what it was written to do, on the suite that checks it. The correction is a third, honest option —
`Finish()`: cancel the record, release the guard, claim no verdict — and the warning is now absent.

## What this does NOT establish

- **No writer uses the coordinator.** M01–M08 have not been run, and no L3 behaviour depends on it.
- `CheckReadiness`'s PIE, simulation and shutdown branches are compiled and reasoned but not exercised; they
  need a play session, which is M01's job at L3.
- The ARC04 fingerprint-bound diagnostic baseline and the compile-baseline cache are not started.
- The measured defect that the MCP adapter refuses a call omitting a declared-default parameter is unchanged.

## Evidence paths

- `Release/EVIDENCE/S04/result-contract.md`, `Release/EVIDENCE/S04/mutation-coordinator.md` (this record)
- `Source/TurboMCP/Public/TurboMCPMutation.h`, `Source/TurboMCP/TurboMCPMutation.cpp`,
  `Source/TurboMCP/TurboMCPMutationTest.cpp`
- Automation log: `%TEMP%\tmcp-automation4\a.log`

## Next action

Add the ARC04 fingerprint-bound diagnostic baseline as a small shared helper, then migrate the first real
writer (the graph repoint core is the smallest that already has a transaction) and run M01–M08 plus the
`tmcp-result-v1` emission for that one writer.
