# BatchEdit rollback transaction lifetime

ID: FIELD-FIX-14
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
The plugin BatchEdit failure path manually ended a still-live scoped transaction, causing its destructor to end again. This is a concrete rollback lifetime defect in the FIELD-04 lost-edit workstream, not proof of the native script or Widget crash root cause.

## Scope
Finalize exactly once through scope destruction; capture and match the exact owned transaction ID before undo. Refuse unsupported transaction/editor context before writes. Keep compile and save deferred.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit b2e8c081314f34c6de1ce801b3c7386840900bc6. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.
