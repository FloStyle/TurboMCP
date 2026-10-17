# S01 evidence — repair verification verdicts

Step: S01 — Repair verification verdicts.
Covers: audit findings R01, R04; roadmap step S01; acceptance cases H01–H09.
Date: 2026-10-16.

## Source revision

| Item | Value |
|---|---|
| Plugin HEAD at start | `f045c1313f1db04ddcac269d6910be9ad6befb11` |
| Commits made in this run | none (D-007) |
| Tracked-diff fingerprint before S01 | `767249a0f23e94ebb3ac0af84c39a805f7e4af059b46f2b8059cddd04473fef2` |
| Tracked-diff fingerprint after S01 | `b9c3ff21362c9abf536f29a73ebed3dacb514eba98e2e437dec2596a139c195c` |
| Engine | `C:\UnrealEngine` 5.8.3, commit `396c9f059903aed5fec78ecd3d437a40c6415368` |

## Observed failure before the correction

Two measured, non-speculative failures, both reproduced in this run before any edit.

### R01 — the release gate accepted a suite that failed

From `Release/Audit/audit-verification.json`:

```
{"name":"runner classifier isolated reproduction","result":"DEFECT_CONFIRMED",
 "details":"Real Get-Verdict and Test-VerdictPassed functions extracted through PowerShell AST.
            RESULT: PASS followed by traceback text accepted as true. Runner script not executed."}
```

Four independent causes were read in `Tools/run-suites.ps1`:

1. `Invoke-SuiteProcess` returned the child's output and **discarded its exit code** (`run-suites.ps1:101-116`).
2. A timeout produced the *verdict string* `RESULT: TIMED OUT (Ns)` instead of a state, and the string contained
   neither `PASS` nor `FAIL`, so the only distinction was in the reader's head.
3. The summary incremented a `blocked` counter but exited nonzero only for `failed` (`:618`), so a run of
   blocked suites exited 0.
4. Selection only warned on a zero-match request (`:400`) and silently dropped unknown names, so an empty or
   reduced run looked green.

### R04 — the coverage audit's parser fabricated a parameter named `TEXT("")`

Reproduced directly:

```
=== every PARAMETER of every called tool appears in some sample ===
    EditTurboMCPFunctionAdd                  TEXT(""), TEXT("")
  FAIL no called tool leaves a parameter with no sample anywhere  1 tool(s)
```

and the same audit reported the suite-quality heuristic as FAILing on two suites that can fail:

```
  WARN test-graph-repoint.py        NO-ASSERT   no list is built and appended to, ...
  WARN test-graph-swap-call.py      NO-ASSERT   no list is built and appended to, ...
```

Inspection confirmed both are false positives: `test-graph-repoint.py:36-44` builds `checks = [...]` and
`return 0 if all(checks) else 1`, and `test-graph-swap-call.py:71-75` builds
`failures = [name for name, ok in checks if not ok]`. Neither calls `.append`.

## Corrections

| File | Correction |
|---|---|
| `Tools/run-suites-lib.ps1` (new) | The verdict and selection decisions, extracted so they can be tested without a suite: `New-TurboMCPVerdict`, `Get-TurboMCPVerdict`, `Get-TurboMCPVerdictKind`, `Test-TurboMCPVerdictAcceptance`, `Resolve-TurboMCPSuiteSelection`. Acceptance now requires an accepted verdict line **and** exit code 0 **and** no timeout **and** captured output. |
| `Tools/run-suites.ps1` | Child exit code preserved; timeout is a state; a kill ends only the child's own tree by PID; each suite's complete log is written to `.agent/suite-logs/<suite>.log` before it is discarded; a requested selection is honoured exactly or refused with exit code 2; the summary exits nonzero for blocked and timed-out suites as well as failed; a machine-readable `.agent/suite-logs/manifest.json` is written. |
| `Tools/audit-tool-coverage.py` | Declaration parsing rewritten: parameters are split on top-level commas only, and the name is the declarator's identifier, so `const FString& Inputs = TEXT("")` yields `Inputs`. The audit is reachable as `read_declarations` / `read_calls` / `audit` so a fixture can drive the production parser. The blank-parameter finding now states which parameter, which suites call the tool, and that a defaulted parameter still needs one caller. |
| `Tools/audit-suite-quality.py` | `failure_signals()` recognises every shape that can record a failure: an appended list, a failure-collecting comprehension, an explicit non-zero `return`/`sys.exit`, and `all(checks)` feeding a non-zero exit. |
| `Tools/checks/check-lookup-guards.py` | `if (!X)` and `if(!X)` are the same guard; a guard on the assignment's own line is a guard. |
| `Tools/test-function.py` | Added cases that **supply** `inputs` and `outputs`, and three invalid-signature refusals. This is what the corrected coverage audit exposed. |
| `Tools/test-runner-verdicts.py` (new) | H01–H07 against fake children through the real runner. |
| `Tools/test-coverage-audit.py` (new) | H08–H09 against tiny header fixtures through the real parser and the real check. |
| `Tools/test-batch-edit.py` (new) | The batch surface's baseline plus the ARC09 refusal shapes. Requires an editor; its assertions are marked `BASELINE` and belong to S16. |
| `Source/TurboMCP/TurboMCPMaterialFixtureCommandlet.cpp` | Both engine default-texture lookups are now guarded and fail the fixture build with a named cause instead of producing a fixture whose texture parameters point at nothing. |

