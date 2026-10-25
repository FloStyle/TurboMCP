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
