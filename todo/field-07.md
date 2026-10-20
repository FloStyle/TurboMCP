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
