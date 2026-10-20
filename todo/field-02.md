# Prevent dependent Widget loss around compile and batch rollback

ID: FIELD-02
Status: todo
Priority: P0
Readiness: needs_reconciliation
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
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.
