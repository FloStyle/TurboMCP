> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# TurboMCP non-core-graph tool-domain audit

## Method and limits

Read-only, static, risk-based audit of current implementation. No builds, editor sessions, tests, source changes, or task-board operations were performed. This report is the only artifact written. Findings below come from current source, not previous feedback. Runtime reproduction, provider integration, transaction restoration, GPU output, and compiled plugin identity remain unverified. This is broad domain coverage, **not an exhaustive line-by-line proof**.

The supplied symbol index was grepped first for Widget/Material/Capture/Trace/Profil/DataTable/Struct/Enum/SourceControl/Reference/ResolveAsset/Signature. It returned only three mostly Python/comment entries, not useful C++ definitions. The supplied dependency index was read before expanding dependencies and contains only its heading. Absolute-path glob/grep/read therefore supplied implementation locations. Engine source was consulted for exact behavior where noted.

## Executive findings

| ID | Priority / classification | Finding |
|---|---|---|
| D1 | High, confirmed defect | Material tidy Preview recompiles and dirties the material. |
| D2 | High, confirmed contract defect | Widget Look preview runs unrestricted zero-parameter functions and widget construction logic without isolation; transient allocation does not guarantee read-only behavior. |
| D3 | High, confirmed defect | Trace attribution uses bytecode offset zero, not the executing instruction. Zero-hit conclusions are unsound. |
| D4 | High, confirmed defect | Play-check can report a completed play window even when the start witness never fired. |
| D5 | High, confirmed defect | Explicit reference direction `both` skips both queries and reports zero/zero. |
| D6 | High, confirmed defect | Data-table set-then-remove batches recreate the row and can report success. |
| D7 | High, confirmed defect | Blueprint `private` variables use EditConst instead of the engine's private metadata; external Blueprint access is not restricted as requested. |
| D8 | Medium/high, confirmed defect | Struct default setters can refuse values, but return values are discarded and success output does not verify defaults. |
| D9 | High, confirmed false assurance / restoration risk | Material tidy ignores rollback failure and does not snapshot the material collection before adding comments. |

No remote-code-execution exploit was demonstrated. D2 concerns a tool capability/policy boundary: local project Blueprint code executes with editor authority even though the operation is presented as Look/read-only.

## 1. Widgets: preview, tree/sync, retarget

### D2 — Look is not an isolation boundary

[Preview implementation:40–47](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L40-L47>) claims nothing other than the PNG outlives the call, while explicitly intending generated-class logic. [162–163](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L162-L163>) creates a transient UUserWidget. [237–256](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L237-L256>) looks up any named UFunction, rejects only missing functions or NumParms != 0, then executes ProcessEvent. No allowlist, purity requirement, permission boundary, world isolation, or rollback surrounds it. [266–279](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L266-L279>) builds/renders Slate. [352–355](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L352-L355>) merely marks the instance garbage. That cannot undo changes a function makes to other objects, files, editor state, or global systems. Garbage marking is also not synchronous destruction as the comment claims.

Acceptance: classify script-capable preview as effectful and require explicit opt-in; provide a genuinely non-script designer-tree mode if read-only behavior is promised. Test a zero-parameter function that mutates another asset and a construction callback that performs an external effect. No read-only route may run either silently. Report effects and limitations rather than promising transient-instance isolation.

