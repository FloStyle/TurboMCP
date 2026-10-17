# R07: Reconcile geometry, layout and capture correctness

ID: R07
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/graphs.md
Kind: investigation

## Problem
Measure actual geometry and captured framing. Distinguish estimation, unsupported geometry and real wire cycles. Resolve conflicting report/fixer rules before adding heuristics. Keep UI presentation improvements out of correctness repairs.

## Scope
Layout/Report, GraphTidy/FixBackward/Knots, Capture/Asset/Selector and their fixtures.

## Dependencies
R06

## Acceptance
- An unopened graph is covered by geometry tests.
- Every claimed framed node is visible or the operation refuses honestly.
- Layout rechecks its own reported findings and explains any remaining defects.

## Verification
- `python Tools/execute-tests.py --profile engine --suites test-layout,test-graph-tidy,test-graph-fix-backward,test-capture-selectors --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [02ba0f24-4c9d-41cf-8a02-b75644a13b0c](02ba0f24.md)
- [a8341453-1aae-48d9-ad3f-fec762cfa0e5](a8341453.md)
- [be9c2edb-2826-4421-941e-f02b00041b4d](be9c2edb.md)
- [c421299d-8275-4cc5-85d1-bc85cbb49276](c421299d.md)
- [c5c54253-37b8-452c-84d5-93f088f11ea8](c5c54253.md)
- [e6fc78bd-754d-43c1-bdad-78d7abaeaaee](e6fc78bd.md)
