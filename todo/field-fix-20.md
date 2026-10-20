# Graph Tidy undo ownership

ID: FIELD-FIX-20
Status: done
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Graph Tidy changed node/comment positions and optional getter links without owning an undo transaction. This is a source-confirmed recovery gap in FIELD-06; overlap-free layout remains a separate open requirement.

## Scope
Open one editor transaction after no-op handling, capture graph/Blueprint/existing nodes, make created getter copies transactional, and refuse missing undo, PIE or nested transaction context. No implicit compilation or saving.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit b9769cc90b598815455315162e16c5381f40ac08. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Repeated layout drift implementation

9a0fd05a7cedc9970b0432de040e15e304660c06: moving nodes no longer count as fixed obstacles for boxless/scoped placement. Preserve the group minimum X/Y origin unless a genuine fixed boundary requires moving right. This fixes source-confirmed repeated rightward drift and unwanted Y=0 reset. Unbuilt and untested; complete visual layout acceptance remains open.
