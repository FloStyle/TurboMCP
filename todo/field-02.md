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

## Additional native containment candidate

Native ToolsetRegistry compile helper now queries the actual editor transaction and refuses before invoking compilation. This source-batch candidate has not been built or tested; direct calls bypassing the helper and the dependent-parent loss reproduction remain open.

## Superseding delivery requirement: stock engine, plugin only

Owner explicitly rejects custom-engine deployment. Engine source and three Python dependencies restored to pinned stock revision, dependency SHA-1 verified. Previous engine commits retained as diagnostic evidence only. Source batch continues in TurboMCP with no test runs: in-place DataTable prevalidation, validated pin-default tool with dirty/undo handling, transaction/save safeguards and bounded batch outcomes. Registry filters disable only named unsafe native alternatives and direct users to plugin tools; general script batching has no equivalent and remains unavailable through the protected registry. Direct Python/editor code is outside this protection. Engine binaries must be rebuilt after stock source restoration; no engine patch is a deployment prerequisite. Exact assignment field-plugin-only owns the implementation files.

## Owner-authorized fast source pass

2026-10-20: owner requests fast plugin-only urgent fixes without tests. Integrator approves field-fast-followup: transaction rollback ownership, central MCP write refusal during PIE/nested transactions, actual Slate outline measurement without changing stored node sizes, exact function-spawner identities, and honest tool effect/status diagnostics. No engine changes, DSH dispatch or runtime certification. Remaining feature requests and unproven crash causes stay open.
