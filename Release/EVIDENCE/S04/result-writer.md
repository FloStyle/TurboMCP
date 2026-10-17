# S04 evidence — the C++ side of the result contract (ARC02)

Step: S04 — Implement the fixed shared contracts.
Covers: ARC02's **writer**, complementing the reader already recorded in
[result-contract.md](<C:/Users/Example/Desktop/TurboMCP/Release/EVIDENCE/S04/result-contract.md>) and the ARC03
coordinator in [mutation-coordinator.md](<C:/Users/Example/Desktop/TurboMCP/Release/EVIDENCE/S04/mutation-coordinator.md>).
Date: 2026-10-16.

**State: `implemented_unverified` for the step.** The writer compiles and its native cases pass. No tool is
migrated, so the line is never emitted by a public answer yet and C09 has not been run.

## What was built

| File | What it is |
|---|---|
| `Source/TurboMCP/Public/TurboMCPResult.h` (new) | `FTurboMCPResult` with the closed enumerations (`ETurboMCPResultOutcome`, `Compile`, `Rollback`, `Save`), the tri-state `Changed` (0 false / 1 true / **2 null**), `Diagnostics`, `Effects`, the truncation fields, `RenderLine()`, `AppendTo()`, `SuggestedExitCode()`, and the token functions. |
| `Source/TurboMCP/TurboMCPResult.cpp` (new) | The writer, with explicit JSON escaping. |
| `Source/TurboMCP/TurboMCPResultTest.cpp` (new) | Native cases: `TurboMCP.Result.LineContract`. |

The header states the reason it is separate from the coordinator: **the coordinator decides what happened, this
decides how to say it.** A writer that formats its own line can spell an outcome no client can classify, and a
writer that decides its own outcome can report `unchanged` about an edit that may have applied — which is
finding G05.

## Test results

| Command | Result |
|---|---|
| `pwsh -File Tools\build.ps1 -Engine C:\UnrealEngine -NoStop` | **exit 0**, 8.7 s, artifact 3 725 312 bytes |
| `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests TurboMCP.Result;Quit" -unattended -nullrhi` | `Found 1 automation tests` → `TurboMCP.Result.LineContract` → **`Test Completed. Result={Success}`** |

**A false alarm worth recording, because it is exactly the trap this repository keeps documenting.** The log
contained twenty `LogAutomationTest: Error: Condition failed` lines, and they looked like twenty failed
assertions in the new test. Counting them by position settled it: **20 before `Test Started`, 0 after.** They
are engine startup warnings from unrelated systems. The first reading — "my test failed twenty times" — was
wrong, and the check that distinguished the two was a comparison of line numbers rather than an impression.

The native cases assert:

- every token maps to the exact snake_case string the Python reader accepts;
- a complete success renders `schema`, `outcome`, `code`, `changed`, `assets`, `compile`, `rollback`, `save`,
  `complete` and `operationId`, and **no optional field appears when unset**, so "absent" is distinguishable
  from "empty";
- assets are **sorted and de-duplicated** in the line, so two runs over the same set produce the same text;
- **`changed` renders as the JSON literal `null`** when the live state is not known, and a failed rollback maps
  to exit **2**, not 1 — the pair ARC05 requires and G05 records as previously wrong;
- escaping: a diagnostic containing `"`, `\` and a newline stays on ONE line, because a raw newline would split
  the authoritative line and the client would find no result at all;
- `AppendTo` puts the line **last, exactly once**, with or without a trailing newline in the body — a line that
  is not last is not authoritative (C05);
- two operation ids differ, and none is claimed as a deduplication key.

## Build errors fixed on the way

Both were real API mismatches, not typos: `TestTrue` takes **two** arguments on this engine revision (a detail
belongs in the message), so three call sites did not compile.

## What this does NOT establish

- **No public answer carries the line yet.** The writer is exercised by a native case, not by a tool, so C09
  (L3) has not run and a client reading a real tool has never seen the format end to end.
- The ARC04 fingerprint-bound diagnostic baseline and the ARC05 published helper are still not started.
- Nothing is migrated to the ARC03 coordinator, so M01–M08 have not run.

## Evidence paths

- `Release/EVIDENCE/S04/result-contract.md` (the Python reader), `mutation-coordinator.md` (ARC03),
  this record (the C++ writer)
- `Source/TurboMCP/Public/TurboMCPResult.h`, `Source/TurboMCP/TurboMCPResult.cpp`,
  `Source/TurboMCP/TurboMCPResultTest.cpp`
- Automation log: `%TEMP%\tmcp-automation5\a.log`

## Next action

Migrate ONE writer — the graph repoint edit is the smallest that already owns a transaction — so it emits the
line through `AppendTo` and reports its outcome through the coordinator. That is what makes C09 runnable and
what the M cases need before S04 can close.
