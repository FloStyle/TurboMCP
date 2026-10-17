# Owner decisions and unresolved questions

This file records decisions, their authority, and open questions. It does not record progress; the
ledger is [PROGRESS.json](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>) and the owner
status page is [STATUS.md](<C:/Users/Example/Desktop/TurboMCP/Release/STATUS.md>).

## D-001 — Approved architecture revision

- Decision: architecture revision 1 ([ARCHITECTURE.md](<C:/Users/Example/Desktop/TurboMCP/Release/ARCHITECTURE.md>), ARC01–ARC15) is binding.
- Authority: the owner's launch prompt for this run: "I approve the release boundaries and
  implementation decisions in `Release\ARCHITECTURE.md` revision 1."
- Effect: the executor implements these decisions and does not reopen them without a concrete
  engine/API contradiction recorded with evidence.
- Recorded: 2026-10-16, at S00.

## D-002 — Release boundary for this candidate

- Decision: prepare an experimental Windows source release for the pinned engine revision.
- Authority: ARC01 plus the launch prompt ("Prepare an experimental Windows source release on the
  pinned engine").
- Effect: Launcher, Linux, macOS and adjacent engine versions are not advertised. Stable 1.0
  guarantees stay unavailable until their gates pass.
- Recorded: 2026-10-16, at S00.

## D-003 — Batch scope

- Decision: implement only the existing `graphRepoint` operation kind, in one Blueprint asset and
  one graph per batch, under one transaction and one final compile, with no save.
- Authority: ARC09 plus the launch prompt ("Implement the specified single-graph batch subset").
- Effect: broad `T12` (atomic multi-asset batch edit) stays in backlog and is never marked done. A
  separate subset card owns the restricted implementation.
- Recorded: 2026-10-16, at S00.

## D-004 — Feature exclusions carried into backlog

- Decision: curve editing, general asset CRUD, widget animation authoring, input injection, general
  material graph authoring, and general multi-asset editing are `deferred_by_scope` for this
  release.
- Authority: ARC01 and ARC09, approved by the launch prompt.
- Effect: 52 supplied board cards are parked in backlog with that disposition and are counted
  separately from verified work. They are not marked implemented.
- Recorded: 2026-10-16, at S00. Card-level detail: `Release/Audit/BOARD_MAP.json`.

## D-005 — Mutation of an existing done declaration

- Decision: an existing `done` column is treated as a declaration, never as evidence.
- Authority: ARC15 and the execution protocol, section 5 ("An existing card may already be done with
  stale or failed evidence").
- Effect: 27 `done` cards are routed for re-verification. Seven of them carry
  `snapshotVerificationBypassed = true`, and at least two of those have a **failed** latest
  execution on the board. None of the 27 is counted as verified until a current-run acceptance
  passes.
- Recorded: 2026-10-16, at S00.

## D-006 — Publication

- Decision: publication, pushing, tagging and uploading are **not** authorized in this run.
- Authority: the launch prompt ("Do not publish remotely without my separate authorization") and
  ARC15.
- Effect: the run stops at `RELEASE_CANDIDATE_READY`/`NOT_READY` with a local candidate and exact
  publication commands prepared but not executed.
- Recorded: 2026-10-16, at S00.

## D-007 — Commits

- Decision: no commits are made in this run.
- Authority: the roadmap, section 1: "If commits are not authorized, maintain a patch fingerprint
  instead." No commit authorization was given.
- Effect: all work is carried in the working tree. Each work unit records the tracked-diff
  fingerprint and the SHA-256 of every changed file, so the exact reviewed state is identifiable.
  Pre-existing dirty changes are preserved.
- Recorded: 2026-10-16, at S00.

## Open questions

None at S00. Architecture exceptions will be raised only against a concrete engine/API
contradiction, with the observation that establishes it, and recorded here before the dependent
work proceeds.


## D-008 — An empty graph name is a USED convention; the last two resolvers are NOT unified

**Decision: `TurboMCPGraphPatch.cpp` and `TurboMCPGraphReplace.cpp` keep their own resolvers until the empty-name
semantics are decided for every caller at once. They are NOT unified onto `TurboMCPNodeId::FindGraphByName`.**

**Established by MEASUREMENT, not by reading.** A grep of the suites found real callers passing an EMPTY graph
name:

    test-comments.py:139   {"asset": ENGINE_ASSET, "graphName": ""}
    test-comments.py:160   {"asset": EMPTY_ASSET,  "graphName": ""}
    test-comments.py:189   {"asset": "TA11_TestEnum", "graphName": ""}
    test-graph-check.py:205 {"asset": ASSET, "graphName": "", "text": GOOD}
    test-graph-dead.py:309  {"asset": ASSET, "graphName": ""}

So an empty graph name is a **live, depended-upon convention** meaning "the asset's default graph" (or "scan the
asset"), and it is exercised by three suites.

**Why this blocks a mechanical unification.** The two local resolvers return the FIRST graph when the name is
empty (`if (Wanted.IsEmpty() || ...)`), while `TurboMCPNodeId::FindGraphByName` **refuses** an empty name
(`if (... || InGraphName.IsEmpty()) return nullptr;`). Unifying would therefore turn a working call into a
refusal for every one of those call sites.

**And it cannot simply be fixed inside the shared resolver**, because **the READER uses the same function**: making
it accept an empty name would change what `ReadTurboMCPGraphLoad` does for every reader that passes one.

**The decision this leaves open, and it is a contract question for the owner:**

* should an empty graph name mean **the default graph**, **all graphs**, or **refuse**? Today the answer differs
  by tool: Patch/Replace take the first graph, the reader refuses, and the tests expect the first-or-scan
  behaviour.

**Until that is answered, the correct action is to leave both resolvers alone.** Six of eight are unified on
repeat-verified evidence; these two are held back by a used convention, and that is a reason to stop, not a
reason to guess.

**Evidence:** `Release/EVIDENCE/S04/all-five-resolvers-unified.md`, and the grep above.
