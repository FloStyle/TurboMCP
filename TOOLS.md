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
| `UTurboMCPEditToolset` | Edit | **Yes** | **34** |
| **Total** | | | **94** |

**A reader in `Read` or `Look` cannot write**, because those classes hold no editing code - it is a
property of the type, not a convention.

## `UTurboMCPReadToolset` — Read, writes: No

| Tool | Description | Line |
|---|---|---|
| `ReadTurboMCPStatus` | Read the MCP server state and this plugin's registration state | [72](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L72) |
| `ReadTurboMCPToolIndex` | Read every tool the server serves, one line each | [89](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L89) |
| `ReadTurboMCPToolHelp` | Read the parameters and known traps of one tool | [112](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L112) |
| `ReadTurboMCPAssetSearch` | Read assets matching a query, like the editor's quick open search | [133](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L133) |
| `ReadTurboMCPAssetDescribe` | Read what one or more assets are, in any path form | [151](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L151) |
| `ReadTurboMCPTypeFind` | Read the exact type path a pin or a variable needs | [172](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L172) |
| `ReadTurboMCPDirtyList` | Read every asset with unsaved changes | [188](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L188) |
| `ReadTurboMCPNodeResolve` | Turn node IDs into nodes | [210](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L210) |
| `ReadTurboMCPNodeFind` | Find nodes in a Blueprint by what they say | [227](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L227) |
| `ReadTurboMCPGraphLoad` | Read one graph as text | [255](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L255) |
| `ReadTurboMCPBlueprintLoad` | Read whole Blueprints: the main Read tool | [294](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L294) |
| `ReadTurboMCPRefs` | Read what an asset references, and what references it | [321](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L321) |
| `ReadTurboMCPOverrides` | Read what a Blueprint can override, and what it already does | [348](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L348) |
| `ReadTurboMCPCommentList` | Read the comment boxes of a Blueprint's graphs as data | [369](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L369) |
| `ReadTurboMCPDataTable` | Describe a data table | [396](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L396) |
| `ReadTurboMCPCallLog` | Read the call log: what the server recorded about its own use | [422](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L422) |
| `ReadTurboMCPCallCost` | What the calls COST: which were most expensive, which were slowest, which failed, and the totals per tool and per asset | [461](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L461) |
| `ReadTurboMCPToolSchema` | The exact schema of one tool: its signature, one row per parameter, and its trap notes | [481](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L481) |
| `ReadTurboMCPMaterialLabels` | Read the named reroutes of a material: every label, its readers, and the unused ones | [493](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L493) |
| `ReadTurboMCPMaterialLoad` | Read a whole material: its parameters, then every connected output as a tree | [506](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L506) |
| `ReadTurboMCPMaterialTrace` | Read ONE output of a material rather than all of them | [519](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L519) |
| `ReadTurboMCPMaterialBlocks` | Read a material by BLOCK of logic, one block per comment box | [531](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L531) |
| `ReadTurboMCPMaterialInstance` | Read a material INSTANCE: its parent chain, what it overrides, and - the list that matters - what it inherits from a parent it does not own | [550](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L550) |
| `ReadTurboMCPPlayCheck` | Play the project for a few seconds and report the warnings and errors it raised | [576](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L576) |
| `ReadTurboMCPTrace` | Read the trace: what is armed, and how often each point has run | [591](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L591) |
| `ReadTurboMCPBPLint` | Lint one or more Blueprints and report what is wrong with them, with each check NAMED. `assets` takes one or more asset names | [613](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L613) |
| `ReadTurboMCPAssetCost` | The size map and hard references of one asset, SPLIT BY PROVENANCE. `D-52`: *"A size is reported with its provenance | [632](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L632) |
| `ReadTurboMCPWidgetTree` | Read a Widget Blueprint's tree as the text dialect of `04_FORMATS.md` section 8 | [662](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L662) |
| `ReadTurboMCPPureChecks` | Call the two PURE functions behind `R-CALL-COST` with LITERAL inputs, and print what they returned | [701](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L701) |
| `ReadTurboMCPVcsState` | Read what source control holds for each asset, and refuse by name when it holds nothing | [738](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L738) |

## `UTurboMCPLookToolset` — Look, writes: No

| Tool | Description | Line |
|---|---|---|
| `LookTurboMCPToolset` | Look for a live editor window that a capture could use | [764](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L764) |
| `LookTurboMCPLayoutReport` | Look at the layout of one graph: measure it and report, without moving anything | [792](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L792) |
| `LookTurboMCPWidgetPreview` | Render a Widget Blueprint with sample values and write a PNG. Pass the widget, sample values as a JSON object of variable name to value, an optional function to call after the values are set, and a size as WxH. Empty sample values means the widget's own defaults; an empty size means 1280x720 | [822](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L822) |
| `LookTurboMCPCapture` | Capture a picture of a Blueprint's graph editor, framed one of seven ways | [880](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L880) |

## `UTurboMCPPreviewToolset` — Preview, writes: No

