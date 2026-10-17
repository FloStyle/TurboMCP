# Gate recorder adaptation on exact registry API evidence

ID: ue59-recorder
Status: backlog
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/protocol.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md), [R14](roadmap-r14.md).

Recorder identity and registry wrapping are integration boundaries. The prior removal claim belongs to the pinned forward comparison, not a claim about all future 5.9 builds.

### Next action and acceptance

Confirm supported public identity APIs at the comparison revision, specify lifecycle/ownership and version guards, and prove baseline preservation. Do not add a transport replacement or patch engine source to bypass the registry.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPAnalyticsProvider.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPAnalyticsProvider.cpp)
- [engine.lock.json](../engine.lock.json)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-call-log --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: ue59. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

FToolset::GetToolsetClass is removed in the pinned 5.9 development tree. Replace the override and identity calls in TurboMCPCallRecorder.cpp through a version adapter. Acceptance: both supported baseline compilation and a separate 5.9 build, plus recorder registration/unregistration behavior. Do not upgrade engine.lock.json baseline.

## Verification

Pending.

</details>
