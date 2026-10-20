# Signature type and parameter identity preflight

ID: FIELD-FIX-17
Status: done
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
Stock container helpers return scalar int for unsupported nested containers or unhashable keys, while the plugin previously reported type resolution success. Function signatures also accepted names that collide under Unreal FName identity. These are source-confirmed FIELD-09 authoring defects; unrelated generic-node type selection remains open.

## Scope
Refuse unsupported container shapes and unhashable keys before calling stock helpers. Reset output type state. Reject case-insensitive duplicates and input/output namespace collisions before graph creation.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 6d93305562c99127aa21e067b90843fa1737cebc. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Exact scalar and struct types

0a5e7d0 prevents stock GetStructType fallback-to-int from being accepted as a resolved struct, for both aliases and asset paths. Explicit float now selects the float subcategory; double/real retain double. This closes a source-confirmed signature/variable type substitution gap, not the generic operator-node inference report. Unbuilt and untested.
