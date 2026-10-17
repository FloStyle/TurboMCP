# Declared tool reference

Generated from the plugin header. Declarations are not release acceptance evidence.

| Toolset | Function | Description |
| --- | --- | --- |
| `UTurboMCPReadToolset` | `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state |
| `UTurboMCPReadToolset` | `ReadTurboMCPToolIndex` | Read every tool the server serves, one line each |
| `UTurboMCPReadToolset` | `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool |
| `UTurboMCPReadToolset` | `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search |
| `UTurboMCPReadToolset` | `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form |
| `UTurboMCPReadToolset` | `ReadTurboMCPTypeFind` | Read the exact type path a pin or a variable needs |
| `UTurboMCPReadToolset` | `ReadTurboMCPDirtyList` | Read every asset with unsaved changes |
| `UTurboMCPReadToolset` | `ReadTurboMCPNodeResolve` | Turn node IDs into nodes |
| `UTurboMCPReadToolset` | `ReadTurboMCPNodeFind` | Find nodes in a Blueprint by what they say |
| `UTurboMCPReadToolset` | `ReadTurboMCPGraphLoad` | Read one graph as text |
| `UTurboMCPReadToolset` | `ReadTurboMCPBlueprintLoad` | Read whole Blueprints: the main Read tool |
| `UTurboMCPReadToolset` | `ReadTurboMCPRefs` | Read what an asset references, and what references it |
| `UTurboMCPReadToolset` | `ReadTurboMCPOverrides` | Read what a Blueprint can override, and what it already does |
| `UTurboMCPReadToolset` | `ReadTurboMCPCommentList` | Read the comment boxes of a Blueprint's graphs as data |
| `UTurboMCPReadToolset` | `ReadTurboMCPDataTable` | Describe a data table |
| `UTurboMCPReadToolset` | `ReadTurboMCPCallLog` | Read the call log: what the server recorded about its own use |
| `UTurboMCPReadToolset` | `ReadTurboMCPCallCost` | What the calls COST: which were most expensive, which were slowest, which failed, and the totals per tool and per asset |
| `UTurboMCPReadToolset` | `ReadTurboMCPToolSchema` | The exact schema of one tool: its signature, one row per parameter, and its trap notes |
| `UTurboMCPReadToolset` | `ReadTurboMCPMaterialLabels` | Read the named reroutes of a material: every label, its readers, and the unused ones |
| `UTurboMCPReadToolset` | `ReadTurboMCPMaterialLoad` | Read a whole material: its parameters, then every connected output as a tree |
| `UTurboMCPReadToolset` | `ReadTurboMCPMaterialTrace` | Read ONE output of a material rather than all of them |
| `UTurboMCPReadToolset` | `ReadTurboMCPMaterialBlocks` | Read a material by BLOCK of logic, one block per comment box |
| `UTurboMCPReadToolset` | `ReadTurboMCPMaterialInstance` | Read a material INSTANCE: its parent chain, what it overrides, and - the list that matters - what it inherits from a parent it does not own |
| `UTurboMCPReadToolset` | `ReadTurboMCPPlayCheck` | Play the project for a few seconds and report the warnings and errors it raised |
| `UTurboMCPReadToolset` | `ReadTurboMCPTrace` | Read the trace: what is armed, and how often each point has run |
| `UTurboMCPReadToolset` | `ReadTurboMCPBPLint` | Lint one or more Blueprints and report what is wrong with them, with each check NAMED. `assets` takes one or more asset names |
| `UTurboMCPReadToolset` | `ReadTurboMCPAssetCost` | The size map and hard references of one asset, SPLIT BY PROVENANCE. `D-52`: *"A size is reported with its provenance |
| `UTurboMCPReadToolset` | `ReadTurboMCPWidgetTree` | Read a Widget Blueprint's tree as the text dialect of `04_FORMATS.md` section 8 |
| `UTurboMCPReadToolset` | `ReadTurboMCPPureChecks` | Call the two PURE functions behind `R-CALL-COST` with LITERAL inputs, and print what they returned |
| `UTurboMCPReadToolset` | `ReadTurboMCPVcsState` | Read what source control holds for each asset, and refuse by name when it holds nothing |
| `UTurboMCPLookToolset` | `LookTurboMCPToolset` | Look for a live editor window that a capture could use |
| `UTurboMCPLookToolset` | `LookTurboMCPLayoutReport` | Look at the layout of one graph: measure it and report, without moving anything |
| `UTurboMCPLookToolset` | `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 |
| `UTurboMCPLookToolset` | `LookTurboMCPCapture` | Capture a picture of a Blueprint's graph editor, framed one of seven ways |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPToolset` | Preview the size of this plugin's own tool help |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPEnumAdd` | Work out what creating an enum asset would do, without creating it |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPVarsAdd` | Work out what adding Blueprint variables would do, without adding them |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPOverrideAdd` | Work out what creating an override would do, without creating it |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPWidgetRetarget` | Work out which widget bindings a class swap has left broken, without changing anything |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPComment` | Work out what a comment-box change would do, without making it |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphDead` | Work out which nodes are dead and would be removed, without removing them |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPLocalVars` | Work out which values could become local variables, without promoting any |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPNodeType` | Finds the REAL node type id the engine's node creation tool will accept |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPNodePins` | The pins a node type produces, WITHOUT TOUCHING the asset's own graph |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphKnots` | Work out which wires cross a node, and where a knot would fix them, without placing any |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphTidy` | Work out a clean layout for one graph, WITHOUT moving anything |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphFixBackward` | Repair the bad spots in a mostly-good graph, WITHOUT moving anything |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphCheck` | Check graph text before writing it, and write nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphAppend` | Add a block to a graph from graph text, and write nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and list what it removes |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphSwapCall` |  |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphRepoint` |  |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPBatchEdit` |  |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and change nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPMaterialLabelPromote` | List the values in a material worth a name, and change nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPMaterialBlocks` | `P-MAT-BLOCKS`: make labels follow blocks |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPDataTableRows` | Preview row operations on a DataTable |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphCopy` | Plan copying a block of nodes from one graph to another, and write nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPGraphCollapse` | Plan extracting a selection into a function or a macro, and write nothing |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPStructNew` | Work out what creating a user struct asset would do, without creating it |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPStructFields` | Work out what editing a user struct's fields would do, without changing any |
| `UTurboMCPPreviewToolset` | `PreviewTurboMCPVcsCheckout` | Work out what a checkout would do, per asset, without checking anything out |
| `UTurboMCPEditToolset` | `ReadTurboMCPEditQueue` | Read whether the editor can accept a write right now |
| `UTurboMCPEditToolset` | `EditTurboMCPCompile` | Compile one or more Blueprints or widgets, with a real answer each |
| `UTurboMCPEditToolset` | `EditTurboMCPSave` | Save one or more assets without noise |
| `UTurboMCPEditToolset` | `EditTurboMCPSaveWithoutThumbnail` | Save one or more assets WITHOUT the editor's thumbnail step |
| `UTurboMCPEditToolset` | `EditTurboMCPEnumAdd` | Create an enum asset from a list of values |
| `UTurboMCPEditToolset` | `EditTurboMCPVarsAdd` | Add Blueprint variables in bulk |
| `UTurboMCPEditToolset` | `EditTurboMCPWidgetsAsVars` | Mark widgets as variables, or unmark them, in one call |
| `UTurboMCPEditToolset` | `EditTurboMCPOverrideAdd` | Create an override on purpose |
| `UTurboMCPEditToolset` | `EditTurboMCPOverrideRemove` | Remove an override this Blueprint has |
| `UTurboMCPEditToolset` | `EditTurboMCPFunctionAdd` | Add a function to a Blueprint |
| `UTurboMCPEditToolset` | `EditTurboMCPFunctionRemove` | Remove a function from a Blueprint, so its name is free at once |
| `UTurboMCPEditToolset` | `EditTurboMCPWidgetRetarget` | Repair widget bindings after a class swap |
| `UTurboMCPEditToolset` | `EditTurboMCPWidgetBindAdd` | Bind an event on a widget to a function name |
| `UTurboMCPEditToolset` | `EditTurboMCPComment` | Make a comment-box change |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphDead` | Remove the dead copies in a graph |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphKnots` | Place the knots the preview listed |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphTidy` | Apply the layout the preview worked out |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphFixBackward` | Apply the repairs the preview listed |
| `UTurboMCPEditToolset` | `EditTurboMCPLocalVars` | Promote the SAFE values to local variables |
| `UTurboMCPEditToolset` | `EditTurboMCPCallLog` | Turn the call log on or off |
| `UTurboMCPEditToolset` | `EditTurboMCPCallLogClear` | Drop the in-memory call log entries |
| `UTurboMCPEditToolset` | `EditTurboMCPTrace` | Mark a node to be traced, or clear the marks |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphAppend` | Add a block to a graph from graph text |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and report every node it removed |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphSwapCall` |  |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphRepoint` |  |
| `UTurboMCPEditToolset` | `EditTurboMCPBatchEdit` |  |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch |
| `UTurboMCPEditToolset` | `EditTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and PROVE it changed nothing but geometry |
| `UTurboMCPEditToolset` | `EditTurboMCPMaterialLabelPromote` | Give a material values names: create the declaration and rewire the readers |
| `UTurboMCPEditToolset` | `EditTurboMCPMaterialBlocks` | `E-MAT-BLOCKS`: make labels follow blocks, changing exactly what the Preview listed |
| `UTurboMCPEditToolset` | `EditTurboMCPMaterialInstanceSet` | Set texture overrides on a material instance |
| `UTurboMCPEditToolset` | `EditTurboMCPDataTableRows` | Apply row operations to a DataTable in ONE transaction, then re-read the rows back from the table |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphCopy` | Copy a block of nodes from one graph to another, and report what could not come with it |
| `UTurboMCPEditToolset` | `EditTurboMCPGraphCollapse` | Extract a selection into a function or a macro |
| `UTurboMCPEditToolset` | `EditTurboMCPStructNew` | Create a user struct asset, with its fields in one call |
| `UTurboMCPEditToolset` | `EditTurboMCPStructFields` | Add, change, reorder and remove fields on an existing user struct |
| `UTurboMCPEditToolset` | `EditTurboMCPVcsCheckout` | Take the lock before a write: check one or more assets out through the project own provider |
