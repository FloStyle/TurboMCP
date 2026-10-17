# R03: Verify fixture recovery and binary identity

ID: R03
Status: done
Priority: P1
Readiness: ready
Owner: integration-owner
Design: docs/design/verification.md
Kind: investigation
Review: source-reviewed

## Problem
Establish owned recovery, loaded binary identity and separate first/second fixture trials. Historical serialization drift and intermittent failures remain open under 9935c0ea and efa76989; this foundation task does not certify their root causes.

## Scope
Tools fixture generation/isolation/build provenance, TestProject fixtures and related tests. Do not regenerate every asset or commit changed binaries without explaining their cause.

## Dependencies
R02

## Acceptance
- Snapshot the exact owned fixture boundary before the first engine operation. Reject links that escape the checkout.
- After normal completion, assertion failure, startup failure and total timeout, stop owned descendants before restoring exact fixture bytes. Preserve unrelated files and leases.
- Retain a durable recovery manifest when the supervisor crashes. A later run must refuse pending recovery. Recovery must verify ownership and absence of a live run before copying files.
- Capture first-run and second-run results independently. Do not retry a failed mutation to obtain a passing report.
- Every fixture drift claim has before/after hashes, producing command and an owned editor identity.
- The initial failing run is retained; later passes do not replace it.
- Each confirmed repair names exact files and failure-path checks; withdrawn findings cite measured evidence.

## Verification
- `python Tools/test-build-provenance.py`
- `python Tools/execute-tests.py --profile build --execute`
- `python Tools/execute-tests.py --profile fixture --suites test-graph-swap-call,test-graph-repoint --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence

Reviewed against the pinned 5.8.3 engine and the candidate described in
[the infrastructure evidence](../docs/audit/infrastructure-review.md).
Focused ownership, crash/recovery and task-claim regressions pass. Real build,
tool-help and two graph suites pass. First/second trials after fixture generation
pass with separate logs. A forced real-engine timeout and supervisor death both
preserved evidence and restored 16 content files. The failed initial unity build
is retained; this module now compiles without unity grouping. No full engine
rebuild or product feature change was required.

This closes the bounded ownership/reproducibility foundation, not all historical
fixture claims, every plugin tool or release acceptance. Imported defects still
need current source and runtime reconciliation before implementation.

## Related findings
- [9935c0ea-9d4a-4287-9786-6a805c38d76d](9935c0ea.md)
- [b4d297e2-ba48-4981-b122-6c9dd48dc1ab](b4d297e2.md)
- [efa76989-7e08-4673-8ac1-901c709cdc04](efa76989.md)

## Current review

**Decision:** completed. **Follow-up owner:** [R03](roadmap-r03.md).

Bounded crash recovery and identity checks are verified. Deterministic serialization and intermittent fixture failure remain separate open findings.

### Next action and acceptance

State that this foundation completion does not close 9935c0ea or efa76989. Use identity-verified first/second trials; keep superseded legacy-DLL evidence excluded.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tools/execution_recovery.py](../Tools/execution_recovery.py)
- [Tools/plugin_identity.py](../Tools/plugin_identity.py)
- [Tools/execute-tests.py](../Tools/execute-tests.py)

Starting checks for the assigned follow-up:

- `python Tools/test-execution-recovery.py`
- `python Tools/test-plugin-identity.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
