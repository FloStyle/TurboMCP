# Tool catalogue

Each capability has an ID such as `R-BP-LOAD`. The first letter is the
kind: R Read, L Look, P Preview, E Edit. A pair written `P/E-X` is a
Preview tool and its Edit twin. You choose the real tool names (see
section 1.3).

Each entry says what the tool must do, what it refuses, and when it is
done. Test IDs (`T-...`) point to `06_ACCEPTANCE.md`.

---

## 1. General

### 1.1 Toolsets

Group the tools into a small number of toolsets by job (for example:
Blueprint reading, material reading and editing, looking, graph editing,
agent help). A toolset that only holds Read and Look tools must not be able
to change an asset at all. Keep that true by structure, not by care.

### 1.2 Common parameters

- Asset inputs follow principle P5.
- A list input (several assets, several graphs) accepts a JSON array AND a
  single string with items split on `,` or `|`. Agents use both.
- Optional string parameters must really be optional in the MCP schema.
  See `07_ENGINE_NOTES.md` for how the registry marks parameters required.
- Large outputs take `TokenBudget` (0 means no limit).

### 1.3 Naming

- Names say the action and the target in plain words.
- Paired Preview and Edit tools use identical target words, and differ
  only in the action word (for example "Suggest X" and "Apply X"). Pick one pair
  of action words and use it for every pair.
- No abbreviations an agent would have to guess.
- Parameter names are consistent across all tools: the same idea has the
  same name everywhere (the engine's own tools do not do this, and it costs
  agents a failed call each time).

### 1.4 Panel labels

Every tool appears in the editor window list as:

```
[Kind Target] - [Source] - [Description]
```

- Kind: one of the four kinds.
- Source is the plugin's short name for its own tools, and `Unreal MCP` for
  engine tools.
- Description is a short plain sentence.
- No commas, no words in capitals, no long dashes. Explicit words only.
- Order: Read, Look, Preview, Edit. A Preview and its Edit twin use the same
  Target words.

---

## 2. Agent help (Read and Preview)

### R-TOOL-INDEX: list every tool

- Output: first the MCP server state (running or not, full address), and
  whether the server is in tool search mode (see engine notes). Then one
  line per tool in every registered toolset, engine and own: toolset, tool,
  kind, source, first sentence of its description.
- Also shown in the panel.
- Test: T-HELP-1.

### R-TOOL-SCHEMA: compact help for one tool

- Input: toolset name, tool name. Without a tool name, it gives a one line
  summary per tool in that toolset.
- Accepts the toolset name in its full form and in its short form. When the
  name fits nothing, list the closest real names.
- Output: a compact signature, one row per parameter: type, whether it is
  required, default value, and the opening sentence of its description.
  After that, the trap notes for that tool.
- Works for engine tools too. Read the schemas from the registry.
- The trap notes come from a data file that ships with the plugin rather
  than from code, so they change without a rebuild. Start the file with every engine
  tool trap in `07_ENGINE_NOTES.md` section 3.
- Output under 2000 characters for any single tool.
- Test: T-HELP-2.

### P-NODE-TYPE: find the real node type ID

- Input: Blueprint, graph, a guessed node type ID or plain words.
- Matches the guess against every node type ID that the engine's node
  creation tools accept in that graph. Normalise both sides the same way
  before comparing: case, spaces, underscores, brackets, and the category
  part before the last `|`.
- Output: no more than N matches (10 unless set). Each match gives the
  exact ID, the class that owns the function or variable, the class that
  declares it when the same name is overloaded on several classes, pure or
  not, and a short reason it matched.
  When the ID a graph text uses and the ID the engine's node tool uses
  differ, print both.
- Must find functions and variables added in this session before the asset
  is saved or compiled, and say if the asset must be compiled first for the
  engine's tool to accept the ID.
- The same matcher is used for "did you mean" in every other tool.
- Test: T-HELP-3.

### P-NODE-PINS: pins of a node type

- Input: Blueprint, graph, node type ID.
- Output: each pin's name, type, direction and default value, plus its
  position counted separately for inputs and outputs. Write the direction
  in the spelling the engine's pin tools expect. Says if the node is pure, latent, or allowed in this graph kind
  (latent nodes are not allowed in functions).
- Builds the node in a transient package and destroys it. Graph node count
  must not change (P3).
