# Repoint exact connection replacement

ID: FIELD-FIX-19
Status: done
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Repoint accepted only long reader IDs and refused stock schema BREAK_OTHERS_B even when the only link to break was the source link explicitly being replaced. Source-target aliasing also allowed removal of the replacement node. Source-confirmed FIELD-05 targeted-write defects.

## Scope
Accept exact short/long IDs with ambiguity refusal. Permit preview replacement only of the sole planned source link. Apply checks the old link, refuses extra schema effects after disconnect, and checks reciprocal read-back. Refuse source-target node aliasing before mutation.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit cf8275e2e93c8734ce131a50574995fbee225ecc (and b21b1c1). Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.
