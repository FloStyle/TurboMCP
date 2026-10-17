# R04: Audit callable behavior and result contracts

ID: R04
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation

## Problem
For each tool record actual callable schema, side effects, supported scope, refusal behavior and test evidence. Distinguish declared, definition located, source reviewed, runtime observed and release accepted. Start with result-envelope double wrapping and invalid numeric/default parsing. Do not invent a new public result schema.

## Scope
All 101 rows of docs/audit/capabilities.json, declarations, wrapper definitions, schema/result helpers and matching tests. Review one tool family at a time.

## Dependencies
R03

## Acceptance
- Every tool has an explicit audit status and a source/test reference; none disappears from the inventory.
- Runtime calls use owned disposable fixtures; execution-capable Read/Preview tools are identified explicitly.
- Each confirmed defect maps to one bounded repair with observable acceptance, not an unbounded refactor.

## Verification
- `python Tools/audit-inventory.py --check`
- `python Tools/test-result-contract.py`
- `python Tools/test-client-retry.py`
- `Run the selected family suites using Tools/run-suites.ps1 after build`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [019cf7af-dfe5-4cda-91b9-1ca706ace4e5](019cf7af.md)
- [7de8d185-a025-4485-837e-2d03975691b0](7de8d185.md)
- [abdd2c08-6544-4730-b183-e4d856aad868](abdd2c08.md)
