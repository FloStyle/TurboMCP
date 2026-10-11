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
| TA-14 | Data table asset with a project-owned row struct | 3 rows, 4 fields, one field of a struct type. Built by hand, because a struct cannot be built by a commandlet (Q-15) |
| TA-15 | Blueprint actor, "kit actor" | Four components in a hierarchy: a scene root, a static mesh, a point light under the static mesh, and a box collision. Two properties set away from their class default, one component flagged as inherited from the Blueprint and one added on the instance |
| TA-16 | Test level, "kit level" | 12 actors: two lights, a player start, three static meshes, two copies of TA-15 with one component override each, one actor in a folder, one actor hidden in game, one editor-only actor, one actor whose label is deliberately duplicated with another |
| TA-17 | Curve float and a curve table | A curve with 5 keys and non-zero tangents; a curve table with 3 rows |
| TA-18 | Animation Blueprint and Anim Sequence | A state machine with two states and one transition; a sequence of 1 second with 3 keys on one bone |
| TA-19 | Niagara system | One emitter, so a capture has something to frame |
| TA-20 | Material parameter collection | 5 parameters: 2 scalar, 2 vector, one read by no material |
| TA-21 | User struct asset | 4 fields, built by hand, because `AddVariable` needs an editor factory (Q-15) |

