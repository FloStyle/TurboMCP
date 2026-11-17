# Tool reference

[Documentation](../README.md) · [Installation](installation.md)

Generated from plugin declarations. For validation and compatibility limits, see [project status](../status.md).

- [UTurboMCPReadToolset](#uturbomcpreadtoolset)
- [UTurboMCPLookToolset](#uturbomcplooktoolset)
- [UTurboMCPPreviewToolset](#uturbomcppreviewtoolset)
- [UTurboMCPEditToolset](#uturbomcpedittoolset)

## UTurboMCPReadToolset

41 declared tools.

| Function | Description |
| --- | --- |
| `ReadTurboMCPSlateText` | Inspect currently mounted Slate text and ancestor visibility without waiting, focusing or sending input |
| `ReadTurboMCPPlayFunction` | Inspect one exact Blueprint-callable function on an existing PIE runtime object without invoking it |
| `ReadTurboMCPEditorLog` | Read bounded editor/PIE output captured during this enabled plugin session, not tool-call history or a disk file |
| `ReadTurboMCPDataAssetProperty` | Read reflected properties of data assets or already-loaded Blueprint class defaults |
| `ReadTurboMCPPlayInputState` | Inspect the selected PIE player's focus, held synthetic input and Enhanced Input context priorities; no input or asset mutation |
| `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state |
| `ReadTurboMCPToolIndex` | Read a compact summary of the registered toolsets and their available tool counts |
| `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool |
| `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search |
| `ReadTurboMCPBulkExport` | Export bounded pages from one bulk reader into a single JSON report under Saved/TurboMCP/Exports |
| `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form |
| `ReadTurboMCPTypeFind` | Find reflected type paths without loading or editing assets; matches do not guarantee pin or writer support |
| `ReadTurboMCPDirtyList` | List loaded content packages marked dirty; does not include unsaved maps or other editor state |
| `ReadTurboMCPNodeResolve` | Resolve node identities and inspect pins; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPNodeContext` | Inspect static execution context and input sources; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPNodeFind` | Search Blueprint nodes by text; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPGraphLoad` | Inspect one graph as text or JSON, or export a complete supported graph for Replace |
| `ReadTurboMCPBlueprintLoad` | Inspect Blueprint structure and selected graph content; loads assets without requesting edits, compile or save |
| `ReadTurboMCPRefs` | Read saved registry references into or out of an asset |
| `ReadTurboMCPRefsBatch` | Read saved registry edges for multiple packages in one call |
| `ReadTurboMCPOverrides` | Read available and implemented Blueprint overrides from parent classes and interfaces |
| `ReadTurboMCPCommentList` | Read Blueprint comment rectangles, membership and nesting |
| `ReadTurboMCPDataTable` | Read DataTable schema, row names, selected values and registry-based readers |
| `ReadTurboMCPCallLog` | Read retained call-log records, newest first |
| `ReadTurboMCPCallCost` | Summarize retained call costs, durations and failures |
| `ReadTurboMCPToolSchema` | Inspect registered tool metadata and local trap notes without invoking the described tool |
| `ReadTurboMCPMaterialLabels` | Read named material reroute labels, declarations, source expressions and usage counts |
| `ReadTurboMCPMaterialLoad` | Read material parameters and connected output trees, or the dedicated instance report |
| `ReadTurboMCPMaterialTrace` | Read one connected material output tree and its label definitions; unused-expression findings consider all connected outputs |
| `ReadTurboMCPMaterialBlocks` | Read material logic grouped by comment boxes, with incoming and outgoing values |
| `ReadTurboMCPMaterialInstance` | Read material-instance parent chain, local overrides, inherited values and orphaned overrides |
| `ReadTurboMCPPlayCheck` | Start or poll a timed Play In Editor session that executes project code with editor permissions, is not sandboxed or automatically undone, and does not explicitly save assets |
| `ReadTurboMCPTrace` | Report the trace subscription, armed points and retained hits |
| `ReadTurboMCPBPLint` | Inspect Blueprints for selected lint findings; loads assets but does not explicitly edit, compile or save them |
| `ReadTurboMCPAssetCost` | Report recorded package disk sizes and direct registry dependencies; does not explicitly edit, compile or save assets |
| `ReadTurboMCPWidgetProperty` | Read supported widget or parent-slot properties without compiling or saving |
| `ReadTurboMCPCommonButtonStyle` | Inspect or change explicit shared CommonButtonStyle Blueprint defaults |
| `ReadTurboMCPWidgetTree` | Inspect selected Widget Blueprint hierarchy data; loads the asset without explicitly editing, compiling or saving it |
| `ReadTurboMCPWidgetInventory` | Find Widget Blueprints by class, references, overrides and graph nodes |
| `ReadTurboMCPPureChecks` | Inspect call-cost string normalization and asset extraction for literal inputs; does not run the named tools or read the call log |
| `ReadTurboMCPVcsState` | Query the selected source-control provider; may refresh cached state and contact its server, but does not request checkout, save or submission |

## UTurboMCPLookToolset

6 declared tools.

| Function | Description |
| --- | --- |
| `LookTurboMCPToolset` | Look for a live editor window that a capture could use |
| `LookTurboMCPLayoutReport` | Inspect graph geometry and layout findings; loads the Blueprint and measures Slate geometry without requesting graph edits, compile or save |
| `LookTurboMCPLayoutDoneCheck` | Return layout findings and capture the same graph scope; may open editor UI and writes an image file, but does not request asset edits, compile or save |
| `LookTurboMCPGraphPositions` | Read node IDs, stored positions and measured sizes; loads the Blueprint and uses rendering without requesting graph edits, compile or save |
| `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 |
| `LookTurboMCPCapture` | Capture a graph, asset editor, active window or live PIE game layer as a PNG. Start with Mode=graph for an overview |

## UTurboMCPPreviewToolset

68 declared tools.

| Function | Description |
| --- | --- |
| `PreviewTurboMCPAssetDelete` | Check one saved asset for deletion without deleting it; loading and garbage collection may run |
| `PreviewTurboMCPFunctionLibraryCreate` | Validate a new Blueprint function library at an unused destination; no disk save or overwrite |
| `PreviewTurboMCPDataAssetCreate` | Create a fresh typed Data Asset, preserving class defaults |
| `PreviewTurboMCPPlayInvoke` | Validate a scoped PIE runtime call without executing it |
| `PreviewTurboMCPCustomEventAdd` | Validate creation of a standalone typed custom event in an existing event graph |
| `PreviewTurboMCPDiagnosticConsole` | Preview a diagnostic query or process-wide change without executing it; obtain a single-use confirmation for changes |
| `PreviewTurboMCPDataAssetProperty` | Preview one data-asset property using temporary validation |
| `PreviewTurboMCPInterfaceRemove` | Remove a directly implemented interface while keeping its function bodies as ordinary functions |
| `PreviewTurboMCPDispatcherAdd` | Declare an event dispatcher with named, typed input parameters |
| `PreviewTurboMCPFunctionRename` | Rename an owned Blueprint function and discovered callers |
| `PreviewTurboMCPFunctionFlags` | Inspect or change owned function purity and receiver constness |
| `PreviewTurboMCPMergeMembers` | Prepare missing member declarations before copying graph bodies between Blueprints |
| `PreviewTurboMCPGraphTransfer` | Copy one complete owned graph, including its nodes, connections, layout, comments and locals |
| `PreviewTurboMCPWidgetProperty` | Validate and preview one widget or parent-slot property; returns before/after values without changing the widget |
| `PreviewTurboMCPCommonButtonStyle` | Inspect or change explicit shared CommonButtonStyle Blueprint defaults |
| `PreviewTurboMCPWidgetAdd` | Preview adding a designer widget; loads classes but constructs no widget and makes no explicit edit |
| `PreviewTurboMCPWidgetRemove` | Preview removal; no mutation is performed |
| `PreviewTurboMCPWidgetSubtreeCopy` | Copy one widget and its descendants into another Widget Blueprint |
| `PreviewTurboMCPInterfaceAdd` | Add a Blueprint interface contract and its required function declarations |
| `PreviewTurboMCPLocalVariableAdd` | Declare a typed local variable in one function; do not create getter/setter nodes |
| `PreviewTurboMCPToolset` | Report registered schema sizes without invoking tools or changing assets |
| `PreviewTurboMCPEnumAdd` | Inspect a proposed new enum and check for an existing asset; does not create, compile or save an asset |
| `PreviewTurboMCPVarsAdd` | Parse a variable creation request; may load referenced types but does not add variables, compile the target or save |
| `PreviewTurboMCPVariableRemove` | Remove one unused Blueprint-owned member variable after reference inspection; never deletes graph nodes intentionally |
| `PreviewTurboMCPVariableRename` | Preview an owned member-variable rename and list affected Blueprint assets and getter/setter references |
| `PreviewTurboMCPLocalVariableRemove` | Remove one unused local declaration from an exact owned function |
| `PreviewTurboMCPLocalVariableRename` | Rename one local variable and its exact local getter/setter references |
| `PreviewTurboMCPOverrideAdd` | Preview an event or function override; loads the Blueprint and inspects editor metadata without creating the override, compiling or saving |
| `PreviewTurboMCPWidgetRetarget` | Inspect widget bindings for missing widgets or delegates without validating function signatures, removing bindings, compiling or saving |
| `PreviewTurboMCPComment` | Plan comment changes; loads the Blueprint and may measure transient graph widgets, without applying edits, compiling or saving |
| `PreviewTurboMCPGraphDead` | Identify removal candidates using static graph reachability; loads the Blueprint without requesting edits, compilation or saving |
| `PreviewTurboMCPLocalVars` | Inspect repeated computed outputs, stable input-array counts and stock loop indexes in a Blueprint function; candidates and exclusions are advisory |
| `PreviewTurboMCPNodeType` | Search available node types; loads Blueprint context and queries editor node actions, without requesting graph edits, compilation or saving |
| `PreviewTurboMCPNodePins` | Inspect pins by constructing a temporary node; loads assets and executes editor probe code, without requesting compilation or saving |
| `PreviewTurboMCPGraphKnots` | Plan reroutes for crossing wires in one explicit Blueprint graph; measures native geometry without changing the live graph; straight-segment estimates do not certify rendered curves; no compile or save |
| `PreviewTurboMCPGraphTidy` | Plan a layout for one graph without committing graph changes |
| `PreviewTurboMCPGraphFixBackward` | Preview supported position repairs across one Blueprint graph; loads and measures transient graph widgets, but does not move live nodes, compile or save |
| `PreviewTurboMCPGraphCheck` | Check graph-text syntax, node types, pin names and supported literal types; loads the Blueprint for inspection but does not write graph changes, compile or save |
| `PreviewTurboMCPGraphAppend` | Preview adding nodes without editing; Placement=local searches nearby; Placement=outside uses space beyond existing bounds and may create long wires |
| `PreviewTurboMCPGraphReplace` | Plan graph replacement and list additions, updates and explicit deletions without editing, compiling or saving |
| `PreviewTurboMCPAssetUndo` | Inspect the latest global undo entry for the named Blueprint and return its confirmation GUID; does not undo |
| `PreviewTurboMCPGraphGroupMove` | Check a group translation; loads the Blueprint and measures transient graph widgets, without requesting live node moves, compilation or saving |
| `PreviewTurboMCPGraphGroupPlacement` | Suggest a group translation; loads the Blueprint and renders transient graph widgets for measurement, without requesting live node moves, compilation or saving |
| `PreviewTurboMCPAssetRevert` | Preview discarding all unsaved changes in a saved Blueprint or data-asset package; does not reload or save |
| `PreviewTurboMCPGraphSwapCall` | Preview call swap |
| `PreviewTurboMCPGraphRepoint` | Plan moving consumers to another output; loads the Blueprint and may construct a probe node, without applying links, compiling or saving |
| `PreviewTurboMCPBatchEdit` | Simulate ordered repoints on a transient graph; loads the Blueprint without applying live edits, compiling or saving |
| `PreviewTurboMCPGraphConnect` | Connect exact existing node/pin IDs |
| `PreviewTurboMCPGraphRemove` | Remove exact nodes and links with scratch preview and one undo |
| `PreviewTurboMCPBindFunction` | Preview connecting an existing function through a Create Event node on a transient graph; no live graph edits, compile or save |
| `PreviewTurboMCPNodePinGrowth` | Preview growth of variadic pins on Make Array, Sequence or associative operators; does not shrink, compile or save |
| `PreviewTurboMCPVariableFlags` | Preview flags on an existing owned member variable; preserves identity and graph references, without compile or save |
| `PreviewTurboMCPWidgetEvent` | Preview a designer widget delegate event; returns pins and an existing/new node ID on edit, without compile or save |
| `PreviewTurboMCPBlueprintReparent` | Preview Blueprint reparenting; execution refreshes nodes and compiles but never saves, and undo is not guaranteed across reinstancing |
| `PreviewTurboMCPDelegateRetarget` | Preview the function selected by an existing Create Event node in place; preserves its identity, object input and consumers, with no compile or save |
| `PreviewTurboMCPBindNewEvent` | Preview creating a typed custom event wired to an existing delegate input, such as the Event pin of a Bind Event node |
| `PreviewTurboMCPGraphSplitValue` | Preview copies of a shared pure value on a transient graph without editing, compiling or saving the asset; requires an idle editor |
| `PreviewTurboMCPGraphSequence` | Preview ordered Sequence rows on a transient graph without editing the asset; no compile or save, and requires an idle editor |
| `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ |
| `PreviewTurboMCPMaterialTidy` | Plan material positions and descriptive column boxes; does not compile, apply layout or save; accepts Materials only, not functions or instances |
| `PreviewTurboMCPMaterialLabelPromote` | Plan named reroutes for exact material outputs read by multiple expressions |
| `PreviewTurboMCPMaterialBlocks` | Plan material label and reroute cleanup using the loaded graph |
| `PreviewTurboMCPDataTableRows` | Inspect proposed row edits using temporary row data; loads the table and may load objects referenced by imported values; does not write rows, compile or save |
| `PreviewTurboMCPGraphCopy` | Plan a node copy and report external links; loads both Blueprints without applying the copy, compiling or saving |
| `PreviewTurboMCPGraphCollapse` | Extract a supported selection into a new function |
| `PreviewTurboMCPStructNew` | Inspect a proposed user-defined struct; type resolution may load referenced assets; does not create, compile or save the struct |
| `PreviewTurboMCPStructFields` | Inspect proposed struct field edits; loads the struct and referenced types and queries source-control status; does not edit, compile, check out or save the struct |
| `PreviewTurboMCPVcsCheckout` | Inspect checkout eligibility; refreshes provider state and may contact the server, but does not request checkout, asset compilation or saving |

## UTurboMCPEditToolset

81 declared tools.

| Function | Description |
| --- | --- |
| `EditTurboMCPAssetDelete` | Permanently delete one saved asset after fresh checks and preview confirmation; no undo or automatic retry |
| `EditTurboMCPFunctionLibraryCreate` | Create a new Blueprint function library at an unused destination; no disk save or overwrite |
| `EditTurboMCPDataAssetCreate` | Create a fresh typed Data Asset, preserving class defaults |
| `EditTurboMCPPlayInvoke` | Validate a scoped PIE runtime call and execute it once with preview confirmation |
| `EditTurboMCPCustomEventAdd` | Create a standalone typed custom event in an existing event graph |
| `EditTurboMCPDiagnosticConsole` | Run a diagnostic query or a separately confirmed process-wide change; dispatch alone does not verify later effects |
| `EditTurboMCPDataAssetProperty` | Edit one data-asset property using temporary validation |
| `EditTurboMCPInterfaceRemove` | Remove a directly implemented interface while keeping its function bodies as ordinary functions |
| `EditTurboMCPMenuActivate` | Request activation of one named runtime button through normal Slate input |
| `EditTurboMCPDispatcherAdd` | Declare an event dispatcher with named, typed input parameters |
| `EditTurboMCPFunctionRename` | Rename an owned Blueprint function and discovered callers |
| `EditTurboMCPFunctionFlags` | Inspect or change owned function purity and receiver constness |
| `EditTurboMCPMergeMembers` | Prepare missing member declarations before copying graph bodies between Blueprints |
| `EditTurboMCPGraphTransfer` | Copy one complete owned graph, including its nodes, connections, layout, comments and locals |
| `EditTurboMCPWidgetSubtreeCopy` | Copy one widget and its descendants into another Widget Blueprint |
| `EditTurboMCPInterfaceAdd` | Add a Blueprint interface contract and its required function declarations |
| `EditTurboMCPLocalVariableAdd` | Declare a typed local variable in one function; do not create getter/setter nodes |
| `EditTurboMCPPlayInput` | Route bounded keyboard/gamepad input through Slate in PIE; executes game/UI code |
| `ReadTurboMCPEditQueue` | Inspect editor readiness for ordinary transactional asset edits; does not edit, compile or save |
| `EditTurboMCPCompile` | Compile up to 25 named assets and report compiler and engine diagnostics; does not save |
| `EditTurboMCPSave` | Write named asset packages to disk, including packages whose dirty flag is clear; does not explicitly compile |
| `EditTurboMCPSaveWithoutThumbnail` | Write named asset packages directly to disk without refreshing thumbnails; does not explicitly compile |
| `EditTurboMCPEnumAdd` | Create a new user-defined enum in memory in an idle editor outside PIE and active transactions; marks its package dirty and may run enum callbacks, with no tool-owned undo transaction, explicit Blueprint compile or save |
| `EditTurboMCPVarsAdd` | Add Blueprint variables; may regenerate the skeleton repeatedly and run editor callbacks, can leave partial changes without rollback, and does not perform a full compile or save or create a dedicated undo transaction |
| `EditTurboMCPVariableRemove` | Remove one unused Blueprint-owned member variable after reference inspection; never deletes graph nodes intentionally |
| `EditTurboMCPVariableRename` | Rename a member and supported getter/setter references using fresh confirmation; refreshes skeletons, so compile affected assets separately before saving |
| `EditTurboMCPLocalVariableRemove` | Remove one unused local declaration from an exact owned function |
| `EditTurboMCPLocalVariableRename` | Rename one local variable and its exact local getter/setter references |
| `EditTurboMCPWidgetProperty` | Set one supported widget or parent-slot property in memory; returns before/after values read after native editor notification |
| `EditTurboMCPCommonButtonStyle` | Inspect or change explicit shared CommonButtonStyle Blueprint defaults |
| `EditTurboMCPWidgetAdd` | Add one designer widget at the end of a named panel using Unreal's palette operation |
| `EditTurboMCPWidgetRemove` | Remove the widget subtree through the stock editor operation, with an undo transaction and structural refresh |
| `EditTurboMCPWidgetRename` | Rename an existing designer widget using Unreal's undoable editor operation |
| `EditTurboMCPWidgetsAsVars` | Set named widgets' Is Variable flags; changes may regenerate the Blueprint skeleton and run editor callbacks, do not perform a full compile or save, and have no dedicated undo transaction |
| `EditTurboMCPOverrideAdd` | Create an event or function override through the editor API; may regenerate the skeleton and run editor callbacks, uses engine-managed undo, and does not perform a full compile or save |
| `EditTurboMCPOverrideRemove` | Delete an override event node or an entire override function graph and its contents; uses an undo transaction, may regenerate the skeleton and run editor callbacks, and does not perform a full compile or save |
| `EditTurboMCPFunctionAdd` | Create a function graph and optional signature; loads types, regenerates the skeleton and refreshes matching self-call nodes, may run editor callbacks, and does not perform a full compile or save or provide a dedicated undo transaction |
| `EditTurboMCPFunctionRemove` | Delete a function graph and all its nodes without repairing callers or bindings; no full compile, save or dedicated undo transaction, and editor callbacks may close asset editors and regenerate the skeleton |
| `EditTurboMCPWidgetRetarget` | Remove bindings whose widget or delegate is missing without validating function signatures; changes stay in memory with no compile, save or dedicated undo transaction |
| `EditTurboMCPWidgetBindAdd` | Add or replace a single-cast designer binding after checking the reflected function signature and property purity |
| `EditTurboMCPComment` | Create, change, resize or delete comment boxes in an undo transaction; loads/measures the graph and does not compile or save |
| `EditTurboMCPGraphDead` | Delete statically selected node groups and their links in one undo transaction; requires an idle editor outside PIE; does not compile or save |
| `EditTurboMCPGraphKnots` | Insert planned reroutes in one undo transaction without moving existing nodes; requires an idle editor and explicit graph; partial results require inspection before saving; no automatic compile or save |
| `EditTurboMCPGraphTidy` | Recompute and apply a layout using the same options as PreviewTurboMCPGraphTidy |
| `EditTurboMCPGraphFixBackward` | Replan and apply supported position repairs across one Blueprint graph in one undo transaction; marks the Blueprint modified, but does not compile or save |
| `EditTurboMCPLocalVars` | Promote reviewed scalar arithmetic or explicitly selected immutable input-array counts at entry, and selected stable stock loop indexes at LoopBody, retaining original calculations |
| `EditTurboMCPCallLog` | Enable or disable call recording and request persistence in editor settings |
| `EditTurboMCPCallLogClear` | Clear retained in-memory call records after confirmation |
| `EditTurboMCPTrace` | Manage in-memory trace points; does not edit graph nodes, compile or save assets, and clearing recorded hits is not undoable |
| `EditTurboMCPGraphAppend` | Add nodes and connections without deleting existing nodes; Placement=local searches nearby; Placement=outside uses space beyond existing bounds and may create long wires |
| `EditTurboMCPGraphReplace` | Replace supported graph content from a fresh complete edit export; only explicit deletions are allowed, in one undo transaction without compile or save |
| `EditTurboMCPAssetUndo` | Undo the latest global TurboMCP transaction for the named Blueprint; does not explicitly compile or save |
| `EditTurboMCPGraphGroupMove` | Move selected nodes and comment contents in one undo transaction; loads and measures the graph, marks the Blueprint modified, and does not compile or save |
| `EditTurboMCPAssetRevert` | Reload a saved Blueprint or data-asset package, discarding ALL unsaved changes approved by preview; never saves |
| `EditTurboMCPGraphPositions` | Set exact node positions in one undo step; not tidy and overlaps are allowed; moving a comment moves only its box, not its contents; requires an idle editor; no compile or save |
| `EditTurboMCPGraphSwapCall` | Replace one call node and transfer its compatible links and unconnected input defaults |
| `EditTurboMCPGraphRepoint` | Move consumers to another output, optionally creating a target and deleting the source, in one undo transaction; does not compile or save |
| `EditTurboMCPBatchEdit` | Apply ordered repoints in one undo transaction, including requested source deletion; loads the Blueprint and does not compile or save |
| `EditTurboMCPGraphConnect` | Connect exact existing node/pin IDs |
| `EditTurboMCPGraphRemove` | Remove exact nodes and links with scratch preview and one undo |
| `EditTurboMCPBindFunction` | Connect an existing function through a Create Event node in one undo transaction; replaces the target input and marks the asset modified, with no compile or save |
| `EditTurboMCPNodePinGrowth` | Edit growth of variadic pins on Make Array, Sequence or associative operators; does not shrink, compile or save |
| `EditTurboMCPVariableFlags` | Edit flags on an existing owned member variable; preserves identity and graph references, without compile or save |
| `EditTurboMCPWidgetEvent` | Edit a designer widget delegate event; returns pins and an existing/new node ID on edit, without compile or save |
| `EditTurboMCPBlueprintReparent` | Edit Blueprint reparenting; execution refreshes nodes and compiles but never saves, and undo is not guaranteed across reinstancing |
| `EditTurboMCPDelegateRetarget` | Edit the function selected by an existing Create Event node in place; preserves its identity, object input and consumers, with no compile or save |
| `EditTurboMCPBindNewEvent` | Create a typed custom event wired to an existing delegate input in one undo transaction |
| `EditTurboMCPGraphSplitValue` | Copy a shared pure value per connected input in one undo transaction; removes the original and marks the asset modified, without compile, save or automatic tidy |
| `EditTurboMCPGraphSequence` | Replace an ordered synchronous call chain with Sequence rows in one undo transaction; marks the asset modified but does not compile, save or tidy it |
| `EditTurboMCPPinDefault` | Set one unconnected K2 input pin default; records an undo transaction and marks the Blueprint dirty; schema callbacks may run and object literals may load assets; does not explicitly compile or save |
| `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch |
| `EditTurboMCPMaterialTidy` | Move material expressions and write comment boxes in an undo transaction; recompiles before and after layout and runs editor callbacks, but does not save; failure cleanup is attempted, not certified restoration |
| `EditTurboMCPMaterialLabelPromote` | Promote each exact material output and mask read by at least two distinct direct expressions to named reroutes |
| `EditTurboMCPMaterialBlocks` | Add labels, rewire inputs and delete redundant usage/reroute nodes in an undo transaction; recompiles materials and runs editor callbacks, does not save, and verifies only expression identities after attempted rollback |
| `EditTurboMCPMaterialInstanceSet` | Set texture overrides on a Material Instance Constant; loads the instance and textures and runs PostEditChange, which may update rendering resources; no tool-owned undo transaction or save is performed |
| `EditTurboMCPDataTableRows` | Set or remove DataTable rows in an undo transaction; removals discard row data and edits may run editor callbacks and mark the package dirty; does not explicitly compile or save |
| `EditTurboMCPGraphCopy` | Copy selected nodes into a different graph in one undo transaction; preserves the source and does not compile or save |
| `EditTurboMCPGraphCollapse` | Extract a supported selection into a new function |
| `EditTurboMCPStructNew` | Create and compile a user-defined struct in memory within an undo transaction in an idle editor outside PIE; marks its package dirty and may run editor callbacks; does not save |
| `EditTurboMCPStructFields` | Add, change, reorder or remove struct fields in an undo transaction in an idle editor outside PIE; may check out files, compile structs and trigger dependent editor updates; removal or type changes may lose data; does not save |
| `EditTurboMCPVcsCheckout` | Request provider checkout per asset; may contact the server and change source-control or file state, with no automatic rollback of partial success and no asset compile/save request |

### Replace a supported Blueprint graph

1. Call `ReadTurboMCPGraphLoad` with `Format=edit` for the exact authored graph. Use offset zero, no entry filter, and the first revision. Check that the first line is a `tmcp-edit-v1` header with `"complete":true`. Keep that header and the full lowercase GUID on each retained node form.
2. To delete a node, remove its form and add one `; tmcp-delete-v1: {"nodeId":"<32 lowercase hex digits>"}` line. Update each retained input that referred to its outputs with the intended replacement link or literal. A missing node form or incomplete input is refused. Entry, result, event and comment nodes cannot be deleted by Replace.
3. To add a node, use the existing node type syntax and explicit input lines. Use `; label Name` on its head and `(-> Name:OutputPin)` wherever a link refers to it. For an existing node, use the full GUID in a link. Existing geometry and unedited pin defaults are preserved. New execution nodes must have a path from an entry.
4. Call `PreviewTurboMCPGraphReplace` and inspect each listed addition, update and deletion. Submit the same text to `EditTurboMCPGraphReplace` if the preview is correct. If the graph changes, read a fresh edit export and review the changes again.

The initial edit format covers stock K2 call, branch and variable nodes and existing stock function and event terminators. It refuses graph states it cannot represent, such as function locals, unsupported node classes, split or orphaned pins, ambiguous endpoints and incomplete links. Edit exports cannot be paginated; a result over the character limit is incomplete. Inspection formats such as `read` and `json` remain available. Use the relevant targeted tool when Replace cannot express the intended change. Replace does not compile or save.
