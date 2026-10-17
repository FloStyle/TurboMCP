# R15: Activate gated private CI and release preparation

ID: R15
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: implementation
Review: source-reviewed

## Problem
Activate CI with authorized workflow credentials; preserve read-only PR validation. Build deterministic source/package manifests, retain evidence artifacts, and add a manually authorized release job that fails on missing gates. Separate private development credentials from publication credentials.

## Scope
Tools/ci templates, release evidence/export/package commands and GitHub release configuration. No publication during implementation.

## Dependencies
R11

## Acceptance
- PR validation is active and required on main once configured.
- Dry runs produce the exact full source tree and exact-target plugin packages with checksums.
- Publishing requires an explicit release action and accepted evidence; no push to main automatically publishes.
- A failed or partial build cannot create a release marked successful.

## Verification
- `python Tools/validate.py`
- `python Tools/export-public.py --preview --output <new-directory>`
- `Run CI and release dry-run failure controls`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [c2865829-9206-4df7-8cf1-89d8d1c319bc](c2865829.md)
- [public-cutover](public-cutover.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R15](roadmap-r15.md).

CI exists as a template, not an active required GitHub check. Workflow credentials and repository-plan restrictions are external prerequisites. Publication remains separate.

### Next action and acceptance

Implement/read-test dry-run refusal controls first; activate CI only with suitable credentials. Record branch protection availability honestly. Use complete tracked-tree exports and target-specific packages; never auto-publish from main.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Tools/ci/validate.yml](../Tools/ci/validate.yml)
- [Tools/export-public.py](../Tools/export-public.py)
- [Tools/release-check.py](../Tools/release-check.py)
- [release/status.json](../release/status.json)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
