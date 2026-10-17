> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# TurboMCP graph and Blueprint edit safety audit

## Verdict and method

**Do not treat the graph editing surface as release-safe for unattended Blueprint changes.** The inspected code has concrete preview mutation, undo ordering, partial-application, and semantic-preservation defects. Passing fixture tests would not establish the missing guarantees.

This is a read-only, static source audit of `C:\Users\Example\Desktop\TurboMCP`. No source changes, builds, editor launches, asset-mutating tests, or board operations were performed by this auditor. The only output written is this report. Findings describe the current working files, not necessarily HEAD or the loaded plugin binary. The lead auditor identified dirty graph repoint and swap implementations, their headers, the toolset header, and the untracked batch implementation. These were preserved; repoint/swap/batch findings below explicitly apply to that in-progress state. No independent HEAD comparison was performed by this auditor.

The required symbol index was grepped first for Repoint/SwapCall/BatchEdit/GraphPatch/GraphAppend and returned no matches. The dependency index was read before dependency expansion: it contained only its heading and no dependency edges. Source discovery therefore used absolute-path glob/grep/read. Failed discovery of nonexistent Scripts/Tests directories was followed by discovery of the actual Tools directory; nothing was executed.

Severity: **P1** = release blocker for the affected claimed safety guarantee; **P2** = significant correctness/scope issue. **Defect** means visible control/data-flow contradiction; **risk** means a consequential scenario still needs engine/runtime confirmation; **unsupported** means intentionally missing behavior, not a fabricated implementation bug. Runtime reproduction remains unverified for all findings.

## Release-blocking findings

### G01 — Preview planning spawns into the actual graph (P1, defect)

