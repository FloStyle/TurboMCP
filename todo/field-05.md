# Make graph read and targeted write identities round-trip

ID: FIELD-05
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
External library calls and inline reader forms fail round-trip; localized type IDs and numeric pin indices can disagree. Full paths worked around one misleading empty-graph refusal.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: GL-14 GL-17 GL-13 GC-14 GC-15 X-43 X-44 X-46 X-47 X-50 X-53 X-58 X-61 ED-03.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Build a small pure/impure external-library fixture. Verify read, append and copy preserve target identity, types, defaults and links. Test stable pin names and another locale; explicitly reject unsupported forms without mutation.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

External library calls and inline reader forms fail round-trip; localized type IDs and numeric pin indices can disagree. Full paths worked around one misleading empty-graph refusal.

Build a small pure/impure external-library fixture. Verify read, append and copy preserve target identity, types, defaults and links. Test stable pin names and another locale; explicitly reject unsupported forms without mutation.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Owner-authorized fast source pass

2026-10-20: owner requests fast plugin-only urgent fixes without tests. Integrator approves field-fast-followup: transaction rollback ownership, central MCP write refusal during PIE/nested transactions, actual Slate outline measurement without changing stored node sizes, exact function-spawner identities, and honest tool effect/status diagnostics. No engine changes, DSH dispatch or runtime certification. Remaining feature requests and unproven crash causes stay open.
