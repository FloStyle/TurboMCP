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

### P/E-GRAPH-SWAP-CALL: replace one call node by named pins

- Replaces exactly one `UK2Node_CallFunction` selected by its reader node ID with a call node created from the canonical node type ID.
- Migrates every linked pin by exact pin name and direction, never by pin index. A linked old pin missing on the replacement is named and refused; it is never silently disconnected.
- `keepOld` defaults to `true`: the old call remains unplugged. With `keepOld=false`, the old node is deleted only after every replacement link is verified.
- Preview and Edit share one side-effect-free plan. Edit uses one transaction, rolls back on a failed connection or verification, compiles, and reports the replacement ID, migrated pins, unmatched pins, and old-node disposition.
- Tool arguments are `asset`, `graphName`, `nodeId`, `replacementTypeId`, and `keepOld` (`true` or `false`).
- This is deliberately separate from GRAPH-REPLACE: swap is a localized node transformation, while replace rewrites and may delete an entire body.
- Test: T-WRITE-6.

### P/E-GRAPH-REPOINT: repoint every reader of an output

- Input: `asset`, `graphName`, `sourceNodeId`, `sourcePin`, and either an existing `targetNodeId` or `targetTypeId` plus `targetPin` for a new source. `keepSource` defaults to `true`.
- Preview resolves the source once, enumerates every named consumer, and validates the exact target output and every proposed connection without writing.
- Edit uses one transaction, breaks and reconnects each reader, reads back every consumer independently, and reports every `consumerNodeId.consumerPin`, old endpoint, new endpoint, count, and whether the old source was retained. A failed connection or readback rolls back all links and any created node.
- A missing or direction-incompatible pin is refused by name and leaves the original links intact. A new target reports its real node ID and pin after apply. `keepSource=false` deletes the old source only after verification.
- This is separate from GRAPH-SWAP-CALL: repoint changes all consumers of one output and may create a replacement source.
- Test: T-WRITE-7.

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

The write entry points are the same three names as section 8, with `MAT` in
place of `GRAPH`: `P/E-MAT-APPEND`, `P/E-MAT-REPLACE`, `P/E-MAT-PATCH`, and
the gate `P-MAT-CHECK`. They differ from the graph ones in exactly one way:
the compiled result must be the same, because a material's output is what it
computes and not the text of its graph. The same measured check that
`E-MAT-TIDY` and `E-MAT-LABEL-PROMOTE` already perform (set of expressions
identical, no new engine error, `StateId` reported and not asserted) is the
gate for all three.

A material instance is NOT written through these. It is not a graph: it is a
list of overrides, and it gets its own pair (`P/E-MAT-INST-SET`).

### R-MAT-INST-LOAD: read a material instance

- Input: one material instance, optional detail.
- Output: the parent chain up to the base material, then every parameter the
  instance overrides with its value, then every parameter the parent has that
  this instance does NOT override, marked as inherited.
- The second list is the one that matters: an agent asked to change a
  parameter needs to know whether the value it wants to change is really on
  this instance or inherited from a parent it does not own.
- A parameter name the parent does not declare is reported as orphaned
  rather than dropped, because a renamed parameter leaves one behind.
- Test: T-MAT-10.

### P/E-MAT-INST-SET: change overrides on a material instance

- Input: instance, a list of `Parameter = value` pairs, and a flag for what
  to do with a parameter the parent does not declare.
- Preview: each parameter with its current value, its new value, and whether
  it is inherited, overridden here or orphaned. A parameter that does not
  exist on the parent is refused by name with the closest real parameter
  names, because setting one would write a value nothing reads.
- Edit: sets the overrides and re-reads them through R-MAT-INST-LOAD rather
  than reporting its own intention.
- Also reports the compiled `StateId` before and after, and rolls back when
  the engine raises a NEW error.
- Test: T-MAT-11.

### R-MAT-ARGS: read a material parameter collection

- Input: collection asset.
- Output: one line per parameter with its name, its type (scalar or vector),
  its default value and how many loaded materials or Blueprints read it.
- A collection parameter nothing reads is printed as unused, with its index,
  because a collection is small and an unused entry is nearly free to see
  and expensive to guess.
- Test: T-MAT-12.

### P/E-MAT-ARGS-SET: edit a material parameter collection

- Input: collection, list of parameters to add, change or remove, each as
  `Name: Type = Default`.
- Preview: every change, and for a removal, the loaded readers that reference
  it by index. **A removal changes the index of every later parameter**, so
  the readers are listed and the shift is stated in words. When readers
  exist, the removal is refused unless `force` names the count.
- Edit: applies, then re-reads the collection through R-MAT-ARGS.
- Test: T-MAT-13.

---

## 10. Graph structure tools

The capabilities in this section are the ones an agent needs to restructure
a Blueprint rather than to edit one body. They are all in the graph writing
family and share its pipeline (`08_ROADMAP.md`).

### E-GRAPH-RENAME: rename a variable, function or event, with the references

- Input: Blueprint, a kind (`variable`, `function`, `event`, `custom event`),
  the current name, the new name.
- Renames the member AND every reference to it, in every graph of the
  Blueprint, through the editor's own rename path rather than by rewriting
  nodes: `FKismetEditorUtilities` and `UBlueprintEditorLibrary` already do
  this, and a second implementation would disagree with the editor the first
  time either changed.
- Refuses a new name that is already used, and NAMES what holds it, exactly
  as `E-FUNCTION-ADD` does.