[GraphAppend:277–309](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L277-L309>) calls `SpawnNodeOfType(Blueprint, Graph, ...)` during Plan, captures pins, then removes the returned node. This is the real graph, not the transient probe graph. [NodeType:344–351](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeType.cpp#L344-L351>) invokes the engine spawner against that graph and allocates pins. There is no encompassing transaction, dirty-state restoration, or Blueprint-wide snapshot in this plan path. Removing the returned node cannot prove that arbitrary spawner side effects were undone.

This also affects replace and patch previews because they delegate to append planning: [Replace:90–105](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L90-L105>) and [Patch:115–121](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L115-L121>). A patch that ultimately refuses a new form can already have probed it in the real asset. Dedicated transient probing exists at [NodeType:267–308](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeType.cpp#L267-L308>) but append does not use it here.

**Consequences:** live graph mutation is definite; persistent dirty flags, event/delegate/member changes, undo pollution, selection invalidation, and custom-spawner side effects depend on the invoked engine spawner. A matching final node count or unchanged disk timestamp does not prove preview purity.

**Regression:** snapshot object identities, Blueprint fields, graph/node/pin arrays, links, package dirty state and undo queue; run accepted and refused previews using ordinary calls plus custom-event/delegate-producing spawners; require exact equality and no mutation notifications. Use isolated scratch assets, not production content.

### G02 — Patch records the transaction after changing the pin (P1, defect)

[Patch:393–402](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L393-L402>) assigns `Target->DefaultValue = Literal` before `Node->Modify()`. Blueprint/graph Modify at 344–345 does not establish a pre-change transaction record for each independently serialized existing node. The first changed pin on each previously unrecorded node can therefore enter the undo record already changed. Subsequent rollback or user undo need not restore its original value.

The raw assignment also precedes `TrySetDefaultValue`. If the schema refuses a proposed value, it has already been installed in DefaultValue. The apply code increments PinsWritten without comparing engine-effective literal state. The same raw-assignment ordering appears for newly spawned nodes in [Append:695–706](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L695-L706>); undo consequences differ for newly created objects, but validation bypass remains.

**Regression:** patch one existing node, undo once, compare every default representation and pointer identity; patch two nodes and inject a late failure, then compare the original first pin. Exercise enum, object, text, numeric range, malformed struct and wildcard defaults. Never assign raw state before Modify/schema validation.

### G03 — Patch silently drops requested links in mixed updates (P1, defect)

[Patch:181–188](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L181-L188>) explicitly skips link values, claiming they are planned/applied as links. Link-only forms are refused as having no pin values at [218–227](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L218-L227>). In a form with a literal and a link, the literal enters Updates but the link is not applied: [Apply:343–426](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L343-L426>) iterates literal Updates and never calls append apply or iterates Append.Links. The comment at 413 claiming wires are made here is not implementation.

**Consequence:** mixed literal+rewire requests can return changed while leaving the wire unchanged. Link-only refusal is an unsupported feature; silent mixed-request omission is a defect.

**Regression:** submit a patch changing one string and one source endpoint; compare exact bidirectional adjacency and literal value. Either implement both atomically or reject every patch containing a link before any mutation.

### G04 — Replace is two committed edits, not one atomic replacement (P1, defect)

[Replace:282–286](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L282-L286>) first completes Append::Apply and its transaction. Only afterward does [Replace:303–328](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L303-L328>) re-resolve the graph and open a separate removal transaction. Any failure between these phases leaves the append committed. A successful replacement with both additions and removals requires two undo operations. The comment at 324–325 saying this prevents half-application is contrary to the control flow.

The removal compare is also weaker than its wording: [Replace:379–396](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L379-L396>) checks only nodes that were actually removed, not all planned removals. A planned key that no longer matches is skipped at 353–355 and absent from that comparison.

**Regression:** replace a graph with both addition and deletion; require one Undo to restore exact old graph and one Redo to restore new graph. Inject resolve/removal/readback failures after successful additions. Require atomic restoration, not a report-only warning.

### G05 — Rollback claims do not depend on rollback success (P1, defect; nested-transaction risk)

[Rollback:13–29](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRollback.cpp#L13-L29>) returns false without GEditor; otherwise ends one active transaction and undoes the latest record. Graph callers mostly discard the bool, yet report NOTHING written/rolled back: [Append:650–659](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L650-L659>), [Patch:365–375](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L365-L375>), [Swap:203–215,238–243](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp#L203-L243>), [Repoint:80–91](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L80-L91>). No graph-wide post-undo identity/default/link comparison verifies those claims.

The helper is not tied to a transaction identity/token. Its header's double-End safety claim [Rollback header:39–44](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/TurboMCPRollback.h#L39-L44>) assumes the active count reaches zero. If a caller owns an outer transaction, ending one nested scope need not push a fresh independent undo record; wrong-record undo or outer-scope interference is a **risk**, not a reproduced event. No duplicate-object restoration was found in the focused graph paths: pointer preservation depends on engine transactions and correct Modify coverage, not a tested snapshot strategy.

**Regression:** place a known unrelated edit in undo history; fail graph edits with no editor, disabled transactions, nested transactions, and mid-apply failure. Verify that unrelated edits survive, the entire failed edit is restored, original UObject identities/pins remain valid, and no failed edit can be redone. Return rollback-failed/partial state honestly.

### G06 — Safe call swap resets unlinked arguments (P1, defect in dirty implementation)

[Swap:138–157](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp#L138-L157>) collects only linked pins. [Swap:185–223](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp#L185-L223>) spawns a fresh node and migrates links; it never transfers or warns about unlinked defaults (DefaultValue, DefaultObject, DefaultTextValue), split-pin structure, or other call-node configuration. Even swapping PrintString to PrintString changes a customized string argument back to the spawner default. Keeping the old node unplugged does not preserve the executing call's semantics.

Preview checks presence/name/direction, not schema compatibility, at [Swap:138–160](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp#L138-L160>); incompatible same-name pins fail only after mutation. Standalone swap's dirty header defaults `bCompile=false` [Swap header:40–45](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/Public/TurboMCPGraphSwapCall.h#L40-L45>), so it does not explicitly full-compile after apply.

**Regression:** same-type swap with nondefault string, object and text literals, split struct pins, ref parameters and wildcard-dependent outputs. Require identical runtime-relevant state or an explicit refusal with the lost values listed. Check every migrated endpoint and old-node remaining links, not just node presence.

### G07 — Write-readiness is advisory, not an enforced graph guard (P1, defect in advertised guard coverage)

[Toolsets:309–350](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPToolsets.cpp#L309-L350>) says writes are refused during running/queued PIE and simulation. [Patch tool wrapper:43–72](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatchTools.cpp#L43-L72>) directly plans/applies; the inspected graph apply paths have no editor-available/PIE/SIE guard. A source-wide grep of relevant guard symbols found enforcement in save, but not these graph writers. Call-recorder grep found no corresponding guard. An external engine tool-dispatch guard was not verified, so do not infer its existence.

[Compile:157–162](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L157-L162>) explicitly allows compile during play, contradicting the broader readiness wording. Whether a particular compile is engine-safe in PIE is separate from this reporting inconsistency. No focused graph `bEditable`/read-only/inherited-graph checks were found.

**Regression:** invoke every Preview/Edit directly during queued/running PIE, SIE, absent GEditor, read-only/noneditable graph contexts and shutdown; assert refusal before spawner invocation/Modify/transaction start. Test dispatcher and direct C++ entry points separately.

## Additional defects and correctness risks

### G08 — Repoint refuses normal occupied data-input migration (P2 defect; engine response requires live confirmation)

[Repoint:55–59](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L55-L59>) asks CanCreateConnection while the reader remains connected to its old source and accepts only CONNECT_RESPONSE_MAKE. A normal K2 occupied single-source data input requires a BREAK_OTHERS response; the intended repoint is then refused rather than evaluated as replacement of the selected old edge. Existing tests only exercise exec `then`, not ordinary data inputs.

**Regression:** float/string/object getter with two consumers, repoint to existing compatible getter and to a newly spawned getter; distinguish legitimate break-old response from destructive break-unrelated links and conversions. Preserve all unrelated edges.

### G09 — Source deletion is wider than the repointed output (P1 risk/defect in dirty implementation)

[Repoint:55–60](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L55-L60>) plans readers of only one named output. [Repoint:81](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L81>) removes the entire source node with Graph->RemoveNode when keepSource=false, without checking other data outputs or execution links, and without an explicit same-source/target guard. A multi-output/impure source can lose unrelated behavior. Exact low-level removal/link cleanup behavior depends on engine implementation; it is not a substitute for safety validation.

**Regression:** source with second live output plus execution links; repoint one output with deletion requested and require refusal or full accounted migration. Reject source=target deletion explicitly.

### G10 — Node identity is inconsistent and unstable across collision-width changes (P2)

[NodeId:282–319](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp#L282-L319>) dynamically lengthens GUID prefixes; this avoids ordinary prefix collision in freshly rendered IDs, but IDs can change when another node with the same prefix is added or deleted. [Repoint:24–26](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L24-L26>) and [Swap:37–43](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp#L37-L43>) resolve by exact current rendered long ID rather than the shared prefix resolver. A fully qualified full GUID is not necessarily accepted there. During multi-step mutation, IDs captured before a spawn/removal can cease matching.

Append's bare-ID lookup is worse: [Append:324–335](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L324-L335>) adds six-digit `Digits` keys to a TMap, so duplicate prefixes overwrite silently; the form differs in case from canonical lowercase output. [Append:349–351,406–412](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L349-L412>) also selects the first new form for duplicate type IDs unless local labels are used. Ambiguous references should refuse, not pick an entry.

Duplicate full NodeGuids are not detected by UniqueKeyDigits: it skips equal GUIDs at 301. The shared FindNodesByKey does expose multiple matches [NodeId:247–279](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp#L247-L279>), but exact-ID helpers bypass that safeguard.

**Regression:** controlled GUID-prefix collisions, identical full GUIDs, upper/lowercase bare IDs, full GUID input, graph rename, collision introduced by spawn, and two same-type new forms. Assert ambiguity refusal and stable internal GUID/object resolution throughout apply.

### G11 — Compile/readback success is much narrower than validity (P1 safety gap)

Patch, replace and repoint call CompileBlueprint without collecting status/errors: [Patch:424–426](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L424-L426>), [Replace:372–374](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L372-L374>), [Repoint:82](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp#L82>). Compile errors therefore do not trigger rollback or appear in the changed report. Append only structurally marks at [Append:896–918](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L896-L918>), despite a nearby COMPILE comment; its readback checks created-node membership, not literal equality or compiled validity. Structural marking can have engine compilation side effects but is not an explicit full CompileBlueprint validity result.

Append's failed compare is appended under a changed report and Apply still returns true at [935–959](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L935-L959>). Patch similarly reports node-set FAILED COMPARE rather than reverting [Patch:488–511](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L488-L511>). A no-op patch still Modify/structurally-marks/compiles, while reporting unchanged based only on literal changes.

Dedicated compile is stronger: [Compile:223–277](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L223-L277>) reads compiler counts and engine log spans and distinguishes unsaved state. It does not supply the writers with this safety gate. Its message renderer intentionally does not extract node-object tokens [91–123](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L91-L123>), so comments promising node IDs are not implemented as stable IDs.

**Regression:** valid-before/invalid-after edit, pre-existing-error Blueprint, warning-only compile, engine ensure, failed readback, and repeated identical patch. Require explicit compile outcome, atomic rollback policy based on before/after validity, and dirty/undo stability for true no-ops.

### G12 — Batch rollback wording and plan staleness (P2, untracked implementation)

[Batch:40–48](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp#L40-L48>) plans every operation against the original state before [69–81](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp#L69-L81>) applies them sequentially. Dependent operations do not get sequential planning or explicit conflict detection. Repoint ApplyCore does not verify every planned old edge still exists before breaking it. Batch only supports graphRepoint and requires targetNodeId as a string even when targetTypeId creates a target.

The failure report always opens `unchanged` and labels the previous operations `appliedAndRolledBack`, even when rollback=false at 76–77. That is a concrete reporting contradiction. Compile/save are honestly reported deferred, not performed; do not describe the batch as a complete compile-and-save transaction.

**Regression:** same reader touched by two operations, first operation deletes second's source, two assets, rollback failure, and empty/malformed/unknown operations. Require valid sequential semantics or preflight refusal and accurate partial-state reporting.

## Reader, grammar and unsupported scope

### G13 — Shared pure-node definitions are never emitted (P2, definite reader omission)

[GraphText:359–370](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L359-L370>) returns `$nodeID` for shared pure values and inserts into InOutSharedValues. All ten SharedValues occurrences in this file were checked: none iterate/render that set. [RenderEntryBody:675–689](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L675-L689>) constructs it, passes it to RenderChain, and returns only Body. Shared producers' operation/default/input information can therefore be absent even in full detail. All-wire reachability then prevents them from being listed as disconnected.

**Regression:** pure arithmetic node with two consumers and its own upstream literals/links; require exactly one complete producer definition plus two references in bodies/full, including cross-entry sharing.

### G14 — Quoting is not a reversible grammar (P2, defect)

[GraphText:398–419](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L398-L419>) interpolates raw string/text values inside quotes without escaping quotes, backslashes, CR/LF. Numeric-looking string values are rendered as numbers based on lexical appearance rather than pin type. [GraphTextLex:108–132](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphTextLex.cpp#L108-L132>) toggles quote state at every quote without escapes; [193–197](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphTextLex.cpp#L193-L197>) strips all trailing closing parentheses without balancing the value. This is not a robust nested/value grammar.

[Lex:90–94](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphTextLex.cpp#L90-L94>) special-cases only arrow/:then/:else as non-node heads, whereas reader emits arbitrary named exec continuations and goto [GraphText:593–599,645–653](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L593-L653>). Checker treats parenthesized inline values as an unchecked nested region [Check:635–648](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphCheck.cpp#L635-L648>); acceptance is not recursive writer compatibility.

**Regression:** quote, semicolon, newline, slash, parentheses, Unicode, numeric-string, localized FText, nested values, sequence/switch outputs, goto joins and quoted pin names; compare parse AST and effective defaults, not just absence of findings.

### G15 — Read output is not a complete graph serialization (unsupported, with destructive-use risk)

This is explicitly **not** reported as a broken promise that every read must be lossless: [GraphLoad:91–105](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphLoad.cpp#L91-L105>) refuses format=write as unimplemented. Manifest/signatures intentionally summarize; bodies/full also have material limits:

- output pins show counts, not endpoints [GraphText:502–514](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L502-L514>);
- references omit source pin names [GraphText:392–395](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L392-L395>), but writer links require Node:Pin [Lex:257–269](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphTextLex.cpp#L257-L269>);
- 60 printed-pin limit [GraphText:522–526](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L522-L526>), inline depth 12, exec-chain guard 2000, disconnected listing capped at 40 [823–830](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L823-L830>);
- no-entry graphs return only a note, not their nodes [GraphText:760–768](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphText.cpp#L760-L768>);
- graph text is hard-truncated with no continuation cursor [GraphLoad:157–166](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphLoad.cpp#L157-L166>); requesting a manifest cannot recover omitted body details;
- graph lookup enumerates top-level Blueprint graph arrays, not arbitrary recursive subgraphs [NodeId:322–357](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp#L322-L357>).

Replace derives its keep set only from existing-node forms and deletes other ordinary nodes [Replace:132–189](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L132-L189>). Thus feeding an incomplete readable view into replace is unsafe unless rejected or explicitly reviewed. Some such inputs are rejected by the parser; this audit does not claim every truncated/shared-value example reaches apply. A safety contract needs an explicit completeness token and fail-closed replace validation, not inference from parse acceptance.

### G16 — Node creation/type identity and wildcards need additional safety proof (P2 risks)

[NodeType:196–209](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeType.cpp#L196-L209>) uses UI-derived TypeId; available IDs are deduplicated [247–258](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeType.cpp#L247-L258>) while build/spawn returns the first equal entry [287–308,337–353](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPNodeType.cpp#L287-L353>). No ambiguity rejection was found. Distinct functions/spawners with identical UI category/title can therefore resolve by registry order. BuildProbeNode invokes a spawner under a transient graph whose outer is the real Blueprint; this is better isolation than append's live probe but is not proof against Blueprint-level custom-spawner side effects.

Spawn calls AllocateDefaultPins after Invoke; whether Invoke already allocated pins depends on spawner/engine behavior and must be tested before declaring a duplicate-pin defect. No explicit wildcard specialization pass was found in append/check. Schema connection notifications may specialize types, but append sets literals before links [Append:637–711](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp#L637-L711>); reconstruction can invalidate captured assumptions. These are risks, not proof all wildcard nodes fail.

**Regression:** duplicate palette titles from distinct function libraries; custom spawner member side effects; reroute, array/map/set and promotable operators; several link orders; split/recombined struct pins; verify no duplicate pins, correct specialization, final defaults, and exact symmetric edges.

## Save truthfulness

### G17 — Dirty-state observation is not independent proof of a durable package write (P2 risk)

[Compile/save:535–558](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L535-L558>) bases saved solely on `!Package->IsDirty()`, even if save returned false; it performs no file-existence/content/reopen verification. This may accommodate observed engine return-value anomalies, but a dirty flag is in-memory state, not independent persistence evidence. A real write followed by immediate re-dirty is conversely reported unsaved after retry. Post-save errors are summarized; exact durable state remains unverified.

The no-thumbnail route uses `.uasset` unconditionally for all accepted /Game packages [492–495](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L492-L495>), and there is no asset-class gate in that route. If the resolver admits a UWorld/map, its required map extension is not selected. Map reachability through this tool remains unverified; restrict supported classes or derive the package extension correctly. Asset flags are set at 523 before direct saving and not restored on failure.

Normal editor save explicitly warns Widget Blueprints may run design-time thumbnail code [591–600](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L591-L600>). No-thumbnail correctly selects a different direct-write path, not a hidden retry into the editor thumbnail path. Neither save variant supplies multi-asset atomicity or filesystem rollback. Source-control checkout is explicitly advisory, not a gate [401–406](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp#L401-L406>).

**Regression:** injected false return with clean flag, true return with re-dirty, missing/wrong-path file, disk-full/read-only/checkout failure, unload/reopen content verification, Widget side effects and UWorld rejection/extension. Require per-asset durable outcome and honest partial-save report; do not equate engine-log absence with durability.

## Test review and coverage inventory

No tests were run. Existing test results/comments are historical claims, not this audit's evidence of current runtime behavior. Tests were inspected through full reads or targeted grep as identified below.

| Area | Inspected test evidence | What it proves statically about test intent / important gap |
|---|---|---|
| Repoint | [test-graph-repoint:24–44](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-repoint.py#L24-L44>) full read | Exec then to existing target, keepSource=true; refusal for nonexistent pin; checks mostly reports/substrings. No data input, deletion, new target, identity, undo, rollback. |
| Swap | [test-graph-swap-call:40–78](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-swap-call.py#L40-L78>) full read | Same PrintString replacement; keepOld=true; node and reference substring presence. No custom-default conservation, exact full adjacency, incompatible types, undo or failure injection. |
| Patch | [test-graph-patch:257–316](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-patch.py#L257-L316>) targeted assertion grep | One literal, node-set preservation, read value, repeated patch unchanged; refusal checks. No transaction undo/rollback, mixed link+literal, effective object/text defaults. |
| Replace | [test-graph-replace:207–302](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-replace.py#L207-L302>) targeted assertion grep | Removed keys, preserved entry, simple reader roundtrip and refusal. No single-Undo atomicity, phase-two failure injection or full incomplete-view matrix. |
| Append | [test-graph-append:265–454](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-append.py#L265-L454>) targeted assertion grep | Growth, old IDs, links/local labels, refusal, no saved claim; preview checks graph count. Count cannot establish preview purity or pointer identity. |
| Reader/check roundtrip | [test-graph-roundtrip:26–60](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-roundtrip.py#L26-L60>) full read | Connected target-pin visibility and checker acceptance only; **does not write the text back**. |
| Reader bounds | [test-graph-text-guards:64–113](<C:/Users/Example/Desktop/TurboMCP/Tools/test-graph-text-guards.py#L64-L113>) targeted assertion grep | Cycles/depth/cap/server survival/log end; does not establish completeness or reversibility. |
| Compile | [test-compile-ensure:108–180](<C:/Users/Example/Desktop/TurboMCP/Tools/test-compile-ensure.py#L108-L180>) selected body read | Deliberate ensure fixture and report shape/log attribution; not graph-writer rollback after compilation fails. |
| Save | [test-save-thumbnail-option:172–251](<C:/Users/Example/Desktop/TurboMCP/Tools/test-save-thumbnail-option.py#L172-L251>) targeted assertion grep | Schema surface, saved report, thumbnail warning and delayed dirty-list state; no independent disk reopen/content check in inspected assertions. |

Repoint/swap suites are not assertion-free: they evaluate check lists and exit nonzero on failure. A static checker searching only an assertion helper or Python assert can falsely flag them as NO-ASSERT. Their substantive weakness is narrow fixtures and weak oracles, not an empty executable test body.

Implementation read coverage: complete rollback implementation/header, repoint implementation/header, swap implementation/header, batch implementation, node-ID implementation, graph-load implementation's relevant rendering/output path; detailed selected ranges of append plan/apply, replace plan/apply, patch plan/apply, graph-text renderer, lexer, graph checker, node-type resolver/probe/spawn, compile/save and toolset readiness/patch wrapper. All SharedValues references and key rollback/guard/compile/default mutation sites were grepped. Layout/tidy/knots/fix-backward/structure and material tools were only discovered or cross-referenced, **not exhaustively audited here**. Some test files were located but not read, notably specialized append-chain/check/node-pins-owner/reader-child suites; do not imply they have no relevant cases.

## Release gate and unverified limits

1. Fix or disable live-preview spawning, patch undo ordering, mixed patch links, non-atomic replace, and default-dropping swap before promoting these tools as safe.
2. Centralize mutation eligibility and transaction ownership; make rollback result and post-rollback identity/state proof part of the actual response.
3. Use exact GUID/object identity internally, reject ambiguous external IDs/type IDs, validate plan freshness before modifying any graph.
4. Define separate guarantees: human-readable graph view, writable subset, complete serialization, compile success, and durable save. They are not interchangeable.
5. Run the proposed fault-injection and editor Undo/Redo tests on disposable fixtures, then compare persisted assets after reopen. This audit intentionally did not do so.

Unverified: target Unreal engine implementation of schema responses, spawner callbacks, transaction nesting, pin reconstruction and package save internals; current binary/source parity; all dirty changes relative to HEAD; external registry/thread/PIE guards; actual map reachability through the asset resolver; randomized wildcard/identity collision execution; complete repository test coverage. No runtime PASS, build failure, crash reproduction, or universal asset-corruption claim is made. These limits do not negate the definite source-level ordering and omitted-operation defects above.