Additional reporting defect: [357–381](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetPreview.cpp#L357-L381>) counts errors but only calls SummariseSince when Warnings > 0. An errors-only run emits counts, not error details, despite promising all script errors/warnings. Runtime behavior of the shared log capture was not independently audited here.

Strengths: size bounds, JSON-object shape, class compatibility, and generated-class existence are checked; missing sample properties are counted and named. [Focused preview tests:140–203](<C:/Users/Example/Desktop/TurboMCP/Tools/test-widget-preview.py#L140-L203>) cover these refusals, including parameter-bearing function rejection. These are not tests of arbitrary zero-parameter side effects.

Tree/sync: [Walk/Read:304–369](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetTreeSync.cpp#L304-L369>) distinguishes absent root from printed tree and uses a shared renderer. A repository grep found Walk/Read definitions but no Sync Plan/Apply definitions or Edit/PreviewWidgetTree entry points. Treat write-side tree synchronization as **not established**, not as a verified working feature. Parser and nested-widget behavior were not fully inspected.

Retarget: [Apply:311–378](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetRetarget.cpp#L311-L378>) rechecks each current binding against current widget delegates before removal and rereads remaining broken bindings. This is stronger than trusting stale indexes. [351–353](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetRetarget.cpp#L351-L353>) explicitly does not repair stale call nodes: supported scope is binding removal, not full class retarget repair. If read-back still finds broken bindings, [380–387](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPWidgetRetarget.cpp#L380-L387>) appends a warning to a `changed:` response rather than a machine-clear failure. Full binding signature compatibility and undo behavior remain unverified.

## 2. Materials, instances, and layout

### D1 — Material layout Preview writes state

[Preview entry:29–59](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidyTools.cpp#L29-L59>) says it changes nothing/writes nothing, then calls Plan. [Plan:99–104](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidy.cpp#L99-L104>) calls RecompileMaterial for a baseline. This is not a read API: [engine implementation:984–1000](<C:/ExampleUnrealengine/Engine/Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp#L984-L1000>) invokes PreEditChange, PostEditChange, and MarkPackageDirty. [1035–1052](<C:/ExampleUnrealengine/Engine/Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp#L1035-L1052>) also refreshes editor/viewports and builds streaming data. Preview can therefore dirty an asset even when no edit is accepted.

The same baseline pattern was located in [MaterialBlocks Plan:226](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialBlocks.cpp#L226>) and [MaterialLabelsPromote Plan:130](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialLabelsPromote.cpp#L130>). Their full preview-entry call chains were not separately read, so tidy is the fully established end-to-end finding; the other Plan APIs are confirmed effectful.

Acceptance: Preview must not call recompilation/edit APIs. Use existing diagnostics with an explicit unknown/stale status. Compare dirty flags, serialized properties, StateId, child-instance state, and undo history before/after both accepted and refused previews. A pure preview must leave them unchanged.

### D9 — Rollback claims exceed restoration evidence

[Tidy Apply:428–519](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidy.cpp#L428-L519>) opens a transaction, calls Modify on moved expressions/existing comments, but directly adds new comments to the material's collection without first calling Material->Modify. A transaction alone does not snapshot arbitrary owner-array changes. [610–635](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidy.cpp#L610-L635>) ignores EndAndUndoActiveTransaction's return value and unconditionally states ROLLED BACK. [Rollback helper:13–29](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRollback.cpp#L13-L29>) can explicitly return false without undo when GEditor is absent and otherwise returns UndoTransaction's boolean. The helper does not prove restoration.

The baseline recompile occurs before the layout transaction, so even successful undo cannot establish that Preview/Plan side effects were undone. Runtime nested-transaction interaction and actual comment persistence after undo were not exercised.

MaterialBlocks has improved evidence: [790–852](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialBlocks.cpp#L790-L852>) rereads expression keys after undo and reports unverified/missing results. However, matching only expression keys does not prove connections, property values, comment collection, dirty state, or external effects were restored. `ROLLBACK VERIFIED ... undo put it back` is broader than the predicate measured.

Acceptance: snapshot all changed owners before mutation; record/check undo result; verify full material topology, expression properties, comments, positions and dirty state against baseline. Fault-inject every post-write error path. Report rollback failed/unverified on any discrepancy. Never use expression-set equality as proof of complete restoration or semantic equivalence.

### Material-instance limitations and risks

[Texture override writer:114–181](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialInstanceTools.cpp#L114-L181>) validates all requests before setting them, a useful safeguard. It selects the first FMaterialParameterInfo whose **name** matches [134–141](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialInstanceTools.cpp#L134-L141>), discarding layer/blend association and index as selectors. Duplicate names across associations can select an unintended parameter. This needs a layered-material fixture before impact is claimed as reproduced.

[44–60](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialInstanceTools.cpp#L44-L60>) validates only Color/LinearColor sRGB compatibility; all other sampler types return true. [183–196](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialInstanceTools.cpp#L183-L196>) sets values and PostEditChange, then prints a reader result, without comparing each requested override to read-back or checking compile compatibility. No explicit scoped transaction is present in this entry point. Restrict claims to texture overrides; scalar/vector/static switch editing is not implemented here.

Acceptance: uniquely identify association/index, refuse ambiguity, validate texture dimensionality and sampler requirements or explicitly report unverified compatibility, and compare requested values with actual overrides. Verify undo as a separate criterion.

Geometry: tidy documents fallback expression dimensions rather than measured Slate geometry ([tool documentation:40–42](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPMaterialTidyTools.cpp#L40-L42>)). This is a legitimate limitation, not proof of overlap-free layout. Its claimed compiled-result preservation checks expression identities and compiler errors, not semantic equality.

## 3. Graph capture and layout

[Capture:700–733](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCapture.cpp#L700-L733>) now explicitly rejects no-target and zero/negative frame width/height before image creation. Old claims that these are silently normalized must not be repeated. Width/height finite-value checks were not found in this inspected path; practical NaN reachability depends on the rectangle producer and remains unverified.

[735–758](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCapture.cpp#L735-L758>) opens/focuses an asset editor and graph; [770–772](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCapture.cpp#L770-L772>) clamps fit zoom. Capture therefore has UI side effects, even aside from PNG output. Full focus restoration was not traced. [745–755](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPCapture.cpp#L745-L755>) uses static_cast from generic editor instance to FBlueprintEditor; its null check does not validate dynamic type. A nonstandard Blueprint editor is a compatibility risk, not a reproduced crash.

Acceptance: test zero/negative/finite extremes, empty graph, unopened graph, huge graph below minimum fit zoom, specialized editor assets, DPI scales and clipping. Report actual clipping and preserve prior focus/view selection where promised. Graph-tidy, general layout-report internals, capture-asset, and selector implementations were not fully reviewed in this domain pass.

## 4. Trace, play, profiling and cost

### D3 — Wrong instruction offset corrupts trace attribution

[Trace callback:126–127](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L126-L127>) always passes DebugOpcodeOffset=0. [Engine caller:413–425](<C:/ExampleUnrealengine/Engine/Source/Editor/UnrealEd/Private/Kismet2/KismetDebugUtilities.cpp#L413-L425>) uses the current BreakpointOffset; [engine locator:584–591](<C:/ExampleUnrealengine/Engine/Source/Editor/UnrealEd/Private/Kismet2/KismetDebugUtilities.cpp#L584-L591>) forwards that offset to the debug map. Distinct instructions in one UFunction therefore do not receive distinct lookups in TurboMCP. Missing or misattributed hits follow depending on the debug mapping.

[Arm:274–290](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L274-L290>) stores arbitrary IDs without resolving them, yet announces armed points. [388–391](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L388-L391>) says zero means none has run. That conclusion is invalid for nonexistent IDs, missing instrumentation, and the offset bug. Subscription existence is not proof of correct attribution. [103–104](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L103-L104>) updates diagnostic TMap/counters before acquiring the mutex at line 144: concurrency safety remains a risk if callbacks occur on several threads.

Strength: the 1024-hit cap and dropped-hit reporting are explicit. [Focused test:291–325](<C:/Users/Example/Desktop/TurboMCP/Tools/test-trace.py#L291-L325>) tests a single chosen node and positive hits; it does not establish distinguishing multiple offsets in the same function or excluding an unexecuted branch.

Acceptance: derive actual code offset using the engine's convention; exercise multiple executed nodes and a nonexecuted branch within one function, verify exact attribution/counts, validate armed IDs, and distinguish unknown observability from observed zero execution.

### D4 — Never-start protection is disabled by normal timer progression

[TickWatch:233–252](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp#L233-L252>) requires !bEndRequested for the start-grace timeout, but sets bEndRequested as soon as elapsed >= budget regardless of GStartObserved. With ordinary ticks reaching budget before budget+15, the never-start guard becomes unreachable. [Report:382–418](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp#L382-L418>) accepts end-requested plus no session in progress, checks only bNeverStarted, then [428–430](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp#L428-L430>) claims the requested duration was played. A failed/cancelled start can therefore become a clean run. A late start after an ineffective stop request can also remain running because no second stop is issued.

[Map validation:506–519](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPPlayCheck.cpp#L506-L519>) only verifies LoadPackage succeeds, not that it contains a UWorld. Existing non-map packages pass this guard.

Acceptance: model requested/started/stopping/stopped/failed separately; require the start witness before reporting a run; measure duration from actual start; retry/observe stop correctly; validate map class. Test cancelled start, startup > budget, never-start, non-map package, and manual early stop.

Cost: [dependency gathering:52–88](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAssetCost.cpp#L52-L88>) uses direct registry dependencies, not a recursive resident-memory/cooked-cost measurement. Unknown-size entries are removed from Hard/Soft/EditorOnly lists before counts printed at [118–121](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAssetCost.cpp#L118-L121>); those counts can understate dependency cardinality. [232–237](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAssetCost.cpp#L232-L237>) queries Hard, Soft, and EditorOnly independently, so categories may overlap. Query return success/readiness is not checked, unlike the reference reader. [157–160](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPAssetCost.cpp#L157-L160>) can consequently state a real no-dependency result without proving query completeness. Unknown bytes are correctly distinguished from zero bytes. Call-cost/profiling implementations were not fully inspected; no runtime performance claims are validated here.

## 5. Data tables, structs, enums, variables and signatures

### D6 — Data-table operation order is not preserved

[Plan:420–431](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L420-L431>) collects removals independently. [Apply:511–543](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L511-L543>) folds **all** sets into FinalText, ignoring remove operations. [552–575](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L552-L575>) executes all removals first, then all sets, adding missing rows. For `R = Field=value` followed by `- R`, final state is R present, contrary to the ordered request. Verification only checks set fields [608–637](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L608-L637>), not final requested absence, so this can be reported successful.

The existing focused test's heading claims add/change/remove in one call, but [247–251](<C:/Users/Example/Desktop/TurboMCP/Tools/test-datatable-rows.py#L247-L251>) contains two identical sets and [268–271](<C:/Users/Example/Desktop/TurboMCP/Tools/test-datatable-rows.py#L268-L271>) removes in a separate call. It does not cover this defect.

Acceptance: apply operations to a sequential shadow model and validate the final requested state before mutation. Test set/remove, remove/set, repeated remove, case-equivalent row names, and an initially absent row. Verify final absence as well as field values.

Further limitations: [SplitTopLevel:63–85](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L63-L85>) toggles quote state on every quote without escape handling; escaped quotes can corrupt field splitting. [630–635](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L630-L635>) compares exported values to raw input text rather than semantic values, risking false failures for accepted normalized spellings. [649–655](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L649-L655>) now explicitly reports partial writes and dirty state: do not describe it as silently atomic. It is not rollback.

### D8 — Struct defaults may fail silently

[Struct ApplyFields:1563–1569](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L1563-L1569>) and [1635–1640](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L1635-L1640>) discard ChangeVariableDefaultValue's boolean. [Engine setter:460–487](<C:/ExampleUnrealengine/Engine/Source/Editor/UnrealEd/Private/Kismet2/StructureEditorUtils.cpp#L460-L487>) refuses invalid defaults and returns false. [1722–1730](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L1722-L1730>) reports changed/add counts and only names/types, not verified requested defaults. This is a false-success route for accepted field/type requests with invalid default text.

Struct edits are not atomic: [1542–1555](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L1542-L1555>) deliberately leaves a newly added field after rename failure; [1687–1710](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L1687-L1710>) reports applied counts and returns without rollback. That partial behavior is disclosed, but must not be mistaken for transaction-backed all-or-nothing semantics.

Acceptance: validate defaults prewrite in temporary storage; treat unchanged-valid differently from refused; compare actual defaults after compile; report accurate per-option failures and establish explicit atomic versus partial policy.

### D7 — `private` variables are not made Blueprint-private

[VarsAdd:693–705](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPVarsAdd.cpp#L693-L705>) implements private with CPF_EditConst. [Engine access rules:9151–9197](<C:/ExampleUnrealengine/Engine/Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp#L9151-L9197>) use native-private flags or MD_Private metadata; Blueprint read/write access checks use MD_Private, not EditConst. The requested access restriction is therefore not established. [759–775](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPVarsAdd.cpp#L759-L775>) verifies only GUID existence, not access flags, type or default.

Acceptance: use the engine's Blueprint private metadata/API; verify compiled access rules from another Blueprint and from the owning Blueprint. Include both reads and writes, with unrelated editable/read-only flags unchanged.

Enums: [185–223](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp#L185-L223>) refuses overwrite and distinguishes failed creation. [264–272](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp#L264-L272>) explicitly admits a created asset remains if setting names fails. [279–305](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp#L279-L305>) counts accepted display-name setters but does not name refused display-name requests, then reports changed. Partial display-name failure needs explicit reporting and read-back.

Function signatures: [330–358](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPFunction.cpp#L330-L358>) ignores CreateUserDefinedPin return values; [390–391](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPFunction.cpp#L390-L391>) prints intended pins, not actual pins. [361–368](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPFunction.cpp#L361-L368>) captures raw pin pointers, calls RefreshAllNodes, then dereferences old pointers. Reconstruction invalidation/lost-link false reporting is a risk requiring engine-level lifetime verification. Name-collision cleanup and explicit missing-entry/result failures are strengths. Acceptance: reread new signature by stable identities, compare actual pin types/directions, and compare links through refreshed node/pin identities.

## 6. References and asset resolution

### D5 — `both` is accepted but not executed

[Direction parsing:166–175](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L166-L175>) sets bWantOut only for empty/out and bWantIn only for empty/in. bWantBoth accepts the literal `both` solely for validation. Query branches are [218](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L218>) and [279](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L279>), so neither runs for `both`; [312](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L312>) prints zero/zero. This is a deterministic false-empty result.

[Tests:100–107](<C:/Users/Example/Desktop/TurboMCP/Tools/test-refs.py#L100-L107>) use empty direction and weakly equate presence of `counts:` with both directions. They do not exercise literal `both`.

Acceptance: explicit both must equal empty/default and the union of separate in/out queries for a saved fixture with known edges. Unrequested directions must be omitted or marked unqueried, not shown as measured zero.

Strength: [178–200](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L178-L200>) waits for registry completion and refuses incomplete gathering; old feedback claiming this guard is absent is obsolete. Unsaved-edge caveats and truncation disclosure exist [314–327](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L314-L327>). Hard-cap truncation has no general continuation cursor. Registry queries' booleans are not checked. Asset resolver was searched, not fully read: plugin short-name resolution, redirectors, ambiguities, generated-class normalization and load side effects remain unverified. [Reference tests:126–166](<C:/Users/Example/Desktop/TurboMCP/Tools/test-refs.py#L126-L166>) document /Game-only short-name search as an intentional limitation.

## 7. Source control

The automatic-checkout policy is explicitly **AttemptAndReport**, not mandatory locking: [policy:44–55](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp#L44-L55>). Provider failure does not gate the asset write. This is a policy limitation/risk in exclusive-lock workflows, not evidence that the code silently claims a lock. [198–202](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp#L198-L202>) explicitly says non-checkout providers do not take a lock. [428–452](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp#L428-L452>) captures the operation result and provider messages. This is a strength over inferring success from generic checkout helpers.

[Plan reuse:265–310](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp#L265-L310>) derives preview/write decisions together. [402–429](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp#L402-L429>) reacquires the current provider but uses the prior plan's state and policy flags; provider/state changes between plan and apply are not revalidated here. Network/provider-side effects of status reads and underlying file-resolution correctness were not established.

Acceptance: explicitly distinguish checkout attempted, provider success, and verified lock ownership. Offer strict gating if required by the product. Test provider switch, disabled/unavailable provider, unknown state, checked-out-by-other-user, stale local state, cancellation and failed save after checkout. Do not assume checkout rollback accompanies asset rollback.

## Coverage matrix and remaining verification

| Domain | Inspected depth | Not established |
|---|---|---|
| Widget preview | Full implementation; focused negative tests | Runtime scripts, lifecycle, GPU pixels, complete logs |
| Widget tree/sync | Walk/Read tail, entry wrapper, definition search | Full parser, nested traversal, write implementation |
| Widget retarget | Apply and read-back | Full plan/signature compatibility, compile/undo |
| Material tidy | Plan baseline, Apply, tool wrappers; engine recompile | Runtime undo, geometric quality, semantic equivalence |
| Material blocks/labels | Targeted Plan baseline searches; blocks rollback reread | Full transforms and all failure paths |
| Material instance | Full read/write wrapper | Layered materials, engine setter guarantees, transaction behavior |
| Capture/layout | Target-frame validation/editor-opening/fit path; geometry search | Full image pipeline, generic layout, focus restoration |
| Trace | Full implementation; focused test tail; engine locator | Actual correct node attribution, concurrency |
| Play-check | Full implementation | Startup/teardown runtime faults, log threading guarantees |
| Asset cost | Full implementation | Registry cold-start fidelity, cooked/runtime cost |
| Profiling/call cost | Discovery only | Entire performance methodology |
| Data-table rows | Full implementation; focused test batch | Actual importer behavior, rollback, complete schema reader |
| Structs | ApplyFields and default setter engine dependency | Full Plan/New parser, dependent asset effects |
| Enums | Apply | Full planner/display-name validation |
| Variables | Mutation/flags/read-back tail; engine access rules | Full planner, local variables, default compile correctness |
| Function signatures | Creation/apply tail and pointer search | Full parser, removals, pin lifetime |
| References | Core reader and complete focused test | Registry query failure cases and full resolver |
| Source control | Full automatic checkout implementation | Base provider planner/status tools and live providers |

All acceptance criteria above are proposed checks, **not executed tests**. Focused tests were read as evidence of coverage shape, not accepted as proof from an existing PASS record. Highest-value follow-up is D1–D7 regression coverage, followed by complete rollback state comparisons and fault-injected partial-write tests.
