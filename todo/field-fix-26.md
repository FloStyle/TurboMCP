# Default-on tool call recording

ID: FIELD-FIX-26
Status: done
Priority: P2
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
X-63: fresh configurations disable the call log, losing useful field diagnostics.

## Scope
Enable the recorder default. Explicit persisted configuration remains authoritative; existing users can enable recording through the panel or EditTurboMCPCallLog. No forced preference migration.

## Completion evidence
The settings class default is true. Implemented, unbuilt_untested.

## Acceptance
Deferred: fresh settings record calls; persisted false stays disabled; manual toggling remains available.

## Verification
No tests/builds per owner. Historical default-off specification superseded by explicit user instruction.