- Reports, as separate counts: how many getters, setters, call nodes, local
  uses, widget property bindings and delegate bindings were updated, and how
  many references it could NOT update, each named. A rename that leaves one
  broken reference compiles into an error the agent did not ask for.
- Then compiles and reports new errors, and rolls back on any new one.
- NO PREVIEW TWIN, deliberately, and the description says so: the rename is
  not reversible by a Preview, and its refusals are all name checks that
  `R-BP-LOAD` and `R-OVERRIDES` already answer.
- Test: T-STRUCT-9.

### E-GRAPH-COLLAPSE: extract a selection into a function or a macro

- Input: Blueprint, graph, the node IDs to extract, the new member's name,
  and `function` or `macro`.
- The selection's boundary is computed, not guessed: every link that crosses
  the selection edge becomes an input or an output of the new member, in
  order. A selection with no exec boundary is refused, naming why, because a
  function needs one entry and one result.
- Uses the engine's own `FBlueprintEditorUtils::CollapseNodesIntoGraph`. A
  hand-built member would not be the thing the editor's own Collapse makes.
- Refuses a selection that contains an entry, an event or a result node,
  naming it, because collapsing those is not a refactor the agent asked for.
- Reports the new signature (inputs and outputs with their types) READ BACK
  from the created member, the replaced node count, and the compile result.
  One transaction; rolled back on any new compile error.
- Test: T-STRUCT-10.

### E-GRAPH-COPY: copy a block from one graph to another

- Input: source asset, source graph, node IDs, target asset, target graph,
  and optional target node ID to link the copied block's entry into.
- The boundary is computed the same way as collapse: a link that leaves the
  selection and stays inside the target scope is re-linked; a link that
  leaves it and has nothing to point at in the target is REPORTED by name and
  left unconnected rather than silently dropped.
- A variable, function or event the target asset does not have REFUSES the
  whole copy and names each missing member, because half a block is worse
  than none.
- Same asset and graph on both sides is refused by name and pointed at the
  paste-in-place case, which is what a duplicate actually is.
- One transaction. Test: T-STRUCT-11.

### P/E-INTERFACE: add, implement and remove interfaces

- Input: Blueprint, interface asset path, action (`add`, `remove`).
- `add` adds the interface and reports which of its functions are not
  implemented yet, by name, from the same source `R-OVERRIDES` uses.
- `remove` REFUSES while the Blueprint still implements any of the
  interface's functions, and names them, unless `force` is given: removing
  the interface with implementations still present leaves graphs nothing
  calls and a Blueprint that no longer compiles.
- A name that fits more than one interface asset is refused with both paths
  (P2).
- Preview lists the change and what would be left unimplemented. Edit
  re-reads the interface list from the Blueprint afterwards. One transaction.
- Test: T-STRUCT-12.

---

## 11. Widget authoring

`R-BP-LOAD` already reads a widget tree. Nothing writes one. This section is
that writer, and it needs a text dialect of its own, because a widget tree is
not a graph: it is an ordered hierarchy with per-slot layout on every child.
The dialect is specified in `04_FORMATS.md` section 8 and follows the same
rule as `D-15`: what the reader prints is what the writer accepts.

### P/E-WIDGET-TREE: write the widget tree

- Input: Widget Blueprint, tree text in the dialect of `04_FORMATS.md`
  section 8, and an action: `append`, `replace`, `patch`.
- The three actions mean exactly what their graph twins mean: `append` adds
  and cannot remove; `replace` rewrites the whole tree under the root and
  LISTS every widget it removes; `patch` matches by widget path and changes
  only what the text describes.
- Every widget is addressed by its path in the tree
  (`Root/Overlay/Card/PriceText`), which is the form `R-BP-LOAD` prints, so
  an agent never invents a name. A path that does not exist is refused with
  the closest real paths.
- Renaming a widget is a REFUSAL, not a rename: a name that changes breaks
  every binding and every call node that names it. `P/E-WIDGET-SET` changes
  the name and updates the references, in one place, on purpose.
- A panel's slot count is enforced where the engine enforces it: adding a
  third child to a `SizeBox` is refused by name rather than accepted and
  discovered at compile time.
- The reader and the writer share one code path, as `D-15` requires: the
  tree prints in the dialect this accepts, at any detail level.
- One transaction, compile, read back, compare, roll back on a new error.
- Tests: T-WIDGET-1 to T-WIDGET-4.

### P/E-WIDGET-SET: widget properties, slots and style

- Input: Widget Blueprint, widget path, and property assignments.
- Covers three layers, and names which one each assignment lands on:
  - the widget's own properties (`Visibility`, `ToolTipText`, `bIsEnabled`),
  - its SLOT properties, which live on the slot type and not on the widget
    (`Padding`, `HorizontalAlignment`, `Anchors`, `Offsets`, `Size`),
  - its STYLE, for the widgets that carry one (`FButtonStyle`,
    `FSlateBrush`, fonts, colours), addressed by style member.
- Values are set through the engine's own text import, so the accepted
  spellings are the details panel's and not a format invented here. A
  property name that does not exist is refused with the closest real names
  from that widget's reflection, which is what `07_ENGINE_NOTES.md`
  section 3.2 requires, because widget property names are the C++
  names and their case varies.
- A property marked not instance editable is refused by name; setting it
  would write a value the instance cannot hold.
- Renaming a widget is done HERE and here only: the name change is followed
  by updating every binding and every call node that names it, and the count
  of each is reported. When a reference cannot be updated it is named and the
  whole change is rolled back.
