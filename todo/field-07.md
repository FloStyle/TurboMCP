# Detect broken execution paths and duplicate graph chains

ID: FIELD-07
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Clean compilation coexisted with disconnected or duplicated execution chains. Reader reachability and multi-source exec interpretation need focused acceptance.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: GL-15 GL-16 X-38 X-39 X-41 X-42.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Use live/dead-chain controls with shared data dependencies. Verify intended event-to-action reachability and untouched nodes. Separate append semantics from replacement and do not infer behavior from compile alone.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Clean compilation coexisted with disconnected or duplicated execution chains. Reader reachability and multi-source exec interpretation need focused acceptance.

Use live/dead-chain controls with shared data dependencies. Verify intended event-to-action reachability and untouched nodes. Separate append semantics from replacement and do not infer behavior from compile alone.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Owner-authorized fast source pass

2026-10-20: owner requests fast plugin-only urgent fixes without tests. Integrator approves field-fast-followup: transaction rollback ownership, central MCP write refusal during PIE/nested transactions, actual Slate outline measurement without changing stored node sizes, exact function-spawner identities, and honest tool effect/status diagnostics. No engine changes, DSH dispatch or runtime certification. Remaining feature requests and unproven crash causes stay open.

## Fast batch result (2026-10-20)

Source batch prevents nested script writes through the protected plugin registry and warns compile does not prove reachability. No new execution-chain analysis or runtime proof; task stays open.

Source-only, no build or test run at owner request. Not runtime acceptance.

## Field-note diagnostic follow-up

FIELD-FIX-08 implements X-41 compile diagnostics only. GL-16 reader already uses shared execution-aware reachability; needs regression evidence, not a speculative rewrite. Writer enforcement and full semantic acceptance remain open.

## Append reachability follow-up

FIELD-FIX-09 implements X-41 Append rejection of new unreachable exec-input nodes; applies through Replace too. Other writers, GL-16 reader controls and runtime execution correctness remain open. Disconnected staging now refuses; connect new execution paths in the same request.
