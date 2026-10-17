# R09: Specify truthful references and runtime observations

ID: R09
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation
Review: source-reviewed

## Problem
Verify direction=both is the union of actual in/out edges for every accepted asset form. Separate no events from failed event-to-node matching. Preserve or explicitly explain trace arm lifecycle changes. Never infer never-ran or unused from unavailable evidence.

## Scope
Refs, AssetCost, BlueprintRead, Trace, PlayCheck and call observation helpers.

## Dependencies
R08

## Acceptance
- Short-name and full-path references agree for the same resolved asset.
- Trace reports distinguish unavailable, unmatched and observed absence.
- Budgeted output is bounded with explicit truncation/completeness.

## Verification
- `python Tools/execute-tests.py --profile engine --suites test-refs,test-trace,test-play-check,test-asset-cost --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [02674939-2883-4cec-b2eb-24588dcfeac6](02674939.md)
- [3eae39b1-82f6-480c-bf6e-e20d3bef5fe6](3eae39b1.md)
- [484d5638-06b1-44ea-a597-97097644699d](484d5638.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R09](roadmap-r09.md).

Explicit both skips reference queries and trace uses a constant bytecode offset. Existing refs tests can pass despite the first defect.

### Next action and acceptance

Use nonzero saved reference edges and actual executed-node witnesses. Require explicit missing/unmapped states. Complete the observations audit without silently closing known defects from broad suite passes.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L167)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L127)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-refs,test-trace,test-play-check --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
