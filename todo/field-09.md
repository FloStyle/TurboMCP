# Triage remaining Blueprint schema and editing gaps

ID: FIELD-09
Status: todo
Priority: P2
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Reports identify missing defaults, parameter types, overrides, naming and struct-value support. Missing empty graph and wrong equality type remain uncertain or confounded.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: BL-12 BL-13 BL-14 NF-08 X-37 X-49 FA-07 TH-11 LT-07 OV-03 ED-07 ED-08 ED-09 VA-04.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Specify small independent reproductions and link existing owners before implementation. Preserve successful native workarounds; do not widen crash fixes into a feature rewrite.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Reports identify missing defaults, parameter types, overrides, naming and struct-value support. Missing empty graph and wrong equality type remain uncertain or confounded.

Specify small independent reproductions and link existing owners before implementation. Preserve successful native workarounds; do not widen crash fixes into a feature rewrite.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.