- Test: T-HELP-4.

### R-TYPE-FIND: look up a type

- Input: words, kind (struct, enum, class, any).
- Searches native C++ types by reflection AND user-made struct and enum
  assets. Says which kind each result is.
- Output: the exact path a pin or variable type needs, the module or asset
  it lives in.
- Test: T-HELP-5.

### R-ASSET-SEARCH: find any asset

- Behaves like the editor's quick open search (Ctrl+P). All asset types.
- Query: words split by spaces; every word has to appear in either the
  asset name or its folder. Two filters may appear at any position:
  `type:<class words>` and `path:<folder words>`. A query can be a filter
  and nothing else.
- Ranking: exact name first, names that start with the words next, names
  that contain them after, folder-only matches last. On a tie the shorter
  name comes first.
- Includes assets that are in memory but not on disk.
- Output: each result on its own line with its full path, its class, and,
  for a Blueprint, its parent. Capped, with a count of what was left out.
- Test: T-ASSET-1.

### R-ASSET-DESCRIBE: what is this asset

- Input: one or more assets in any path form.
- Output, one line each: package path, object path, class path if any,
  class, parent, whether it is loaded, whether it has unsaved changes, on disk or in
  memory only. All path forms of one asset give the same line.
- Test: T-ASSET-2.

### R-DIRTY-LIST: unsaved assets

- Every asset with unsaved changes, one line each, with its class.

---

## 3. Blueprint and widget reading

### R-BP-LOAD: read whole Blueprints

The main Read tool. One call gives an agent everything it needs to plan a
change.

Inputs: assets (one or many), detail level, graph filter, output format,
token budget.

Detail levels, from small to large:

| Level | Contents |
|---|---|
| manifest | Asset path, class, parent, interfaces, counts, the names of every graph, variable and component |
| signatures | Manifest, plus every variable with type, default, category and flags; every function and event with full signature; components; widget tree; the relation layer (below); no graph bodies |
| bodies | Signatures, plus every graph body in the graph text format |
| full | Bodies, plus the editing details: node type IDs, knots on links, pin indexes |

The relation layer, at `signatures` and above. This is where bugs become
visible without reading bodies:

- call graph: which graph calls which function or event
- variable map: for each variable, which graphs read it and which write it
- assets touched: every content asset the Blueprint references, by full
  path, without engine script packages and without generated skeleton
  classes
- data tables read, and with which row lookups
- functions nothing calls, and events nothing binds
- disconnected node groups in each graph, each printed once (never also as
  part of the connected graph), with node IDs

Widget Blueprints, at `signatures` and above, a compact widget tree. One
line per widget with:

- name, class, whether it is a variable
- visibility, and slot padding when it is not zero
- for a placed user widget: the properties set on this placed copy that
  differ from the class default (instance editable variables store a
  separate value per placed copy, which agents need to know)
- bound events: which graph events are bound to this widget's delegates
- a warning when a bound event or a call node expects a different widget
  class than the widget that now has that name (see E-WIDGET-RETARGET)

Do not claim a widget is unreachable unless that was checked. "Not a
variable" is a fact. "Nothing uses it" is a claim that needs proof.

Graph filter: a list of names (section 1.2); any graph whose name contains
an item matches, and case does not matter. When nothing matches, say so and list the real graph names.

Token budget: when the output would pass the budget, step down the detail
level per graph, biggest graphs first, and end with a block that lists what
was reduced and the exact call that gets the rest.

The full path of each asset is printed once at the top, so the agent has
the exact form for later calls.

Format: the graph text format (`04_FORMATS.md`), or JSON with the same
content.

Tests: T-BP-1 to T-BP-8.

### R-GRAPH-LOAD: read one graph

- Input: asset, graph name, format, detail.
- Format `read` is the default graph text. Format `write` prints the same
  graph in the form the engine's own graph write tool accepts (see
  `04_FORMATS.md` section 2.6). Whatever that tool cannot build is turned
  into a comment line naming what was left out and which tool builds it,
  so pasting the output never fails on it.
- A graph name that does not exist: list the real graph names.
- Tests: T-BP-9, T-BP-10.

### R-REFS: references in and out

- Out: content assets this asset uses. In: assets that use this asset (who depends
  on it).
- Full paths, with class.
- Test: T-BP-11.

