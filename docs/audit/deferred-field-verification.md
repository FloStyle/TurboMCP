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


### Pin default refusal diagnostics

fb328d9 replaces the combined refusal with separate missing-input, orphaned, execution, connected, read-only and ignored-default reasons. Schema validation errors include pin name and category/subcategory. Implemented, unbuilt_untested. This improves the TurboMCP replacement diagnostics; it does not add arbitrary struct literal or by-reference support to native tools. Deferred: each refusal reason and unchanged asset state; valid default edits unchanged. No tests/builds run.


### FIELD-FIX-27
54b7ed9: verify standard member getter/setter canonical IDs versus reader, boolean prefixes, spaces, widget members, categories, locale and legacy aliases. Done, unbuilt_untested. Full DSL whitespace handling not claimed.


### FIELD-FIX-28
4c4a38e: verify schema-valid empty defaults, invalid empty defaults and split parent/child pin linting. Done, unbuilt_untested. LT-07 remains unresolved; no tests/builds run.

## 2026-10-21 — Reference direction and cast lint (`22e4545`)

- 484d5638: implemented, unbuilt/untested. Check explicit both against the union of in/out and empty direction on saved known dependencies across accepted path forms.
- 7bbe9c3c: implemented, unbuilt/untested. Check typed result consumers, boolean and success/failure execution consumers, incomplete cast nodes and truly unused outputs. No input-type/nullability redundancy claim or deletion recommendation.

## 2026-10-21 — Widget references and enum creation

- 1256778b: implemented, unbuilt/untested. Check changed editable object, soft/class/interface references against class defaults, including null, without placement-slot noise.
- 214d6e7a: implemented, unbuilt/untested. Check qualified names/value/order/count, display-name refusals, comma/pipe input, case-only duplicates and reused plans. Failed read-back must report retained unsaved partial state.

## 2026-10-21 — Trace locations and loaded struct readers (`73befe8`)

- 02674939: unbuilt/untested. Exercise different armed nodes in one function; verify exact opcode mapping, missing debug data and arm lifetime separately. Arm-loss root cause remains unresolved.
- 7bd81b84: unbuilt/untested. Make/break-only usage, split pins, map value structs, member and DataTable readers, one asset listing per Blueprint; verify force count changes. Unloaded/transitive dependencies are outside this direct loaded scan.

## 2026-10-21 — Execution fan-out (`4948232`)

Implemented slice, unbuilt/untested: enumerate ordinary/entry execution endpoints and diagnose unsupported fan-out. Verify branch joins, malformed links, multiple outputs, and reader-error refusal on write-back. Broader d3897f07 remains open for knot/helper coverage.

## 2026-10-21 — Reroute traversal (`cb782dc`)

Untested: execution reroute fan-out, long chains, cycles/shared reroutes, malformed reciprocal edges, normal linear chains and upstream data reroutes. Full-mode knot identity remains open under d3897f07. No tests/builds run.

## 2026-10-21 — Variable names (`4dfe394`)

VA-04 implemented / untested. Verify digit-leading variables, case-only duplicates, overlength input, and collisions with functions/graphs/components before mutation. Existing alphanumeric/underscore grammar otherwise retained. No tests/builds.

## New reports 2026-10-20 S5 / 2026-10-21 S1 (`05cedc3`)

Untested fixes: short node IDs canonicalized before patch planning with ambiguous IDs refused; early refusal no longer reports a measured zero-node graph. Check short/full asset paths and IDs on Widget Blueprints. Status/index now give call_tool JSON and native safety-filter replacements. Native dispatcher error unchanged (X-68 open); strict conversion reporting/custom-node swap ED-11/FT-67 remain open. Positive DT-09, GC-17 and RF-08 are scoped field evidence, not blanket acceptance.

## 2026-10-21 — Strict append conversions (`3470fc2`)

Implemented / untested: refuse CanCreateConnection conversion-node responses before TryCreateConnection and roll back the graph write, reporting rollback failure separately. Check int/bool mismatch, explicit conversion nodes, matching pins, wildcard specialization and rollback. ED-11 remains open because native connector behavior is unchanged.

## 2026-10-21 S2 (`e2ed536`)

