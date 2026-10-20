# Tool reference

[Documentation](../README.md) · [Installation](installation.md)

Generated from plugin declarations. For validation and compatibility limits, see [project status](../status.md).

- [UTurboMCPReadToolset](#uturbomcpreadtoolset)
- [UTurboMCPLookToolset](#uturbomcplooktoolset)
- [UTurboMCPPreviewToolset](#uturbomcppreviewtoolset)
- [UTurboMCPEditToolset](#uturbomcpedittoolset)

## UTurboMCPReadToolset

30 declared tools.

| Function | Description |
| --- | --- |
| `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state |
| `ReadTurboMCPToolIndex` | Read every tool the server serves, one line each |
| `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool |
| `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search |
| `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form |
| `ReadTurboMCPTypeFind` | Read the exact type path a pin or a variable needs |
| `ReadTurboMCPDirtyList` | Read every asset with unsaved changes |
| `ReadTurboMCPNodeResolve` | Turn node IDs into nodes |
| `ReadTurboMCPNodeFind` | Find nodes in a Blueprint by what they say |
| `ReadTurboMCPGraphLoad` | Read one graph as text |
| `ReadTurboMCPBlueprintLoad` | Read whole Blueprints: the main Read tool |
| `ReadTurboMCPRefs` | Read what an asset references, and what references it |
| `ReadTurboMCPOverrides` | Read what a Blueprint can override, and what it already does |
| `ReadTurboMCPCommentList` | Read the comment boxes of a Blueprint's graphs as data |
| `ReadTurboMCPDataTable` | Describe a data table |
| `ReadTurboMCPCallLog` | Read the call log: what the server recorded about its own use |
| `ReadTurboMCPCallCost` | What the calls COST: which were most expensive, which were slowest, which failed, and the totals per tool and per asset |
| `ReadTurboMCPToolSchema` | The exact schema of one tool: its signature, one row per parameter, and its trap notes |
| `ReadTurboMCPMaterialLabels` | Read the named reroutes of a material: every label, its readers, and the unused ones |
| `ReadTurboMCPMaterialLoad` | Read a whole material: its parameters, then every connected output as a tree |
| `ReadTurboMCPMaterialTrace` | Read ONE output of a material rather than all of them |
| `ReadTurboMCPMaterialBlocks` | Read a material by BLOCK of logic, one block per comment box |
| `ReadTurboMCPMaterialInstance` | Read a material INSTANCE: its parent chain, what it overrides, and - the list that matters - what it inherits from a parent it does not own |
| `ReadTurboMCPPlayCheck` | Play the project for a few seconds and report the warnings and errors it raised |
| `ReadTurboMCPTrace` | Read the trace: what is armed, and how often each point has run |
| `ReadTurboMCPBPLint` | Lint one or more Blueprints and report what is wrong with them, with each check NAMED. `assets` takes one or more asset names |
| `ReadTurboMCPAssetCost` | The size map and hard references of one asset, SPLIT BY PROVENANCE. `D-52`: *"A size is reported with its provenance |
| `ReadTurboMCPWidgetTree` | Read a Widget Blueprint's tree as the text dialect of `04_FORMATS.md` section 8 |
| `ReadTurboMCPPureChecks` | Call the two PURE functions behind `R-CALL-COST` with LITERAL inputs, and print what they returned |
| `ReadTurboMCPVcsState` | Read what source control holds for each asset, and refuse by name when it holds nothing |

## UTurboMCPLookToolset

4 declared tools.

| Function | Description |
| --- | --- |
| `LookTurboMCPToolset` | Look for a live editor window that a capture could use |
| `LookTurboMCPLayoutReport` | Look at the layout of one graph: measure it and report, without moving anything |
| `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 |
| `LookTurboMCPCapture` | Capture a picture of a Blueprint's graph editor, framed one of seven ways |

## UTurboMCPPreviewToolset

29 declared tools.

| Function | Description |
| --- | --- |
| `PreviewTurboMCPToolset` | Preview the size of this plugin's own tool help |
| `PreviewTurboMCPEnumAdd` | Work out what creating an enum asset would do, without creating it |
| `PreviewTurboMCPVarsAdd` | Work out what adding Blueprint variables would do, without adding them |
| `PreviewTurboMCPOverrideAdd` | Work out what creating an override would do, without creating it |
| `PreviewTurboMCPWidgetRetarget` | Work out which widget bindings a class swap has left broken, without changing anything |
| `PreviewTurboMCPComment` | Work out what a comment-box change would do, without making it |
| `PreviewTurboMCPGraphDead` | Work out which nodes are dead and would be removed, without removing them |
| `PreviewTurboMCPLocalVars` | Work out which values could become local variables, without promoting any |
| `PreviewTurboMCPNodeType` | Finds the REAL node type id the engine's node creation tool will accept |
| `PreviewTurboMCPNodePins` | The pins a node type produces, WITHOUT TOUCHING the asset's own graph |
| `PreviewTurboMCPGraphKnots` | Work out which wires cross a node, and where a knot would fix them, without placing any |
| `PreviewTurboMCPGraphTidy` | Work out a clean layout for one graph, WITHOUT moving anything |
| `PreviewTurboMCPGraphFixBackward` | Repair the bad spots in a mostly-good graph, WITHOUT moving anything |
| `PreviewTurboMCPGraphCheck` | Check graph text before writing it, and write nothing |
| `PreviewTurboMCPGraphAppend` | Add a block to a graph from graph text, and write nothing |
| `PreviewTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and list what it removes |
| `PreviewTurboMCPGraphSwapCall` |  |
| `PreviewTurboMCPGraphRepoint` |  |
| `PreviewTurboMCPBatchEdit` |  |
| `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ |
| `PreviewTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and change nothing |
| `PreviewTurboMCPMaterialLabelPromote` | List the values in a material worth a name, and change nothing |
| `PreviewTurboMCPMaterialBlocks` | `P-MAT-BLOCKS`: make labels follow blocks |
| `PreviewTurboMCPDataTableRows` | Preview row operations on a DataTable |
| `PreviewTurboMCPGraphCopy` | Plan copying a block of nodes from one graph to another, and write nothing |
| `PreviewTurboMCPGraphCollapse` | Plan extracting a selection into a function or a macro, and write nothing |
| `PreviewTurboMCPStructNew` | Work out what creating a user struct asset would do, without creating it |
| `PreviewTurboMCPStructFields` | Work out what editing a user struct's fields would do, without changing any |
| `PreviewTurboMCPVcsCheckout` | Work out what a checkout would do, per asset, without checking anything out |

## UTurboMCPEditToolset

38 declared tools.

| Function | Description |
| --- | --- |
| `ReadTurboMCPEditQueue` | Read whether the editor can accept a write right now |
| `EditTurboMCPCompile` | Compile one or more Blueprints or widgets, with a real answer each |
| `EditTurboMCPSave` | Save one or more assets without noise |
| `EditTurboMCPSaveWithoutThumbnail` | Save one or more assets WITHOUT the editor's thumbnail step |
| `EditTurboMCPEnumAdd` | Create an enum asset from a list of values |
| `EditTurboMCPVarsAdd` | Add Blueprint variables in bulk |
| `EditTurboMCPWidgetsAsVars` | Mark widgets as variables, or unmark them, in one call |
| `EditTurboMCPOverrideAdd` | Create an override on purpose |
| `EditTurboMCPOverrideRemove` | Remove an override this Blueprint has |
| `EditTurboMCPFunctionAdd` | Add a function to a Blueprint |
| `EditTurboMCPFunctionRemove` | Remove a function from a Blueprint, so its name is free at once |
| `EditTurboMCPWidgetRetarget` | Repair widget bindings after a class swap |
| `EditTurboMCPWidgetBindAdd` | Bind an event on a widget to a function name |
| `EditTurboMCPComment` | Make a comment-box change |
| `EditTurboMCPGraphDead` | Remove the dead copies in a graph |
| `EditTurboMCPGraphKnots` | Place the knots the preview listed |
| `EditTurboMCPGraphTidy` | Apply the layout the preview worked out |
| `EditTurboMCPGraphFixBackward` | Apply the repairs the preview listed |
| `EditTurboMCPLocalVars` | Promote the SAFE values to local variables |
| `EditTurboMCPCallLog` | Turn the call log on or off |
| `EditTurboMCPCallLogClear` | Drop the in-memory call log entries |
| `EditTurboMCPTrace` | Mark a node to be traced, or clear the marks |
| `EditTurboMCPGraphAppend` | Add a block to a graph from graph text |
| `EditTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and report every node it removed |
| `EditTurboMCPGraphSwapCall` |  |
| `EditTurboMCPGraphRepoint` |  |
| `EditTurboMCPBatchEdit` |  |
| `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch |
| `EditTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and PROVE it changed nothing but geometry |
| `EditTurboMCPMaterialLabelPromote` | Give a material values names: create the declaration and rewire the readers |
| `EditTurboMCPMaterialBlocks` | `E-MAT-BLOCKS`: make labels follow blocks, changing exactly what the Preview listed |
| `EditTurboMCPMaterialInstanceSet` | Set texture overrides on a material instance |
| `EditTurboMCPDataTableRows` | Apply row operations to a DataTable in ONE transaction, then re-read the rows back from the table |
| `EditTurboMCPGraphCopy` | Copy a block of nodes from one graph to another, and report what could not come with it |
| `EditTurboMCPGraphCollapse` | Extract a selection into a function or a macro |
| `EditTurboMCPStructNew` | Create a user struct asset, with its fields in one call |
| `EditTurboMCPStructFields` | Add, change, reorder and remove fields on an existing user struct |
| `EditTurboMCPVcsCheckout` | Take the lock before a write: check one or more assets out through the project own provider |

## Stock-engine write safeguards (candidate)

TurboMCP filters selected native MCP tools while loaded. Refresh discovery after updating the plugin. No engine source patch is required. This candidate has not completed runtime regression.

| Filtered native tool | Plugin workflow |
| --- | --- |
| DataTableTools.add_rows / set_rows / rename_rows | PreviewTurboMCPDataTableRows, then EditTurboMCPDataTableRows; use explicit row operations. Rename is not an automatic drop-in replacement. |
| BlueprintTools.set_pin_value | EditTurboMCPPinDefault with exact reader node identity and input pin name; unconnected K2 defaults only. |
| BlueprintTools.compile_blueprint | EditTurboMCPCompile, followed by explicit EditTurboMCPSave when desired. |
| ProgrammaticToolset.execute_tool_script | Sequential tool calls, or Preview/EditTurboMCPBatchEdit for supported graphRepoint operations only. Arbitrary script batching has no equivalent. |

These filters do not intercept direct Python calls or editor actions. They contain known MCP routes; they do not repair Unreal globally. Pin edits mark the Blueprint dirty and capture undo but do not compile or save automatically.
