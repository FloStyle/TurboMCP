# Graph replacement single-transaction implementation

ID: FIELD-FIX-01
Status: done
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: focused_native_passed; broader_field_validation_pending

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope
Append and removal now share one transaction; failure rolls back both phases, and removal verification accounts for every planned target.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 6defafdabf7e5a97345555963e4cbcc52f95e4ab. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

### 2026-10-24 replacement protection and post-removal recovery

65dd983 repairs a confirmed protection gap: Replace previously bypassed CanUserDeleteNode for nodes other than its hardcoded entry/result/event/comment classes. Planning now reports a refusal for an unmentioned engine-protected node, and Apply independently checks deletion permission before Append mutates the graph. It also checks the complete expected remaining GUID set/count after removal and modification callbacks, rather than checking only that removed IDs disappeared.

Replacement transaction finalization now follows the exact owned record. Scope cleanup does not end a record already ended by Append failure; failure recovery only undoes the matching finalized record. Failure output clears any success report and states restoration remains unverified. Header documentation now accurately states that Replace does not compile or save.

Supervised 79e4a84b75ae44dcbad6494420c80373 passed build and test-replace-plan without native warnings. New evidence: forced failure after real append/rewiring/removal restored all four original node identities, node count, reciprocal original edge, removed-node default and position, with no active transaction. Existing successful replacement checks still pass. A protected macro tunnel is refused in both planning and an independently checked stale/corrupt accepted plan, without creation or deletion. Scope gate passed 65dd983 against 66422d41fa9bc0f146a2e6f176dc34f3aafe7b5e.

Implemented and focused-native-tested; broader field validation remains pending. These checks do not certify all node classes, editor callbacks, breakpoint/watch restoration, disk persistence or complete rollback of arbitrary Blueprint properties. No full offline or 125-tool acceptance claim; previously recorded offline failures remain.

## Current review

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked done; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
