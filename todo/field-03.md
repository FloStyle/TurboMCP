# Verify pin-only changes persist and report save state accurately

ID: FIELD-03
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Two pin-only edits reportedly changed memory without dirtying the package. Compile status uses the dirty flag and save skips clean packages; this is not disk equality proof.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: ED-04.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Trace the native setter and reproduce pin-only mutation. Verify dirty state, save outcome and reload read-back. Distinguish no dirty flag from verified persisted content.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Two pin-only edits reportedly changed memory without dirtying the package. Compile status uses the dirty flag and save skips clean packages; this is not disk equality proof.

Trace the native setter and reproduce pin-only mutation. Verify dirty state, save outcome and reload read-back. Distinguish no dirty flag from verified persisted content.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.
