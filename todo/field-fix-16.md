# FunctionAdd reconstructed pin lifetime

ID: FIELD-FIX-16
Status: done
Priority: P0
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
FunctionAdd captured raw call-pin pointers, refreshed all Blueprint nodes, then dereferenced those pointers. Node reconstruction can replace pins. This source-confirmed lifetime defect belongs to the FIELD-05 targeted-edit workstream; it is not proof of the reported DataTable or Widget loss cause.

## Scope
Use stock FWeakGraphPinPtr for both endpoints, resolve after reconstruction, and check reciprocal links. Preserve the existing missing-link diagnostic. No engine modification.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit b4067beb2a253239774b3460bdffd05d2563a6d9. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Signature creation result

FunctionAdd checks each created pin name, direction and exact type. A missing or altered result removes the newly created function and reports whether cleanup removed the graph, rather than reporting the requested signature as successfully created. Implementation closed; unbuilt and untested.

## Targeted caller refresh

FunctionAdd retains the skeleton update but reconstructs only self-call nodes naming the newly created function; external same-named calls and unrelated graph nodes are excluded. Add also refuses PIE, off-game-thread and active-transaction invocation before mutation. This closes the broad reconstruction scope defect, not dependent Widget-loss root cause. Unbuilt and untested.

## Removal outcome

9360665 checks actual graph absence before reporting removal, distinguishes it from name-reuse eligibility, and refuses ambiguous names or unsafe editor contexts. Implementation closed; unbuilt and untested.

## Partial outcome reporting

cc8121f reports partial rather than changed when caller links are lost or function implementation read-back fails. Such results no longer recommend immediate compile/save and explicitly state that changes remain in memory. This closes false-success reporting only; it does not restore lost links. Unbuilt and untested.
