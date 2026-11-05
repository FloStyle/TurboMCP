# Engine issues retained for a future Unreal 5.9 proposal

Status: proposal backlog; no engine modification or upstream submission authorized.
TurboMCP must continue to run on stock Unreal. Historical experimental engine patches were reverted; they are evidence, not a deployment requirement.

## DataTable reimport and object lifetime

Recorded baseline: Unreal 5.8.3, 396c9f059903aed5fec78ecd3d437a40c6415368. A synthetic held transient FTableRowBase DataTable import hit UObjectArray.cpp:405 refcount assertion (run 5dc032a4dbd7443796e5f51dcc6b4466). CSVImportFactory.cpp reconstructed an existing table rather than retaining its object identity. An experimental change reused ExistingTable; focused run 9f90e8fe6b46468d9d6a45f74f4a49d8 passed. Historical engine commit 1ab08f9b290. This establishes the synthetic lifetime case, not the exact workplace reference holder. TurboMCP avoids the whole-table native JSON rewrite and performs validated in-place row updates. Owner reports stable subsequent field editing and accepts the plugin issue as fixed for now.

Future proposal: recheck 5.9 source, preserve table identity during reimport, and test held references, reference-bearing rows, open/closed editors, notifications and save/reload. Do not assume the old patch applies unchanged.

## Native pin defaults and compile/transaction boundaries

Historical engine experiment 0144a1931f3b8034d6fe953b676d0edaf4f4ef6f addressed BlueprintGraphPin default validation, undo/dirty handling, and refusing compilation inside an active transaction. Related native programmatic.py work delayed result publication until transaction cleanup, checked failed undo and released resources in finally. These changes were not comprehensively validated and are not shipped engine requirements. TurboMCP supplies guarded pin-default and bounded graph-edit paths and filters specific unsafe native alternatives. General native script timeout/late completion remains unresolved; duplicate execution was suspected, not proven.

Future proposal: validate before mutation, preserve prior defaults on refusal, use owned transaction restoration, separate compile/save from edit rollback, and return truthful partial/unknown outcomes. Establish request correlation before claiming duplicate-execution fixes. See field-03 and field-04 task histories.

## Native DataTable request validation

Historical untested engine experiment cc5b062305f3201d4f27ee8497bd54e9671efd1e covered empty/None row names, case-colliding requests, duplicate field aliases and schema/export mismatches in native data_table.py. Reassess on 5.9; keep refusal semantics consistent with FName identity. The plugin has its own safeguards. No upstream fix is claimed.

## Opening an asset for inspection dirties it

User clarification, 2026-10-23, Oct22 S2/X-93: opening an asset to inspect it through default Unreal MCP can mark it dirty; this is not exclusive to TurboMCP. The user may then save unintentionally. Exact tool call and whether serialized data changes are not yet established. Future investigation should compare dirty packages and serialized/property state before opening, after opening and after inspection on disposable assets, including native editor-only controls and dependent assets. Never clear dirty flags as a substitute for understanding changes.

## Graph redraw/capture artifact

S14/CP-14: excessive contrast, shadows and outlines after MCP graph changes; manual node creation clears the rendering. User suspects a Blueprint capture may have occurred, but does not confirm it. No data corruption established. TurboMCP already isolates measurement and refreshes graph panels. Compare capture/no-capture and native manual refresh before assigning an engine cause. Keep as a plugin/Slate boundary investigation, not an established engine defect.

## Evidence and delivery limits

Historical evidence is recorded in todo/field-01.md, field-02.md, field-03.md, field-04.md and docs/audit/deferred-field-verification.md. Old experimental commits are local engine history; no private project code or assets are needed in a proposal. A future proposal must include a sanitized reproducer, exact 5.9 revision, expected/actual result and validation evidence. No custom engine requirement, automatic publication or claim that 5.9 is affected follows from this document.

## 2026-10-23 local native-open observation

