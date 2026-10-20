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

## Copy spacing preservation

FIELD-FIX-06 restores exact integer source positions plus the common copy offset after the spawn helper snaps nodes independently. Planning and apply refuse coordinates that would overflow the offset. Implementation done; unbuilt and untested. Deferred evidence: off-grid block relative spacing and coordinate limits. Destination overlap avoidance remains unresolved under FIELD-06.

## Copy anchor refusal

FIELD-FIX-06 implemented, unbuilt and untested: nonempty anchors refuse before mutation in preview and apply. Deferred evidence: anchor request leaves target unchanged; empty-anchor copy retains existing behavior. Feature support remains FIELD-05 work.

## Copied node result identities

Copy results now include explicit source-to-target canonical long node IDs, retaining object names as supplementary text. Follow-up tools no longer need to infer target IDs from UObject names. Implementation done; unbuilt and untested. Deferred evidence: returned target IDs resolve to the copied nodes in the destination graph.

## Selection identity integrity

The shared structural selection reader now rejects multiple matching nodes and repeated references to the same node, including short/long aliases. This prevents duplicate creation followed by overwritten copy-map entries. Implementation done; unbuilt and untested. Deferred evidence: repeated IDs and ambiguous IDs refuse without mutation, while unique selections retain order.

## Protected repoint source deletion

FIELD-FIX-19 checks CanUserDeleteNode in planning and before apply mutation when keepSource=false. Protected sources refuse and advise keepSource=true. Implementation done; unbuilt and untested. Deferred evidence: protected source deletion refuses unchanged; retaining source and deleting ordinary eligible sources retain expected behavior.

## Repoint stale reader counts

FIELD-FIX-19 refuses before mutation when the source output reader count differs from its plan. Existing per-reader identity checks still apply. This prevents newly added consumers being silently excluded when retaining the source. Implementation done; unbuilt and untested. Deferred evidence: added/removed readers between plan and apply refuse unchanged; unchanged reader sets repoint normally.

## Repoint undo ownership

Standalone repoint validates transaction availability and captures its owned undo ID before mutation. Failure uses that original identity rather than adopting the active record at failure time. Caller-owned batch rollback remains caller-owned. Implementation done; unbuilt and untested. Deferred evidence: undo unavailable refuses unchanged; forced failure restores only its owned transaction.

## Repoint final count rollback

Final node-count readback now runs before standalone commit. A mismatch invokes owned rollback instead of reporting an unknown committed edit without attempting restoration. Caller-owned batch recovery remains external. Implementation done; unbuilt and untested. Deferred evidence: unexpected node-count change triggers restoration; successful repoint commits once.

## Ambiguous canonical ID hints

NodeFind no longer picks the last canonical ID when multiple graph node types share a display title. It omits that ambiguous hint while retaining search results. Implementation done; unbuilt and untested. Deferred evidence: repeated identical IDs retain hints, conflicting IDs suppress them. This is not confirmation or closure of the ED-08 generic equality-node diagnosis.

## Exact node ID precedence

Probe and spawn resolution now prefer exact type IDs before considering legacy aliases. Unrelated legacy alias collisions no longer make an exact ID ambiguous. Legacy-only ambiguous requests still refuse. Implementation done; unbuilt and untested. Deferred evidence: exact/legacy collision and ambiguous legacy-only controls.

## Pin discovery exact membership

Pin discovery validates exact IDs against the full collected type inventory rather than the top-ten ranked suggestions. Unknown IDs still return suggestions. Implementation done; unbuilt and untested. Deferred evidence: a valid ID remains accepted regardless of suggestion ranking or truncation.

## Structural copy containment

Stock SpawnNodeFromTemplate invokes MarkBlueprintAsStructurallyModified for structural K2 nodes (including custom events). Planning and apply now refuse these nodes and non-K2 nodes before mutation, avoiding that implicit compile-inside-undo path. Implementation done; unbuilt and untested. Safe structural-node copying remains unsupported; this does not close FIELD-02 Widget-loss causality. Deferred evidence: structural nodes refuse unchanged; ordinary K2 nodes retain copy behavior.

## Scope accounting correction — node type follow-ups

Commits f46efa6, 6c20991 and d62d639 modified TurboMCPNodeType.cpp while claiming field-nodefind-words, which covered NodeResolve and the toolset header instead. This was a scope-recording error; the later assignment does not retroactively satisfy that gate. The integrator inspected these bounded changes and retained them under the user-authorized fix pass; they remain unbuilt and untested. Future NodeType changes must use field-node-type-resolution.

## NodeResolve bounded readable output

