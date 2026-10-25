# Schema-aware missing-default lint

ID: FIELD-FIX-28
Status: done
Priority: P2
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
The unconnected-pin check treats empty defaults as absent even when the K2 schema accepts them; split parent pins can also be checked despite being represented by child pins.

## Scope
Skip split parents and schema-valid empty defaults. Existing hidden-pin guard retained. This is diagnostic correction, not proof of LT-07 world-context root cause or compiler acceptance.

## Completion evidence
4c4a38e. Implemented, unbuilt_untested.

## Acceptance
Deferred: valid empty string/text defaults and split parents do not warn; invalid empty defaults still warn; child pins checked independently. Compiler remains authoritative for required wiring.

## Verification
No tests/builds per owner.