Supervised run `f616e04a3382410e96dd713a2f25635f`, candidate `f41d99a`, stock pinned engine, real rendering enabled. Build and diagnostic completed; supervisor restored 16 fixture files and stopped its editor. Startup took about 246 seconds (cold shader preparation).

Native `EditorToolset.EditorAppToolset.OpenEditorForAsset` opened `/Game/Tests/BP_NodeTest.BP_NodeTest` and `/Game/Tests/WBP_NodeTest.WBP_NodeTest`. `ReadTurboMCPDirtyList` reported zero dirty packages before each open, immediately afterwards and three seconds afterwards. No asset save was requested.

This only establishes that opening these two fixtures did not reproduce X-93. Inspection/capture, other asset types and project dependencies remain untested. CP-14 rendering artifact has not been exercised by this run. Neither issue is closed.

Evidence: `.local/executions/f616e04a3382410e96dd713a2f25635f/report.json` and `02-suites/test-field-open-diagnostic.log` in the execution checkout. Native discovery also confirms `SlateInspectorToolset` is available for subsequent autonomous screenshots and UI observations.


## Off-screen SVG icons - confirmed capture boundary, 2026-10-25

On stock 5.8.3, FWidgetRenderer graph captures requested SVG icons at their final zoom without updating Slate's shared vector cache. Function/local-variable glyphs and resize handles appeared as white squares. PNG icons rendered correctly; missing assets, alpha readback and simple warm-up at a different zoom were ruled out. TurboMCP now paints at the final scale, runs the renderer resource update with an empty window buffer, and repaints. Actual purchase captures confirm visible icons (9e1cc0a; final build/scenario c8735a8f289248269dad165ce361fd5d). Future 5.9 proposal: document or provide a public off-screen SVG resource preparation path. This is distinct from unproven live graph contrast issue CP-14; no engine patch is required by the plugin fix.


## Object-keyed map corruption after rejected stock writer import (Oct27 X-113)

Status: reproduced on pinned stock 5.8.3; not tested on 5.9. No engine changes.

Corrected fixture commit: `f2b1ccfc1633f5e3188a986919fe968d9b093d98`. Native run: `88c27e3e88834583af0e4d8420911842`. Doctor and build passed; `test-data-asset-safety` failed its preservation assertion, as recorded. The initial abstract-UObject fixture was invalid; its earlier failure is not the proof.

The owned data asset contains a map from a concrete external UObject reference to an owned instanced child. Feed the reader JSON back to `UToolsetLibrary::SetObjectProperties`. With `EBypassContainerCheck::No`, the call succeeds and preserves the original entry. With `Yes`, it returns false and changes the key to `None`, retaining the child value. The package dirty flag remains false in both cases, so dirty-state checks alone miss the corruption. No fixture package is saved.

The same run creates a child from its class path successfully and confirms the correct owning outer. X-116's historical false-success inputs remain unavailable; do not label all instanced creation broken.

Engine investigation targets: ToolsetLibrary passes live property memory to ImportPropertyWithNotify; the bypass path imports directly, while ReferenceConverter enforces subobject ownership. Proposed engine acceptance: rejected container imports leave every key/value and dirty state unchanged, with errors captured at the public call boundary. Confirm the fix independently on a specific 5.9 revision before submission.

Plugin work remains OCT27-14: validate on temporary owned state, preserve external identities, independently read back, and commit only validated changes with rollback. Simply selecting non-bypass mode is not proof of atomic safety for arbitrary input. This is an engine defect record, not an implemented plugin fix.

## PIE stop result versus teardown errors (NOV04-37 / X-119)

Status: source-reviewed on 2026-11-05; reported failure not reproduced and cause unresolved. No engine changes.

The field report says three stock stop calls returned teardown script-error text, with PIE still running after one call. Current stock `EditorAppToolset.cpp` implements `StopPIE` around line 965: it starts `FPIEShutdownWatcher` before `RequestEndPlayMap`. The watcher around line 831 subscribes to `ShutdownPIE`, completes only after that event, and reports a 30-second timeout. `IsPIERunning` checks `GEditor->PlayWorld`. The inspected file has no local modifications. This contradicts an immediate-success diagnosis; it does not explain the reported error interception or prove shutdown always succeeds.

