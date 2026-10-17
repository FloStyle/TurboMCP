# R06: Reconcile graph identity and representation repairs

ID: R06
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/graphs.md
Kind: investigation

## Problem
Separate identity, read completeness, grammar and mutation defects into independently testable repairs. Enforce D-008, preserve every link endpoint, refuse ambiguous keys and unsupported destructive formats. Compare reader output against canonical writer input without pretending compact reports are lossless.

## Scope
NodeId/NodeResolve/NodeType, GraphText/Lex/Load, GraphAppend/Patch/Replace/Repoint/SwapCall, reachability/lint and corresponding tests.

## Dependencies
R05

## Acceptance
- Each imported graph finding is reproduced, explained as already fixed, or explicitly blocked with an exact missing fixture.
- Repairs have negative controls for ambiguity, empty graph, shared pure nodes, wildcard pins and multiple links.
- Read completeness fixes precede destructive writer expansion.

## Verification
- `python Tools/test-lookup-guards.py`
- `python Tools/execute-tests.py --profile engine --suites test-graph-text-guards,test-graph-roundtrip,test-graph-append,test-graph-repoint --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [100b52a6-19fa-4437-982e-34485a9695ea](100b52a6.md)
- [261bb8db-950c-49f3-b7f2-fff304a3ec8a](261bb8db.md)
- [4de12720-cdc0-473e-8cb0-d9b4f5431a7b](4de12720.md)
- [4f1bfd61-2e79-4f4f-ab66-644a6d1acc76](4f1bfd61.md)
- [7bbe9c3c-19c2-4b62-aad2-9cd098d0fedf](7bbe9c3c.md)
- [9db83209-5824-4dd4-a08d-fdfffce29f2c](9db83209.md)
- [a28a09fd-cb1a-4b58-baee-085beff7ce10](a28a09fd.md)
- [c1e04796-f433-4f97-8945-d1b08459e136](c1e04796.md)
- [ca5bbbfc-9ffe-4ae4-b92c-32c18d688b1a](ca5bbbfc.md)
- [d3897f07-b45f-4726-91e9-9c59829985cd](d3897f07.md)
- [e866ba0d-4f6e-408e-9df7-d90a6467d410](e866ba0d.md)
- [ff18311b-9e72-49c5-8a65-5a96ccf5e028](ff18311b.md)
