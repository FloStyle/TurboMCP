# R11: Complete the pinned baseline acceptance matrix

ID: R11
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: verification
Review: source-reviewed

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
- `python Tools/execute-tests.py --profile build --execute`
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

## Current review

**Decision:** plan. **Follow-up owner:** [R11](roadmap-r11.md).

The 97 acceptance cases remain required and unmapped as a complete release set. One passing family suite is not a release acceptance result.

### Next action and acceptance

Map every case to an exact assertion/selector, fixture and independent oracle. Record pass/fail/blocked/missing plus candidate and DLL identity. Do not run an undifferentiated all-tools sweep on production assets.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tests/acceptance.json](../Tests/acceptance.json)
- [Tests/catalog.json](../Tests/catalog.json)
- [Tools/release-check.py](../Tools/release-check.py)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