Reproduction required: use a disposable actor whose EndPlay produces a controlled Blueprint script error; start PIE, call the stock stop tool, retain its complete response, and independently poll session state. Compare a clean EndPlay control. Record whether the shutdown event happened and whether script-error collection replaced the operation result. Do not infer stopped state from a request or from error text. Keep plugin edit refusals while PIE is active. Future engine proposal: report operation completion/state separately from collected project script errors, if this reproduction confirms conflation. No custom engine requirement or current fix is claimed.

### Controlled stop observation, 2026-11-05

Run `fc8d5b53ec3746bab9a54082abc7cca7` built successfully on stock Unreal, then failed the test harness. The disposable Blueprint EndPlay event called SetActorHiddenInGame through a linked null actor variable. Clean stop returned `{"returnValue":null}`. The null-reference case returned `Accessed None trying to read (real) property NullActor`, while independently observed `PlayWorld` was absent and `IsPlaySessionInProgress` was false. The outer automation call also returned script-error text instead of its JSON report, causing the parser failure. Keep this failure; do not label the suite passed. The reported case where PIE remains active was not reproduced. Next investigate registry script-error interception and report completion separately from diagnostics without changing stock engine code.

### Result interception traced and regression retained

`FunctionLibraryToolset.cpp` around lines 157–182 retains an exception handler throughout asynchronous execution. After a successful tool result, a nonempty captured script exception replaces that result with an error. This explains the observed stop response and the enclosing automation response; no engine edits were made.

Reproduction `8a4f59ba87ee40608ad3a351aaa17bd0` passed its assertions with a separate `AutomationTestToolset.GetTestResults` query: clean control and null-reference case each fired ShutdownPIE and left no PlayWorld or active session. Expected diagnostics are limited to the exact fixture null-reference message, observed twice. Failed runs `fc8d5b53ec3746bab9a54082abc7cca7` and `45eaa5ea51a848b1b761087a3b9c8a13` remain evidence. Offline gate `6d4e2328e5f84535b1720d5eab3fa05f` passed. Run the focused reproduction with `TMCP_PIE_STOP_ONLY=1` and the supervised engine suite `test-vision-workflow`.

Current stock-engine workflow: call `EditorToolset.EditorAppToolset.StopPIE`, retain any script error, then independently call `EditorToolset.EditorAppToolset.IsPIERunning`. Only proceed with editor mutations after it returns false. If true, report the active session and investigate; do not equate the stop request with completion or blindly repeat mutations. The reported persistent session remains unreproduced. The engine output defect remains open for upstream correction; a passing reproduction is not a fix.


## NOV04-36: text presence and visible snapshot disagree

Status: source-confirmed predicate mismatch; native reproduction pending. No engine edits.

The pinned stock SlateInspectorToolset WaitFor traversal reads text and recurses through every child of visible top-level windows without checking the visibility of each widget or its ancestors. Snapshot RenderWidget instead stops at Hidden and Collapsed widgets. A collapsed row can therefore satisfy a text-presence query while being absent from the visible snapshot. This matches the reported symptom but does not prove the original project's exact call or widget state.

Proposed upstream contract: make visibility semantics explicit. Default text waiting should use the same effective visibility traversal as visible snapshots, or expose a clearly named presence-versus-visible option. A hidden descendant must not count as visible merely because its own visibility is Visible. Preserve a separate presence query if clients need mounted-but-hidden controls. Do not silently call text presence clickability: enabled state, clipping and hit testing are separate concerns.

Pending controlled evidence: one owned Slate window containing a unique visible label, a collapsed label and a visible label under a collapsed parent. Compare stock WaitFor and Snapshot, then reveal the parent and compare again. No user widgets or engine source mutation. Until then, a successful stock WaitFor is text-presence evidence only; verify visible state with Snapshot/capture before interaction.
