# DataTable prepared row application

ID: FIELD-FIX-15
Status: done
Priority: P0
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
DataTable apply parsed validated text a second time directly into live storage and retained AddRow's pointer across editor callbacks. This repair reduces avoidable live-mutation risk; the reported work-machine crash is not reproduced or closed.

## Scope
Retain initialized scratch rows through application, copy using UScriptStruct::CopyScriptStruct, re-fetch new rows after editor callbacks, stop on changed row schema, and refuse off-thread application. No custom engine changes.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 45c930c5a1fbc995f739adc700d19ec6c442f4f8. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Removal failures

Failed RemoveRow or a row still present after removal now enters the batch rollback path instead of returning success. Final read-back also checks every requested removal. Implementation closed, unbuilt and untested; original DataTable crash cause remains open.

## Original undo ownership

DataTable edits acquire and capture their undo identity before mutation. Failure finalizes the scope once and only undoes that same record with no transaction active, rather than adopting whichever transaction is active at failure time. Implementation closed; unbuilt and untested. Crash root cause remains open.


### DataTable name bounds

afd3d63 rejects oversized row/field identifiers before planning FName conversion and checks operation/removal identifiers again before apply. Refusal precedes mutation and enters the plan refusal list. Implemented; unbuilt_untested. Deferred evidence: oversized set/remove row and field identifiers refuse with unchanged rows, while normal names work. Original field crash remains unproven; this guard is not a root-cause claim. No tests/builds run.


### DataTable empty sets and failure stop

535280a rejects separator-only/empty set operations in Parse and Plan; they can no longer silently create default rows. Apply stops later removals/sets after the first failed mutation and checks row schema before invoking another AddRow callback. Existing owned rollback handles the failure. Implemented; unbuilt_untested. Deferred evidence: empty-set refusal preserves table, injected removal/addition failure skips later mutations and rolls back; ordinary multirow edits remain valid. Original reported crash remains open. No tests/builds run.


### DataTable reusable outputs

6520c0f resets parse/plan/report outputs; parsing publishes operations only after complete success. Replanning preserves aliased input but replaces the prior plan. Missing table/schema records a refusal as well as returning an error. Current MCP wrapper already uses fresh locals; this closes helper-level stale/partial output behavior, not a reproduced field crash. Implemented, unbuilt_untested. Deferred evidence: sequential reuse, late parse error, aliased replan and early apply refusal cannot expose prior operations or success reports. No tests/builds run.