DT-10 and FT-69 implemented / untested: verify explicit 0/False old field values and old/new endpoint movement reports after execution insertion. Added null-continuation refusal before schema connection. GC-18 reopens broader patch resolution despite earlier short-key fix; GC-19 operator IDs and X-69 unexplained normal shutdown remain open. GC-20 is scoped positive field evidence only. Exact field build unknown; no tests/builds run.

## 2026-10-21 — Full node-ID regression (`31cadfe`)

Implemented / untested: 05cedc3 incorrectly passed full IDs into a hex-key helper. Now parse identity first, verify asset/graph, then match key; comment parser accepts bare keys and quoted graph names. Check full/short asset IDs, bare keys, ambiguity, cross-graph refusal, quoted names and local labels. GC-18 remains open for execution-link patch behavior; no tests/builds run.

## 2026-10-21 — Patch link omission

Implemented / untested: explicit refusal of link-bearing Patch requests before mutation; avoids partial literal-only success for mixed requests. Unique non-orphaned input lookup prevents output/input same-name confusion. Verify link-only, mixed, literal-only and duplicate pin names. GC-18 remains open: native link editing via Patch is not implemented.

## Operator keyword discovery — 33f44d8

Implemented, untested: node search now includes Unreal action-menu keywords at lower priority than IDs/titles. No tests/build/editor run by user instruction. GC-19 stays open: writer resolution and the reported gameplay-tag operator identity are not proven fixed.

## Call-cost cap validation — 9951aaf

019cf7af implemented, Untested. Strict digits, positive value, overflow-safe saturation at 50, validation before empty-log handling, truthful truncation guidance. No tests/build/editor run. Deferred coverage: empty/populated logs, malformed/fractional/zero/huge caps; totals unchanged.

## Node search coverage — c6e173a

Implemented, Untested: recursive engine graph enumeration plus editor event graphs, pointer deduplication. Resolver output no longer promises native writer compatibility for all IDs. ca5bbbfc remains open for other readers/named graph resolution. No tests/build/editor run.

## Nested graph resolution — 2b16d12

Implemented, Untested: shared graph-name resolver enumerates engine child graphs and editor event graphs, deduplicates pointers, and refuses ambiguous lookup when caller cannot inspect a match count. Broad ca5bbbfc remains open for other readers. No tests/build/editor run. Deferred checks include duplicate nested names and targeted edits to unique nested graphs.

## Blueprint reader graph inventory — 7f85a91

Implemented, Untested: BlueprintRead includes recursive engine graph inventory and editor event graphs, deduplicates graphs, and sorts equal names by object path. No tests/build/editor run. The broad coverage investigation remains open pending reconciliation of other consumers; no claim of whole-plugin coverage.

## Struct removal count and warnings — a10e537

Implemented, Untested: strict 0..2147483647 force count; warning reports potential dependency impact rather than proven breakage. No tests/build/editor run. Broader 1f5fe672 remains open for dirty-set reporting.

## Reader numeric limits — 4ad7b9b

Implemented, Untested: DirtyList limit and BlueprintLoad character budget reject fractional, malformed, negative, zero and overflowing values; empty preserves defaults. Follows the numeric input defect recorded in 019cf7af. No tests/build/editor run.

## Focused Widget Blueprint reads — 50f36f5

Implemented, Untested: suppress unrelated widget trees for graph-filtered reads, respect effective reduced detail and available character budget for tree inclusion, and count reader errors as refused assets. Broader 0e4658ad remains open for help/query semantics. No tests/build/editor run.

## Second-pass tracking policy

Implementation follow-ups are tracked in field-testing-2026-10-20.json second_pass and remain Open; they are not closed merely because a mitigation landed. The list highlights known partial submissions and is not an exhaustive defect inventory. Verification-only follow-up remains an Untested flag on implemented work, not a new task. Earlier chronological entries describe the state at their commit and may be superseded by later entries.

## Asset filter diagnostics — a8c38d7

Implemented, Untested: refuse unsupported query prefixes and empty type/path filters instead of misleading no-match or broadened results. 0e4658ad stays open for parameter examples. No tests/build/editor run.

## Tool help requiredness — 4a03957

Implemented, Untested: required/optional labels come directly from inputSchema.required. Broader 0e4658ad remains open for examples and accepted-value details. No tests/build/editor run.

## Published help constraints — 4204098

Implemented, Untested: stable parameter ordering and verbatim JSON schema enums/constants/defaults/bounds/patterns. 0e4658ad remains open for tool-specific examples and unpublished constraints. No tests/build/editor run.

