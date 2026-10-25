# Graph Tidy undo ownership

ID: FIELD-FIX-20
Status: done
Priority: P1
Readiness: done
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

## Getter splitting preservation

Source c2e4894 restricts Tidy getter splitting to exact, pure VariableGet nodes with a single unsplit value output. Target/input-bearing getters, validated getters, split pins and subclasses stay intact because copying only VariableReference cannot preserve their complete semantics. The same eligibility guard applies during planning and application. This implementation slice is closed; unbuilt and untested. Targeted overlap-free layout remains open.


### Tidy rollback result and dirty baseline

12b65cc captures dirty state and node count before Modify or node creation. Rollback now requires matching wire length, restored consumer links/positions and node count before reporting unchanged or restoring a clean flag. Incomplete recovery reports partial and retains dirty state. Implemented, unbuilt_untested; visual layout acceptance remains open. Deferred evidence: clean/dirty starting assets and forced rollback failures report accurately. No tests/builds run.


### Getter destination and preview score

bbd54b9 positions split getter copies beside the consumer planned destination rather than its old coordinates. Planned wire length substitutes the copy location for each rewired consumer instead of continuing to score the original getter connection. Implemented; unbuilt_untested. Deferred evidence: a moved multi-consumer getter graph places copies near moved consumers and preview accounts for split links. Rendered overlap and skipped-copy behavior remain unverified; FIELD-06 stays open. No tests/builds run.


### Getter-copy collision placement

c1d4447 reserves planned non-comment node rectangles and previously placed getter copies. New copies shift below intersecting rectangles without moving existing nodes; positions round before collision calculation and refuse coordinates outside int32 limits. Implemented, unbuilt_untested. Deferred evidence: multiple getter copies and occupied consumer-side space avoid node intersections. Comment containment, runtime copy geometry and overall layout remain open; this does not certify overlap-free Tidy. No tests/builds run.


### Getter split failure handling

e6615fb revalidates planned getter/consumer membership and reciprocal links before mutation. Copy creation/type/link failures stop later changes and enter rollback even under Force. No silent skipped splits are reported as successful layout. Implemented, unbuilt_untested. Deferred evidence: stale pin plans refuse, failed allocation/type/link outcomes restore prior graph, Force does not bypass failure. No tests/builds run.
