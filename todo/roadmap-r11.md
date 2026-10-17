# R11: Close the 5.8.3 release acceptance matrix

ID: R11
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: verification

## Problem
Map all 97 required cases to executable checks; remove obsolete board wording without weakening outcomes. Run a clean candidate and classify every case passed/failed/blocked/missing. Missing or blocked evidence keeps the release closed.

## Scope
Tests/acceptance.json, exact test selectors, candidate evidence and release/status.json. No feature expansion.

## Dependencies
R10

## Acceptance
- Every required case has command, selector, fixture and evidence identity.
- The candidate passes offline, build, native smoke and required integration checks.
- A fresh complete-source copy validates and the tested plugin loads in a clean consumer project.

## Verification
- `python Tools/validate.py`
- `pwsh -File Tools/build.ps1 -Engine <configured-engine>`
- `python Tools/release-check.py --evidence <candidate-evidence>`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [c2865829-9206-4df7-8cf1-89d8d1c319bc](c2865829.md)
- [public-cutover](public-cutover.md)
