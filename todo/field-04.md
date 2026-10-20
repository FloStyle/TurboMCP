# Reconcile batch failures timeouts and possible duplicate writes

ID: FIELD-04
Status: todo
Priority: P1
Readiness: needs_reconciliation
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Reports describe inconsistent partial application, stale log errors and late timeout completion. Duplicate execution is suspected, not established. S2 X-64 and S3 X-66 are one incident.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: X-40 X-48 X-51 X-52 X-57 X-64 X-65 X-66 X-36.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Correlate request identities and per-call logs with graph deltas; reproduce mixed-operation failure and timeout completion. Report applied, rolled-back or unknown outcomes and prevent blind retries. Treat modal stalls separately from crashes.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Reports describe inconsistent partial application, stale log errors and late timeout completion. Duplicate execution is suspected, not established. S2 X-64 and S3 X-66 are one incident.

Correlate request identities and per-call logs with graph deltas; reproduce mixed-operation failure and timeout completion. Report applied, rolled-back or unknown outcomes and prevent blind retries. Treat modal stalls separately from crashes.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.