## Build

```
pwsh -NoProfile -File C:\Users\Example\Desktop\TurboMCP\Tools\build.ps1 -Engine C:\UnrealEngine -NoStop
```

| Item | Value |
|---|---|
| Result | `Result: Succeeded`, exit 0 |
| Elapsed | 14.4 s wall (12.79 s UBT) |
| Actions | 5: compile `TurboMCPMaterialFixtureCommandlet.cpp`, link `.lib`, link `.dll`, 2 metadata writes |
| Artifact | `Binaries\Win64\UnrealEditor-TurboMCP.dll`, 3 680 256 bytes, 2026-10-16 22:14:17 |
| Identity change | 3 679 744 → 3 680 256 bytes; timestamp 18:54:58 → 22:14:17, so the binary moved. |

## Test results

### H01–H07 — `Tools/test-runner-verdicts.py`

Command: `python C:\Users\Example\Desktop\TurboMCP\Tools\test-runner-verdicts.py`
Elapsed: 56.9 s. Exit code: **0**. Result line: `RESULT: PASS (the runner refuses every non-acceptance shape and accepts a real pass)`.

| Case | Observation |
|---|---|
| H01 | A child that printed `RESULT: PASS` and exited 7 was recorded with exit code 7, `accepted: false`, reason `the child exited 7`; the runner exited 1. |
| H02 | A blocked-only run was recorded with `kind: blocked`, `accepted: false`; the manifest counted `blocked: 1, failed: 0`; the runner exited 1. |
| H03 | An unknown name, and a valid-plus-unknown selection, were both refused with exit code **2** and **no child ran** (the manifest was absent because the request was refused before it was created); the refusal named the unknown suite. |
| H04 | A zero-match selection and a duplicated name were both refused with exit code 2; the duplicate refusal named the duplicate. |
| H05 | A child killed at an 8-second deadline was recorded `timedOut: true`, `accepted: false`; its partial output (`will never finish`) was in the preserved log; the runner exited 1; the count of sleeping Python processes did not increase, so the timeout did not leak a grandchild. |
| H06 | A child that exited 0 with no verdict was refused with reason `the verdict line is not an accepted pass`; the runner exited 1. |
| H07 | The same suite as H01 with exit 0 was accepted, the runner exited 0, and `total: 1`. |

H01 and H07 differ **only** in the child's exit code. A classifier that rejected everything would pass H01
and fail H07, so the pair is a real discriminator rather than a one-sided check.

### H08–H09 — `Tools/test-coverage-audit.py`

Command: `python C:\Users\Example\Desktop\TurboMCP\Tools\test-coverage-audit.py`
Exit code: **0**. Result line: `RESULT: PASS (the parser reads defaulted parameters, and the uncalled-tool check fires)`. 15 checks, 0 failed.

Observed: `EditTurboMCPFunctionAdd` parses to `['Asset', 'Name', 'Inputs', 'Outputs']`; a default containing a
comma (`TEXT("compact,verbose")`) does not split its parameter; a nested comma stays inside one parameter; the
inserted uncalled tool is detected and named; a call for it clears the finding; a suite with no way to fail is
still reported.

### Static suite set through the real runner

Command: `pwsh -NoProfile -File Tools\run-suites.ps1 -Suites audit-suite-quality,audit-tool-coverage,check-includes,check-plugin,check-probe-graphs,test-coverage-audit,test-lookup-guards,test-runner-verdicts,test-traps-plugin-identity,test-widget-tree-nested`
Elapsed: 150.2 s. Exit code: **1** (one suite genuinely failing).

