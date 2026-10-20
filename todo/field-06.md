# Make targeted layout usable on newly built graphs

ID: FIELD-06
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

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