## Compact checkout success — 4a3c3cd

1db73082 implemented, Untested. Compact successful attempted checkout; provider info retained, no-lock providers identified, save independence explicit. Failures and skipped operations remain detailed. No tests/build/editor run.

## Typed discovery defaults — 2a0d153

Implemented, Untested: preserve JSON default representation, including false, zero, null, arrays/objects and explicit empty strings. 0e4658ad remains partial. No tests/build/editor run.

## 2026-10-21 — X-70 build regression

Source fix: `9f0ba2e`. Tool-help sorting now explicitly converts JSON keys into FString array entries, supporting FString and FSharedString storage without incompatible map assignment. Reported C2440 implementation closed; **unbuilt / untested**. No build, tests, or editor run requested. Full compilation remains unverified; report captured only the first compiler error. No engine modifications.

## 2026-10-21 S4 — capture and layout triage

`b706a0e`: CP-06 capture framing now uses actual graph-panel geometry, top-left view offset, quantized zoom and matching render scale. LR-10 below-left getters accepted by report and backward repair. Both implemented, unbuilt/untested. FB-06 partially addressed with overlap findings and explicit limits; remains Open for crossing assessment. No tests/build/editor run. TD-12, LR-09 and NT-06 remain open. X-71/GC-21 are feature limitations linked to FT-71/FT-70. GC-22, DD-05 and ED-12 are positive field observations, not blanket certification. All 13 report IDs retained.

## 2026-10-21 S4 — remaining implementation batch

Source `b71b89c`, unbuilt/untested; no test commands or editor runs. TD-12 single-strip regression addressed with execution-component rows both inside/outside boxes, width-based gaps, below-left pure placement. LR-09 shares pin geometry and reroute segment counting. FB-06 reports overlaps/crossings with approximation and cap disclosed. GC-21/FT-70 adds explicit call swap mapping; X-71/FT-71 adds batch positions. Earlier CP-06/LR-10 remain implemented/untested. NT-06/GC-19 remains OPEN: aliases/search strengthened, but exact failed identities/context unavailable. Suggested arbitrary subsystem duplication and automatic delegate routing are not implemented; safe variable getter copying remains.

### New call shapes

- `EditTurboMCPGraphPositions(Asset, GraphName, Positions)` where Positions is JSON `[{"node":"exact reader ID","x":100,"y":200}]`. All entries validated before mutation; one undo; no compile/save.
- Preview/EditTurboMCPGraphSwapCall retains its existing arguments and adds optional `Options`: `{"pin_map":{"OldInput":"NewInput"},"new_inputs":{"ExtraInput":{"node":"exact reader ID","pin":"Value"}}}`. Explicit matching only; no implicit conversions. Existing calls with empty Options preserve same-name behavior.
- Reflected function paths and `Function|` paths now resolve for filtered function actions; exact aliases `Array Length` and `String Append` map to their engine library functions. Build-string and integer-point conversion search includes reflected names/types. This is not evidence that the unidentified field refusal is fully repaired.


## S5 urgent source repairs — 2026-10-21

Source: 2dfc786. No build, tests or editor execution, by owner request. Stock engine unchanged; DSH paused.

Implemented: operator reader IDs; enum literals; reroute metrics; grouped tidy membership/refusal; related rows, exec lanes and pure alignment; capture quantized fit, gamma and aspect; comment ID and named fit; mapped defaults in swap preview; safe latest-transaction undo; explicitly authorized preview/confirmed disk revert; before/after SVG schematic.

Field positives apply to fac37b3, not this repair: dispatcher guidance, library aliases, call-swap map/defaults, batch positions, getter report. Undo was not attempted.

Limits remain explicit in second_pass: SVG is schematic and omits getter copies; comment scopes retain fixed box corners rather than translating groups; semantic main-path/global delegate routing remains heuristic. Undo refuses intervening edits. Revert discards all unsaved changes to the package, may reset undo and reinstance references. Separate preview and single-use confirmation expire after 60 seconds.


## S4/S5 completion pass — 46de58c