### R-OVERRIDES: what can be overridden

- Functions and events from the parent chain and from interfaces, with full
  signatures, inputs and outputs marked correctly, and whether each one is
  already overridden here.
- Test: T-BP-12.

### R-NODE-RESOLVE: node IDs to nodes

- Input: node IDs (`04_FORMATS.md` section 3).
- Output per ID: the node reference the engine's tools accept, node title,
  node type ID, and its current pins by name with direction and per
  direction index.
- An ID whose asset part fits more than one asset is refused with the list.
  An ID whose node no longer exists says so.
- Test: T-BP-13.

### R-NODE-FIND: find nodes

- Input: asset, words, optional graph.
- Compares the words against the title, the called function, the
  variable, the node class and the pin names of each node. One graph or
  all.
- Output: node ID, graph, title, type ID.
- Test: T-BP-14.

### R-DATATABLE: describe a data table

- Row struct with fields and types, row count, row names (capped), and the
  graphs in loaded Blueprints that read this table.
- Row values are out of scope; the engine has tools for those.
- Test: T-BP-15.

### R-COMMENT-LIST: comment boxes in a graph

- Every comment box: title, colour, rectangle, parent box if nested, and
  the IDs of the nodes inside.
- Test: T-LAY-6.

---

## 4. Materials

### R-MAT-LABELS: named reroutes

- Every named reroute label in a material or material function: where it is
  declared, how many places read it, and the ones nothing reads. Call this
  first on any material.
- Test: T-MAT-1.

### R-MAT-LOAD: read a whole material

- Output: parameters with defaults and groups, then every connected output
  property as an expression tree.
- Plain reroutes are invisible: links go through them.
- A named reroute gets one definition, and every reader refers to it by
  label, which keeps the material as one connected picture instead of
  islands.
- A value used in more than one place is defined once and referred to.
  A value that already has a label is referred to only by its label, never
  also by a node ID.
- Every input of every expression is printed: the linked value, or the
  constant the expression keeps for that input when it is not linked. Read
  those constants by reflection, not from a hard coded list of expression
  classes.
- Parameters and expressions that reach no output are flagged.
- A material instance is refused with the path of its parent, unless the
  agent asks for instance overrides, which are then listed as a short table.
- Tests: T-MAT-2 to T-MAT-4.

### R-MAT-TRACE: one output only

- Input: material, output property.
- Prints only what that output depends on: only the labels and shared
  values in its tree. Shares its printing code with R-MAT-LOAD, so the two
  cannot disagree.
- An output property that is not connected: list the connected ones.
- Test: T-MAT-5.

### R-MAT-BLOCKS: material by block of logic

- For each comment box: its inputs (labels or raw wires that come in), its
  outputs (values that leave), and findings: values that cross between
  boxes without a label, labels read inside their own box, two usage nodes
  of one label in one box. Each finding with node IDs.
- Test: T-MAT-6.

### P/E-MAT-LABEL-PROMOTE: give values names

- Preview: values worth a label (used far away or in several boxes), each
  with a proposed name from context (box title, parameter names,
  expression).
- Edit: create the declaration, rewire the readers. The material's compiled
  result must be the same; check it and roll back if not.
- Test: T-MAT-7.

### P/E-MAT-BLOCKS: make labels follow blocks

- Preview: the labels to add, usage nodes to merge, in-block reroutes to
  remove, following the block rule in `05_DECISIONS.md` D-17.
- Edit: changes exactly the items in the Preview list, nothing more. Same
  compiled result check.
- Test: T-MAT-8.

### P/E-MAT-TIDY: lay out a material

- One comment box per block. Same layout rules as graphs (section 6). Same
  compiled result check.
- Test: T-MAT-9.

---

## 5. Look

### L-LAYOUT-REPORT: layout facts for one graph

Includes all nodes in the graph, connected or not. Never lists a node
twice. Reports:

- extent, node count, comment boxes (with nesting)
- total wire length (sum of straight distances between linked pins). This
  is the main layout measure.
- overlapping nodes
- backward wires, exec and data separately. Treat a wire as backward if
  its target node starts to the left of where its source node ends
  (target's left edge against source's right edge), not only when the
  target X is smaller than the source X. A node stacked under its
  predecessor is backward.
