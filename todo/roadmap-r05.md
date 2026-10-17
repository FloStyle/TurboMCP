# R05: Specify and verify mutation lifecycle repairs

ID: R05
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/mutations.md
Kind: investigation

## Problem
Verify transaction ownership, readiness, baseline diagnostics, independent read-back, rollback outcomes and persistence separation. The batch source currently emits unchanged even if rollback fails and defers its final compile. Its baseline test preserves incomplete behavior. Produce the smallest ordered repairs for ARC03-ARC09.

## Scope
TurboMCPMutation, Rollback, Compile, GraphRepoint, BatchEditTools and read-only dirty-state callers. No general multi-asset batch expansion.

## Dependencies
R04

## Acceptance
- Each mutation family states its transaction/compile/save owner and failure recovery.
- Restricted batch cases B01-B05 replace baseline success assumptions with the required contract; broad batch remains deferred.
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
