# S04 evidence — the shared result contract (partial)

Step: S04 — Implement the fixed shared contracts.
Covers so far: ARC02 and acceptance cases **C03–C08**. Not yet done: the ARC03 coordinator, the ARC04
fingerprint-bound diagnostic baselines, the ARC05 transaction/rollback boundaries, and the per-family
migrations (DOC1–DOC5, R05–R07, G05, G07, G11, G17).
Date: 2026-10-16.

**State: `implemented_unverified` for the step. The result-contract half is implemented and C03–C08 pass;
nothing is migrated to it yet, so no tool emits a result line and the contract cannot yet be observed through
the wire.**

## What was built

| File | What it is |
|---|---|
| `Tools/tmcp_result.py` (new) | The ARC02 contract as a pure module: `format_result`, `parse_result_line`, `parse_answer`, `split_answer`, `body_of`, `TurboMCPResult.suggest_exit_code`, `canonical_assets`, `fingerprint`, `apply_output_budget`, `mark_truncated`, `new_operation_id`. |
| `Tools/tmcp.py` | `StructuredAnswer` (all text blocks in order, the body, the parsed result), `fetch_blocks` (one place extracts blocks for both methods), `call_structured`, and a CLI whose exit status comes from the metadata. |
| `Tools/test-result-contract.py` (new) | C03–C08, 60 checks, no editor needed. |

## Test results

Command: `python C:\Users\Example\Desktop\TurboMCP\Tools\test-result-contract.py`
Exit code: **0**. Result line: `RESULT: PASS (the final line is authoritative, malformed metadata is a protocol
failure, and the metadata survives an output budget)`.

| Case | Observation |
|---|---|
| C03 | Refusal → 1, partial save → 1, **failed rollback → 2**, completed success → 0, incomplete answer → 2, unknown outcome → 2. **Both negative controls pass:** a body full of alarming prose with a `changed` line exits **0**, and a reassuring body with a `refused` line exits **1**. |
| C04 | Nine malformed shapes refused (no line, prefix with no JSON, JSON that is not an object, unparseable JSON, missing key, wrong schema, unknown outcome, out-of-range enumeration, empty `operationId`). A **missing** line parses to `None` — the caller decides — while a **present but invalid** line raises. Conflating those two is how a migrated tool's silence would read as success. |
| C05 | The final line decides, not a forged one earlier in the body. A line that is **not last** is not authoritative. The compatibility body keeps the forged line visible and drops the authoritative one. |
| C06 | Every content block collected in order; one decoded body; the metadata parsed once; the compatibility method returns the body without the line. |
| C07 | A native engine tool has **no** TurboMCP result and is not falsely certified; a migrated tool with no line is a **protocol failure**. |
| C08 | Truncation reported, metadata intact and still valid, the omission explicit and counted, the notice a comment so no partial syntax is presented as whole, a budget smaller than the metadata keeps the metadata and empties the body, and a body that already fits is not truncated. |

## Two real client defects found by the C cases

**1. `body` was computed from the raw blocks instead of the decoded text.** With an FString-wrapped answer, the
wrapper and the metadata line are the same single JSON line, so there was no line for `body_of` to strip and the
whole `{"returnValue": ...}` wrapper came back as the "body" — while the structured method decoded it correctly.
Every wrapped migrated tool would have shown raw JSON to the compatibility method. Fixed: `body` is computed
from `unwrapped_text()`.

**2. The wrapper arrives in two shapes and only one was unwrapped.** The engine supplies an FString answer either
as an object (`{"returnValue": "..."}`) or as a **JSON-encoded string of that object**
(`"{\"returnValue\": \"...\"}"`). Only the object form was handled, so the second reached the parser as a quoted
blob and every migrated FString tool would have looked like a protocol failure. Fixed: `unwrapped_text()` peels
both, with a loop guard.

A third defect was in `fetch_blocks` during writing: `str(block.get("text", ""))` was written as
`str(block)` by mistake, turning every block into the Python repr of the whole content object, so no wrapper
could ever be unwrapped. Caught by C06 before it reached a build.

## Three test defects found, each of which read as a product defect

This is the third round in which the harness, not the product, was the thing that was wrong, so it is recorded
rather than left in the diff:

1. `C06 the FString wrapper is decoded for the text method` asserted `value.startswith("changed: wrapped")` while
   the client was right: the compatibility method returns the **body**, and with a wrapper the answer is one
   JSON line, so the body IS that line. The assertion tested the wrong property of the right behaviour.
2. `C08 the answer fits the budget OR the metadata alone forced it` **could not fail** — it was true whenever
   the budget merely forced the body away, which hid a real off-by-one (903 characters for a 900 budget,
   because the newline separating the body from the result line was not counted, and the truncation notice was
   budgeted for a one-digit count). Both the contract and the assertion are exact now. A check that cannot fail
   is the defect S01 existed to remove, and it reappeared here in a suite written to prove the result contract.
3. The `--no-error-check` mapping and the CLI's exit precedence were asserted by inspection only; the CLI path
   itself has no test yet.

## What this does NOT establish

- **No tool emits a result line yet.** The contract is implemented and tested in isolation; `Tools/tmcp.py`
  reads it, but until a writer is migrated there is nothing on the wire to read, so **no acceptance case that
  requires a real tool's metadata has passed** (C09 is L3 and has not been run).
- The ARC03 mutation coordinator, the ARC04 diagnostic baselines and the ARC05 transaction/rollback boundaries
  are not started.
- The measured finding that the engine's MCP adapter **refuses a call that omits a declared-default
  parameter** (`inputs`/`outputs` on at least five tools) is unchanged; it is a contract question for the
  migration, not something the result line fixes.
- `L3` C09 has not been run; it needs the editor and a migrated tool.

## Evidence paths

- `Release/EVIDENCE/S04/result-contract.md` (this record)
- `Tools/tmcp_result.py`, `Tools/tmcp.py`, `Tools/test-result-contract.py`

## Next action

Continue S04: add the ARC03 mutation coordinator and the ARC04 diagnostic baseline as small shared helpers, then
migrate the first real writer and run its M cases (M01–M08) before that leaf closes. The result formatter's
parser is the oracle for the migrations, so the writer under test must not produce its own expected line.