- Preview lists old value, new value and the layer for each assignment. Edit
  re-reads each value afterwards rather than reporting its intent. One
  transaction.
- Test: T-WIDGET-5.

### R-WIDGET-ANIM: read widget animations

- Input: Widget Blueprint, optional animation name.
- Output per animation: its name, its length in seconds, whether it loops,
  and one line per track: the target widget's path, the property animated,
  and every key with its time, its value and its interpolation.
- A track whose target widget no longer exists is reported as broken with the
  missing path rather than skipped, because a widget rename is exactly how
  that happens and the animation is the last place anyone looks.
- Test: T-WIDGET-6.

### P/E-WIDGET-ANIM: write widget animations

- Input: Widget Blueprint, animation name, and a list of tracks, each naming
  a widget path, a property and its keys.
- An animation that does not exist is created; one that exists is refused
  unless `replace` is asked for, and the refusal names it.
- Preview: every track with the keys it would write, and for an existing
  animation the keys it would change or drop, listed rather than counted.
- Edit: one transaction, then re-reads through R-WIDGET-ANIM and reports
  every key that did not read back as written.
- Test: T-WIDGET-7.

### P/E-WIDGET-BIND: property and event bindings

- Input: Widget Blueprint, widget path, kind (`property` or `event`), the
  target function or property, action (`add`, `remove`).
- `event` and `property` are different things and are refused apart: an
  event binding answers a delegate and points at a function; a property
  binding answers a value and points at a function or a property. A caller
  that names the wrong one is told which is which.
- `R-BP-LOAD` already reads event bindings. This pair adds the WRITE side and
  the property-binding side, and reuses `EditTurboMCPWidgetBindAdd`'s code
  rather than growing a second binder (`E-WIDGET-BIND-ADD` stays as the
  low-level single-binding tool and is pointed at by this one).
- A remove that would leave a function with no caller is reported as such and
  not refused, because an agent may be about to delete that function next.
- Preview re-reads through the readers; Edit re-counts the bindings from the
  Blueprint afterwards. One transaction.
- Test: T-WIDGET-8.

---

## 12. Assets, structs and data

### P/E-DATATABLE-ROWS: rows of a data table

- Input: DataTable, and a list of row operations, each naming a row and
  either the field values or `remove`.
- Values are written through the row struct's own text import, so the
  accepted spellings are the struct's export text and not a format invented
  here. A field name that does not exist is refused with the real field
  names.
- A row added must have a struct-compatible name (the engine requires it, and
  deriving a name silently is how two rows end up fighting over one entry).
- Preview: each row with its current and new values, listed field by field,
  and each removal listed by name.
- Edit: one transaction, then re-reads the row names and each changed value
  back from the table. A table with unsaved changes is reported as such.
- This is only possible because the table asset already exists: `RowStruct`
  is read-only once created (Q-6), so this tool never creates a table.
- Test: T-DATA-1.

### P/E-STRUCT-NEW and P/E-STRUCT-FIELDS: user structs

- `P/E-STRUCT-NEW`: create a user struct asset at a package path, with its
  fields in one call. Each field is `Name: Type`, plus optional
  `default=`, `tooltip=`, `category=`. Refuses an existing path rather than
  overwriting it.
- `P/E-STRUCT-FIELDS`: add, change, reorder and remove fields on an existing
  struct. A removal is reported with the count of loaded readers that use
  that field, and refused unless `force` names that count, because a struct
  field removal breaks every asset that reads it.
- Both go through `FStructureEditorUtils`, which is what the editor's own
  struct editor calls.
- **These cannot be done from a commandlet.** `AddVariable` needs a
  `UUserDefinedStructEditorData` that only the editor factory installs; the
  crash that proved this is recorded in Q-15 (TA-10). They are therefore
  EDITOR-ONLY, and their tests run against a live editor. A commandlet that
  needs a user struct imports it rather than building it.
- **CORRECTED, MEASURED, AND THE CORRECTION IS NARROWER THAN IT LOOKS.** The
  crash is real and the commandlet limit stands. But the line above names the
  wrong cause, and it was read that way for long enough to be written into
  `D-51`. `FStructureEditorUtils::CreateUserDefinedStruct()`
  (`Engine/Source/Editor/UnrealEd/Private/Kismet2/StructureEditorUtils.cpp:42-64`)
  is a PUBLIC STATIC that INSTALLS the `UUserDefinedStructEditorData` itself at
  its **line 49-50**, and then calls `AddVariable` on it at its own **line 59**.
  The commandlet's crash came from calling `NewObject<UUserDefinedStruct>`
  directly and then reaching for the field list - it stopped ONE CALL SHORT of
  the API that works.
  What that means for this spec: **these tools need an EDITOR PROCESS, not an
  OPEN ASSET EDITOR.** Nothing in `CreateUserDefinedStruct`, `AddVariable`,
  `RemoveVariable`, `RenameVariable`, `ChangeVariableType`, `MoveVariable` or
  `CompileStructure` reads `GEditor` or an editor tab. So they are not
  EDITOR-ONLY in the sense the paragraph above implies; they are
  LIVE-EDITOR-ONLY, which is a different and much weaker limit - the same one
  every other tool in this plugin already has. The prohibition that matters is
  unchanged: a COMMANDLET still cannot make one, and still imports it instead.
- Tests: T-DATA-2, T-DATA-3.

### P/E-ENUM-EDIT: change an existing enum