Supersedes the three open implementation limits in the preceding S5 entry:
- FT-74: native before/after PNG previews now include planned getter copies. A transient Blueprint shell owns a duplicated graph; the entire Blueprint is not duplicated, avoiding PostDuplicateBlueprint compilation/reinstancing. No live edit is used to make the image.
- FT-75: PreviewTurboMCPGraphGroupMove / EditTurboMCPGraphGroupMove translate selected comment groups, nested boxes and member nodes together. DeltaX/DeltaY are integer offsets. Preflight rejects new collisions or changed external box membership. One undo transaction, no save or compile.
- FT-76/TD-15: native pin geometry guides alignment, pure expression chains get reserved horizontal space, delegate-weighted row ordering evaluates all row relationships, loop completion returns to the main lane. Optional comma-separated MainPath reader IDs choose a connected main execution path explicitly. Ordinary tidy defaults remain deterministic. Fix-backward remains conservative and reports collision/geometry limitations.

All implemented, unbuilt and untested. No regression or field-success claim. The original S4 file is no longer at its supplied Desktop path; reconciliation used its preserved tracker findings and the S5 report's detailed corrections and evidence. General scratch-asset deletion and arbitrary side-effecting node duplication remain outside these defect repairs; neither is silently implemented or claimed.


## 2026-10-21 S6 repair batch — 3972ea9

TD-17/18/19, LR-13, CP-09/10 and GC-27 implemented by source inspection only. No builds/tests/editor runs. Verify full-detail overlap handling at multiple zooms, populated correctly coloured native previews, region/node captures, mainPath auto and array-form repoints in the next field session. These are validation flags on the fixes, not new issue tasks. FT-78 Sequence refactor, FT-80 trivial-value splitting and the unfinished configurable-gap/all-layout-tools portion of FT-81 remain open features. Positive TD-16/FB-07/X-74 evidence applies to field revision 721450c only.


### S6 build follow-up: C4459
User reported a UBT compile failure caused by the namespace MeasuredSizes cache shadowing the Plan-local map. Renamed the cache to CachedAlignmentNodeSizes. Implemented; no build/tests run. Earlier S6 batch is not build-verified.


## S7 build blockers (X-75, X-76)
Shared JSON keys now convert through FString(*Key); LayoutReport retains the resolver mutable graph pointer; comment fitting initializes bounds and uses exclusive assignment branches. All reported compile diagnostics addressed in source. No build/tests run. 721450c remains the last field-confirmed compiling revision; later runtime behavior is not verified.


## S8 — X-77 missing header
Removed nonexistent Misc/LexFromString.h and restored capture own-header-first ordering. LexTryParseString is declared by the existing Containers/UnrealString.h via UnrealString.h.inl. Source inspected only; no build/tests run. S8 reports no recurrence of S7 diagnostics but no successful complete build. FT-82 full compile gate recorded as a proposed workflow change.


## S9 field confirmation and follow-up
703e002 compiled in the field. Images/colours, diagnostic pairs/overlaps, automatic main path and array batches confirmed. New source changes: independent lane cursors and sequence lane preservation, skip dispatcher pin alignment, explicit offset sign and batch preflight limitation. No new build/tests. TD-23 remains partial pending role-column/shared-pure/summary handling; GC-29 sequential planning and FT-83–86 remain open. Engine connector X-78 is not fixed by a plugin patch yet.


## S9 completion — 2a3bf44 (implemented, unbuilt/untested)

- GC-29: sequential transient batch planning; one live transaction remains.
- X-78 / GC-30 / FT-83: `PreviewTurboMCPGraphConnect` / `EditTurboMCPGraphConnect` replaces one existing data input, validates schema and exact single-source read-back. Native engine connector remains unchanged.
- FT-84: `PreviewTurboMCPGraphSequence` / `EditTurboMCPGraphSequence`, `Groups=[["reader ID 1","reader ID 2"],["reader ID 3","reader ID 4"]]`, `bSplitValues=true`. Only contiguous ordered synchronous single-path call groups; refuses latent/branch/cycle shapes. Continuation is the final output. Keeps call order, one undo, no compile/save.
- FT-85: `PreviewTurboMCPGraphSplitValue` / `EditTurboMCPGraphSplitValue` accepts NodeId; pure self/getter/literal whitelist only. Sequence optionally does this for its selected consumers in the same edit.
- TD-23 / FT-86: tidy uses shared stage columns for exclusive linear Sequence outputs, full-detail pin placement, expanded rows and final cross-row summary placement. Optional `HorizontalGap=120`, `VerticalGap=100`, `LanePitch=280`; bounds validated. Nonlinear/nested/merged outputs retain conservative general layout rather than semantic guesses.

