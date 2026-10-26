# Tool reference

[Documentation](../README.md) · [Installation](installation.md)

Generated from plugin declarations. For validation and compatibility limits, see [project status](../status.md).

- [UTurboMCPReadToolset](#uturbomcpreadtoolset)
- [UTurboMCPLookToolset](#uturbomcplooktoolset)
- [UTurboMCPPreviewToolset](#uturbomcppreviewtoolset)
- [UTurboMCPEditToolset](#uturbomcpedittoolset)

## UTurboMCPReadToolset

32 declared tools.

| Function | Description |
| --- | --- |
| `ReadTurboMCPPlayInputState` | Inspect the single PIE player's focus, held synthetic input and Enhanced Input context priorities; no input or asset mutation |
| `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state |
| `ReadTurboMCPToolIndex` | Read every tool the server serves, one line each |
| `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool |
| `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search |
| `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form |
| `ReadTurboMCPTypeFind` | Read the exact type path a pin or a variable needs |
| `ReadTurboMCPDirtyList` | Read every asset with unsaved changes |
| `ReadTurboMCPNodeResolve` | Resolve node identities and inspect pins; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPNodeContext` | Inspect static execution context and input sources; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPNodeFind` | Search Blueprint nodes by text; may load assets, without requesting edits, compile or save |
| `ReadTurboMCPGraphLoad` | Read one graph as text |
| `ReadTurboMCPBlueprintLoad` | Inspect Blueprint structure and selected graph content; loads assets without requesting edits, compile or save |
| `ReadTurboMCPRefs` | Read saved registry references into or out of an asset |
| `ReadTurboMCPOverrides` | Read available and implemented Blueprint overrides from parent classes and interfaces |
| `ReadTurboMCPCommentList` | Read Blueprint comment rectangles, membership and nesting |
| `ReadTurboMCPDataTable` | Read DataTable schema, row names, selected values and registry-based readers |
| `ReadTurboMCPCallLog` | Read retained call-log records, newest first |
| `ReadTurboMCPCallCost` | Summarize retained call costs, durations and failures |
| `ReadTurboMCPToolSchema` | Inspect registered tool metadata and local trap notes without invoking the described tool |
| `ReadTurboMCPMaterialLabels` | Read named material reroute labels, writers and reader counts; no explicit edit, compile or save |
| `ReadTurboMCPMaterialLoad` | Read material parameters and connected output trees, or an instance override summary |
| `ReadTurboMCPMaterialTrace` | Read one connected material output tree; no explicit edit, compile or save |
| `ReadTurboMCPMaterialBlocks` | Read material logic grouped by comment boxes; no explicit edit, compile or save |
| `ReadTurboMCPMaterialInstance` | Read material-instance parent chain, local overrides, inherited values and orphaned overrides |
| `ReadTurboMCPPlayCheck` | Start or poll a timed Play In Editor session that executes project code with editor permissions, is not sandboxed or automatically undone, and does not explicitly save assets |
| `ReadTurboMCPTrace` | Report the trace subscription, armed points and retained hits |
| `ReadTurboMCPBPLint` | Inspect Blueprints for selected lint findings; loads assets but does not explicitly edit, compile or save them |
| `ReadTurboMCPAssetCost` | Report recorded package disk sizes and direct registry dependencies; does not explicitly edit, compile or save assets |
| `ReadTurboMCPWidgetTree` | Inspect selected Widget Blueprint hierarchy data; loads the asset without explicitly editing, compiling or saving it |
| `ReadTurboMCPPureChecks` | Inspect call-cost string normalization and asset extraction for literal inputs; does not run the named tools or read the call log |
| `ReadTurboMCPVcsState` | Query the selected source-control provider; may refresh cached state and contact its server, but does not request checkout, save or submission |

## UTurboMCPLookToolset

6 declared tools.

| Function | Description |
| --- | --- |
| `LookTurboMCPToolset` | Look for a live editor window that a capture could use |
| `LookTurboMCPLayoutReport` | Inspect graph geometry and layout findings; loads the Blueprint and measures Slate geometry without requesting graph edits, compile or save |
| `LookTurboMCPLayoutDoneCheck` | Return layout findings and capture a graph image; may open editor UI and writes an image file, but does not request asset edits, compile or save |
| `LookTurboMCPGraphPositions` | Read node IDs, stored positions and measured sizes; loads the Blueprint and uses rendering without requesting graph edits, compile or save |
| `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 |
| `LookTurboMCPCapture` | Capture a graph, asset editor, active window or live PIE game layer as a PNG. Start with Mode=graph for an overview |

## UTurboMCPPreviewToolset

41 declared tools.

| Function | Description |
| --- | --- |
| `PreviewTurboMCPInterfaceAdd` | Preview adding an implemented interface on a Blueprint or Widget Blueprint |
| `PreviewTurboMCPLocalVariableAdd` | Preview a typed local declaration in one owned function |
| `PreviewTurboMCPToolset` | Preview the size of this plugin's own tool help |
| `PreviewTurboMCPEnumAdd` | Inspect a proposed new enum and check for an existing asset; does not create, compile or save an asset |
| `PreviewTurboMCPVarsAdd` | Parse a variable creation request; may load referenced types but does not add variables, compile the target or save |
| `PreviewTurboMCPOverrideAdd` | Preview an event or function override; loads the Blueprint and inspects editor metadata without creating the override, compiling or saving |
| `PreviewTurboMCPWidgetRetarget` | Inspect widget bindings for missing widgets or delegates; loads the asset but does not remove bindings, compile or save it |
| `PreviewTurboMCPComment` | Plan comment changes; loads the Blueprint and may measure transient graph widgets, without applying edits, compiling or saving |
| `PreviewTurboMCPGraphDead` | Identify removal candidates using static graph reachability; loads the Blueprint without requesting edits, compilation or saving |
| `PreviewTurboMCPLocalVars` | Inspect repeated pure-node outputs in a Blueprint function; loads the asset but does not edit, compile or save; candidates are advisory; edit requires reviewed scalar arithmetic, stable inputs, a clean compile baseline and one connected execution successor |
| `PreviewTurboMCPNodeType` | Search available node types; loads Blueprint context and queries editor node actions, without requesting graph edits, compilation or saving |
| `PreviewTurboMCPNodePins` | Inspect pins by constructing a temporary node; loads assets and executes editor probe code, without requesting compilation or saving |
| `PreviewTurboMCPGraphKnots` | Plan reroute nodes for estimated wire crossings; loads the Blueprint without requesting graph edits, compilation or saving |
| `PreviewTurboMCPGraphTidy` | Plan a layout for one graph without committing graph changes |
| `PreviewTurboMCPGraphFixBackward` | Repair the bad spots in a mostly-good graph, WITHOUT moving anything |
| `PreviewTurboMCPGraphCheck` | Check graph-text syntax, node types, pin names and supported literal types; loads the Blueprint for inspection but does not write graph changes, compile or save |
| `PreviewTurboMCPGraphAppend` | Add a block to a graph from graph text, and write nothing |
| `PreviewTurboMCPGraphReplace` | Plan graph replacement and list deletions; loads the Blueprint without applying the replacement, compiling or saving |
| `PreviewTurboMCPAssetUndo` | Inspect the latest global undo entry for the named Blueprint and return its confirmation GUID; does not undo |
| `PreviewTurboMCPGraphGroupMove` | Check a group translation; loads the Blueprint and measures transient graph widgets, without requesting live node moves, compilation or saving |
| `PreviewTurboMCPGraphGroupPlacement` | Suggest a group translation; loads the Blueprint and renders transient graph widgets for measurement, without requesting live node moves, compilation or saving |
| `PreviewTurboMCPAssetRevert` | Preview discarding all unsaved changes in a saved Blueprint package; does not reload or save |
| `PreviewTurboMCPGraphSwapCall` | Preview call swap |
| `PreviewTurboMCPGraphRepoint` | Plan moving consumers to another output; loads the Blueprint and may construct a probe node, without applying links, compiling or saving |
| `PreviewTurboMCPBatchEdit` | Simulate ordered repoints on a transient graph; loads the Blueprint without applying live edits, compiling or saving |
| `PreviewTurboMCPGraphConnect` | Connect exact existing node/pin IDs |
| `PreviewTurboMCPGraphRemove` | Remove exact nodes and links with scratch preview and one undo |
| `PreviewTurboMCPBindFunction` | Load the Blueprint and simulate delegate binding on a transient graph; no live graph edits, compilation or saving are requested |
| `PreviewTurboMCPBindNewEvent` | Preview creating a typed custom event wired to an existing delegate input, such as the Event pin of a Bind Event node |
| `PreviewTurboMCPGraphSplitValue` | Copy a pure self/getter/literal per consumer; never impure calls |
| `PreviewTurboMCPGraphSequence` | Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows |
| `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ |
| `PreviewTurboMCPMaterialTidy` | Plan material positions and comment boxes; loads the asset and recompiles materials to collect baseline errors, may run editor callbacks, but does not apply layout or save |
| `PreviewTurboMCPMaterialLabelPromote` | Plan named reroutes for material values used by multiple expressions; loads and recompiles materials for baseline errors and may run editor callbacks, but does not add labels, rewire inputs or save |
| `PreviewTurboMCPMaterialBlocks` | Plan material label and reroute cleanup; loads and recompiles materials for baseline errors and may run editor callbacks, but does not apply graph changes or save |
| `PreviewTurboMCPDataTableRows` | Inspect proposed row edits using temporary row data; loads the table and may load objects referenced by imported values; does not write rows, compile or save |
| `PreviewTurboMCPGraphCopy` | Plan a node copy and report external links; loads both Blueprints without applying the copy, compiling or saving |
| `PreviewTurboMCPGraphCollapse` | Collapse is not implemented: inspect selection boundaries only; may load the Blueprint, but does not extract nodes, create a member, compile or save |
| `PreviewTurboMCPStructNew` | Inspect a proposed user-defined struct; type resolution may load referenced assets; does not create, compile or save the struct |
| `PreviewTurboMCPStructFields` | Inspect proposed struct field edits; loads the struct and referenced types and queries source-control status; does not edit, compile, check out or save the struct |
| `PreviewTurboMCPVcsCheckout` | Inspect checkout eligibility; refreshes provider state and may contact the server, but does not request checkout, asset compilation or saving |

## UTurboMCPEditToolset

52 declared tools.

| Function | Description |
| --- | --- |
| `EditTurboMCPInterfaceAdd` | Add an implemented interface on a Blueprint or Widget Blueprint |
| `EditTurboMCPLocalVariableAdd` | Add a typed local declaration in one owned function |
| `EditTurboMCPPlayInput` | Execute bounded keyboard/gamepad input through Slate in live PIE. Runtime exception to asset-edit tools: runs game/UI code, no asset transaction/save/undo |
| `ReadTurboMCPEditQueue` | Read whether the editor can accept a write right now |
| `EditTurboMCPCompile` | Compile up to 25 named assets and report compiler and engine diagnostics; does not save |
| `EditTurboMCPSave` | Write named asset packages to disk, including packages whose dirty flag is clear; does not explicitly compile |
| `EditTurboMCPSaveWithoutThumbnail` | Write named asset packages directly to disk without refreshing thumbnails; does not explicitly compile |
| `EditTurboMCPEnumAdd` | Create a new user-defined enum in memory and mark its package dirty; editor enum callbacks may run; no tool-owned undo transaction, explicit Blueprint compile or save is performed |
| `EditTurboMCPVarsAdd` | Add Blueprint variables; may regenerate the skeleton repeatedly and run editor callbacks, can leave partial changes without rollback, and does not perform a full compile or save or create a dedicated undo transaction |
| `EditTurboMCPWidgetsAsVars` | Set named widgets' Is Variable flags; changes may regenerate the Blueprint skeleton and run editor callbacks, do not perform a full compile or save, and have no dedicated undo transaction |
| `EditTurboMCPOverrideAdd` | Create an event or function override through the editor API; may regenerate the skeleton and run editor callbacks, uses engine-managed undo, and does not perform a full compile or save |
| `EditTurboMCPOverrideRemove` | Delete an override event node or an entire override function graph and its contents; uses an undo transaction, may regenerate the skeleton and run editor callbacks, and does not perform a full compile or save |
| `EditTurboMCPFunctionAdd` | Create a function graph and optional signature; loads types, regenerates the skeleton and refreshes matching self-call nodes, may run editor callbacks, and does not perform a full compile or save or provide a dedicated undo transaction |
| `EditTurboMCPFunctionRemove` | Delete a function graph and all its nodes; may close asset editors, regenerate the skeleton and run editor callbacks, does not perform a full compile or save, and provides no dedicated undo transaction |
| `EditTurboMCPWidgetRetarget` | Remove bindings whose widget or delegate is missing; changes the asset in memory without compiling or saving, and does not create a dedicated undo transaction |
| `EditTurboMCPWidgetBindAdd` | Add or replace a single-cast designer binding after checking the reflected function signature and property purity |
| `EditTurboMCPComment` | Create, change, resize or delete comment boxes in an undo transaction; loads/measures the graph and does not compile or save |
| `EditTurboMCPGraphDead` | Delete statically selected node groups and their links; no tool-owned undo transaction, compilation or save is performed |
| `EditTurboMCPGraphKnots` | Insert reroute nodes and reconnect wires in an undo transaction; attempts rollback if estimated crossings do not decrease, without compiling or saving |
| `EditTurboMCPGraphTidy` | Recompute and apply a layout using the same options as PreviewTurboMCPGraphTidy |
| `EditTurboMCPGraphFixBackward` | Apply the repairs the preview listed |
| `EditTurboMCPLocalVars` | Promote repeated reviewed scalar arithmetic values in a Blueprint function: retain calculations, assign locals at function entry and replace consumer reads |
| `EditTurboMCPCallLog` | Enable or disable call recording and request persistence in editor settings |
| `EditTurboMCPCallLogClear` | Clear retained in-memory call records after confirmation |
| `EditTurboMCPTrace` | Manage in-memory trace points; does not edit graph nodes, compile or save assets, and clearing recorded hits is not undoable |
| `EditTurboMCPGraphAppend` | Add nodes and connections to an existing graph without deleting existing nodes |
| `EditTurboMCPGraphReplace` | Replace graph content and delete omitted eligible nodes in one undo transaction; loads the Blueprint, but does not compile or save |
| `EditTurboMCPAssetUndo` | Undo the latest global TurboMCP transaction for the named Blueprint; does not explicitly compile or save |
| `EditTurboMCPGraphGroupMove` | Move selected nodes and comment contents in one undo transaction; loads and measures the graph, marks the Blueprint modified, and does not compile or save |
| `EditTurboMCPAssetRevert` | Reload a saved Blueprint package, discarding ALL unsaved changes, including edits made after preview; does not save |
| `EditTurboMCPGraphPositions` | Set node positions in one undo step |
| `EditTurboMCPGraphSwapCall` | Replace one call node and transfer its compatible links and unconnected input defaults |
| `EditTurboMCPGraphRepoint` | Move consumers to another output, optionally creating a target and deleting the source, in one undo transaction; does not compile or save |
| `EditTurboMCPBatchEdit` | Apply ordered repoints in one undo transaction, including requested source deletion; loads the Blueprint and does not compile or save |
| `EditTurboMCPGraphConnect` | Connect exact existing node/pin IDs |
| `EditTurboMCPGraphRemove` | Remove exact nodes and links with scratch preview and one undo |
| `EditTurboMCPBindFunction` | Create a delegate binding and replace its target input connection in one undo transaction; loads the Blueprint, but does not compile or save |
| `EditTurboMCPBindNewEvent` | Create a typed custom event wired to an existing delegate input in one undo transaction |
| `EditTurboMCPGraphSplitValue` | Copy a pure self/getter/literal per consumer; never impure calls |
| `EditTurboMCPGraphSequence` | Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows |
| `EditTurboMCPPinDefault` | Set one unconnected K2 input pin default; records an undo transaction and marks the Blueprint dirty; schema callbacks may run and object literals may load assets; does not explicitly compile or save |
| `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch |
| `EditTurboMCPMaterialTidy` | Move material expressions and write comment boxes in an undo transaction; recompiles before and after layout and runs editor callbacks, but does not save; failure cleanup is attempted, not certified restoration |
| `EditTurboMCPMaterialLabelPromote` | Add named reroute declarations and usage nodes and rewire material inputs in an undo transaction; recompiles materials and runs editor callbacks, does not save, and cannot guarantee restoration after a failed edit |
| `EditTurboMCPMaterialBlocks` | Add labels, rewire inputs and delete redundant usage/reroute nodes in an undo transaction; recompiles materials and runs editor callbacks, does not save, and verifies only expression identities after attempted rollback |
| `EditTurboMCPMaterialInstanceSet` | Set texture overrides on a Material Instance Constant; loads the instance and textures and runs PostEditChange, which may update rendering resources; no tool-owned undo transaction or save is performed |
| `EditTurboMCPDataTableRows` | Set or remove DataTable rows in an undo transaction; removals discard row data and edits may run editor callbacks and mark the package dirty; does not explicitly compile or save |
| `EditTurboMCPGraphCopy` | Copy selected nodes and reconnect planned links in an owned undo transaction; loads both Blueprints, but does not compile or save |
| `EditTurboMCPGraphCollapse` | Collapse is not implemented: inspect selection boundaries only; may load the Blueprint, but does not extract nodes, create a member, compile or save |
| `EditTurboMCPStructNew` | Create and compile a user-defined struct in memory within an undo transaction; marks its package dirty and may run editor callbacks; does not save |
| `EditTurboMCPStructFields` | Add, change, reorder or remove struct fields in an undo transaction; may check out files, compile structs and trigger dependent editor updates; removal or type changes may lose data; does not save |
| `EditTurboMCPVcsCheckout` | Request provider checkout per asset; may contact the server and change source-control or file state, with no automatic rollback of partial success and no asset compile/save request |