- Input: enum asset, and the full desired value list with display names.
- `P/E-ENUM-ADD` creates an enum and refuses an existing path. This is the
  other half: it changes one, and it is a separate tool because the refusal
  in the add is the safe behaviour and must not be relaxed.
- Preview: values added, renamed, removed, and REORDERED, each listed.
  `_MAX` and the underlying type are read-only and any attempt to change them
  is refused by name.
- A removal is reported with the count of loaded readers that reference that
  value. Because a user enum's value ORDER is its serialised identity,
  reordering is reported as a change that affects saved data, in words.
- Test: T-DATA-4.

### R-ASSET-COST: the size map and hard references of one asset

- Input: one asset, optional detail.
- Output: the asset's own serialised size, the size of what it pulls in,
  split into hard references, soft references and editor-only references;
  then the largest contributors, capped, each with its size and why it is
  referenced.
- The numbers come from the asset registry's own size and dependency data
  plus the package's serialised size. A size that cannot be read is reported
  as unknown with the reason, never as zero (`P1`, `D-05`).
- This is the check that catches "a heavy hard reference in a Blueprint that
  a data asset could have carried", which is invisible in any read of the
  graph.
- Test: T-ASSET-3.

### P-ASSET-DIFF: diff two versions of one asset, in text

- Input: two assets, or one asset and a saved copy of it, plus an optional
  scope.
- Prints, in the same text the readers produce, only what differs: variables
  added, removed, retyped; functions added, removed, with a changed
  signature; graph bodies with a line-level diff of the graph text; widget
  tree changes; material expression and parameter changes.
- It is a PREVIEW and not a Read, because it is the tool an agent calls
  before and after an edit to see what it did, and it belongs beside the
  other "work it out and report it" tools. It writes nothing.
- When the two assets are of different classes the diff is refused by name,
  because a diff between a widget and a material is not a diff.
- A field it cannot compare is listed as not compared, with the reason. A
  diff that quietly omits a section reads as "nothing changed there", which
  is the one thing a diff must never say wrongly.
- Test: T-ASSET-4.

### E-ASSET-NEW, E-ASSET-RENAME, E-ASSET-MOVE, E-ASSET-DUPLICATE

- `E-ASSET-NEW`: create an empty asset of a named class at a path. Refuses an
  existing path.
- `E-ASSET-RENAME` and `E-ASSET-MOVE`: rename or move, then FIX THE
  REDIRECTORS, because a rename that leaves redirectors behind breaks every
  asset that pointed at the old path and does it silently, at a later date,
  in someone else's session.
- `E-ASSET-DUPLICATE`: copy an asset to a new path.
- All four report the OLD and NEW object paths, the count of other assets
  whose references were updated, and the redirectors fixed. They call the
  editor's own asset tools, which already do the fixup, rather than moving
  files on disk.
- A rename or move of an asset that is currently open in an editor tab
  REFUSES and names the tab, because the editor's own tools are the only
  thing that can close it safely and this tool cannot.
- Tests: T-ASSET-5, T-ASSET-6.

### R-CURVE-LOAD and P/E-CURVE-SET: curve assets and curve tables

- `R-CURVE-LOAD`: read a `UCurveFloat`, `UCurveVector`, `UCurveLinearColor`
  or `UCurveTable`. Prints the interpolation, the key count and every key
  with its time and value. For a curve table, also prints the row names.
- `P/E-CURVE-SET`: replace the keys of one curve or one curve table row,
  with the same add / replace / remove vocabulary as the other writers, then
  re-read through `R-CURVE-LOAD` and report every key that did not read back.
- A curve with no keys and a table with no rows are both printed as facts,
  not as empty output.
- Tests: T-DATA-5, T-DATA-6.

---

## 13. Level and actors

### R-LEVEL-LOAD: read a level

- Input: a map asset, an optional filter (words, class, folder) and an
  optional detail level.
- Output: the map's own path and world settings, then one line per actor:
  its label, its class, its folder, whether it is editor-only, whether it is
  a Blueprint actor and which Blueprint, and its transform.
- Detail `properties` adds the actor's editor-visible property values that
  differ from the class default, read by reflection. That is the list an
  agent needs before it changes anything, because a value that matches the
  default is not written anywhere.
- Components are NOT printed at this level: `R-ACTOR-LOAD` does that for one
  actor, because a level with 4000 actors cannot carry 4000 component trees
  in one answer.
- The actor count is always printed before the filter is applied, so a filter
  that matches nothing cannot look like an empty level.
- A map that is not currently loaded is loaded read-only; the result says
  whether it had to load it.
- Test: T-LEVEL-1.

### R-ACTOR-LOAD: read one actor and its components

- Input: a map asset and an actor (by label, by path, or by node ID), and an
  optional detail level.
- Output: the actor's class, transform, every editor-visible property with
  its value, and its COMPONENT HIERARCHY: one line per component with its
  name, its class, its parent in the hierarchy, its transform, and the
  properties that differ from the class default.
- An actor whose label fits more than one actor in the level is refused with
  every candidate and its path, because a label is not an identity (P2).
- A Blueprint actor's components are marked as inherited from the Blueprint or
  added on this instance: the difference decides whether changing one belongs
  in `P/E-COMPONENTS` or here.
- Test: T-LEVEL-2.

### E-ACTOR-PLACE: place actors in a level

- Input: a map asset, a list of actors to place: class or Blueprint path,
  transform, label, folder, and optional property values.