Field-agent workflow: read IDs; preview Sequence with explicit groups (include summary last); edit the same groups; EditGraphTidy with MainPath=auto and chosen spacing; compile. Do not use the native pin connector for replacements. A lower call count and visual acceptance are targets enabled by implementation, not measured results. Runtime rollback, undo, saved persistence, full compilation and visuals remain unverified; no tests/builds run by instruction.

## S10 — 2026-10-21 (60fc383)

Implemented TD-24/TD-25 compact horizontal pure-input placement and lane-zero pin alignment, LR-15 intentional Sequence fan-out filtering, CP-11 requested-resolution native graph capture, and FT-87 read-only positions. All new changes are **unbuilt_untested**, not runtime-certified. No tests/build/editor runs per user instruction. Lane pitch is a minimum when measured outlines need clearance; shared impure sources remain single and can still have long wires. Capture retains native zoom limits and reports failure instead of clipping oversized frames.

S10 confirms c886c7a Sequence preview/edit agreement, nine ordered lanes, one undo, clean compile, no crash, and roughly 70 to 7 calls. Value-copy behavior, persistence, other refactors and new S10 repairs are not established by that evidence.

## S11 — dfffa92

Implemented native pin offsets, measured comment fit, explicit wire-length rejection explanation, empty-event review candidates and sparse-box findings. Unbuilt/untested; no tests run. S11 was on c886c7a; position reader FT-88 duplicates FT-87 already shipped in 60fc383. TD-26 coordinated box reflow and X-81 toolset hints remain open. FT-89 below-line shift remains a feature request. Empty overrides are never automatically deleted.

## S12 — 587efd2

X-83 own-header-first include order repaired. No local tests/build. Field evidence: d6ca123 compiled and linked on UE5.8.3/MSVC14.50 with this diagnostic; no editor load observed. This does not establish runtime correctness. Preventive source-convention enforcement remains tracked separately.

## P0 getter identity — 5621ddd

Partial repair: class-qualified reflected property IDs now shared by reader/discovery, legacy context-member preference, no foreign fallback for Variables short IDs. No tests/build/editor runs. Getter-owner group remains OPEN: newly exposed uncompiled widget variables, alternate alias resolution, result owner reporting and preview parity still need implementation review.

## P0 explicit continuation — f469303

GC-44 implemented, unbuilt/untested: explicitly named old targets no longer receive an additional implicit first-output edge. Final all-link read-back added. Multi-output implicit continuation refuses instead of guessing. Preview parity/disclosure and convenience continuation options remain OPEN; no tests/build/editor runs.

## P0 search and pin endpoints — 5d74cf1

AS-04/AS-05/AS-06 and NR-07 implemented, unbuilt_untested. Search now splits whitespace and accepts class: as type: alias. Node resolve exposes exact linked node/pin/direction. No tests/build/editor runs; graph-reader branch-output naming remains separate and open.

## P0 split result — 0ad705e

ED-16 implemented, unbuilt_untested: every created copy reports its full node ID and consumer node/pin. Preview IDs are explicitly provisional. Source review only; no tests, build or editor runs.

## P0 save outcome — 860db27

S1 SV-13 guidance implemented, unbuilt_untested. S2 SV-13/S3 SV-14 remain open: uncertain saves no longer claim no disk change; outer dispatch/log capture still requires inspection for hidden successful result text. No tests/build/editor runs.

## P0 exact exec connector — 7418c94

X-88/FT-91 plugin alternative implemented, unbuilt_untested. K2 schema source confirms BREAK_OTHERS_A replaces output when output is argument A. Both endpoints read back and other incoming target paths preserved. Scratch preview, owned transaction rollback retained. No tests/build/editor runs.

## P0 structural tidy gate — 7d577e5

Partial, unbuilt_untested: structural-first gate applied to preview and live rollback decision. Overlaps, backward exec and linear alignment precede wire length. Branch fan-out/merge offsets excluded from linear drift. Pure-input scoring, copy geometry and branch/merge placement remain open. No tests/build/editor runs.

## P0 comment fit — 89db15e

User selected outward expansion preserving existing contents. Explicit-member fit now unions old rectangle with requested enclosure and reports planned bounds. CB-05 remains partial for layout-report mapping and undo review. No tests/build/editor runs.

## P0 comment undo — 754d54a

