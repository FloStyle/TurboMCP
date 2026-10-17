# R14: Implement bounded 5.9 compatibility adapters

ID: R14
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: implementation

## Problem
Replace the removed GetToolsetClass dependency through a supported identity mechanism. Locate SemanticSearchToolset replacement/move before changing the test descriptor. Verify native images/structured content and JSON null. Preserve 5.8.3 behavior.

## Scope
CallRecorder registry identity, dependency discovery, client media/null handling and focused compatibility tests. Separate engine checkout and artifacts.

## Dependencies
R13, R11

## Acceptance
- Baseline and 5.9 build/discovery pass independently.
- Recorder registration/unregistration and media/null tests pass on each target.
- Do not widen supported versions until full matrix acceptance exists.

## Verification
- `Build and test pinned 5.8.3`
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
