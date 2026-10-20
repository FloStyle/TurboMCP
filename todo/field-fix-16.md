# FunctionAdd reconstructed pin lifetime

ID: FIELD-FIX-16
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
FunctionAdd captured raw call-pin pointers, refreshed all Blueprint nodes, then dereferenced those pointers. Node reconstruction can replace pins. This source-confirmed lifetime defect belongs to the FIELD-05 targeted-edit workstream; it is not proof of the reported DataTable or Widget loss cause.

## Scope
Use stock FWeakGraphPinPtr for both endpoints, resolve after reconstruction, and check reciprocal links. Preserve the existing missing-link diagnostic. No engine modification.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit b4067beb2a253239774b3460bdffd05d2563a6d9. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.
