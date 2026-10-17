# R03: Establish fixture and binary reproducibility evidence

ID: R03
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: investigation

## Problem
Reproduce the first-run-after-rebuild and fixture-drift reports. Capture baseline hashes, build identity, first and second test runs, in-memory and disk restoration. Classify deterministic content changes versus package serialization noise. Create one bounded repair specification per confirmed root cause; do not normalize away meaningful differences.

## Scope
Tools fixture generation/isolation/build provenance, TestProject fixtures and related tests. Do not regenerate every asset or commit changed binaries without explaining their cause.

## Dependencies
R02

## Acceptance
- Every fixture drift claim has before/after hashes, producing command and an owned editor identity.
- The initial failing run is retained; later passes do not replace it.
- Each confirmed repair names exact files and failure-path checks; withdrawn findings cite measured evidence.

## Verification
- `python Tools/test-build-provenance.py`
- `pwsh -File Tools/build.ps1 -Engine <configured-engine>`
- `pwsh -File Tools/run-suites.ps1 -Suites test-graph-swap-call,test-graph-repoint`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [9935c0ea-9d4a-4287-9786-6a805c38d76d](9935c0ea.md)
- [b4d297e2-ba48-4981-b122-6c9dd48dc1ab](b4d297e2.md)
- [efa76989-7e08-4673-8ac1-901c709cdc04](efa76989.md)
