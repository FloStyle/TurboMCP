# R09: Reconcile references and runtime observations

ID: R09
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation

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
- `pwsh -File Tools/run-suites.ps1 -Suites test-refs,test-trace,test-play-check,test-asset-cost`

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
