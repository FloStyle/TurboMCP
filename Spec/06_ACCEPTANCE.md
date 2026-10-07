# Acceptance tests

## 1. Test asset kit

Create these assets in a test content folder of the plugin (for example a
`Content/Tests/` folder inside the plugin), so the tests run in any
project. Build them by hand in the editor, not with the plugin's own tools,
so the tools are tested against human-made assets. Record each asset's
baseline numbers (node count, comment boxes, total wire length) in the
plugin's test record once it is built, and check them before every test
run. If a baseline does not match, the asset was changed: restore it before
testing.

| ID | Asset | What it must contain |
|---|---|---|
| TA-1 | Actor Blueprint, function graph "hand laid" | 60 to 90 nodes laid out by hand, 6 to 8 comment boxes, one box nested in another, a sequence, three branches, variable getters used by several consumers, pure math chains four nodes long, two local variables, a split struct pin, a promotable operator, an enum literal, a call with several exec outputs. Clean layout: no backward wires |
| TA-2 | Same Blueprint, event graph | 3 events, one custom event, two boxes inside boxes, a timer or delay |
| TA-3 | Same Blueprint, function "messy" | Built with the engine's own MCP write tools only: a single long row of about 13 connected nodes, no boxes, plus an unwired copy of 9 of those nodes that reaches no entry point |
| TA-4 | Same Blueprint, function "stacked" | Two exec nodes in sequence with the second placed directly under the first, a getter placed under the setter it feeds, a second copy of it under a branch, one node at the origin linked to nodes far away |
| TA-5 | Material, about 60 expressions | At least 8 named reroutes, 3 comment boxes as blocks, one value crossing two blocks with no label, one label read inside its own block, two usage nodes of one label in one box, lerps with constants on unlinked inputs, a value used in three places, a node that is both the source of a label and used directly elsewhere, one parameter that reaches no output, plain reroute dots on a long wire |
| TA-6 | Material instance of TA-5 | Two overridden parameters |
| TA-7 | Widget Blueprint, "card" | A text, an image, a button with a bound click event, a hidden text with non-zero slot padding, a widget without the variable flag that still has a bound event, an instance editable text variable |
| TA-8 | Widget Blueprint, "page" | Places two copies of TA-7: one with the class default, one with its instance editable variable overridden on the instance. A button child named `ReturnButton` of class TA-7 with a bound event and a call into it |
| TA-9 | Widget Blueprint "card alt" | Another class with the same click delegate as TA-7 but a different set of functions |
| TA-10 | Data table with a user struct asset | 3 rows, 4 fields, read by TA-1 with a row lookup |
| TA-11 | User enum asset | 4 values |
| TA-12 | Name collision pair | Two Blueprints whose initials are the same (for example `BP_TestAlpha` and `BP_TowerArray`), each with a function `Setup` |
| TA-13 | A Blueprint whose name is also a folder name | To test path resolution |

Two things are made during the tests, not stored:

- TA-NEW: a widget created in the session and not saved.
- TA-COLD: any kit asset, after an editor restart, before it is opened.

## 2. Tests

Status for each test is PASS or FAIL plus the measured values, recorded with the
date.

### Agent help

| ID | Test | Pass when |
|---|---|---|
| T-HELP-1 | Tool index | Server state line, tool search mode stated, every engine and own tool one line each |
| T-HELP-2 | Compact help for the engine's graph write tool | Under 2000 characters, correct parameter names, the known traps listed. A wrong toolset name lists close real names |
| T-HELP-3 | Node type lookup | On TA-12: `Setup` lists two candidates with owning classes. A guess with a space or wrong case finds the real ID first. A function added in the session and not saved is found. An overloaded function lists its declaring classes. At most 10 results |
| T-HELP-4 | Pins of a node type | Pins with per direction index; node count of the graph same before and after |
| T-HELP-5 | Type lookup | Finds TA-10's struct asset, TA-11's enum, and a native engine struct, each with the exact type path and its kind |

### Assets

| ID | Test | Pass when |
|---|---|---|
| T-ASSET-1 | Search | `type:Material` alone lists materials, instances and functions. Two words both must match. TA-NEW is found. Each line has class and parent |
| T-ASSET-2 | Describe | All five path forms of TA-7 give the same line. TA-COLD is found, shown as not loaded |

### Blueprint reading

| ID | Test | Pass when |
|---|---|---|
| T-BP-1 | Manifest | Counts match the asset; no bodies |
| T-BP-2 | Signatures | Variables with flags, full signatures, call graph, variable map, assets touched (no engine script packages, no skeleton classes), data table lookups of TA-10 |
| T-BP-3 | Bodies on TA-1 | Every node once; split pin, promotable operator, enum literal and exec outputs readable; local variables declared |
| T-BP-4 | Disconnected groups on TA-3 | The 9 node copy printed once, in its own section, and not also in the live chain |
| T-BP-5 | Budget | A small budget steps down per `04_FORMATS.md` section 5 and ends with the exact calls to get the rest |
| T-BP-6 | Graph filter | `A,B` and `A|B` both return both graphs; a name that fits nothing lists the real names |
| T-BP-7 | Widget tree on TA-8 | Variable flag, visibility, padding, the overridden instance value on one copy only, bound events listed |
| T-BP-8 | Stale bound events | After replacing `ReturnButton` with TA-9 under the same name, the read warns about the bound event and the call node |
| T-BP-9 | One graph, bad name | Lists the real graph names |
| T-BP-10 | Write mode | TA-1 printed in `write` mode, pasted into the engine's graph write tool on an empty function, writes with no error. Unsupported constructs appear as comments |
| T-BP-11 | References | In and out for TA-7, full paths with class |
| T-BP-12 | Overrides | Inputs and outputs marked correctly, already overridden marked |
| T-BP-13 | Resolve IDs | IDs read before adding 5 nodes still resolve to the same nodes after. An asset name that fits two assets is refused with both paths |
| T-BP-14 | Find nodes | Searching a function name finds its call nodes even when the title differs |
| T-BP-15 | Data table | Struct fields, row count and names of TA-10, and TA-1 named as a reader |