- Uses the editor's own actor factory path, so a placed Blueprint actor is
  the same thing the Place Actors panel makes, with its components built.
- A class or Blueprint path that fits nothing is refused with the closest
  asset names for a Blueprint and the closest class names for a native class.
- Reports each placed actor's label and path as the editor now sees it, read
  back from the level, rather than the transform it was asked for.
- One transaction for the whole list. Test: T-LEVEL-3.

### P/E-ACTOR-SET: move and configure actors

- Input: a map asset, one or more actors (label, path or node ID), and
  property or transform assignments.
- Preview: each actor with its old and new values, and for a transform the
  old and new location, rotation and scale.
- Edit: sets the values and RE-READS each one from the actor, reporting every
  assignment that did not read back as written. One transaction.
- Renaming an actor's LABEL is allowed and is not the same as renaming the
  actor's object name, which is a different operation this tool refuses by
  name and points at the content tools for.
- Test: T-LEVEL-4.

### E-COMPONENTS: edit a Blueprint actor's component hierarchy

- Input: a Blueprint actor asset, and a list of component operations: add a
  component of a class, remove one, reparent one, rename one, and set its
  properties.
- This is the Blueprint's component list, NOT a placed instance's overrides:
  a change here reaches every instance, and the result says so in words,
  because that is the mistake this tool exists to make impossible.
- An instance-level change is refused by name and pointed at `E-ACTOR-SET`,
  and the two outputs name their scope explicitly.
- A removal is reported with the count of loaded levels that place this
  Blueprint, because removing a component an instance overrides loses the
  override.
- One transaction, compile, read back, roll back on a new error.
- Test: T-LEVEL-5.

---

## 14. Debugging

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

### Breakpoints: research first, and the research is the first deliverable

`D-36` says trace points come before breakpoints because a breakpoint stops the
editor in a nested loop. That reasoning still stands, and it is now an
EXPERIMENT rather than a paragraph.

**The measurement, before any breakpoint tool is written:**

1. Arm one Blueprint breakpoint by any means, in one node that runs early in a
   play session.
2. Call `R-TOOL-INDEX` over the MCP while the editor is stopped there, with a
   short timeout.
3. Record which happened, with the evidence: the call answered; the call
   blocked until the session was resumed by hand; the editor stopped
   answering the server entirely.
4. Repeat with a breakpoint hit from a node whose Blueprint is being edited in
   an open tab, because the nested loop may only block the thread that owns
   the paused session.

The result decides the shape of everything below and is written into
`07_ENGINE_NOTES.md` section 5 with the date and the engine version.

- If the server **answers**, the tools below are buildable as specified.
- If the server **blocks**, then no tool that pauses can exist in this plugin,
  because the agent that called it would hang until a human resumed the game.
  That is a HARD refusal, not a limitation to work around by waiting. The
  alternative for that case is already built: `E-TRACE` and `R-TRACE` report
  pin values and named variables without pausing, and the honest answer to
  "why can I not step through it" belongs in the refusal text of the
  breakpoint tools, or in a decision that they are not built at all.
- If the server **answers but slowly** (a call that has to wait for the game
  thread), the tools must be split into a request half and a collect half
  exactly like `R-PLAY-CHECK`, and their descriptions must say so.

**The tools, once the measurement allows them:**

### E-BREAKPOINT: set and clear breakpoints

- Input: `action` (`set`, `clear`), one or more node IDs, and an optional
  enabled flag.
- A breakpoint is armed on a NODE, by the same node ID every other tool uses.
- It refuses to arm a node in a graph that is not compiled, naming why: a
  breakpoint on a node the VM does not run is a breakpoint that never hits,
  and "it never stopped" is the worst evidence to debug from.
- Everything it arms is listed by `R-BREAKPOINT`, so `P8` holds: nothing an
  agent sets for debugging is invisible to the human at the keyboard.
- Test: T-DBG-3.

### R-BREAKPOINT: the breakpoints that are armed

- Input: optional asset or graph filter.
- Output: every armed breakpoint with its node ID, its graph, its node title,
  whether it is enabled, and its hit count this session.
- This is the READ half and cannot arm or clear anything; asking it to is
  refused by name and the current state is returned instead, exactly as
  `R-TRACE` does.
- Test: T-DBG-4.

### R-DEBUG-STATE: where the session is stopped

- Input: optional node ID, optional list of variables to read.
- Output: whether a session is paused at all; if it is, where it stopped
  (node ID, graph, the Blueprint instance), the CALL STACK from the top down
  with each frame's function and its current node, and the values of the
  named variables, each read through reflection.
- A variable whose value cannot be read at this stop is reported as unknown
  with the reason, never as a default or a zero (`D-05`). This is the whole
  point of the tool: a wrong variable value during a pause sends an agent
  down a wrong path with full confidence.
- When no session is paused it says which of the three cases it is: no play
  session, a play session running, or a paused session it cannot see.
- Test: T-DBG-5.

### E-DEBUG-CONTROL: continue, step, stop

- Input: `action` (`continue`, `step over`, `step into`, `step out`, `stop`).
- `continue` in this tool means "resume and let the session run", never
  "resume and pause again somewhere".
- `stop` stops a session this plugin did not start. That is deliberate and it
  is the one place `R-PLAY-CHECK`'s "leave someone else's session alone" rule
  does not hold, because a session stopped at a breakpoint is a session the
  user cannot use either. The description says so in those words.
- A step with no paused session is refused by name rather than silently doing
  nothing.
