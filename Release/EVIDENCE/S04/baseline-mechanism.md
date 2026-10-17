# S04 evidence — why the ARC04 baseline is empty: the mechanism, to be verified

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The cause is NARROWED to one mechanism by reading the working code, and it is
NOT yet verified. No code was changed this round.**

## The comparison that narrowed it

Two functions read the same asset's compile diagnostics, and they behave differently:

| | How it reads | Result |
|---|---|---|
| `EditTurboMCPCompile` (the working tool) | `FCompilerResultsLog CompileLog;` then `CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);` then `CompileLog.NumErrors` and `CompileLog.Messages` ([TurboMCPCompile.cpp:223-227](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L223-L227>)) | **7 errors** |
| my baseline in `ApplyTransacted` | the same call, the same options, the same log type | **0 messages** |
| my after-edit read | the same call again, the same way | **6 messages** |

The calls are textually identical, so the difference is not the call — it is **when** it happens.

## The mechanism, stated as the leading hypothesis

**`FKismetEditorUtilities::CompileBlueprint` reports through the log only what it actually recompiles.** On a
blueprint whose saved generated class already exists and whose graphs have not been dirtied, the compile is a
no-op: nothing is scheduled, nothing is reported, and `Log.Messages` is empty. That is the state my **baseline**
runs in — it is the first thing to touch the asset after the editor loaded it.

My **after-edit** read runs *after* the graph was structurally modified, so the compile is real, the functions
are recompiled, and the pre-existing errors surface — six of them, deduplicated from seven.

That is consistent with every measurement: the baseline empty, the after-read populated, and the independent
`EditTurboMCPCompile` tool reporting **seven** because a deliberate Compile forces the work my baseline skipped.

**I have not verified this, and the card says so.** Round 14's record is about exactly this mistake and it has
been the most repeated error of this run, so the mechanism is written down as a hypothesis with the one
instrumented run that would confirm it: print `Log.Messages.Num()` and `Log.NumErrors` immediately after the
baseline compile, on a freshly loaded asset, and again after a `MarkBlueprintAsStructurallyModified`.

## The engineering difference that makes the fix cheap

The working tool does something my baseline does not, and it is visible in the code above: it reads the log
**once, deliberately**, as the *purpose* of the operation. My baseline reads it as a *side effect* of a compile
it performs for its own convenience — so it inherits whatever the compiler decided to skip.

Two candidate fixes, and the choice needs the measurement:

1. **Force the baseline compile to do the work** — dirty the blueprint before the baseline read, so the compile
   is real. Cheap, but it mutates the asset before the transaction opens, which is its own hazard.
2. **Read the baseline from the blueprint's persisted status** rather than from a fresh compile — no compile, no
   skip, and it is the same source the Compile tool ultimately reports. Preferable if the status carries the
   messages; `FBlueprintEditorUtils::GetBlueprintStatus` is the candidate and its contents are not yet checked.

Fixing the message is independent and should happen either way: the refusal must say **"the asset already had N
errors and the edit added M"**, not "the edit introduced" them. That wording is wrong today on any asset with
pre-existing errors, and a caller acting on it would look for a defect in their own change.

## Verified / not verified

| Item | State |
|---|---|
| The six reported diagnostics are PRE-EXISTING (7 errors, measured with no edit) | **verified** (round 15) |
| The new-diagnostic rule fires and rolls back correctly | **verified** |
| The message misattributes them to the edit | **verified** |
| The baseline read returns an empty list where the working tool returns 7 | **verified** |
| The mechanism (a skipped no-op compile) | **hypothesis — NOT verified** |
| Which fix is correct | **not decided** |
| The fixture compiles | **NO — 7 pre-existing errors** |
| The graph repoint migration preserved behaviour | **UNKNOWN** |

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPCompile.cpp:223-227` for the working read, and
  `Source/TurboMCP/TurboMCPGraphRepoint.cpp` for the baseline and the after-edit read
- round 15's measurements: the no-edit compile reporting "0 clean, 0 with warnings, 1 with errors" and the six
  diagnostics the Edit named

## Next action, in one step

One instrumented run in the plugin: print `Log.Messages.Num()` and `Log.NumErrors` immediately after the
BASELINE compile on a freshly loaded asset, and again after `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified`
followed by the same compile. That decides the mechanism. Then fix the baseline by whichever of the two routes
the measurement supports, fix the message to name the pre-existing count, and — separately and in the same pass —
repair or regenerate the fixture so the repoint suite has a compilable asset and can finally exercise its
success path.
