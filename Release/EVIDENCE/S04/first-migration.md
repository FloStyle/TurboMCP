# S04 evidence — the first migrated writer (graph repoint)

Step: S04 — Implement the fixed shared contracts.
Covers: the first real migration, which is what the M cases and C09 need.
Date: 2026-10-16.

**State: `implemented_unverified` for the step.** The writer is migrated and its result line reaches the wire;
the refusal paths pass end to end through the structured client. The SUCCESS path and C09's "one real success"
half are NOT yet observed, and a new wrapper-shape defect is open.

## What changed

| File | Change |
|---|---|
| `TurboMCPGraphRepoint.h/.cpp` | `ApplyTransacted` replaces the two-entry-point pair. MEASURED: the old `Apply()` opened a transaction and then called `ApplyCore()`, which **resolved the asset again and called `Modify()`** — one public edit with two transaction owners, and a failure path that ended a transaction its caller owned (finding G04). Now: resolve once, `Modify`, apply, **compile once**, compare the readback, compare the diagnostics, commit-or-cancel — in one place, ended exactly once. |
| `TurboMCPGraphRepoint.cpp` | `ReadCompileDiagnostics` and `NormalizeDiagnostics`: ARC04's rule in its smallest honest form. The **identity** is severity + message text, sorted and de-duplicated, and the before/after sets are compared as SETS — because "a changed diagnostic with the same count is still new". The limit is stated in the code: the engine's results log exposes the message and severity, not the producing node, so a message whose text is unchanged but whose target moved is not detected. |
| `TurboMCPGraphRepointTools.cpp` | **Both** tools now append the line through `FTurboMCPResult::AppendTo`, on every exit including refusals and errors. The preview emits `planned` with `compile=not_run` (a preview does not certify compilation); the edit emits `changed` only after the readback confirmed it; a failure whose restoration WAS verified sets `changed=false`, and one whose restoration was NOT verified sets **`changed=null`** with `outcome_unknown` — ARC05's rule, and the case a caller must never read as "nothing happened". |
| `TurboMCPResult.h/.cpp` | the C++ writer (recorded in `result-writer.md`) |

## Verified

| Check | Evidence |
|---|---|
| Build | **exit 0**, artifact 3 737 600 bytes |
| Native tests | `Found 4 automation tests`: `TurboMCP.Mutation.CoordinatorStateMachine` **Success**, `TurboMCP.Result.LineContract` **Success**, `TurboMCP.NodeId.RoundTripAndKeyValidation` **Success** |
| **The wire carries the line** | A real call's raw payload: `"HAS RESULT LINE: True"`, ending `..., \"operationId\": \"895f5ef9-...\"}"}` |
| A real refusal, read by the structured client | `outcome=refused`, `code=plan_refused`, `changed=false`, **exit 1**, and the refusal's **diagnostics name the offending pin and node** (`"target has no output pin named 'NoSuchPin'; outputs: then"`), so an agent does not parse the body to find it |
| An unresolvable asset | `outcome=refused`, `code=plan_unavailable`, `changed=false` |
| The compatibility body | does **not** contain the metadata line, and is not empty |

## A pre-existing native failure, and one new defect

**`TurboMCP.T04.QueueForCompilationDuringCompile` reports `Result={Fail}`.** It is not touched by this round's
changes and it may well be pre-existing — but it is a FAILING test in the release's own tree, so it is recorded
rather than ignored. It needs a focused look before any claim about the compile path.

**A NEW WRAPPER-SHAPE DEFECT, measured and NOT fixed.** The raw payload of a migrated answer arrives as
`{"returnValue": "<json-encoded string>"}` and the tail shows a further layer of escaping — a **double**
wrapper. `StructuredAnswer.unwrapped_text()` peels up to three layers and the structured read of the REFUSAL
cases works, but a read through the compatibility path returned only the error sentence and no result, which
means at least one shape is still not decoded on every path. This is the same class as the two wrapper defects
already fixed this run, in a new form. **The next round must pin the exact shape the engine emits for an
FString tool whose answer contains escaped JSON and extend `unwrapped_text` to it, with a case for each shape.**

That defect is why C09's "one real success" half is not claimed: the refusal half is observed end to end, and
the success half was refused by the PLAN for a legitimate reason (the chosen target output was already occupied
— `"target 'then' cannot connect to reader ...: Replace existing output connections"`), which is the plan doing
its job rather than a defect.

## What this does NOT establish

- **C09 is not passed.** The refusal halves are observed; the success half needs a fixture whose target output
  is genuinely unoccupied, and the wrapper defect above has to be settled first.
- **M01–M08 are not run.** The migrated writer uses ARC03's sequence but nothing yet exercises it at L3.
- The ARC04 cross-call baseline cache is not implemented: the baseline is established inside one edit.
- No other tool is migrated, so 99 of 101 tools still answer without a result line.

## Evidence paths

- `Release/EVIDENCE/S04/result-contract.md`, `mutation-coordinator.md`, `result-writer.md`, this record
- `Source/TurboMCP/TurboMCPGraphRepoint.cpp`, `TurboMCPGraphRepointTools.cpp`,
  `Public/TurboMCPGraphRepoint.h`
- Automation log: `%TEMP%\tmcp-auto-all.log`

## Next action

Pin the exact wrapper shape the engine emits for a migrated FString answer and extend `unwrapped_text` to it
with a case per shape; then run C09's success half against a fixture whose target output is free, and M01–M08.