- Every action answers with the new debug state, from the same code path as
  `R-DEBUG-STATE`, so the two cannot disagree (`D-02`).
- Test: T-DBG-6.

### P/E-PIE-SCENARIO: run a play session with scripted input

- Input: a scenario: a map, a list of steps, and a duration. A step is a wait,
  a key press or release, a mouse move, a click or release, or an axis value,
  each with a time offset in milliseconds.
- The scenario is applied through the engine's own input path, so what the
  game receives is an input event and not a direct call into a function. A
  tool that called the handler directly would prove nothing about the input
  path, which is what the capability is for.
- **This is two calls, and that is the engine rather than a choice.** A tool
  call runs on the editor's game thread and PIE is ticked on that same thread,
  so a call that started a session and then played the scenario would block
  the one thread the session needs. `R-PLAY-CHECK` is already split for
  exactly this reason. The first call validates the scenario and starts the
  session; the second collects the result, the warnings and errors with their
  source, the final state of any variables named in the scenario, and any
  trace hits recorded during it.
- A step that names a key that is not a real key is refused by name with the
  closest real key names.
- One session at a time, like `R-PLAY-CHECK`. A second request while one is
  pending is refused by name rather than mixing two runs.
- Preview validates the scenario and reports what it would press, in order,
  with the times, without starting anything.
- Test: T-DBG-7.

### R-AUTOMATION-RUN: run the automation tests and read the report

- Input: an optional filter of test names, an optional filter of flags, and a
  timeout.
- Starts the engine's own automation tests (`FAutomationTestFramework`), waits
  for the run, and prints the report: one line per test with its state
  (success, failure, warning, not run), its duration, and for a failure its
  first error message with the file and line the engine reported.
- A test list that matches nothing is refused with the closest real test
  names, because the engine's test names are long and a typo is the common
  case.
- **Whether a tool call may wait for a test run at all is an open
  measurement**: an automation test can tick the editor, and a call that
  waits on the game thread may deadlock the same way a paused breakpoint
  would. The measurement and the two-call fallback are the same as
  `E-BREAKPOINT`'s, and the result goes into `07_ENGINE_NOTES.md` section 5.
- Test: T-DBG-8.

---

## 15. Quality and maintenance

### R-BP-LINT: what is wrong with this Blueprint

- Input: one or more Blueprints and an optional check filter.
- Every finding carries a node ID, a severity, and one line saying why it is a
  finding. The checks are named, not numbered, so a caller can ask for one:
  - **dead nodes**: the disconnected groups `P-GRAPH-DEAD` would remove, with
    the entry rule already applied, so nothing an event reaches is ever
    reported;
  - **unnecessary casts**: a cast whose result is only used to call a function
    that is already on the class, or a cast on an object whose type is already
    known at that point;
  - **expensive Tick**: work done directly in a Tick or a `ReceiveTick` that
    could be event driven, with the nodes it depends on named. This is a
    REPORT and never a claim of a performance number, because no such number
    can be measured from here;
  - **heavy hard references**: hard references to assets over a threshold,
    read from the registry, each naming the asset and its size (the same
    numbers as `R-ASSET-COST`);
  - **unused variables, functions and events**: computed the way
    `R-BP-LOAD`'s relation layer computes it, printed only as the FACTS the
    relation layer can prove (`D-05`): a variable nothing reads, a function
    nothing calls, an event nothing binds;
  - **unconnected pins and orphaned nodes**: an input pin that is required and
    not linked, and a node nothing reaches.
- Each check says how many things it looked at, so a check that found nothing
  is distinguishable from a check that could not run (`P1`).
- This is a READ tool and it never edits: the fix for a finding is the
  existing tool, named in the finding.
- Test: T-QUAL-1.

---

## 16. Agent and session tooling

### R-CALL-COST: the calls that cost the most

- Input: an optional time range, an optional tool filter and a cap.
- Reads the call log the same code writes and answers the question the log
  exists for: which calls cost the most tokens, which are slowest, which
  failed, and what the totals are per tool and per asset.
- Every line names the exact call that produced it, so the next step is
  another call and not a guess (`P7`).
- An empty answer says which of the three it is: logging off, on with no
  entries, or on with entries that do not match the filter (`P1`). It reads
  the same log `R-CALL-LOG` reads and shares its parser, so the two cannot
  disagree.
- Test: T-HELP-6.

### E-SESSION: one undo for a whole series of edits

- Input: `action` (`begin`, `end`, `abandon`), and a name.
- `begin` opens one editor transaction and a named session. Every Edit tool
  called while the session is open joins that transaction instead of opening
  its own, so the whole series is ONE undo.
- **The session is bounded and it refuses to be unbounded**: a session older
  than a configurable number of minutes, or holding more than a configurable
  number of edits, ends itself and SAYS SO rather than letting an abandoned
  agent leave the editor holding one transaction for the rest of the day.
- `abandon` rolls the whole transaction back, which is the escape hatch an
  agent needs after a series that went wrong halfway.
- `R-EDIT-SESSION` reads the state: whether a session is open, its name, how
  long it has been open and how many edits joined it.
- An Edit that is REFUSED inside a session does not end it or corrupt it: it
  reports the refusal and leaves the session open, because one bad call in a
  series should not throw away the good ones.
- A second `begin` while one is open is refused by name and names the open
  session.
- Test: T-QUAL-2.

### P/E-ASSET-BATCH: many assets in one call

