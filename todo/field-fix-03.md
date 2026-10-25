# Call Swap transaction and replacement-position safeguards

ID: FIELD-FIX-03
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope
Call Swap checks its undo result and transaction identity, captures linked nodes before mutation, removes implicit compile, refuses ambiguous/stale graph plans, checks final node count and migrated links, and retains the original position when deleting the old node. This does not implement literal-default migration or certify visual non-overlap when retaining the old node.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 019a1c05d6e046809a3e8b8ef170af3453164a0f. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Unique pin mapping

901142e requires unique name/direction matches in Call Swap pin lookup, including link and default transfer paths. Duplicate matches no longer select the first pin silently. Implementation closed; unbuilt and untested.

## Current review

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked done; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
