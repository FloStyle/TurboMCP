# R01: Repair test-audit evidence classifications

ID: R01
Status: done
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: implementation
Review: source-reviewed

## Problem
The current quality audit falsely reports NO-ASSERT for test-build-provenance.py and test-repo-boundaries.py, which use Python assert. The coverage audit reports all tools called while six material tools are only name references. Correct the classifications; do not suppress files or count a name as an executed call.

## Scope
Tools/audit-suite-quality.py, Tools/audit-tool-coverage.py, and their focused regression tests. No plugin or engine edits.

## Dependencies
None.

## Acceptance
- Use AST or an equivalently tested parser to recognize a real assertion failure path; assert True and dead/comment text must not become behavioral evidence.
- Output distinguishes parsed call, name-only reference and missing reference. It never says all tools were executed.
- Add positive/negative parser fixtures and demonstrate the old behavior fails the new regression. Keep candidate findings distinct from proved defects.

## Verification
- `python Tools/test-coverage-audit.py`
- `python Tools/audit-tool-coverage.py`
- `python Tools/audit-suite-quality.py`
- `python Tools/validate.py`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Reviewed against baseline 1e41e14f6fef9ee2473fad3433bd2406c18fae86.
The original audit reports NO-ASSERT for `assert actual == expected` and credits
a commented call. Both failures were reproduced from Git HEAD in temporary files.
The revised `python Tools/test-coverage-audit.py` passes positive, literal-only,
comment, assert-True and simple unreachable-code controls. Inventory remains
101 tools: 95 parsed-call references and six name-only references. Runtime
acceptance remains uncertified. This change does not require an engine build.
Full offline validation is required for the integration commit; local evidence
is retained under .local/executions. R02/R03 subsequently completed the bounded execution foundation; see their evidence.

## Related findings
- [048107d9-a439-4e28-955a-562804215ac9](048107d9.md)
- [2f066e6c-8c34-412f-b1b4-451389996086](2f066e6c.md)
- [51678b9f-9760-4548-b8f7-f58c461af520](51678b9f.md)
- [5301f48e-4603-436a-b34f-54c31ab6f0a5](5301f48e.md)
- [53fb9672-b301-4d18-877a-9bf737c0ad6b](53fb9672.md)
- [71e2e86a-9854-4a25-9d17-18f18f5679e5](71e2e86a.md)

## Current review

**Decision:** completed. **Follow-up owner:** [R01](roadmap-r01.md).

AST classification changes and focused controls are implemented. Parsed calls remain source evidence, not runtime coverage.

### Next action and acceptance

Retain the regression gate; remove stale wording that R02/R03 still block all handoff. Do not change counts to imply every declared tool has run.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tools/audit-tool-coverage.py](../Tools/audit-tool-coverage.py)
- [Tools/audit-suite-quality.py](../Tools/audit-suite-quality.py)
- [Tools/test-coverage-audit.py](../Tools/test-coverage-audit.py)

Starting checks for the assigned follow-up:

- `python Tools/test-coverage-audit.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