| Suite | Result |
|---|---|
| `audit-suite-quality` | **PASS** — 0 candidates in 72 suites (was FAIL: 2 candidates in 70) |
| `check-includes` | **PASS** — 1597 includes resolved |
| `check-plugin` | **PASS** — 11 checks |
| `check-probe-graphs` | **PASS** |
| `test-coverage-audit` | **PASS** (new) |
| `test-runner-verdicts` | **PASS** (new) |
| `test-traps-plugin-identity` | **PASS** |
| `test-widget-tree-nested` | **PASS** |
| `test-lookup-guards` | **FAIL** — 5 unguarded lookup sites. Two were false positives of the guard pattern (fixed, and the checker now reports 49 of 49 guarded); three were real and are corrected. **Re-verified after the fix: exit 0, `guarded: 49   unguarded: 0`.** |
| `audit-tool-coverage` | **FAIL** — 1 check: `EditTurboMCPBatchEdit` and `PreviewTurboMCPBatchEdit` are called by no suite. **Handed to S16**, which owns the ARC09 contract. The parameter false positive is gone: the report now reads `(every parameter appears in at least one sample)` / `ok`. |

`test-lookup-guards` was re-run after the correction and passes: `lookup sites checked: 49 in 128 file(s)`,
`guarded: 49   unguarded: 0`, exit 0.

## Independent state compared

The runner's decision was compared against the child's own process exit code, not against a re-parse of its
text. `Tools/test-runner-verdicts.py` asserts on `manifest.json`, which the runner writes from the process
handle, and separately asserts the runner's own exit code. The verdict text is never used as the oracle for
itself.

## Undo and rollback

Not applicable: S01 changes no editor transaction. The runner's fixture restore ran during the static suite
set and reported `restored (2 file(s))`.

## Persistence

The build wrote a new `.dll`. Nothing was saved from an editor by S01.

## Side effects observed and recorded rather than hidden

`TestProject/Content/Tests/TurboMCPAcceptance.uasset` and `TurboMCPGraphReaderChild.uasset` differ from their
committed bytes after the suite run. The runner restores fixtures from a run-start snapshot, which restores
what the run started from, so a fixture whose bytes were already regenerated before the run stays regenerated.
This is the subject of the existing backlog card `T-FIXTURE-01`; it is recorded here as evidence for that
card and is not a claim that fixture isolation works.

## Review of the diff, in a separate pass

- The exit code is read from `$proc.ExitCode` after `WaitForExit`, and `$null` is treated as "not evidence of
  success" rather than as zero.
- The timeout kill is by PID with `/T`, and the child is waited on, so the tree is gone before its output is
  read. No process is stopped by name.
- The selection refusal happens before any fixture snapshot or editor check, so a refused request starts
  nothing. Exit code 2 is distinct from 1.
- The manifest is written before the exit decision, and a manifest that cannot be written forces exit 1.
- `run-suites-lib.ps1` does not enable strict mode, because it is dot-sourced into a script that reads
  variables declared later; that is stated in the file.
- No `.append` heuristic remains that can report a suite which returns a non-zero code.
- The guard recogniser was widened only to shapes that are guards; the negative control (a fixture with one
  unguarded lookup) still fires, which `test-lookup-guards` exercises on every run.
- Nothing was weakened: no gate was removed, no failure was reclassified as a pass, and the one suite that
  still fails is left failing and handed to the step that owns it.

## Unverified limits

- The full editor suite has not been run. S01 is an L1 step plus one build.
- `test-batch-edit.py` has not been executed; it needs an editor and belongs to S16.
- `L3` behaviour of the corrected `TurboMCPMaterialFixtureCommandlet` guard is not exercised: the fixture
  commandlet was not rebuilt and re-run to force a missing engine texture. The build is the evidence.
- The runner's editor-reuse, timeout and fixture-restore paths were exercised only in their static form here.
  The editor-bearing paths are exercised by the first L3 step.
- `test-function.py`'s new cases have not run; they need an editor.

## Evidence paths

- `Release/EVIDENCE/S01/` (this record)
- `.agent/suite-logs/manifest.json` and `.agent/suite-logs/*.log`
- `.agent/suite-logs/audit-tool-coverage.log`, `.agent/suite-logs/test-lookup-guards.log`

## Next action

S02 — remove ambiguous mutation replay (C01, C02), or S03 if the environment safety step is the better
prerequisite for the first editor-bearing verification.