- a getter placed to the right of, or under, its consumer
- nodes at or near the origin far from everything they link to
- disconnected groups, and for each, whether it repeats logic that exists
  in the connected graph (same node types and the same function or variable
  references)
- Each finding with node IDs.
- When the graph has no wires, say so in words.
- Tests: T-LAY-1 to T-LAY-4.

### L-CAPTURE: picture of an asset editor

- Opens the asset's editor on a tab (a graph name, or a mode such as the
  widget Designer) and saves a PNG.
- Framing modes:
  - whole graph
  - what is on screen
  - one comment box, by its index in L-LAYOUT-REPORT, plus N neighbouring
    boxes
  - one node, by node ID, plus N hops of linked nodes
  - the current selection
- Options: maximise the window first, longest image edge (up to 4096).
- Always writes a PNG to a fixed folder under the project's Saved folder
  unless a path is given. Returns the path. Inline image data only when
  asked, and the description says what that costs.
- Reports what it actually framed (which box, which nodes) and the image
  size. A framing that cannot be done is an error that says why (for
  example "asked for box 9, this graph has 4"). It never falls back to the
  whole graph silently.
- Must work on Widget Blueprints (Designer and Graph), materials and
  Blueprints. Agents doing UI work are blind without it.
- Tests: T-LOOK-1 to T-LOOK-4.

### L-WIDGET-PREVIEW: render a widget with sample values

- Input: widget, sample values as JSON (variable name to value), optional
  function to call after the values are set, size.
- Makes a temporary instance off screen, sets the values, calls the
  function, renders it to a PNG, destroys the instance.
- Output: PNG path, and all script errors and warnings produced while
  doing it. No play session needed.
- Test: T-LOOK-5.

### R-PLAY-CHECK: short play check

- Input: seconds, optional map.
- Starts a play in editor session (or uses the one running), waits, stops
  it if it started it, and gives back just the warnings and errors from
  that period, grouped and counted, with their source.
- Uses the engine's own play controls underneath.
- Test: T-LOOK-6.

---

## 6. Graph layout (Preview and Edit)

The layout rules are in `05_DECISIONS.md` D-07 to D-12. Read them first.

### P/E-GRAPH-TIDY: lay out one graph

- Arranges the nodes within each comment box the user made, then fits that
  box to its new contents. Never moves a user's box as a whole.
- Nested boxes: treat each box as the child of the tightest box around it.
  Arrange the inner boxes first, then arrange the outer box with each inner
  box as one block.
- Groups with no box are ordered by execution order and placed left to
  right.
- Column of a node: the length of the longest chain of links (exec and
  data together) leading to it. Then shift each pure node rightward until
  it sits next to the node that uses it.
- A variable getter used by several consumers is split into one getter per
  consumer, placed next to it. Only variable getters get copied, because
  a getter takes no input and changes nothing. Merging getters is an option, off by default, and
  when on it only merges getters that feed the same consumer.
- Adds reroute knots where they make a wire straight or stop it crossing a
  node (same rules as P/E-GRAPH-KNOTS), and counts them in the measure.
- Disconnected groups are not moved into the layout.
- Compares total wire length from before to after. Rolls back if worse, unless
  `Force` is true. Prints both numbers on the first line.
- Tests: T-LAY-5, T-LAY-7, T-LAY-8.

### P/E-GRAPH-FIX-BACKWARD: repair bad spots only

- A local pass for graphs that are mostly fine. Inside one box, for an exec wire that
  runs backward: place the next node, with any pure nodes used only by it,
  on the row of the node before it and to its right, pushing later nodes
  along. A getter pointing backwards goes directly to the left of the node
  that uses it. Move a lone node at the origin next to what it links to.
- Never moves a box. Measures and rolls back like tidy.
- Test: T-LAY-9.

### P/E-GRAPH-KNOTS: reroute knots

- Only where a knot makes a wire one straight run plus one bend, or keeps
  it from passing over a node. Skip wires between neighbouring columns.
  Place each knot level with either the source or the target pin, in free
  space. Never stack two knots. When one pin feeds several distant nodes,
  share a single knot.
- Counts wires crossing nodes before and after. Rolls back if not better.
- Test: T-LAY-10.

### P/E-GRAPH-DEAD: remove dead copies

