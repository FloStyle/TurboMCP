# Improve field diagnostics build identity and tool effect visibility

ID: FIELD-08
Status: in_progress
Priority: P2
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Current product state

Loaded source revision identity and complete human-readable discovery remain product gaps. Re-running already implemented logging checks is verification, not a new feature task.

All new regression execution is deferred by owner. See [verification ledger](../docs/audit/deferred-field-verification.md). Historical approvals below do not authorize tests or engine changes now.

## Problem
Field reports lack call logs and exact loaded build identity. Help truncation and Read-tool side-effect categorization impede diagnosis.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: X-35 X-63 TH-09 TH-10 TH-12 SV-10.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Expose loaded build identity and bounded call evidence, paginate discovery and identify execution-capable tools. Preserve unknown metrics rather than inventing estimates or proof.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Field reports lack call logs and exact loaded build identity. Help truncation and Read-tool side-effect categorization impede diagnosis.

Expose loaded build identity and bounded call evidence, paginate discovery and identify execution-capable tools. Preserve unknown metrics rather than inventing estimates or proof.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Owner-authorized fast source pass

2026-10-20: owner requests fast plugin-only urgent fixes without tests. Integrator approves field-fast-followup: transaction rollback ownership, central MCP write refusal during PIE/nested transactions, actual Slate outline measurement without changing stored node sizes, exact function-spawner identities, and honest tool effect/status diagnostics. No engine changes, DSH dispatch or runtime certification. Remaining feature requests and unproven crash causes stay open.

## Fast batch result (2026-10-20)

cbe1585: status includes engine version, module binary path and file timestamp, explicitly not source-revision proof; reports source revision unknown. Help flags PlayCheck execution effects and directs truncated summaries to the full tool index. Exact embedded source revision and pagination remain open.

Source-only, no build or test run at owner request. Not runtime acceptance.