CB-05/ED-20 implemented, unbuilt_untested: owned comment transaction, pre-mutation Modify and transactional creation. Earlier fixes supply text lookup, index/ID mapping and outward-only fit. Native undo remains chronological. No tests/build/editor runs; undo/redo behavior needs later verification.

## P0 same-Blueprint getter copy — fa91769

ED-19 implemented, unbuilt_untested. Duplicate existing getter pins/member identity instead of reallocating through spawn helpers; clear copied links, assign a new node GUID, retain default/type checks and rollback. Local variables cannot cross graph scope. No tests/build/editor runs. Later verify cross-function member getter copy, source links/defaults preservation and undo.

## P0 form diagnostics — 1a840ce

GC-35 implemented, unbuilt_untested. Tracks opening lines and comments containing ignored closing brackets; preserves complete-input refusal before mutation. No tests/build/editor runs. Later verify nested inline comments, quoted semicolons, balanced forms and multiline strings.

## P0 dirty package reporting

X-93 (October 22 S2) partial, unbuilt_untested. Registry edits snapshot dirty packages and append deltas on success/error without saving or discarding. Off-thread completion reports unavailable; already-dirty modifications are not detected. Root cause remains open. No tests/build/editor runs.

## P0 getter owner readback — 2a9d55e

Partial, unbuilt_untested. Both probe and live spawn compare resolved member declaration owner/name to the action property. Wrong/missing declarations remove the created node before wiring. Skeleton/generated owners normalize by Blueprint. Uncompiled widget discovery remains open. No tests/build/editor runs.

## P0 global callers — 141cd11

X-92 implemented, unbuilt_untested. Body filters no longer limit relations. Static call sites include full node IDs and upstream entry traversal; pure calls follow consumers. FT-96 remains partial for delegate/event binding context. No tests/build/editor runs.

## P0 comment refresh — 26a276d

CP-14 remains partial and unbuilt_untested. Comment edits now notify graph listeners; stock SGraphPanel registers OnGraphChanged. Existing tidy/positions already notify, so their reported rendering artifact remains unresolved. No tests/build/editor runs.

## P0 targeted removal — 4447f52

ED-18/FT-98 implemented, unbuilt_untested. New Preview/EditTurboMCPGraphRemove resolves all node/link targets before mutation on scratch then live, exact reciprocal removal, owned transaction and rollback. Non-deletable nodes refuse. No tests/build/editor runs. Verify undo/redo, unrelated links/defaults/positions and typed wildcard disconnections later.

## P0 append continuation — 06aa485

Partial, unbuilt_untested. Continuation=all traverses only newly created reachable exec nodes to unconnected exits after submitted links. label:pin selects a new node output. Failed resolution rolls back; explicit incoming target links take precedence. Preview states it does not yet dry-run exit resolution. Full parity/exact pre-disclosure remain open. No tests/build/editor runs.

## P0 graph pages — ac0ee60

GL-19 partial, unbuilt_untested. Offset/MaxChars/Revision retrieve complete rendered text; stale tokens refuse and partial pages cannot be written back standalone. Event/function entry filtering remains open. No tests/build/editor runs.

## P0 entry filter — 34109ae

GL-19 implemented, unbuilt_untested: entry-name substring filtering completes the pagination capability. Shared renderer preserves full default behavior; filtered views are explicitly partial and exclude unrelated disconnected inventory. No tests/build/editor runs. Later verify filtering with shared values/tails and revision paging across long Unicode output.

## P0 function input getters — 1ffb537

Partial, unbuilt_untested. Reflection-backed function inputs use stock scoped getter spawners and stable reader IDs. Automatic layout conversion and signatures not yet reflected remain open. No tests/build/editor runs. Verify parameter scope, reference inputs and nested graphs later.

## P0 unreflected input getters

GC-39 implemented, unbuilt_untested. Discovery/creation now use current entry pin declarations and local member scope without skeleton mutation or compile. Reader identity works before reflection. Automatic layout conversion remains open. No tests/build/editor runs; verify compilation of newly declared reference/container inputs later.

## P0 node execution context — f64432d

X-91/FT-95 implemented, unbuilt_untested. Text reader returns full static upstream context, entries, exact links/input defaults and bounded downstream hops. No screenshots or dynamic dispatch inference. No tests/build/editor runs. Verify loops, shared tails, pure call consumers and large graph output later. Append scratch parity remains open: apply currently resolves original asset identity and uses editor undo; a proper transient execution abstraction is still required.

