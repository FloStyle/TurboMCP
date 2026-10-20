# Make targeted layout usable on newly built graphs

ID: FIELD-06
Status: in_progress
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Current product state

The unresolved product requirement is predictable targeted layout on tool-built graphs. Geometry containment does not establish overlap-free layout. Getter input/validated/split semantics are now preserved by excluding those nodes from the reference-only splitting optimization (c2e4894, untested).

All new regression execution is deferred by owner. See [verification ledger](../docs/audit/deferred-field-verification.md). Historical approvals below do not authorize tests or engine changes now.

## Problem
Tidy refused six of six graphs without measured outlines. Native fallback overlapped blocks and was not visually checked. Existing numeric fixture checks do not certify rendered layout.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: TD-08 TD-09 TD-10 FT-66.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Verify newly built and mixed old/new graphs. Keep untouched nodes fixed, place requested nodes without overlap, and inspect rendered output. Specify asynchronous measurement or honestly labeled estimated geometry.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Tidy refused six of six graphs without measured outlines. Native fallback overlapped blocks and was not visually checked. Existing numeric fixture checks do not certify rendered layout.

Verify newly built and mixed old/new graphs. Keep untouched nodes fixed, place requested nodes without overlap, and inspect rendered output. Specify asynchronous measurement or honestly labeled estimated geometry.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Owner-authorized fast source pass

2026-10-20: owner requests fast plugin-only urgent fixes without tests. Integrator approves field-fast-followup: transaction rollback ownership, central MCP write refusal during PIE/nested transactions, actual Slate outline measurement without changing stored node sizes, exact function-spawner identities, and honest tool effect/status diagnostics. No engine changes, DSH dispatch or runtime certification. Remaining feature requests and unproven crash causes stay open.

## Fast batch result (2026-10-20)

cbe1585: Tidy uses SGraphEditor measured bounds in its plan rather than requiring stored K2 node dimensions. No node-size mutation during measurement. Adds macro/event graph enumeration and refuses ambiguous graph names. Visual overlap and all target-only layout guarantees remain unverified; truly headless sizing remains unsupported.

Source-only, no build or test run at owner request. Not runtime acceptance.