- A cross-asset batch is not a new capability so much as a rule, and it is
  written here because it keeps being asked for: **every tool that takes one
  asset takes a list**, in the list form of section 1.2, and answers one line
  per asset, in the order given.
- The rule has three parts and all three are already how `E-COMPILE`,
  `E-SAVE`, `R-ASSET-DESCRIBE`, `P/E-VARS-ADD` and `R-BP-LOAD` behave:
  one line per asset, a failure on one asset does not stop the others, and an
  asset that could not be resolved is named with the resolver's refusal.
- `E-ASSET-BATCH` is the deliberate exception to "there is no batching tool":
  it applies the SAME operation to many assets in ONE transaction, which is
  what makes it worth a tool. It is `E-SESSION` plus a list, and it is
  specified as such rather than as a second implementation.
- Test: T-QUAL-3.

---

## 17. Source control before a write

### E-VCS-CHECKOUT: take the lock before writing

- Input: one or more assets, or no argument to read the state.
- Before an Edit writes to an asset, the file must be writable in the source
  control system the project uses. Perforce will refuse a save that is not
  checked out; git will not, and will happily leave a modified file for
  someone else to find.
- This tool reports, per asset: whether the project has source control on at
  all and which provider; whether the asset's file is under source control;
  whether it is checked out for edit here; and whether it is out of date with
  respect to the depot.
- With assets given it CHECKS THEM OUT, through `ISourceControlModule`'s own
  provider rather than by shelling out to `p4` or `git`, and reports what the
  provider answered per asset.
- **The engine's own checkout path fails headless**, and `07_ENGINE_NOTES.md`
  records the consequence: `FEditorFileUtils` tried to check out during a
  headless save, failed, and made `E-SAVE` report a failure while nothing had
  been written. So this tool must exist BEFORE any automatic checkout is
  wired into the write tools, and the headless behaviour of each provider is
  measured and recorded before it is relied on.
- No automatic checkout happens anywhere until this tool's own tests pass.
  That ordering is the decision, not an implementation note.
- Test: T-VCS-1.

---

## 18. Vision: what a Look may capture

Section 5 specifies `L-CAPTURE` for a Blueprint graph. The capture tools below
extend the same PNG contract of `04_FORMATS.md` section 6 to every editor an
agent works in, and they are grouped here so the framing rules are written
once.

### What every capture tool shares

- The PNG contract of `04_FORMATS.md` section 6: a path under the project's
  Saved folder, the image size, and what was actually framed.
- `MaxEdge` is 4096 and is the same constant everywhere, so two captures of
  the same thing at the same size are the same image.
- A framing that cannot be done is an error naming why, and it NEVER falls
  back to a wider frame. Asking for box 9 of a graph with 4 boxes says exactly
  that.
- An empty frame is refused rather than written as a blank image, because a
  blank image looks like a successful capture of nothing.

### Framing modes added to L-CAPTURE

| Mode | Target | What it frames |
|---|---|---|
| `selection` | none | the nodes currently selected in the open graph |
| `neighbours` | node ID and N | the node plus every node within N hops of linked nodes |
| `box-neighbours` | box index and N | one comment box plus the N boxes it links to |
| `screen` | none | the whole editor window, at its current size |

- `neighbours` followed a link THROUGH a knot as a hop, because a knot is not
  a node an agent thinks in. The hop count is therefore over the graph the
  READER prints, not over the engine's node list, so the number an agent reads
  is the number it can act on.
- `selection` with nothing selected is refused and says so; it does not
  capture the whole graph, which is the silent fallback this rule forbids.
- Test: T-LOOK-7.

### L-CAPTURE-ASSET: capture any asset editor

- Input: an asset, the tab or mode to open (a graph name, a Designer, an
  animation timeline, a Niagara system graph), a framing mode and a size.
- Works on the editors an agent actually works in: material, Material
  Instance, Material Function, Widget Blueprint (Designer and Graph),
  DataTable, Animation Blueprint, Anim Sequence, Niagara, and Blueprint.
- Each editor kind is either supported or REFUSED BY NAME, listed in the
  tool's own answer, so "which editors can this capture" is never a guess.
  A kind that is refused says which tool can capture it instead, if any.
- An editor that cannot be opened headless is reported with that reason rather
  than captured as the wrong tab, because a wrong picture is worse than no
  picture.
- Test: T-LOOK-8.

#### What `L-CAPTURE-ASSET` is, measured: mode `asset-editor`

`asset-editor` is a MODE of `L-CAPTURE`, not a second tool, so there is still one
PNG contract (`04_FORMATS.md` section 6) and one place the picture is written.
`asset` names the asset; **`graphName` carries the KIND** for this mode (a kind
name, case insensitive, or empty to capture whichever editor the asset resolves
to). That reuse is deliberate: `L-CAPTURE`'s signature is an `AICallable` schema
every suite and the coverage audit already read, and a sixth parameter would
change it for every caller to serve one mode. With NO asset, the mode answers the
CAPABILITY question and opens nothing.

**The render path, and why it is one path instead of eight.** Every asset editor
in this engine is an `FAssetEditorToolkit` (`Toolkits/AssetEditorToolkit.h:115-118`),
and `IToolkitHost::GetParentWidget()` (`Toolkits/IToolkitHost.h:25`) is the host's
own widget: for a standalone asset editor that is
`SStandaloneAssetEditorToolkitHost`, whose own implementation is `return AsShared()`
(`SStandaloneAssetEditorToolkitHost.cpp:359-362`). `FWidgetRenderer::DrawWidget`
then renders it off screen. So one path reaches every kind this process can open,
and the result prints which widget was rendered.