| Tool | Description | Line |
|---|---|---|
| `PreviewTurboMCPToolset` | Preview the size of this plugin's own tool help | [915](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L915) |
| `PreviewTurboMCPEnumAdd` | Work out what creating an enum asset would do, without creating it | [932](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L932) |
| `PreviewTurboMCPVarsAdd` | Work out what adding Blueprint variables would do, without adding them | [953](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L953) |
| `PreviewTurboMCPOverrideAdd` | Work out what creating an override would do, without creating it | [971](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L971) |
| `PreviewTurboMCPWidgetRetarget` | Work out which widget bindings a class swap has left broken, without changing anything | [991](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L991) |
| `PreviewTurboMCPComment` | Work out what a comment-box change would do, without making it | [1020](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1020) |
| `PreviewTurboMCPGraphDead` | Work out which nodes are dead and would be removed, without removing them | [1050](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1050) |
| `PreviewTurboMCPLocalVars` | Work out which values could become local variables, without promoting any | [1070](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1070) |
| `PreviewTurboMCPNodeType` | Finds the REAL node type id the engine's node creation tool will accept | [1088](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1088) |
| `PreviewTurboMCPNodePins` | The pins a node type produces, WITHOUT TOUCHING the asset's own graph | [1109](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1109) |
| `PreviewTurboMCPGraphKnots` | Work out which wires cross a node, and where a knot would fix them, without placing any | [1137](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1137) |
| `PreviewTurboMCPGraphTidy` | Work out a clean layout for one graph, WITHOUT moving anything | [1158](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1158) |
| `PreviewTurboMCPGraphFixBackward` | Repair the bad spots in a mostly-good graph, WITHOUT moving anything | [1185](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1185) |
| `PreviewTurboMCPGraphCheck` | Check graph text before writing it, and write nothing | [1213](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1213) |
| `PreviewTurboMCPGraphAppend` | Add a block to a graph from graph text, and write nothing | [1237](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1237) |
| `PreviewTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and list what it removes | [1258](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1258) |
| `PreviewTurboMCPGraphPatch` | Change only what changed: name existing nodes by id and give the pins that differ | [1281](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1281) |
| `PreviewTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and change nothing | [1301](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1301) |
| `PreviewTurboMCPMaterialLabelPromote` | List the values in a material worth a name, and change nothing | [1322](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1322) |
| `PreviewTurboMCPMaterialBlocks` | `P-MAT-BLOCKS`: make labels follow blocks | [1354](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1354) |
| `PreviewTurboMCPDataTableRows` | Preview row operations on a DataTable | [1368](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1368) |
| `PreviewTurboMCPGraphCopy` | Plan copying a block of nodes from one graph to another, and write nothing | [1386](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1386) |
| `PreviewTurboMCPGraphCollapse` | Plan extracting a selection into a function or a macro, and write nothing | [1406](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1406) |
| `PreviewTurboMCPStructNew` | Work out what creating a user struct asset would do, without creating it | [1436](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1436) |
| `PreviewTurboMCPStructFields` | Work out what editing a user struct's fields would do, without changing any | [1468](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1468) |
| `PreviewTurboMCPVcsCheckout` | Work out what a checkout would do, per asset, without checking anything out | [1492](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1492) |

## `UTurboMCPEditToolset` — Edit, writes: **Yes**