The pin counter now increments so the advertised 40-pin cap is effective, with remaining count excluding null entries. String/text default values escape backslashes and line separators to preserve one pin per report line. Implementation done; unbuilt and untested. Deferred evidence: more than 40 pins, null slots and multiline defaults.

## Concrete pin type visibility

NodeResolve now reports subtype object paths, reference/const qualifiers and map-value category/subcategory/object paths. This removes category-only ambiguity for object, enum, struct and map pins. Implementation done; unbuilt and untested. Deferred evidence: reflected object/enum/struct and map pins agree with editor signatures.

## Search result asset identity

NodeFind results and NodeResolve ambiguity suggestions now emit long node IDs containing the asset path. This avoids returning another ambiguous short ID when different assets share a name. Implementation done; unbuilt and untested. Deferred evidence: same-named assets in different folders resolve via returned IDs.


### Blueprint type eligibility

338c78a rejects loaded enum and class types that stock IsAllowableBlueprintVariableType excludes, before returning a usable pin type. FIELD-FIX-17 remains implemented and unbuilt_untested; FIELD-09 generic operator inference remains open. Deferred verification: allowed user enums/classes and all four reference forms still resolve; non-Blueprint enums/classes refuse without graph mutation. No tests or builds run.


### Signature name bounds

888f221 rejects names at or above NAME_SIZE and the case-insensitive None sentinel before FName construction for function and signature pin names. This guards the engine fatal length path and avoids empty-name authoring. Implemented; unbuilt_untested. Deferred checks: oversized ASCII names refuse without mutation; ordinary names remain accepted; None/none refuse. No test/build was run. This is not a diagnosis of the reported DataTable crash.


### DataTable name bounds

afd3d63 rejects oversized row/field identifiers before planning FName conversion and checks operation/removal identifiers again before apply. Refusal precedes mutation and enters the plan refusal list. Implemented; unbuilt_untested. Deferred evidence: oversized set/remove row and field identifiers refuse with unchanged rows, while normal names work. Original field crash remains unproven; this guard is not a root-cause claim. No tests/builds run.


### DataTable empty sets and failure stop

535280a rejects separator-only/empty set operations in Parse and Plan; they can no longer silently create default rows. Apply stops later removals/sets after the first failed mutation and checks row schema before invoking another AddRow callback. Existing owned rollback handles the failure. Implemented; unbuilt_untested. Deferred evidence: empty-set refusal preserves table, injected removal/addition failure skips later mutations and rolls back; ordinary multirow edits remain valid. Original reported crash remains open. No tests/builds run.


### DataTable reusable outputs

6520c0f resets parse/plan/report outputs; parsing publishes operations only after complete success. Replanning preserves aliased input but replaces the prior plan. Missing table/schema records a refusal as well as returning an error. Current MCP wrapper already uses fresh locals; this closes helper-level stale/partial output behavior, not a reproduced field crash. Implemented, unbuilt_untested. Deferred evidence: sequential reuse, late parse error, aliased replan and early apply refusal cannot expose prior operations or success reports. No tests/builds run.


### Tidy rollback result and dirty baseline

12b65cc captures dirty state and node count before Modify or node creation. Rollback now requires matching wire length, restored consumer links/positions and node count before reporting unchanged or restoring a clean flag. Incomplete recovery reports partial and retains dirty state. Implemented, unbuilt_untested; visual layout acceptance remains open. Deferred evidence: clean/dirty starting assets and forced rollback failures report accurately. No tests/builds run.


### Getter destination and preview score

bbd54b9 positions split getter copies beside the consumer planned destination rather than its old coordinates. Planned wire length substitutes the copy location for each rewired consumer instead of continuing to score the original getter connection. Implemented; unbuilt_untested. Deferred evidence: a moved multi-consumer getter graph places copies near moved consumers and preview accounts for split links. Rendered overlap and skipped-copy behavior remain unverified; FIELD-06 stays open. No tests/builds run.


### Getter-copy collision placement

c1d4447 reserves planned non-comment node rectangles and previously placed getter copies. New copies shift below intersecting rectangles without moving existing nodes; positions round before collision calculation and refuse coordinates outside int32 limits. Implemented, unbuilt_untested. Deferred evidence: multiple getter copies and occupied consumer-side space avoid node intersections. Comment containment, runtime copy geometry and overall layout remain open; this does not certify overlap-free Tidy. No tests/builds run.


### Getter split failure handling

e6615fb revalidates planned getter/consumer membership and reciprocal links before mutation. Copy creation/type/link failures stop later changes and enter rollback even under Force. No silent skipped splits are reported as successful layout. Implemented, unbuilt_untested. Deferred evidence: stale pin plans refuse, failed allocation/type/link outcomes restore prior graph, Force does not bypass failure. No tests/builds run.


