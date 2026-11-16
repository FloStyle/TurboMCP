# R14: Implement only an approved compatibility adapter

ID: R14
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/protocol.md
Kind: implementation
Review: source-reviewed

## Problem
Replace the removed GetToolsetClass dependency through a supported identity mechanism. Locate SemanticSearchToolset replacement/move before changing the test descriptor. Verify native images/structured content and JSON null. Preserve the current pinned 5.8.4 behavior.

## Scope
CallRecorder registry identity, dependency discovery, client media/null handling and focused compatibility tests. Separate engine checkout and artifacts.

## Dependencies
R13, R11

## Acceptance
- Baseline and 5.9 build/discovery pass independently.
- Recorder registration/unregistration and media/null tests pass on each target.
- Do not widen supported versions until full matrix acceptance exists.

## Verification
- `Build and test pinned 5.8.4`
- `Build and test the selected pinned 5.9 target`
- `Run recorder lifecycle, discovery, media and null-reference fixtures`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [ue59-protocol](ue59-protocol.md)
- [ue59-recorder](ue59-recorder.md)

## Current review

**Decision:** requires spec. **Follow-up owner:** [R14](roadmap-r14.md).

This implementation task was marked ready before R13 identified the supported replacement API or exact adapter design. That is premature even though dependencies prevent immediate dispatch.

### Next action and acceptance

Keep needs_reconciliation until R13 supplies file/API/version boundaries and both-engine test commands. Then approve one adapter at a time. Missing forward runtime is a blocker for certification, not permission to invent API shims.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp)
- [Tools/tmcp.py](../Tools/tmcp.py#L139)
- [engine.lock.json](../engine.lock.json)

Starting checks for the assigned follow-up:

- `python Tools/test-result-contract.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

## Preparation update — 2026-11-14

Follow the ordered [5.9 preparation queue](../docs/engine-compatibility.md#preparation-queue--2026-11-14). Refresh R13's historical 5.8.3 comparison against the current 5.8.4 pin before selecting replacement APIs. The recorded 5.9 development snapshot remains unsupported and has not been refreshed in this update. No implementation assignment or runtime acceptance is implied; readiness remains needs_reconciliation. Keep current fixes and build outputs separate from the compatibility target.