- Candidates: disconnected groups that cannot be reached from any entry
  and whose writes are never read. Never a group that holds an event, custom
  event, or function entry.
- Preview says for each group whether it repeats live logic.
- Edit removes them, then reports node counts.
- Test: T-LAY-11.

### P/E-COMMENT: comment boxes

- Create a box around given node IDs. Rename, recolour, resize an existing
  box. Delete a box (never its nodes). Fit every box to its contents and
  list any box left with nothing inside.
- Test: T-LAY-12.

### P/E-LOCAL-VARS: pure values to local variables

- Preview: values computed by pure nodes and used more than once in a
  function, each marked safe or unsafe with the reason. Safe means
  everything feeding the value is a function input or a literal, so
  capturing it once cannot change the result.
- Edit: promote only values the Preview marked safe.
- Test: T-LAY-13.

---

## 7. Blueprint structure (Preview and Edit)

### P/E-VARS-ADD: add variables in bulk

- Input: a list where each item gives name, type, default, category, flags
  and tooltip.
- Type: any type R-TYPE-FIND can find (enum, struct, object, class, soft
  references) and containers (array, set, map).
- Flags: private, read only, instance editable, expose on spawn.
- Refuses names that exist. Compiles a single time after adding, which lets
  the defaults be set and read back in this same call.
- Test: T-STRUCT-1.

### P/E-ENUM-ADD: create an enum asset

- Path, values with display names. Refuses an existing path.
- Test: T-STRUCT-2.

### E-WIDGETS-AS-VARS: mark widgets as variables

- Many widgets in one call. No Preview twin needed; it is a flag change and
  the Edit reports old and new state. The description says so.
- Test: T-STRUCT-3.

### P/E-OVERRIDE-ADD: create an override on purpose

- Input: Blueprint, function or event name from R-OVERRIDES.
- Preview prints the exact signature. Edit creates the override with its
  entry and result nodes. Refuses if it is already overridden, and says
  where.
- Test: T-STRUCT-4.

### E-FUNCTION-ADD and E-FUNCTION-REMOVE

- Remove a function so its name is free at once. Add a function with a
  given name; if the name is in use, refuse and name what uses it. Add
  refuses a name that matches a parent function and points to
  P/E-OVERRIDE-ADD.
- Test: T-STRUCT-5.

### P/E-WIDGET-RETARGET: fix bound events after a class swap

- When a widget was replaced by one of another class under the same name,
  bound events and call nodes may still expect the old class.
- Preview lists them. Edit: keep each bound event when the new class has a
  delegate with the same name and signature (update it and rebuild the
  node); remove it and say so when it does not. Call nodes the same way.
- Test: T-STRUCT-6.

### E-COMPILE: compile with a real answer

- Many assets. For each asset a single line: clean, or the warnings and
  errors, each tied to the ID of the node that caused it. Says if the asset still has unsaved
  changes.
- Test: T-STRUCT-7.

### E-SAVE: save without noise

- Many assets, any path form.
- Refuses while a play or simulate session runs, with that reason.
- Saves one asset at a time. Retries a failed asset once.
- Reports each asset from its unsaved flag after the save, never by
  trusting what the save call returned. States: saved, had no changes, or
  failed (with the reason).
- No engine log text. Counts log warnings raised during the save and names
  the asset that raised them.
- Test: T-STRUCT-8.

---

## 8. Graph writing (the bulk writer)

One pipeline, several entry points:

```
text -> parse -> resolve -> check -> apply -> verify
        (no writes in these three)    (compile, read back, compare)
```

### P-GRAPH-CHECK: check graph text before writing

- Parse, resolve and check only. No writes.
- Reports, each with line and column: every node type it cannot resolve
  with the closest real IDs (P-NODE-TYPE matcher), every pin name that does
  not exist with the real names, every type mismatch naming both sides,
  every construct it will refuse and why (for example a latent node in a
  function).
- Test: T-WRITE-1.

### P/E-GRAPH-APPEND: add a block to a graph

- Adds nodes, links them to existing nodes by node ID. Cannot remove
  anything.
- Test: T-WRITE-2.

### P/E-GRAPH-REPLACE: rewrite one function or event body

- Replaces the body of one function, override, or event. Writes into the
  EXISTING entry node and result nodes; never adds a second entry. Removes
  every node the new text does not describe, and lists what it removed.