### Reroute reader endpoint guard

34c545e checks the first reroute link for null, input direction, matching category and reciprocal membership before dereferencing/following it. A broken endpoint leaves the reroute in the reader path instead of crashing or silently following an invalid edge. Implemented; unbuilt_untested. Deferred evidence: malformed/null reroute links do not crash; valid exec reroutes still traverse. Separate from X-39 continuation diagnosis, which remains open. No tests/builds run.


### Reader branch depth and truncation

49421e1 bounds recursive execution-branch rendering at depth 64 and emits an explicit incomplete-dump error. The 2000-node linear limit now warns only if traversal actually attempts another node, avoiding a false error at exact completion. Empty branch continuations do not trigger depth warnings. Implemented; unbuilt_untested. Deferred evidence: deeply nested branches terminate with a clear warning, exactly 2000 linear nodes complete, longer chains report truncation. No tests/builds run.


### Reader error write-back gate

e07223a makes GraphCheck refuse standalone reader error comment lines before asset resolution, with the source line number. Append and other writers using this gate cannot silently discard an explicit malformed/truncated-reader diagnostic and treat its dump as complete. Implemented, unbuilt_untested. Scope is reader error markers only, not all forms of incomplete or unsupported DSL. Deferred evidence: valid comments/string literals remain accepted; indented reader error comments refuse before mutation. No tests/builds run.


### Whole graph text completeness

59004c8 scans graph text for balanced parentheses and closed double-quoted values before asset loading. Escapes, comments and the top-level read envelope are handled; unexpected closing delimiters include line/column. Reader error markers inside quoted content are not treated as diagnostics. Implemented, unbuilt_untested. Deferred evidence: truncated forms/strings refuse without writes, balanced reader output and quoted delimiters remain usable. This is structural completeness, not full semantic round-trip support. No tests/builds run.


### Function graph latent-node validation

235b961 replaces the graph-class-name heuristic with Blueprint FunctionGraphs membership. Stock Unreal creates ordinary function graphs as UEdGraph, so the prior check could bypass the existing IsLatentForMacros refusal. Implemented; unbuilt_untested. Deferred: ordinary function rejects latent nodes, event graph remains permitted, non-latent function nodes remain accepted. This is a source-level preflight correction, not proof of crash resolution or full graph round-trip correctness. No tests or builds run.


### Ambiguous graph validation target

9a1d039 makes GraphCheck use the shared FindGraphByName resolver and refuse multiple distinct matches instead of checking an arbitrary first graph. The append writer already used this resolver; this closes the preflight inconsistency, not a proven wrong-target mutation. Implemented; unbuilt_untested. Deferred: duplicate graph names refuse, a graph referenced in multiple arrays counts once, and unique names still validate. No tests/builds run.


### X-37 object pin values

Reconciled the existing cbe1585 implementation: NodeResolve prints unlinked DefaultObject values as object:<full path>, with linked pins retaining link counts. X-37 implementation is done, unbuilt_untested. Deferred evidence: an assigned object pin shows its correct path and a connected pin reports its connections. No tests/builds run; no new runtime claim.


### FIELD-FIX-21 summary completeness

aa15bdb: verify function inventory with empty/missing entry nodes, unavailable reflection labels, graph kind and event owner labels. Implemented, unbuilt_untested. BL-13 reproduction remains unresolved. No tests/builds run.


### FIELD-FIX-22
Toolset help pagination, offset refusal, deterministic ordering, description budget and unchanged full help require later verification. Done, unbuilt_untested. No tests/builds run.


### FIELD-FIX-23
9c20aac: concise acting checkout header/footer and corrected successful-save conditions. Verify failure with clean package, successful widget save, no-provider and checkout errors later. Implemented, unbuilt_untested; no tests/builds run.


### FIELD-FIX-24
c05d159: status version/compile stamp and PlayCheck execution labels. Verify on a later build and all discovery surfaces. Implemented, unbuilt_untested. No revision provenance or toolset migration claim.


### Pin readout identifiers and flags
2e1c0eb: verify direction-local vs raw pin-array indices, GUIDs, null slots, and default editing flags. Implemented, unbuilt_untested; no tests/builds.


### FIELD-FIX-25
ee12e2d object/interface type parsing: verify generated widget class paths, native object classes, Blueprint interfaces, invalid wrapper/class combinations and container types. Done, unbuilt_untested. No tests/builds run.


### FIELD-FIX-26
Default-on recorder: verify fresh config, persisted false and manual toggle later. Implemented, unbuilt_untested. No tests/builds run.
