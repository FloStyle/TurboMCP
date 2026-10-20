# Reproduce and contain native DataTable JSON-write crashes

ID: FIELD-01
Status: in_progress
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Two crashes are reported, including a direct one-row call after restart. Native add/set/rename share a whole-table JSON rewrite; the exact assertion cause is unverified.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: DT-07.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Obtain exact build and crash artifacts; reproduce on a disposable table with a sanitized reference-bearing row struct. Avoid the affected native JSON-write path pending diagnosis. Preserve existing rows and references through save/read-back.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Two crashes are reported, including a direct one-row call after restart. Native add/set/rename share a whole-table JSON rewrite; the exact assertion cause is unverified.

Obtain exact build and crash artifacts; reproduce on a disposable table with a sanitized reference-bearing row struct. Avoid the affected native JSON-write path pending diagnosis. Preserve existing rows and references through save/read-back.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Approved synthetic reproduction

Use a disposable transient DataTable with FTableRowBase to isolate object lifetime from private row fields. Exercise FillDataTableFromJSONString first without and then with a TStrongObjectPtr. Record a crash as diagnostic failure, not acceptance. No engine changes or production assets in this assignment. The reference-bearing row and open/closed editor variants remain follow-ups. Private crash artifacts are unavailable; the owner authorizes synthetic reproduction without work code.

Exact assignment: field-datatable-repro. Supervisor command: `python Tools/execute-tests.py --profile engine --suites test-datatable-import-repro --execute --max-seconds 600`.