- Refuses on a whole event graph unless one event is named.
- Test: T-WRITE-3, T-WRITE-4.

### P/E-GRAPH-PATCH: change only what changed

- Reads the live graph, matches the submitted text by node ID, applies only
  the differences. Nodes the text does not mention are left alone.
- Test: T-WRITE-5.

Rules for all write entry points:

- Accepts exactly the graph text format that the readers print. No second
  dialect.
- Keyword arguments for pins only. Pins not named keep their defaults. A
  positional argument is refused with the pin names.
- Any output pin can be chosen by name.
- A call without a target always means this Blueprint's own function, even
  when another class has a function with that name.
- Lays out the nodes it created (section 6 rules).
- After writing: compile, read the graph back, compare with the input, and
  list each input line that the read back does not match. A failed compare is reported as a
  failure, not a success with a note.
- One transaction. On any new compile error, roll back. First result line
  is `unchanged` or a summary.

Node kinds the writer must support, from real project graphs. Anything else
is refused by name.

| Kind | Notes |
|---|---|
| Function entry, function result, override entry, event, custom event | Write into existing ones. A function may have more than one result node |
| Branch, sequence, select, switch on enum | Pin count may depend on a value |
| Variable get and set, including local variables and function inputs | Inputs print as inputs, not as variables |
| Function call: library, self, other object, interface message | When the target is another object, use the interface message node |
| Call with several exec outputs (enum expanded as exec) | |
| Enum and object literals on pins | Object literals need the full object path |
| Break and make struct, split struct pins | Split after creation, in order, before linking |
| Promotable operators (math, compare, boolean) | Their type is set by what they are wired to |
| Format text | Pins appear after the format string is set |
| Create widget, spawn actor from class | The class input decides the other pins |
| Create event, bind and unbind event, delegates | |
| Cast | |

---

## 9. Materials writing (later)

Same pipeline as section 8, for material graphs, after R-MAT-LOAD prints
every input (constants included). Test: T-WRITE-6.

---

## 10. Debugging

### E-CALL-LOG and R-CALL-LOG: record every tool call

- Turned on by a checkbox in the panel. It starts unticked, and the choice
  persists in editor settings.
- When on, a JSON line for each call holding the timestamp, the toolset and
  tool, the arguments, the outcome or error text, how long it took, result
  size and token estimate.
- Covers engine tools too. See `07_ENGINE_NOTES.md` section 2.4 for the
  hook. Never replace an existing hook without passing events on to it.
- Write and flush a start record before the call runs, and an end record
  after it, so the last call before an editor crash is in the log.
- Read tool: filter by tool, errors only, time range; or a summary of how
  many calls and errors per tool, tokens in total, and the slowest calls.
- Panel: current log file, call count, button to open the log folder.
- Test: T-DBG-1.

### E-TRACE and R-TRACE: trace points

- Mark a node by ID. Each time it runs in a play session, record time, node,
  pin values and named variables to a trace log. No pause.
- Clear one or all. Read the trace.
- The panel shows all active trace points and has one button to remove
  them all.
- Test: T-DBG-2.

### Breakpoints (research first)

Only after checking that the MCP server still answers while the editor is
stopped at a Blueprint breakpoint. If it does not, an agent would hang.
Then: set and clear breakpoints by node ID, read the debug state (where it
stopped, the stack of calls, variable values), continue, step. Shown in
the panel with a remove all button.

---

## 11. Editor window

- One dockable window, in the editor's Tools menu, in its own section.
  Choose a plain icon from the editor's own icon set.
- Top row: whether the server runs, shown as a green or red dot, followed
  by its full address. Re-read on every paint.
- Tool list: every tool from R-TOOL-INDEX, engine and own, in the label
  format of section 1.4.
- Inputs shown only when the selected tool uses them.
- Asset picker modelled on the editor's quick open dialog: list view
  with readable names and icons, the type filter chips row, and filters and
  view settings remembered between openings and editor restarts. A tool
  may set a starting filter only when the user has saved none.
- Use the Content Browser selection as input.
- Graph dropdown filled from the picked asset.
- Output pane with line count and token estimate.
- Call log controls (section 10) and the trace point list.
- Calls the same functions as the MCP (P9).
- Tests: T-PANEL-1 to T-PANEL-6.
