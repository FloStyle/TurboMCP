# Repoint exact connection replacement

ID: FIELD-FIX-19
Status: done
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Repoint accepted only long reader IDs and refused stock schema BREAK_OTHERS_B even when the only link to break was the source link explicitly being replaced. Source-target aliasing also allowed removal of the replacement node. Source-confirmed FIELD-05 targeted-write defects.

## Scope
Accept exact short/long IDs with ambiguity refusal. Permit preview replacement only of the sole planned source link. Apply checks the old link, refuses extra schema effects after disconnect, and checks reciprocal read-back. Refuse source-target node aliasing before mutation.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit cf8275e2e93c8734ce131a50574995fbee225ecc (and b21b1c1). Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Source removal follow-up

99ea6450b3268781e1e57875dba11924c12dcb57: preview refuses source deletion when other pins still have connections; apply rechecks every source pin before removal and rolls back if links remain. KeepSource=true remains available. Implementation closed, unbuilt and untested.

## Unique pin identity

Repoint now resolves a unique name-and-direction pin in both Plan and Apply. Duplicate matches refuse instead of silently rewiring the first matching pin. Missing/ambiguous output diagnostics say no unique output. Implementation closed, unbuilt and untested.

## Explicit deletion option

5cce82e parses trimmed true/false and 1/0 explicitly, retaining the empty keepSource=true default. Invalid values refuse before loading assets instead of silently choosing true; whitespace no longer changes false into true. Implementation closed; unbuilt and untested.

## Protected repoint source deletion

FIELD-FIX-19 checks CanUserDeleteNode in planning and before apply mutation when keepSource=false. Protected sources refuse and advise keepSource=true. Implementation done; unbuilt and untested. Deferred evidence: protected source deletion refuses unchanged; retaining source and deleting ordinary eligible sources retain expected behavior.

## Repoint stale reader counts

FIELD-FIX-19 refuses before mutation when the source output reader count differs from its plan. Existing per-reader identity checks still apply. This prevents newly added consumers being silently excluded when retaining the source. Implementation done; unbuilt and untested. Deferred evidence: added/removed readers between plan and apply refuse unchanged; unchanged reader sets repoint normally.

## Repoint undo ownership

Standalone repoint validates transaction availability and captures its owned undo ID before mutation. Failure uses that original identity rather than adopting the active record at failure time. Caller-owned batch rollback remains caller-owned. Implementation done; unbuilt and untested. Deferred evidence: undo unavailable refuses unchanged; forced failure restores only its owned transaction.

## Repoint final count rollback

Final node-count readback now runs before standalone commit. A mismatch invokes owned rollback instead of reporting an unknown committed edit without attempting restoration. Caller-owned batch recovery remains external. Implementation done; unbuilt and untested. Deferred evidence: unexpected node-count change triggers restoration; successful repoint commits once.
