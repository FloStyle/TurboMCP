# Prevent dependent Widget loss around compile and batch rollback

ID: FIELD-02
Status: in_progress
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
A failed batch containing compile reportedly removed 23 child widgets from three dependent assets in memory. Restart without saving recovered them; persistent loss was not reported.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: X-45.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Reproduce with a child Widget and two parent fixtures. Compare hierarchy, bindings, defaults and dirty state after deliberate failure. Keep compile outside edit transactions; specify a guard before changing rollback behavior.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

A failed batch containing compile reportedly removed 23 child widgets from three dependent assets in memory. Restart without saving recovered them; persistent loss was not reported.

Reproduce with a child Widget and two parent fixtures. Compare hierarchy, bindings, defaults and dirty state after deliberate failure. Keep compile outside edit transactions; specify a guard before changing rollback behavior.

## Handoff
Containment implemented and tested: TurboMCP compile refuses active editor transactions before loading assets. Native compile paths and the reported dependent-widget loss remain open. Codex owns urgent follow-up; DSH stays paused.

## Approved immediate safeguard

Refuse EditTurboMCPCompile whenever GEditor reports an active transaction, before resolving or loading assets. Do not end or undo the caller transaction. Test the real wrapper on a synthetic Widget fixture: refusal, unchanged graph/widget identities and dirty state, transaction remains active, then normal compile succeeds after the scope ends. This is containment of the TurboMCP entry point, not proof that native compile or every reinstance route is protected. Reproduce the full nested-widget failure separately.

Verification: `python Tools/execute-tests.py --profile engine --suites test-widget-roundtrip,test-monday-disk-loop --execute --max-seconds 900`.

## Safeguard evidence

Candidate 4b70306e12289c3d47cc62859280b1da831e677f: supervised build plus test-widget-roundtrip and test-monday-disk-loop passed (execution ba0d5ab258634d9d95243ef0556fd0cf; 16 fixture files restored). Offline validation d8e1534797b54b9e9353030d851b6769 passed all 26 checks. This does not reproduce or certify recovery of the reported dependent-parent widget loss.