## DT-11 selected DataTable columns (2026-10-22)
Implemented optional Columns projection using reflected property export. Unbuilt and untested by request. Verify native/user-defined row structs, tag/text/container/static-array fields, unknown names and output caps; no row mutation or saving is intended.

## ED-17 position format help (2026-10-22)
Implemented explicit tool and parameter examples plus matching refusal guidance. Untested: verify generated tool discovery exposes this documentation. Position-edit behavior was not changed.

## Targeted removal connection notifications (2026-10-22)
Source follow-up 800b1e3: surviving endpoints receive pin and node callbacks after the raw batch, resolving pin IDs anew between callbacks. Both preview and edit use this path. Unbuilt/untested: wildcard and split-pin reconstruction, multiple removals on one node, deletion of linked nodes, preview isolation and undo restoration. No tests/build/editor runs.

## Automatic thumbnail-free widget saves (2026-10-22)
Both save entry points now serialize Widget Blueprints directly, preventing thumbnail design-time script execution in the MCP call. Existing thumbnails are not refreshed. Unbuilt/untested: project/plugin mount paths, source control/read-only failures, mixed asset batches, dirty state, persistence and reload. General save-result preservation for script exceptions outside thumbnail rendering remains open.

## Comment no-op dirtying (2026-10-22)
Defer owned transaction and Modify until a concrete create/delete/set/fit change. Skip identical bounds and field values. Unbuilt/untested: clean and already-dirty packages, repeated set/fit, missing targets, mixed changed/unchanged boxes and undo. Broader unrelated-package dirtying remains open.

## FT-94 layout done-check (2026-10-22)
Added combined layout text, ten longest wires with exact endpoints and full-graph capture. Unbuilt/untested: discovery, large graphs/output caps, full framing, failed capture preservation and endpoint ordering. No automatic layout approval or edits.

## LR-18/LR-19 alignment tolerance (2026-10-22)
Human report tools default to 64 units; configurable 0..1024 with strict 8 available. Internal measurement defaults unchanged. Unbuilt/untested: schema, offset boundary values, overlap/backward findings and comparison with accepted field layouts.

## GC-33 current designer getter discovery (2026-10-22)
Live WidgetTree variable declarations supplement missing reflected actions; getter uses explicit object type and self member, reader preserves owner-qualified ID. Unbuilt/untested: native/user-widget types, same names across assets, compile/reconstruction, rename and append/undo. No compilation or editor runs performed.

## GC-34 / GC-32 standalone closers (2026-10-22)
Shared pin parser ignores delimiter-only closing lines after comment removal. Balance checking remains separate. Unbuilt/untested: pinless labeled forms, nested closures, trailing comments and malformed parentheses. GC-32 remains partial because default-validation preview parity is not complete.

## GC-42 function delegate binding (2026-10-22)
Added typed existing-self-function binding through Create Event, signature and engine eligibility checks, transient preview and owned rollback. Unbuilt/untested: compatible/incompatible signatures, existing delegate source replacement, preview isolation, undo and compile. New typed custom event creation and non-self targets remain open.

## GC-42 explicit object delegate context (2026-10-22)
Function binding now accepts exact object output endpoints, wires context before signature selection and checks surviving links. Unbuilt/untested: foreign Blueprint/native objects, incompatible functions, object/delegate pin reconstruction, preview isolation and undo. New signature-derived custom events remain open.

## GC-42 signature-derived event creation (2026-10-22)
Added BindNewEvent preview/edit using stock CreateFromFunction, event graph/name guards and delegate-signature resolution. Body intentionally empty for caller wiring. Return-valued signatures require function binding. Unbuilt/untested: typed/ref parameters, preview isolation, event name collisions, compile, undo and source replacement.

## FT-96 delegate relation evidence (2026-10-22)
Blueprint relation reads list direct function/custom-event delegate connections and binding-entry context. Runtime invocation is not inferred. Unbuilt/untested: self/external scope, disconnected nodes, name collisions and text caps. Designer/component binding and intermediate routes remain open.

## FT-96 component/widget and actor binding reads (2026-10-22)
Added bound-event owner property/class, actor identity and event node ID to Blueprint relations. Unbuilt/untested: widget hover/click, component events, missing/deleted owners and actor-bound level scripts. Intermediate delegate routes remain open.