**TA-21 DOES NOT EXIST, MEASURED.** There is no `TA21_*` asset under
`TestProject/Content` and no `UserDefinedStruct` asset anywhere in the
repository - the kit holds `TA01_HandLaid`, `TA07_Card`, `TA11_KitEnum`,
`TA11_TestEnum`, `TA13_KitPath`, `TA14_KitDataTable`, `TA18_ColourSuite`,
`TA20_KitParameters` and no struct. `T-DATA-3` ("Add, reorder and remove a field
of TA-21") therefore names a fixture that does not exist, which makes this the
FOURTH row in this project to do so, after `T-MAT-10`->`TA-6`,
`T-LEVEL-1/2`->`TA-15`/`TA-16` and `T-WIDGET-4`->`TA-8`.

`T-DATA-3` IS SATISFIED WITHOUT IT ANYWAY, and that is the honest reading rather
than a workaround: `T-DATA-2` creates a struct through the tool, and `T-DATA-3`
edits one. `Tools/test-struct.py` BUILDS the struct it then edits, through this
plugin's own new tools, and asserts every change against the struct's own
compiled field list and against the saved `.uasset`. So the missing fixture is
recorded rather than required. The note on this row's reason is also out of
date: a struct CAN now be built from a live editor (`03_TOOLS.md`, the
`P/E-STRUCT-*` correction, and `D-51`'s correction); only a COMMANDLET cannot,
and TA-21 was never needed by one.

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
| T-MAT-10 | Read an instance | TA-6's parent chain, the two overridden parameters with their values, and the parent's parameters it does NOT override all marked inherited. A parameter name the parent does not declare is reported as orphaned |
| T-MAT-11 | Set instance overrides | Two overrides set in one call read back through the reader; a parameter that does not exist on the parent is refused with close real names; a value that raises a new engine error rolls the edit back |
| T-MAT-12 | Read a collection | TA-20's 5 parameters with types, defaults and reader counts, and the one nothing reads printed as unused with its index |
| T-MAT-13 | Edit a collection | Add and change a parameter, then remove one with loaded readers: the readers are listed, the index shift of every later parameter is stated, and the removal is refused until `force` names the count |

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

### Graph structure

| ID | Test | Pass when |
|---|---|---|
| T-STRUCT-9 | Rename with references | Rename a variable of TA-1 that has getters, setters and a widget binding: every reference reads back with the new name, the counts are reported, the asset compiles, and a new name that is taken is refused naming what holds it |
| T-STRUCT-10 | Collapse | Extract three connected nodes of TA-1 into a function: the signature read back matches the crossing links, the nodes are gone from the body, the asset compiles, and a selection holding an event is refused by name |
| T-STRUCT-11 | Copy a block | Copy three nodes from TA-1's `hand laid` into TA-2's event graph and link the block to an existing event: it compiles; a reference to a member the target does not have refuses the whole copy and names it |
| T-STRUCT-12 | Interfaces | Add an interface: its unimplemented functions are listed. Remove it while implemented: refused and they are named. With `force`: removed, and the orphans are reported |

### Widget authoring

| ID | Test | Pass when |
|---|---|---|
| T-WIDGET-1 | Append | Add a button inside a box inside TA-7's tree by path: it compiles, the tree reads back with the button in the order given, and a path that does not exist refuses with close real paths |
| T-WIDGET-2 | Replace | Rewrite TA-7's tree: every removed widget is listed by name, the reader and writer round trip is identical, and a text that renames a widget is refused pointing at `P/E-WIDGET-SET` |
| T-WIDGET-3 | Patch | Change one widget's class in one line: exactly that widget changes and the rest of the tree is proven unchanged by reading it back |
| T-WIDGET-4 | Round trip | TA-7 and TA-8 read and written back unchanged read back identical, animations included |
| T-WIDGET-5 | Properties, slot and style | Set visibility, slot padding and a button style in one call: each reads back from the right object, a C++ property name is matched despite its case, a non-instance-editable property is refused by name, and renaming a bound widget updates its binding with the count reported |
| T-WIDGET-6 | Read animations | TA-7 with an animation whose target widget was renamed: the broken track is reported with the missing path, not dropped |
| T-WIDGET-7 | Write animations | Add a 0.5 second animation with 3 keys on one widget: every key reads back as written; rewriting an existing animation without `replace` is refused by name |
| T-WIDGET-8 | Bindings | A property binding is added and a wrong-kind request is refused explaining which is which; removing an event binding names the function it orphans |

### Data and assets

| ID | Test | Pass when |
|---|---|---|
| T-DATA-1 | Data table rows | Add a row to TA-14, change one field of another and remove a third, in one call: every change reads back from the table, each removal is listed, and a field name that does not exist is refused with the real field names |
| T-DATA-2 | New struct | Create a struct with 4 fields including one struct-typed field: it exists and reads back through `R-TYPE-FIND`; an existing path is refused |
| T-DATA-3 | Struct fields | Add, reorder and remove a field of TA-21: each change reads back; a removal with a loaded reader is refused and names the reader and the count, and applies with `force` |
| T-DATA-4 | Enum edit | Add and rename a value of TA-11, then reorder two: the reorder is reported as affecting saved data; an attempt to change `_MAX` is refused by name |
| T-DATA-5 | Curve read | TA-17's curve prints its 5 keys with times and values and its interpolation; its table prints its 3 row names |
| T-DATA-6 | Curve write | Replace the keys of one curve table row: every key reads back, and a key the engine would store differently is reported rather than silently rounded |
| T-ASSET-3 | Asset cost | TA-1's cost: its own size, hard references, soft references and the largest contributors are listed with sizes; an asset whose size cannot be read reports unknown with the reason, not zero |
| T-ASSET-4 | Diff | Rename a variable of a COPY of TA-1 and diff the copy against the original: exactly that rename is printed, and a section that could not be compared is listed as not compared |
| T-ASSET-5 | New, duplicate, rename | Create an asset at a new path; duplicate TA-1; rename it: old and new paths reported, the assets that referenced it are counted, and one redirector is confirmed fixed by resolving the old path |
| T-ASSET-6 | Rename an open asset | Rename an asset that has an editor tab open: refused, naming the tab |

### Level and actors

| ID | Test | Pass when |
|---|---|---|
| T-LEVEL-1 | Read a level | TA-16's actor count before the filter; each actor's label, class, folder, editor-only flag and transform; a filter matching nothing says so and still prints the total |
| T-LEVEL-2 | Read one actor | TA-15's component hierarchy with parents, and the one component overridden on the placed instance marked as an instance change rather than an inherited one; a duplicated label is refused with both candidates |
| T-LEVEL-3 | Place actors | Place three actors including two of TA-15 with different transforms: each label and path read back from the level, and a class path fitting nothing lists the closest real names |
| T-LEVEL-4 | Move and configure | Move one actor and change one property: old and new reported and re-read; renaming the actor's object name is refused by name and pointed at the content tools |
| T-LEVEL-5 | Components | Add a component to TA-15 and reparent another: the result says the change reaches every instance, it compiles, an instance-level change is refused and pointed at `E-ACTOR-SET`, and a removal names the levels that place the Blueprint |

### Vision and comparison

| ID | Test | Pass when |
|---|---|---|
| T-LOOK-7 | Added framing modes | `selection` on three selected nodes frames exactly those; `selection` with nothing selected is refused; `neighbours` at N=1 frames the node and exactly its linked neighbours, counting a link through a knot once |
| T-LOOK-8 | Asset editors | A PNG is written for a material, a Material Instance, a Material Function, a Widget Designer, a DataTable, an Anim Blueprint, an Anim Sequence and a Niagara system, each showing that editor; every editor kind it cannot do is refused BY NAME in the same answer |
| T-LOOK-9 | Level and PIE | The level viewport, a framed actor and the level selection each produce a PNG; `pie` with no session is refused and names the two tools that start one, and with a session shows the game frame |
| T-LOOK-10 | Image compare | Two captures of the same framing and size with nothing changed report no difference; after moving a node the difference count is non-zero, the bounding box contains that node, and the difference PNG exists. Two captures of different sizes are refused |

### Debugging: measurement first

| ID | Test | Pass when |
|---|---|---|
| T-MEASURE-1 | Breakpoint and the server | The procedure in `03_TOOLS.md` section 14 is run and its outcome recorded in `07_ENGINE_NOTES.md` section 5 with the date and the engine version. Pass when the outcome is one of the three named results WITH its evidence, not when the server answers |
| T-MEASURE-2 | Automation test from a tool call | The same shape: a run of one engine automation test started from a tool call, and whether the call returned or blocked, recorded the same way |

### Debugging tools (only after T-MEASURE-1 allows them)

| ID | Test | Pass when |
|---|---|---|
| T-DBG-3 | Breakpoints | Arm two nodes and clear one: `R-BREAKPOINT` lists exactly the armed one with its graph and title; arming a node in an uncompiled graph is refused naming why |
| T-DBG-4 | Read breakpoints | A filter narrows the list; asking it to clear is refused by name and the state is returned instead |
| T-DBG-5 | Debug state | Paused at a known node: the node, the graph and the call stack from the top down match what the editor shows; a named variable reads its real value; a variable that cannot be read says unknown with the reason; with no session running it says which of the three cases it is |
| T-DBG-6 | Debug control | Step over moves one node and the state is re-read from the same code path; a step with no paused session is refused by name; any action lists the armed breakpoints afterwards |
| T-DBG-7 | PIE scenario | A scenario pressing one key at 500 ms makes the game actor that reacts to that key change state, proven by a trace hit or a variable named in the scenario; a step naming a key that is not real is refused with close real key names; the first call returns immediately and the second collects |
| T-DBG-8 | Automation report | One known-passing and one deliberately failing engine test: both appear with their state, the failure carries its first error, and a filter matching nothing lists close real test names |

### Quality, session and source control

| ID | Test | Pass when |
|---|---|---|
| T-QUAL-1 | Blueprint lint | On TA-3: the dead copy is reported with its node IDs. On TA-4: the backward getters and the node at the origin are reported. A hard reference over the threshold in TA-1 is reported with its size. Each check reports how many things it looked at, and one check named alone returns only that check |
| T-QUAL-2 | Session | Three edits inside one session are reversed by ONE undo; a session past its limit ends itself and says so; `abandon` rolls the whole series back and the asset reads back at its pre-session state; a second `begin` is refused by name |
| T-QUAL-3 | Batch | Five assets, one of them unresolvable: four lines report the change and the fifth names the resolver's refusal, and the whole batch is one undo |
| T-HELP-6 | Call cost | With the log on: the most expensive call, the slowest and the totals per tool match the log lines the same calls wrote; with the log off the answer says the log is off rather than reporting zero |
| T-VCS-1 | Checkout | With source control off: the answer says so per asset. With it on: the state is reported per asset, a checkout is attempted through the provider, and the provider's own answer is repeated. A headless run of the same call records what the provider did, because that is the case that already failed once |

### Build portability

| ID | Test | Pass when |
|---|---|---|
| T-BUILD-1 | The documented build | Following `09_BUILD.md` from a clean checkout builds the plugin and its tools appear in the MCP tool list after an editor restart. Every step that could differ on another machine says what to change |
| T-BUILD-2 | A missing engine module | Renaming one engine module the plugin depends on produces the failure `09_BUILD.md` documents, and the file's guidance identifies it. Pass when the documented symptom matches what the build actually prints |

