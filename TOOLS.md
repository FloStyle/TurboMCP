# TurboMCP tool catalogue

**This file is generated from the declarations in
[`Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h)**
by `Tools/make-tools-doc.py`, so it cannot drift from the code. The engine turns each of these into an
MCP tool at load time, and **the first sentence of the doc comment above each function is the
description an agent receives** - that is why the summaries below are extracted rather than written.

| Toolset | Kind | Writes? | Tools |
|---|---|---|---|
| `UTurboMCPReadToolset` | Read | No | **30** |
| `UTurboMCPLookToolset` | Look | No | **4** |
| `UTurboMCPPreviewToolset` | Preview | No | **26** |
| `UTurboMCPEditToolset` | Edit | **Yes** | **33** |
| **Total** | | | **93** |

**A reader in `Read` or `Look` cannot write**, because those classes hold no editing code - it is a
property of the type, not a convention.

## `UTurboMCPReadToolset` — Read, writes: No

| Tool | Description | Line |
|---|---|---|
| `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state | [71](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L71) |
| `ReadTurboMCPToolIndex` | Read every tool the server serves, one line each | [88](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L88) |
| `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool | [111](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L111) |
| `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search | [132](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L132) |
| `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form | [150](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L150) |
| `ReadTurboMCPTypeFind` | Read the exact type path a pin or a variable needs | [171](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L171) |
| `ReadTurboMCPDirtyList` | Read every asset with unsaved changes | [187](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L187) |
| `ReadTurboMCPNodeResolve` | Turn node IDs into nodes | [209](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L209) |
| `ReadTurboMCPNodeFind` | Find nodes in a Blueprint by what they say | [226](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L226) |
| `ReadTurboMCPGraphLoad` | Read one graph as text | [254](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L254) |
| `ReadTurboMCPBlueprintLoad` | Read whole Blueprints: the main Read tool | [293](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L293) |
| `ReadTurboMCPRefs` | Read what an asset references, and what references it | [320](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L320) |
| `ReadTurboMCPOverrides` | Read what a Blueprint can override, and what it already does | [347](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L347) |
| `ReadTurboMCPCommentList` | Read the comment boxes of a Blueprint's graphs as data | [368](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L368) |
| `ReadTurboMCPDataTable` | Describe a data table | [395](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L395) |
| `ReadTurboMCPCallLog` | Read the call log: what the server recorded about its own use | [421](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L421) |
| `ReadTurboMCPCallCost` | What the calls COST: which were most expensive, which were slowest, which failed, and the totals per tool and per asset | [460](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L460) |
| `ReadTurboMCPToolSchema` | The exact schema of one tool: its signature, one row per parameter, and its trap notes | [480](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L480) |
| `ReadTurboMCPMaterialLabels` | Read the named reroutes of a material: every label, its readers, and the unused ones | [492](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L492) |
| `ReadTurboMCPMaterialLoad` | Read a whole material: its parameters, then every connected output as a tree | [505](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L505) |
| `ReadTurboMCPMaterialTrace` | Read ONE output of a material rather than all of them | [518](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L518) |
| `ReadTurboMCPMaterialBlocks` | Read a material by BLOCK of logic, one block per comment box | [530](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L530) |
| `ReadTurboMCPMaterialInstance` | Read a material INSTANCE: its parent chain, what it overrides, and - the list that matters - what it inherits from a parent it does not own | [549](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L549) |
| `ReadTurboMCPPlayCheck` | Play the project for a few seconds and report the warnings and errors it raised | [575](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L575) |
| `ReadTurboMCPTrace` | Read the trace: what is armed, and how often each point has run | [590](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L590) |
| `ReadTurboMCPBPLint` | Lint one or more Blueprints and report what is wrong with them, with each check NAMED. `assets` takes one or more asset names | [612](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L612) |
| `ReadTurboMCPAssetCost` | The size map and hard references of one asset, SPLIT BY PROVENANCE. `D-52`: *"A size is reported with its provenance | [631](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L631) |
| `ReadTurboMCPWidgetTree` | Read a Widget Blueprint's tree as the text dialect of `04_FORMATS.md` section 8 | [661](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L661) |
| `ReadTurboMCPPureChecks` | Call the two PURE functions behind `R-CALL-COST` with LITERAL inputs, and print what they returned | [700](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L700) |
| `ReadTurboMCPVcsState` | Read what source control holds for each asset, and refuse by name when it holds nothing | [737](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L737) |

## `UTurboMCPLookToolset` — Look, writes: No

| Tool | Description | Line |
|---|---|---|
| `LookTurboMCPToolset` | Look for a live editor window that a capture could use | [763](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L763) |
| `LookTurboMCPLayoutReport` | Look at the layout of one graph: measure it and report, without moving anything | [791](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L791) |
| `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 | [821](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L821) |
| `LookTurboMCPCapture` | Capture a picture of a Blueprint's graph editor, framed one of seven ways | [879](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L879) |

## `UTurboMCPPreviewToolset` — Preview, writes: No

| Tool | Description | Line |
|---|---|---|
| `PreviewTurboMCPToolset` | Preview the size of this plugin's own tool help | [914](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L914) |
| `PreviewTurboMCPEnumAdd` | Work out what creating an enum asset would do, without creating it | [931](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L931) |
| `PreviewTurboMCPVarsAdd` | Work out what adding Blueprint variables would do, without adding them | [952](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L952) |
| `PreviewTurboMCPOverrideAdd` | Work out what creating an override would do, without creating it | [970](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L970) |
| `PreviewTurboMCPWidgetRetarget` | Work out which widget bindings a class swap has left broken, without changing anything | [990](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L990) |
| `PreviewTurboMCPComment` | Work out what a comment-box change would do, without making it | [1019](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1019) |
| `PreviewTurboMCPGraphDead` | Work out which nodes are dead and would be removed, without removing them | [1049](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1049) |
| `PreviewTurboMCPLocalVars` | Work out which values could become local variables, without promoting any | [1069](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1069) |
| `PreviewTurboMCPNodeType` | Finds the REAL node type id the engine's node creation tool will accept | [1087](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1087) |
| `PreviewTurboMCPNodePins` | The pins a node type produces, WITHOUT TOUCHING the asset's own graph | [1108](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1108) |
| `PreviewTurboMCPGraphKnots` | Work out which wires cross a node, and where a knot would fix them, without placing any | [1136](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1136) |
| `PreviewTurboMCPGraphTidy` | Work out a clean layout for one graph, WITHOUT moving anything | [1157](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1157) |
| `PreviewTurboMCPGraphFixBackward` | Repair the bad spots in a mostly-good graph, WITHOUT moving anything | [1184](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1184) |
| `PreviewTurboMCPGraphCheck` | Check graph text before writing it, and write nothing | [1212](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1212) |
| `PreviewTurboMCPGraphAppend` | Add a block to a graph from graph text, and write nothing | [1236](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1236) |
| `PreviewTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and list what it removes | [1257](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1257) |
| `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ | [1280](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1280) |
| `PreviewTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and change nothing | [1300](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1300) |
| `PreviewTurboMCPMaterialLabelPromote` | List the values in a material worth a name, and change nothing | [1321](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1321) |
| `PreviewTurboMCPMaterialBlocks` | `P-MAT-BLOCKS`: make labels follow blocks | [1353](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1353) |
| `PreviewTurboMCPDataTableRows` | Preview row operations on a DataTable | [1367](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1367) |
| `PreviewTurboMCPGraphCopy` | Plan copying a block of nodes from one graph to another, and write nothing | [1385](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1385) |
| `PreviewTurboMCPGraphCollapse` | Plan extracting a selection into a function or a macro, and write nothing | [1405](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1405) |
| `PreviewTurboMCPStructNew` | Work out what creating a user struct asset would do, without creating it | [1435](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1435) |
| `PreviewTurboMCPStructFields` | Work out what editing a user struct's fields would do, without changing any | [1467](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1467) |
| `PreviewTurboMCPVcsCheckout` | Work out what a checkout would do, per asset, without checking anything out | [1491](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1491) |

## `UTurboMCPEditToolset` — Edit, writes: **Yes**

| Tool | Description | Line |
|---|---|---|
| `ReadTurboMCPEditQueue` | Read whether the editor can accept a write right now | [1516](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1516) |
| `EditTurboMCPCompile` | Compile one or more Blueprints or widgets, with a real answer each | [1536](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1536) |
| `EditTurboMCPSave` | Save one or more assets without noise | [1566](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1566) |
| `EditTurboMCPEnumAdd` | Create an enum asset from a list of values | [1586](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1586) |
| `EditTurboMCPVarsAdd` | Add Blueprint variables in bulk | [1608](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1608) |
| `EditTurboMCPWidgetsAsVars` | Mark widgets as variables, or unmark them, in one call | [1629](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1629) |
| `EditTurboMCPOverrideAdd` | Create an override on purpose | [1652](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1652) |
| `EditTurboMCPOverrideRemove` | Remove an override this Blueprint has | [1673](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1673) |
| `EditTurboMCPFunctionAdd` | Add a function to a Blueprint | [1699](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1699) |
| `EditTurboMCPFunctionRemove` | Remove a function from a Blueprint, so its name is free at once | [1713](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1713) |
| `EditTurboMCPWidgetRetarget` | Repair widget bindings after a class swap | [1732](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1732) |
| `EditTurboMCPWidgetBindAdd` | Bind an event on a widget to a function name | [1753](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1753) |
| `EditTurboMCPComment` | Make a comment-box change | [1775](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1775) |
| `EditTurboMCPGraphDead` | Remove the dead copies in a graph | [1802](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1802) |
| `EditTurboMCPGraphKnots` | Place the knots the preview listed | [1819](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1819) |
| `EditTurboMCPGraphTidy` | Apply the layout the preview worked out | [1835](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1835) |
| `EditTurboMCPGraphFixBackward` | Apply the repairs the preview listed | [1854](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1854) |
| `EditTurboMCPLocalVars` | Promote the SAFE values to local variables | [1877](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1877) |
| `EditTurboMCPCallLog` | Turn the call log on or off | [1895](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1895) |
| `EditTurboMCPCallLogClear` | Drop the in-memory call log entries | [1905](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1905) |
| `EditTurboMCPTrace` | Mark a node to be traced, or clear the marks | [1923](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1923) |
| `EditTurboMCPGraphAppend` | Add a block to a graph from graph text | [1944](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1944) |
| `EditTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and report every node it removed | [1965](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1965) |
| `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch | [1990](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1990) |
| `EditTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and PROVE it changed nothing but geometry | [2011](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2011) |
| `EditTurboMCPMaterialLabelPromote` | Give a material values names: create the declaration and rewire the readers | [2033](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2033) |
| `EditTurboMCPMaterialBlocks` | `E-MAT-BLOCKS`: make labels follow blocks, changing exactly what the Preview listed | [2058](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2058) |
| `EditTurboMCPDataTableRows` | Apply row operations to a DataTable in ONE transaction, then re-read the rows back from the table | [2069](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2069) |
| `EditTurboMCPGraphCopy` | Copy a block of nodes from one graph to another, and report what could not come with it | [2087](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2087) |
| `EditTurboMCPGraphCollapse` | Extract a selection into a function or a macro | [2110](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2110) |
| `EditTurboMCPStructNew` | Create a user struct asset, with its fields in one call | [2141](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2141) |
| `EditTurboMCPStructFields` | Add, change, reorder and remove fields on an existing user struct | [2167](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2167) |
| `EditTurboMCPVcsCheckout` | Take the lock before a write: check one or more assets out through the project own provider | [2197](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2197) |

## Calling one

The MCP server runs in the editor on **`http://localhost:8000/mcp`**. With tool search enabled - the
engine default, kept here - a client sees three meta-tools and reaches everything else through the third:

- `list_toolsets` — what is registered, including these four;
- `describe_toolset` — one toolset's tools and their schemas;
- `call_tool` — takes `toolset_name` and the **bare** function name, with `arguments` as a **JSON object, not a string**.

So the tool above named `ReadTurboMCPStatus` is called with `toolset_name` = `TurboMCP.Read` and
`tool_name` = `ReadTurboMCPStatus`. The full dotted name is what a listing prints; `call_tool` wants the
bare one. Both facts are in [`Spec/07_ENGINE_NOTES.md`](Spec/07_ENGINE_NOTES.md), and they are repeated
here because they are the two things a caller gets wrong first.

