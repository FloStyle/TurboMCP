# Deferred field verification ledger (not a product task)

Legacy reference: FIELD-REG-01 (not a task ID)
Verification: deferred by owner
Owner: Codex
Design: docs/design/mutations.md
Kind: evidence-ledger
Review: source-reviewed

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope
Build the stock-engine plugin, then verify DataTable edits with editor open and closed; Widget compile/save preservation; graph append/patch/replace failure rollback and single undo; validated pin writes; layout and stable node identities.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain deferred in this evidence ledger. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
None. Pending; no tests or build run for the latest source batches.

## Dependencies

FIELD-FIX-01 source implementation; owner authorization to resume testing.

## Verification

Deferred by owner. See FIELD-REG-01; implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence and a separate pending regression record.

## Repoint regression pending

Verify failure after the first rewired reader restores all original links, failed target creation leaves no node, batch failure rolls back earlier operations, one successful batch needs one undo, compile/save are not run, and ambiguous graph names refuse. Source aad28b7 is unbuilt and untested.

## Call Swap regression pending

Verify rejected migration restores links/nodes without undoing previous work; final link and node count verification; no implicit compile; replacement retains position; overflow and ambiguous graph refusal. Source 019a1c05d6e046809a3e8b8ef170af3453164a0f is unbuilt and untested.

## Swap defaults regression pending

FIELD-FIX-04: string, numeric, object and localized-text preservation; incompatible/split pin refusal; callback readback; rollback after default transfer and link failure. No build/tests run.

## Pin-default rollback regression pending

FIELD-FIX-05: callback reconstruction, title-changing values, duplicate pin refusal, restoration on readback failure, failed undo reports partial, and prior user undo history remains untouched. Unbuilt and untested.

## Graph Copy follow-up

FIELD-FIX-06: internal and crossing links, refused destructive/conversion connections, failure rollback, node count, previous undo history, and no implicit compile. Unbuilt and untested.

## Variable-add field-note follow-up

FIELD-FIX-07: invalid later type must add zero variables; duplicate/case names; transaction/PIE refusal; dependent Widget survival and actual partial engine refusal. Unbuilt and untested.

## Field-note diagnostic follow-up

FIELD-FIX-08: live/dead exec chains sharing data, function results, macro tunnels and capped diagnostics. No build/tests run.

## Append reachability follow-up

FIELD-FIX-09: disconnected new impure chain with shared entry data refuses and restores; live chain succeeds; pre-existing dead nodes/pure nodes excluded; macro/tunnel roots; Replace composition. Unbuilt and untested.

## BL-12 follow-up

FIELD-FIX-10: local float/string/struct defaults, unset values, newline escaping and graph DSL comment handling. Unbuilt and untested.

## GL-15 follow-up

FIELD-FIX-11: two branch outputs into one exec input, reroutes, invalid endpoints, and multi-source data refusal; confirm partial evidence survives output. Unbuilt/untested.

## NF-08 NodeFind word search — deferred evidence

Implementation closed in FIELD-FIX-12. Unbuilt and untested. Later check spaces, tabs/newlines, commas/pipes, empty terms, AND matching and reported parsed terms. This is verification evidence debt, not a product task.

## OV-03 override origins — deferred evidence

FIELD-FIX-13 implementation closed, unbuilt and untested. Later check local functions excluded from counts, genuine parent overrides, direct and inherited interface declarations, filters and zero-results wording.

## BatchEdit rollback lifetime — deferred evidence

FIELD-FIX-14 source implementation closed, unbuilt and untested. Later check failure before first write, failure after an applied operation, exact owned undo, preservation of previous user undo history, and successful single undo. Native script rollback and Widget loss remain separate open problems.

## DataTable prepared rows — deferred evidence

FIELD-FIX-15 implemented, unbuilt and untested. Later check native row structs with strings/arrays/object references, existing and new rows, editor-open callbacks, rollback and read-back. Work-machine crash cause remains open.

Source clarification: stock UTransBuffer::End guards ActiveCount >= 1, so the second BatchEdit end was normally a no-op, not a proven crash. FIELD-FIX-14 removes redundant finalization and strengthens exact transaction ownership; it is not crash reproduction evidence.

## FunctionAdd pin lifetime — deferred evidence

FIELD-FIX-16 implemented, unbuilt and untested. Later check an unresolved call becoming resolvable through function creation, reconstruction replacing pins, preserved reciprocal links, and genuinely missing endpoint diagnostics.

## Signature preflight - deferred evidence

FIELD-FIX-17 implemented, unbuilt and untested. Later check supported array/set/map types, nested containers, unhashable keys, fresh output types, case aliases and cross-direction parameter collisions. Refused signatures must not mutate assets.

