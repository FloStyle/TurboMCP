# Verify forward protocol changes on pinned source and runtime

ID: ue59-protocol
Status: backlog
Priority: P2
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed

## Current review

**Decision:** deferred. **Follow-up owner:** [R13](roadmap-r13.md), [R14](roadmap-r14.md).

Baseline source and client handling are available; the local 5.8.3 build does not prove forward discovery, mixed media, nulls or child-actor refusals. Existing forward notes need revision-qualified verification.

### Next action and acceptance

R13 must locate exact native interfaces and distinguish source presence from default discovery. R14 owns target runtime probes once a bounded adapter is approved. Preserve media blocks; never flatten them into a successful text-only claim.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp)
- [Tools/tmcp.py](../Tools/tmcp.py#L139)
- [TestProject/TurboMCPTest.uproject](../TestProject/TurboMCPTest.uproject)
- [docs/audit/native-overlap.json](../docs/audit/native-overlap.json)

Starting checks for the assigned follow-up:

- `python Tools/test-result-contract.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: ue59. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Reconcile SemanticSearchToolset discovery, image content/structuredContent handling, UObject null values and child-actor refusals. Acceptance: fixtures exercise text-only and mixed-media responses, null references, dependency discovery, and expected refusal behavior on the pinned forward tree.

## Verification

Pending.

</details>
