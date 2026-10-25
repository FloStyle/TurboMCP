# NF-08 NodeFind whitespace search

ID: FIELD-FIX-12
Status: done
Priority: P2
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
NF-08: space-separated search words were interpreted as one phrase, producing misleading zero matches.

## Scope

Tokenize whitespace, commas and pipes into AND terms locally in NodeFind. Report the parsed terms. Shared asset list parsing stays unchanged. No quoted phrase syntax is promised.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open in the deferred verification ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
Source commit 5274770691becf8249f413fd04b68ee5f6b01cb0. Implementation closed; unbuilt and untested.

## Dependencies

FIELD-04 reviewed source scope.

## Verification

Deferred by owner. See [deferred verification ledger](../docs/audit/deferred-field-verification.md); implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence. Deferred checks remain in the verification ledger, not a separate product task.

## Ambiguous canonical ID hints

NodeFind no longer picks the last canonical ID when multiple graph node types share a display title. It omits that ambiguous hint while retaining search results. Implementation done; unbuilt and untested. Deferred evidence: repeated identical IDs retain hints, conflicting IDs suppress them. This is not confirmation or closure of the ED-08 generic equality-node diagnosis.

## Exact node ID precedence

Probe and spawn resolution now prefer exact type IDs before considering legacy aliases. Unrelated legacy alias collisions no longer make an exact ID ambiguous. Legacy-only ambiguous requests still refuse. Implementation done; unbuilt and untested. Deferred evidence: exact/legacy collision and ambiguous legacy-only controls.

## Pin discovery exact membership

Pin discovery validates exact IDs against the full collected type inventory rather than the top-ten ranked suggestions. Unknown IDs still return suggestions. Implementation done; unbuilt and untested. Deferred evidence: a valid ID remains accepted regardless of suggestion ranking or truncation.

## NodeResolve bounded readable output

The pin counter now increments so the advertised 40-pin cap is effective, with remaining count excluding null entries. String/text default values escape backslashes and line separators to preserve one pin per report line. Implementation done; unbuilt and untested. Deferred evidence: more than 40 pins, null slots and multiline defaults.

## Concrete pin type visibility

NodeResolve now reports subtype object paths, reference/const qualifiers and map-value category/subcategory/object paths. This removes category-only ambiguity for object, enum, struct and map pins. Implementation done; unbuilt and untested. Deferred evidence: reflected object/enum/struct and map pins agree with editor signatures.

## Search result asset identity

NodeFind results and NodeResolve ambiguity suggestions now emit long node IDs containing the asset path. This avoids returning another ambiguous short ID when different assets share a name. Implementation done; unbuilt and untested. Deferred evidence: same-named assets in different folders resolve via returned IDs.


### X-37 object pin values

Reconciled the existing cbe1585 implementation: NodeResolve prints unlinked DefaultObject values as object:<full path>, with linked pins retaining link counts. X-37 implementation is done, unbuilt_untested. Deferred evidence: an assigned object pin shows its correct path and a connected pin reports its connections. No tests/builds run; no new runtime claim.


### Pin identity and default edit diagnostics

2e1c0eb labels direction-local indices explicitly and adds raw node pin-array indices and pin GUIDs. It also exposes hidden/orphaned/read-only/ignored/connected-default flags. Implemented, unbuilt_untested. X-61: no longer presents direction-local numbering as an unspecified index. This does not promise every native tool uses the raw array order; use named pins or that tool's documented selector. Deferred: mixed input/output ordering, null slots and reconstructed pins; flags match live pin state.

## Current review

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked done; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
