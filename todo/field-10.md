# Separate project capture GPU failures from tool crashes

ID: FIELD-10
Status: todo
Priority: P2
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Current product state

This is a causality investigation, not a confirmed TurboMCP defect. Project capture GPU exhaustion was reported outside tool calls; do not claim a plugin fix.

All new regression execution is deferred by owner. See [verification ledger](../docs/audit/deferred-field-verification.md). Historical approvals below do not authorize tests or engine changes now.

## Problem
Two GPU out-of-memory crashes are reported during project capture execution outside tool calls. No tool causality is established.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: S3 GPU crash context.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Collect GPU logs, capture dimensions and memory evidence separately. Reproduce reduced capture workloads before attributing failure to TurboMCP.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Two GPU out-of-memory crashes are reported during project capture execution outside tool calls. No tool causality is established.

Collect GPU logs, capture dimensions and memory evidence separately. Reproduce reduced capture workloads before attributing failure to TurboMCP.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.
