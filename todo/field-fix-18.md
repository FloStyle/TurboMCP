# Reader node IDs in targeted edits

ID: FIELD-FIX-18
Status: done
Priority: P1
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
ED-03 reported a node found by NodeFind being refused as missing by Swap. Readers emit short asset-name IDs by default, while Swap and PinDefault only accepted full object-path IDs. This mismatch is confirmed in source; the exact private custom-node case is not reproduced.

## Scope
Accept exact short and long reader IDs within the explicitly resolved graph. Swap refuses duplicate identities instead of picking the first. Custom nodes still receive the existing unsupported-class refusal; arbitrary custom-node swapping is not implemented.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit b88dfdc2c2ea66e48b6ce6d3e7f0a7d8244ba5a6. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.
