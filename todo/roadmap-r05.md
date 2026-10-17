# R05: Specify mutation and restricted-batch repairs

ID: R05
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Verify transaction ownership, readiness, baseline diagnostics, independent read-back, rollback outcomes and persistence separation. The batch source currently emits unchanged even if rollback fails and defers its final compile. Its baseline test preserves incomplete behavior. Produce the smallest ordered repairs for ARC03-ARC09.

## Scope
TurboMCPMutation, Rollback, Compile, GraphRepoint, BatchEditTools and read-only dirty-state callers. No general multi-asset batch expansion.

## Dependencies
R04

## Acceptance
- Each mutation family states its transaction/compile/save owner and failure recovery.
- The repair specification maps B01-B05 to required assertions that will replace baseline success assumptions during implementation; broad batch remains deferred.
- Read-only operations preserve user-owned dirty state and never silently save or check out.

## Verification
- `python Tools/test-result-contract.py`
- `python Tools/execute-tests.py --profile engine --suites test-batch-edit,test-compile-ensure --execute`
- `Review Tests/acceptance.json mutation and batch cases`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [3be553c8-ff93-4646-9bd7-3b3712c1b582](3be553c8.md)
- [5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae](5b6a8e45.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R05](roadmap-r05.md).

Batch still nests transaction ownership through ApplyCore and carries baseline-only tests. Widget preview execution and failed rollback truthfulness are concrete additional boundaries. SaveTurboMCPAssets also reports saved from a clean dirty flag without requiring bSaved in that branch; that conflicts with ARC05 durable-write evidence.

### Next action and acceptance

Deliver ordered specs: core ownership, strict batch planning, final compile/restoration, and effectful preview consent. Investigation may document failing tests; it must not promise to repair them all. Keep ARC03-ARC10 acceptance unchanged.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPMutation.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMutation.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L525)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp#L21)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPRollback.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPRollback.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L548)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-batch-edit,test-compile-ensure --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
