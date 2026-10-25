# Call Swap default value preservation

ID: FIELD-FIX-04
Status: done
Priority: P0
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope

Preserve matching unlinked input string, object and text defaults with schema validation and callback readback. Refuse missing/incompatible, split or orphaned inputs instead of silently losing values. No implicit compile/save.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit fae2cf8c571d28260e3cc19fc031f43f4f10ae7e. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.
