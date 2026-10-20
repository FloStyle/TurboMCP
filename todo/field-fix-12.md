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