**MEASURED, one capture per kind, read back off disk**, all at `1664x849` with the
colour counts below. Every one of them is a real picture, not a flat fill:

| Kind | Asset used | Distinct colours |
|---|---|---|
| Material | `/Engine/EngineMaterials/CubeMaterial` | 3444 |
| Material Instance | `/Engine/VREditor/BasicMeshes/MI_Cube_01` | 1680 |
| Material Function | `/Engine/Functions/Engine_MaterialFunctions02/Math/Pi` | 2383 |
| Widget Blueprint (Designer) | `/Game/Tests/WBP_NodeTest` | 1278 |
| DataTable | `/Game/Tests/Kit/TA14_KitDataTable` | 519 |
| Anim Sequence | `/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle` | 1787 |
| Niagara system | `/Niagara/DefaultAssets/DefaultSystem` | 13282 |
| Blueprint | `/Game/Tests/BP_NodeTest` | 1629 |

**How a kind is decided, AND THE FALSE PREMISE THIS REPLACED.** The table's
`BUILT` / `REFUSED` answer is taken by ASKING THE ENGINE FOR the module that
publishes each kind's editor (`FModuleManager::LoadModuleWithFailureReason`), so
it measures REACHABILITY rather than history. The first version asked
`IsModuleLoaded` instead and treated `false` as "cannot be captured". Measured
against the running editor that was wrong three ways:

- it called DataTable REFUSED while the module was loaded, because the question
  was asked before that kind's own editor had ever been opened;
- it called Niagara REFUSED on the reasoning that the test project's `.uproject`
  does not list Niagara - AND THE CAPTURE SUCCEEDED ANYWAY, 13282 distinct
  colours, because the engine enables Niagara by default (`Niagara.uplugin`,
  `EnabledByDefault: true`);
- the four numbers at the end of the table MOVED BETWEEN TWO CALLS OF THE SAME
  TOOL IN THE SAME PROCESS (`4/3/2`, then `5/3/1`, then `6/3/0`) as each editor
  was opened, which is the proof that the flag was measuring something other than
  the capability it was printed as.

**AND A NAMED ALTERNATIVE IS INFORMATION, NOT A DECISION.** The first version
short-circuited on `AlternativeTool` and reported Widget Blueprint (Designer) as
"captured by another tool" WHILE `/Game/Tests/WBP_NodeTest` was capturing at
`1664x849` with 1278 distinct colours in the same process. So every kind's
reachability is now measured, and the alternative tool is only printed.

**What the table is NOT.** A module that loads is not a promise that a particular
asset's editor opens or draws anything. Only the capture attempt proves that, and
its result is what a caller reads. The table answers the narrower question it is
for - which editor kinds exist for this process at all - and the output says so.

**An empty frame is REFUSED, not written** (`D-41`, and this section above): the
render is read back, its distinct colours counted, and an image of one flat colour
is refused BY NAME with the count, because a blank PNG looks like a successful
capture of nothing.

**Measured gaps, stated rather than implied.** There is no project-owned material
instance the resolver can reach: `/Game/Tests/TurboMCPMaterialFixture` has no
object named after its package (`R-ASSET-SEARCH`), so an engine instance is used
and the substitution is printed in the kind's own reason. No Anim Blueprint
fixture exists (`TA-18` does not exist; `TA18_ColourSuite` is a colour asset), so
that kind is exercised through the CATALOGUE; it is reachable through the same
`FBlueprintEditor` path the graph modes already use
(`Engine/Classes/Animation/AnimBlueprint.h:81`).


### L-CAPTURE-LEVEL: capture a level viewport or a play session

- Input: a map, a framing mode and a size, and optionally a position to look
  at or fit to.
- Modes: `viewport` for the level editor viewport, `selected-actors` to frame
  the current selection, `actor` to frame one actor, and `pie` for the
  running game's viewport.
- `pie` requires a running play session and REFUSES without one, naming the
  two tools that start one (`R-PLAY-CHECK`, `E-PIE-SCENARIO`). It never
  starts a session itself: a capture that started a play session would change
  the editor's state, and a Look cannot (P3).
- `pie` is the frame the game is actually rendering, taken through the play
  session's own viewport, so what it shows is what the player sees and not what
  the editor draws around it.
- A framing that needs a viewport that does not exist yet (an editor tab never
  opened, a session not started) says which one is missing.
- Test: T-LOOK-9.

---

## 19. Before and after

### P-IMAGE-COMPARE: what an image pair differs by

- Input: two PNG paths produced by any capture tool, and an optional
  threshold.
- Output: the image size of each, whether they are the same size at all, the
  count and percentage of pixels that differ beyond the threshold, the
  bounding box of the differences, and a third PNG that is the difference
  image with the changed region marked.
- The two images are produced by the SAME capture call with the SAME framing
  and the SAME size, and that is a rule rather than advice: a comparison
  between two differently framed images is meaningless and the tool says so
  when the sizes disagree.
- Its purpose is the round an agent actually runs: capture, edit, capture
  again, compare, and see whether the graph, the widget or the level moved the
  way it intended. An empty difference is reported as the fact that nothing
  changed, with both paths, so "no change" is evidence and not silence.
- Test: T-LOOK-10.


---

## 20. Editor window

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
- Call log controls (section 14) and the trace point list.
- Calls the same functions as the MCP (P9).
- Tests: T-PANEL-1 to T-PANEL-6.
