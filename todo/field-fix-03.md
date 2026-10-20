# Call Swap transaction and replacement-position safeguards

ID: FIELD-FIX-03
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope
Call Swap checks its undo result and transaction identity, captures linked nodes before mutation, removes implicit compile, refuses ambiguous/stale graph plans, checks final node count and migrated links, and retains the original position when deleting the old node. This does not implement literal-default migration or certify visual non-overlap when retaining the old node.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open under FIELD-REG-01. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 019a1c05d6e046809a3e8b8ef170af3453164a0f. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See FIELD-REG-01; implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence and a separate pending regression record.