## Reader IDs in targeted edits - deferred evidence

FIELD-FIX-18 implemented, unbuilt and untested. Later check NodeFind short IDs and long IDs through Swap/PinDefault, missing IDs, duplicate GUIDs, custom-node class refusal and wrong-asset IDs. No fuzzy matching is authorized.

## Repoint replacement - deferred evidence

FIELD-FIX-19 implemented, unbuilt and untested. Later check scalar consumer rewiring, exact reader IDs, source-target alias, stale links, conversion refusal, reciprocal read-back and owned rollback.

FIELD-FIX-19 source-removal follow-up: later check connected exec pins, other outputs and incoming data links, keepSource=true, and callbacks adding readers before removal. No regression run yet.

## Tidy undo - deferred evidence

FIELD-FIX-20 implemented, unbuilt and untested. Later check one undo restores node positions, comment sizes and getter split links; no-op and worse-layout rollback; PIE/nested refusal. This does not prove overlap-free layout.

FIELD-FIX-20 drift follow-up: later repeat forced Tidy on a boxless graph and selected group with fixed neighbours; compare positions on the second pass and ensure fixed boundaries remain respected.

Tidy getter preservation (c2e4894): deferred evidence for external-target, validated, split and ordinary getter cases; confirm original semantics and undo. This is verification debt attached to FIELD-FIX-20, not a new product task.

Graph Copy defaults (302c405, FIELD-FIX-06): deferred evidence for literal/object/localized-text defaults, pin shape mismatch refusal and rollback, and values after wiring. No test execution; not a new product task.

Exact type preservation (0a5e7d0, FIELD-FIX-17): deferred checks for allowed/refused structs, float versus double/real, and container/signature propagation. Verification debt only; no new product task.

FIELD-FIX-06 external static library calls: cross-asset copy, retained function owner, defaults and links remain untested. Self-call missing-member refusal still requires later evidence.

FIELD-FIX-17 signature description: const-reference, mutable-reference, ordinary output and return directions await verification; implementation follows the stock K2 pin rule.

FIELD-FIX-06: Graph Copy undo-unavailable refusal and captured-record rollback remain untested; no verification task created.

FIELD-FIX-19: Repoint duplicate pin-name refusal and unique controls remain untested. This is evidence debt attached to the implemented fix.

FIELD-FIX-15 removal failure: removal-only and mixed batches, callback re-addition and owned rollback remain untested. Not a separate task.

FIELD-FIX-15: original DataTable undo identity, unavailable undo refusal, and changed/nested transaction failure paths remain untested.

FIELD-FIX-19 keepSource: supported boolean spellings, whitespace and invalid-input refusal remain untested.

FIELD-FIX-16: function signature pin failure and cleanup read-back, plus successful input/output creation, remain untested.

FIELD-FIX-16 targeted refresh: newly resolved self-call pins/links, external same-name call preservation, unrelated graphs and transaction refusal remain untested.

FIELD-FIX-16 removal: successful and failed deletion, duplicate-name refusal and inherited-name diagnostics remain untested.

FIELD-FIX-16 partial outcome classification and successful-result distinction remain untested.

FIELD-FIX-03 unique Call Swap pin matching: duplicate name cases and ordinary default/link transfer remain untested.

FIELD-FIX-06 copy link endpoint ambiguity refusal and rollback remain untested.

## Graph Copy directional endpoints — f2baa7a

FIELD-FIX-06 implementation done; unbuilt and untested by owner request. Deferred evidence: copy a block with same-named input/output pins and confirm exact links; duplicate same-direction endpoints must refuse with restoration. This is a verification flag on the existing fix, not a new product task.

## Copy boundary across graphs

FIELD-FIX-06: planner now classifies an external endpoint against the destination graph, not merely the Blueprint asset. A wire to a node left in the source graph is reported as unlinked instead of causing apply-time rollback. Implementation done; unbuilt and untested. Deferred verification: partial-block copy between two graphs of one Blueprint, checking reported boundary wires and preserved internal links.

## Duplicate internal copy wires

FIELD-FIX-06: reciprocal internal wires are planned once from the output side. Previously the second replay could receive a break-existing connection response and roll back an otherwise valid copy. Implementation done; unbuilt and untested. Deferred evidence: connected exec and data blocks copy successfully with exact wire counts; conflicting connections must still refuse.

## Destination copy eligibility

FIELD-FIX-06 now checks CanDuplicateNode and CanPasteHere during planning and before the apply transaction. Ineligible graph nodes refuse before mutation instead of bypassing Unreal copy/paste restrictions. Implementation done; unbuilt and untested. Deferred evidence: eligible ordinary calls still copy; nonduplicable nodes and destination-incompatible calls refuse without mutation.
