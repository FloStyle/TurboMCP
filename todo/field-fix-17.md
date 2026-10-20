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

## Signature reader direction

Shared signature descriptions now use the stock K2Node_CallFunction input rule: reference parameters remain inputs despite OutParm; return and non-reference outputs remain outputs. This closes a source-confirmed reader direction mismatch in the type/signature scope. Unbuilt and untested.


### Blueprint type eligibility

338c78a rejects loaded enum and class types that stock IsAllowableBlueprintVariableType excludes, before returning a usable pin type. FIELD-FIX-17 remains implemented and unbuilt_untested; FIELD-09 generic operator inference remains open. Deferred verification: allowed user enums/classes and all four reference forms still resolve; non-Blueprint enums/classes refuse without graph mutation. No tests or builds run.


### Signature name bounds

888f221 rejects names at or above NAME_SIZE and the case-insensitive None sentinel before FName construction for function and signature pin names. This guards the engine fatal length path and avoids empty-name authoring. Implemented; unbuilt_untested. Deferred checks: oversized ASCII names refuse without mutation; ordinary names remain accepted; None/none refuse. No test/build was run. This is not a diagnosis of the reported DataTable crash.