| Tool | Description | Line |
|---|---|---|
| `ReadTurboMCPEditQueue` | Read whether the editor can accept a write right now | [1517](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1517) |
| `EditTurboMCPCompile` | Compile one or more Blueprints or widgets, with a real answer each | [1546](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1546) |
| `EditTurboMCPSave` | Save one or more assets without noise | [1586](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1586) |
| `EditTurboMCPSaveWithoutThumbnail` | Save one or more assets WITHOUT the editor's thumbnail step | [1618](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1618) |
| `EditTurboMCPEnumAdd` | Create an enum asset from a list of values | [1638](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1638) |
| `EditTurboMCPVarsAdd` | Add Blueprint variables in bulk | [1660](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1660) |
| `EditTurboMCPWidgetsAsVars` | Mark widgets as variables, or unmark them, in one call | [1681](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1681) |
| `EditTurboMCPOverrideAdd` | Create an override on purpose | [1704](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1704) |
| `EditTurboMCPOverrideRemove` | Remove an override this Blueprint has | [1725](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1725) |
| `EditTurboMCPFunctionAdd` | Add a function to a Blueprint | [1751](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1751) |
| `EditTurboMCPFunctionRemove` | Remove a function from a Blueprint, so its name is free at once | [1765](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1765) |
| `EditTurboMCPWidgetRetarget` | Repair widget bindings after a class swap | [1784](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1784) |
| `EditTurboMCPWidgetBindAdd` | Bind an event on a widget to a function name | [1805](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1805) |
| `EditTurboMCPComment` | Make a comment-box change | [1829](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1829) |
| `EditTurboMCPGraphDead` | Remove the dead copies in a graph | [1856](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1856) |
| `EditTurboMCPGraphKnots` | Place the knots the preview listed | [1873](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1873) |
| `EditTurboMCPGraphTidy` | Apply the layout the preview worked out | [1889](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1889) |
| `EditTurboMCPGraphFixBackward` | Apply the repairs the preview listed | [1908](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1908) |
| `EditTurboMCPLocalVars` | Promote the SAFE values to local variables | [1931](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1931) |
| `EditTurboMCPCallLog` | Turn the call log on or off | [1949](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1949) |
| `EditTurboMCPCallLogClear` | Drop the in-memory call log entries | [1959](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1959) |
| `EditTurboMCPTrace` | Mark a node to be traced, or clear the marks | [1977](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1977) |
| `EditTurboMCPGraphAppend` | Add a block to a graph from graph text | [1998](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L1998) |
| `EditTurboMCPGraphReplace` | Rewrite the body of one function, override or event, and report every node it removed | [2019](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2019) |
| `EditTurboMCPGraphPatch` | The Edit twin of PreviewTurboMCPGraphPatch | [2044](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2044) |
| `EditTurboMCPMaterialTidy` | Lay out a material, one comment box per block, and PROVE it changed nothing but geometry | [2065](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2065) |
| `EditTurboMCPMaterialLabelPromote` | Give a material values names: create the declaration and rewire the readers | [2087](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2087) |
| `EditTurboMCPMaterialBlocks` | `E-MAT-BLOCKS`: make labels follow blocks, changing exactly what the Preview listed | [2112](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2112) |
| `EditTurboMCPDataTableRows` | Apply row operations to a DataTable in ONE transaction, then re-read the rows back from the table | [2123](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2123) |
| `EditTurboMCPGraphCopy` | Copy a block of nodes from one graph to another, and report what could not come with it | [2141](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2141) |
| `EditTurboMCPGraphCollapse` | Extract a selection into a function or a macro | [2164](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2164) |
| `EditTurboMCPStructNew` | Create a user struct asset, with its fields in one call | [2195](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2195) |
| `EditTurboMCPStructFields` | Add, change, reorder and remove fields on an existing user struct | [2221](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2221) |
| `EditTurboMCPVcsCheckout` | Take the lock before a write: check one or more assets out through the project own provider | [2251](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h#L2251) |

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

## Adding a tool

**Three steps, and the third is the one that is always forgotten.** A tool that exists but that no
suite names is a tool the coverage audit reports as uncalled, and the audit passes only at
`uncalled: (none)`.

### 1. Declare it, in the right toolset class

Put it in the class that matches what it does — `UTurboMCPReadToolset` for a reader,
`UTurboMCPEditToolset` for a writer — in
[`TurboMCPToolsets.h`](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h). **The class is the safety
property**: a reader declared in Read holds no editing code, so "this cannot modify your project" is a
fact about the type rather than a promise. A writer needs a `Preview` twin in
`UTurboMCPPreviewToolset` that shares the same `Plan`, so what the plan reports and what the edit does
cannot drift apart.

```cpp
/**
 * One line saying what the tool does. THIS SENTENCE IS WHAT AN AGENT READS, because the
 * engine builds the tool description from this comment through reflection.
 *
 * Then the parameters, and every refusal by name with the engine line that decides it.
 */
UFUNCTION(meta = (AICallable))
static FString ReadTurboMCPYourThing(const FString& Asset);
```

### 2. Write a suite that calls it **by its literal name**

Add `Tools/test-<capability>.py`. *** THE CALL SITE MUST SPELL THE TOOL NAME AS A LITERAL, EXACTLY AS
THE DECLARATION DOES, BECAUSE `Tools/audit-tool-coverage.py` FINDS IT BY REGEX OVER THE SUITE FILES.
*** A name built at run time, or interpolated, is a name the audit cannot see, and the audit will
report the tool as uncalled even though the suite exercises it.

```python
session.call(READ_TOOLSET, "ReadTurboMCPYourThing", {"asset": "/Game/Tests/Thing"})
```

**Every check you write must name an input that makes it fail.** `Tools/audit-suite-quality.py`
**blocks the whole run** when a check cannot fail or cannot pass, so a check that only ever passes is
a build failure here, not a stylistic complaint. And run the break test: change the code so the check
should go red, and confirm it does **with the wrong value visible** — a check that matches a prefix
cannot see a change after the prefix.

### 3. Verify, and run the whole thing

```powershell
python Tools\audit-tool-coverage.py    # must print: uncalled: (none)
python Tools\audit-suite-quality.py    # must print 0 candidates, and it BLOCKS
python Tools\make-tools-doc.py         # regenerate this file, so it lists the new tool
Tools\build.ps1
Tools\run-suites.ps1                   # the signal that matters: passed N, failed 0
```

**AND `Tools/make-tools-doc.py` IS STEP 3 FOR A REASON: THIS FILE IS GENERATED FROM THE DECLARATIONS, SO
THE ONLY WAY TO KEEP IT TRUE IS TO RE-RUN THE GENERATOR.** A hand edit here is lost the next time
anybody regenerates, which is the same defect as a hand-kept list of 93 names.