### Materials

| ID | Test | Pass when |
|---|---|---|
| T-MAT-1 | Labels | All labels, read counts right, none falsely unused |
| T-MAT-2 | Load | Full trees; each value defined once; no definition is only another name; labelled values only by label |
| T-MAT-3 | Constants | Every lerp prints A, B and alpha, linked or constant |
| T-MAT-4 | Instance | TA-6 refused with parent path; with overrides asked, two overrides listed |
| T-MAT-5 | Trace | One output prints only the labels in its tree; an unconnected property lists the connected ones |
| T-MAT-6 | Blocks | Per box inputs and outputs; the three findings of TA-5 found with IDs |
| T-MAT-7 | Promote labels | Material compiles; compiled result unchanged; new label read count right |
| T-MAT-8 | Apply blocks | Only what the Preview listed changes; result unchanged |
| T-MAT-9 | Tidy material | One box per block; result unchanged |

### Layout and looking

| ID | Test | Pass when |
|---|---|---|
| T-LAY-1 | Report on TA-1 | Matches the baseline; zero backward wires |
| T-LAY-2 | Report on TA-4 | Finds the stacked exec wire, both backward getters and the node at the origin |
| T-LAY-3 | Report on TA-3 | Counts every node once; names the dead copy as a repeat of live logic |
| T-LAY-4 | Report on a graph with no wires | Says so in words |
| T-LAY-5 | Tidy TA-1 | No box moved; wire length not worse, or rolled back with numbers |
| T-LAY-6 | Comment list on TA-2 | Nested boxes with their parent |
| T-LAY-7 | Tidy TA-3 | Wire length shorter by at least 30 percent |
| T-LAY-8 | Tidy nested boxes on TA-2 | Applied, not rolled back; children inside parents |
| T-LAY-9 | Fix backward on TA-4 | The two exec nodes share a row, getters left of consumers, no box moved, shorter wiring |
| T-LAY-10 | Knots | Preview lists only wires that cross nodes; after Edit those wires are straight |
| T-LAY-11 | Dead copies on TA-3 | Preview names the 9 node copy; Edit removes it; report then shows no disconnected groups |
| T-LAY-12 | Comment boxes | After the engine's arrange tool on TA-1's copy, fit leaves no empty box and no box cutting a node |
| T-LAY-13 | Local variables | Safe and unsafe marked with reasons; Edit promotes only safe ones; compiles; same behaviour |
| T-LOOK-1 | Capture graph tab | The named graph is shown, not another one; PNG path returned; no image data |
| T-LOOK-2 | Capture one box | Node titles and pin names readable |
| T-LOOK-3 | Capture box 99 | Error gives the real box count; no file written |
| T-LOOK-4 | Capture widget Designer of TA-7 | PNG shows the designer |
| T-LOOK-5 | Widget preview | TA-7 with sample values shows them in the PNG; a script error in the called function is returned |
| T-LOOK-6 | Play check | Returns grouped warnings from a test map that logs one known warning |

### Structure

| ID | Test | Pass when |
|---|---|---|
| T-STRUCT-1 | Add variables | Five variables (enum from TA-11, an object, an array, one instance editable, one with a category) in one call; defaults readable; compiles |
| T-STRUCT-2 | Add enum | Created with display names; existing path refused |
| T-STRUCT-3 | Widgets as variables | Three widgets in one call |
| T-STRUCT-4 | Add override | Right signature in one call; second call refused with location |
| T-STRUCT-5 | Remove then add a function | Same name, no suffix |
| T-STRUCT-6 | Retarget | After the class swap in TA-8, one Edit makes the asset compile, with a list of kept and removed nodes |
| T-STRUCT-7 | Compile | A broken node is named by ID; a clean unsaved asset says compiled, not saved |
| T-STRUCT-8 | Save | Three assets, three short lines, no log text. During simulate: refused |

### Writing

| ID | Test | Pass when |
|---|---|---|
| T-WRITE-1 | Check | A text with a wrong node type, a wrong pin and a type mismatch gets three findings with line, column and close matches; nothing written |
| T-WRITE-2 | Append | A block linked into TA-1 by node IDs compiles; read back matches |
| T-WRITE-3 | Replace | A function body rewritten leaves no old nodes; removed nodes listed; existing entry and result reused; a function with two result nodes works |
| T-WRITE-4 | Round trip | Every function of TA-1 read and written back unchanged reads back identical |
| T-WRITE-5 | Patch | Changing two lines changes exactly two nodes |
| T-WRITE-6 | Material write | TA-5 read and written back gives the same compiled result |

### Debugging and panel

| ID | Test | Pass when |
|---|---|---|
| T-DBG-1 | Call log | Own and engine tool calls, one line each; summary matches; off stops writing; a line exists for a call that never ended |
| T-DBG-2 | Trace point | One line per tick in play with pin values; clear stops it; panel lists it |
| T-PANEL-1 | Server row | Green dot and address when running; red when stopped |
| T-PANEL-2 | Tool list | Label format, order Read Look Preview Edit, engine tools included |
| T-PANEL-3 | Picker | Chips row, readable names, a filter survives closing and an editor restart |
| T-PANEL-4 | Fields | Only the inputs the selected tool uses |
| T-PANEL-5 | Same output | One tool on one asset gives identical text from the panel and the MCP |
| T-PANEL-6 | Status | Line count and token estimate shown |
