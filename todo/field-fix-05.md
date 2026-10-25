# Pin-default failure rollback and stable readback

ID: FIELD-FIX-05
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

Pin-default edits reacquire the node by GUID, require exactly one matching input pin, and undo their own transaction if readback fails. Wrong-thread calls refuse before mutation. Rollback failure remains explicitly partial.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 3de18d0. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.


### Pin default refusal diagnostics

fb328d9 replaces the combined refusal with separate missing-input, orphaned, execution, connected, read-only and ignored-default reasons. Schema validation errors include pin name and category/subcategory. Implemented, unbuilt_untested. This improves the TurboMCP replacement diagnostics; it does not add arbitrary struct literal or by-reference support to native tools. Deferred: each refusal reason and unchanged asset state; valid default edits unchanged. No tests/builds run.

## Current review

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked done; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
