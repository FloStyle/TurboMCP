# Decisions

Each decision has what was chosen, what was rejected, and why. The reasons
come from real use. Do not undo a decision without asking through
`QUESTIONS.md`.

---

## Tools and results

**D-01 Four kinds of tool.** Read, Look, Preview, Edit. The kind is in the
name, the description and the panel label. Rejected: a read/write split
only. Why: agents need to know, before calling, whether a call can change
anything. "Preview" as its own kind lets an agent try a change for free.

**D-02 Preview and Edit share one code path.** Same parse, same
resolution, same plan; the Edit adds the write step. Rejected: separate
implementations. Why: two code paths drift, and then the Preview lies.

**D-03 Refuse ambiguity.** A name that fits two assets, a node type that
fits two overloads, a pin name that fits two pins: refuse and list.
Rejected: pick the best match. Why: the best match was wrong often enough
to lose real work, and the agent never knew.

**D-04 Lists, not regular expressions, for filters.** Agents wrote
`A|B` and `A,B` and both must work. Rejected: regex. Why: a filter that
looked like a regex but was matched as plain text returned nothing, and
nothing said why.

**D-05 Only print what was checked.** For a widget without the variable
flag, print only that fact, not "unreachable". Why: the strong claim was
wrong when a bound event used the widget, and the agent treated the reader
as broken.

**D-06 Images go to disk.** Return the path. Rejected: inline by default.
Why: one inline capture filled an agent's context.

**D-07 Help in a data file.** Known traps for engine tools are data,
shipped with the plugin, shown with the tool's compact help. Rejected:
traps in code or only in docs. Why: they change with every engine update
and every agent report, and must update without a rebuild.

**D-08 The panel shows engine tools too, and calls the same code as the
MCP.** Why: developers debug what agents see, and engine tools are half of
what agents use.

**D-09 The asset picker copies the editor's Open Asset dialog.** List
view, filter chips, remembered settings. Rejected: a column view picker
with a funnel only. Why: in a small popup, column view cut every name, and
filters reset each time.

## Node IDs

**D-10 Full asset name plus GUID key.** See `04_FORMATS.md` section 3.
Rejected, both tried before:
- Asset initials as the prefix (for example `WSC` for `WBP_ShopCard`). Two
  assets with the same initials sent agents to the wrong asset, more than
  once. The tokens saved were not worth it.
- A node number from sort order. Adding or removing a node changed the
  number of every later node, so an ID read before an edit pointed at a
  different node after it.

## Reading

**D-11 Plain reroutes are invisible; named reroutes are labels.** A plain
reroute is tidying with no meaning, so links go through it. A named
reroute carries a value across the graph; printing its readers as leaves
broke the graph into islands. So a label gets one definition and every use
points to it.

**D-12 Knots shown only when editing.** At `full` detail a link through
knots names them. Why: an agent tried to break a link at its two ends and
failed, because the real link ended at a knot.

**D-13 One name per value.** In material output, a value that has a label
is referred to only by that label. Why: printing both the label and a node
ID for one value made agents think there were two values.

**D-14 Constants on unconnected inputs are printed.** Many material
expressions keep a constant for each input that is not linked. Read them by
reflection. Why: a lerp printed with only its alpha hid half its meaning
and blocked any write back.

**D-15 One graph text for read and write.** The writer accepts exactly what
the readers print. Rejected: a reader format tuned for reading plus a
separate write format. Why: this mismatch was the top complaint in every
agent report. Agents read with one format and had to rewrite everything to
write with another. Until the plugin's writer exists, a `write` output
mode bridges to the engine's own writer (D-16).

**D-16 Engine writer bridge.** A Read output mode in the engine writer's
dialect. Constructs it cannot write become comments naming the tool to use.
Why: quick relief while the full writer is built.

**D-17 Material blocks.** The target shape for materials. Confirm it with
the owner before any Edit tool applies it.
- Each comment box holds one piece of logic (a block).
- When a value leaves its block for another block, it travels as a named
  reroute. The producing block declares it; the consuming block reads it.
- Within a block, use direct wires. Reading a label inside the block that
  declares it adds nothing.
- A block reads each label through a single usage node.
- Plain reroute dots are for routing long wires, and never stand for a
  value.
- Label names describe the value. Box titles describe what the block does.

## Layout

**D-18 Never move the author's comment boxes.** Lay out inside each box
and resize the box. Rejected: pack boxes by size. Why: on a hand-laid
function of about 90 nodes and 8 boxes, packing the boxes made total wire
length 2.5 times longer and created six backward exec wires. The author's
box placement says which box feeds which; a packer only knows sizes.

**D-19 One getter per consumer.** Split shared variable getters so each
consumer has its own, next to it. Merging is an option, off by default.
Rejected: merge duplicates by default. Why: merging sent a long wire from
one getter to every consumer across the graph and ruined a readable graph.
Copying a getter costs nothing at runtime and cannot change behaviour.

**D-20 Columns by longest path, pure nodes pulled right.** Over exec and
data links together. Rejected: columns by exec distance only. Why: every
getter landed in the first column and the result was tall and narrow; a
hand-laid graph runs getter, math, math, consumer across columns and reads
wide and short.

**D-21 Measure, roll back, allow force.** Total wire length before and
after; roll back if worse; a `Force` input applies anyway and prints the
numbers first. Why: a tool that silently degrades a graph does more harm than
having no tool. Still, wire length only guides: an agent-built single row
of nodes may become far easier to read yet show a larger total, so an agent
needs a way past the check.

**D-22 Disconnected work stays where it is.** Why: moving it into a tidy
layout makes it look connected.

**D-23 Backward means past the right edge.** Compare the target node's
left edge with the source node's right edge, not their X positions. Rejected: target X
smaller than source X. Why: a node stacked under its predecessor passed the
old check, and the report said "no backward wires" on a graph that had them.

**D-24 Local repair for mostly good graphs.** A pass that fixes only the
bad spots. Why: on a mostly good graph a full layout either changes too
much or rolls back, and the bad spot stays.

**D-25 Knots only where they help.** A knot must make a wire straighter or
stop it crossing a node. Why: knots placed by length alone made graphs
worse.

**D-26 Dead copies.** Agent-built graphs often keep an unwired copy of
logic that also exists in the live chain. Removal only for groups that
reach no entry point, write nothing anyone reads, and hold no event or
entry. Why: other code may call events.

**D-27 Local variables only when safe.** A pure node is evaluated again
every time something reads it, while a local variable stores the value one
time. Promote only when everything feeding
the value is a function input or a literal.

## Writing

**D-28 Write pipeline and build order.** Parse, resolve, check (no
writes), apply, verify. Build Append first (cannot destroy work), then
Replace (Append plus a clear), then Patch (needs a proven resolver).

## Engine target and portability

**D-38 The compilation goal is 5.8.3.** The brief's target line said 5.8.1
and the engine actually built is branch `release`, which is 5.8.3. The goal
is now stated once, as 5.8.3. Rejected: keeping 5.8.1 as the target while
building 5.8.3. Why: a spec whose stated target is not the binary anyone
compiles is a spec whose engine facts are checked against the wrong source,
and `07_ENGINE_NOTES.md` was learned on 5.8.1 (Q-1). Every engine fact in
that file is re-checked against the compiled version before it is relied on
again, and the file says which version each fact was checked on.

**D-39 Building against an engine that is not vanilla 5.8.3 is documented,
not assumed.** The module list, the two engine plugins the plugin depends on
(`ToolsetRegistry`, `ModelContextProtocol`), the fact that a module
dependency needs an entry in `Build.cs` AND in the `.uplugin`, and the
editor-only build requirement are all written down in `09_BUILD.md` with the
evidence for each. Rejected: "it builds here, so it builds anywhere". Why:
the engine's MCP support is experimental and moves; an engine that renamed
one of its modules would fail this plugin's build in a way that reads like a
bug in the plugin.

**D-40 Nothing in the spec may depend on an engine API that only exists in a
version later than the stated target.** When a capability needs an API that
is newer, the capability says so by name and refuses, rather than being
written against it and failing to compile. Why: this plugin is meant to be
reusable across projects, and a hard dependency on one engine build is
exactly what "reusable" forbids.

**D-29 Round trip is the gate.** After a write, read back and compare.
Read a function and write it back unchanged: it must be identical. Why: the
graph text hides defaults, `self` and knots on purpose, so editing the
text and writing it back may silently alter parts no one meant to touch.

**D-30 Keyword arguments only.** Why: a positional value in the engine's
writer landed on the `self` pin.

**D-31 Write into existing entries.** Events, overrides and result nodes
that exist are reused. Why: the engine's writer only creates, so it failed
on any event that existed, and it cannot make result nodes at all.

**D-32 One transaction, compile, roll back.** Why: the engine's writer left
broken nodes in place after a compile error.

## Saving and compiling

**D-33 Report from the unsaved flag.** Save the assets one by one and check
the flag after. Why: an engine save returned an error when it worked, and
another saved two of three assets and said nothing about the third.

**D-34 Refuse to save during play or simulate.** Why: saves made then
failed or half worked.

## Seeing and testing

**D-35 Widget preview off screen.** Render a temporary instance with
sample values. No play session. Why: it is fast and safe, and agents doing
UI work otherwise needed a human for every screenshot.

**D-36 Trace points before breakpoints.** Why: a breakpoint stops the
editor in a nested loop. If the MCP server does not answer there, the agent
waits until a person resumes the game. Trace points never pause.

**D-37 The call log writes at call start.** Why: the editor crashed during
an engine tool call once, and the last call is the one that matters.

---

## Added after the first delivery

These decisions extend the numbered series. The numbers continue; nothing
above is renumbered, so every existing reference still resolves.

### Vision and looking

**D-41 A capture never falls back to a wider frame.** A framing that cannot
be done is an error naming why. Rejected: fall back to the whole graph or the
whole screen. Why: an agent that asked for one node and got the whole graph
cannot tell the difference from the result text alone, and will reason about
a picture it believes is focused. A wrong picture is worse than no picture.

**D-42 Hop counting follows the graph the reader prints.** `neighbours` counts
a hop through a plain reroute knot as one hop, because the reader hides knots
on purpose (`04_FORMATS.md` 2.1 rule 7). Rejected: counting over the engine's
node list. Why: a capture whose neighbourhood came from a different graph than
the one the agent read frames nodes the agent never saw named.

### Materials

**D-43 A material instance is not written through the graph writer.** It is a
list of overrides, so it has its own read and write tools
(`R-MAT-INST-LOAD`, `P/E-MAT-INST-SET`). Rejected: printing an instance as its
parent's graph and writing overrides into that text. Why: an instance's real
behaviour lives in the overrides, and a text that hides them invites an edit
that changes the wrong asset.

**D-44 A collection parameter removal is a refusal by default.** Removing one
shifts the index of every later parameter, and readers reference parameters by
index. So the removal names the loaded readers and is refused unless `force`
names the count. Why: this is the one edit in a parameter collection whose
damage appears in a different asset and much later.

### Writing

**D-45 Every writer states what it can destroy, and deletes by identity.** A
writer that removes anything lists each removal by key with its class and
title, never a count, and it re-resolves every node by key at apply time
rather than trusting a plan from an earlier call. Why: a plan is made and
applied in two calls, and the graph can change between them; a count says how
much is gone and a list says what.

**D-46 A rename refuses rather than renames partially.** `E-GRAPH-RENAME`
refuses when a name is taken, and reports referenced members it could not
update by name and rolls back. Rejected: rename what can be renamed and
report the rest. Why: a rename that leaves one reference behind does not fail
at the rename; it fails at a later compile, in a different asset, for whoever
compiles next.

**D-47 Extraction and copying compute their boundary rather than being told
it.** Collapse and copy derive the inputs and outputs of the extracted block
from the links that cross the selection edge. Rejected: taking the boundary
from the caller. Why: the caller cannot see the crossing links without a read
that this tool can do exactly, and a wrong boundary produces a member with the
wrong signature that still compiles.

### Widgets

**D-48 A widget tree needs its own text dialect.** The widget tree text of
`04_FORMATS.md` section 8 is a second format, sharing the shape rules of the
graph text and nothing else. Rejected: expressing a widget tree in the graph
text. Why: a widget tree's child ORDER is its draw order and its layout lives
on the slot, neither of which the graph text can express. A dialect that
cannot express order would silently reorder children on a round trip.

**D-49 A widget rename happens in exactly one tool.** `P/E-WIDGET-SET` changes
a widget's name and updates every binding and call node that names it.
`P/E-WIDGET-TREE` REFUSES a tree text that renames a widget. Rejected: letting
the tree writer rename. Why: a name change breaks every reference to it, and a
tree writer has no view of the graph side where those references live.

**D-50 Slot layout and widget properties are never confused.** An assignment
names which of the three layers it belongs to: the widget's own property, its
slot's layout, or its style. Why: `Padding`, `Anchors` and `Offsets` live on
the slot type and not on the widget (`07_ENGINE_NOTES.md` section 3.2), and a
tool that tries the widget first reports a property that does not exist for a
property the user can see in the details panel.

### Assets and data

**D-51 A struct editor tool is editor-only, and says so.** Creating and
editing a user struct needs a `UUserDefinedStructEditorData` that only the
editor factory installs; from a commandlet it crashes (Q-15, TA-10). So
`P/E-STRUCT-*` are editor-only tools and their tests run against a live
editor. Rejected: finding a commandlet-safe path first. Why: the crash was
measured, twice, and a tool that works in the editor and crashes headless is
better than no tool as long as the limit is written down.

**D-51 CORRECTION, MEASURED WHEN ROW 7.1 WAS BUILT - THE DECISION STANDS AND ITS
REASON DOES NOT.** The crash is real and a commandlet still cannot make a user
struct. But "only the editor factory installs it" is FALSE, and it is the reason
this decision gives. `FStructureEditorUtils::CreateUserDefinedStruct()`
(`StructureEditorUtils.cpp:42-64`) is the PUBLIC STATIC that installs the
`UUserDefinedStructEditorData` itself, at its **lines 49-50**, and then calls
`AddVariable` at its own **line 59**; the commandlet's crash came from using
`NewObject<UUserDefinedStruct>` directly and stopping one call short of it.
Nothing on the path - `CreateUserDefinedStruct`, `AddVariable`,
`RemoveVariable`, `RenameVariable`, `ChangeVariableType`, `MoveVariable`,
`CompileStructure` - reads `GEditor` or an open asset editor. So the limit is
**LIVE-EDITOR-ONLY, not "needs an open struct editor"**, and the tools build and
pass against the same live editor every other suite in `Tools/` uses. Rejected:
leaving the reason as it was. Why: a decision whose reason is wrong gets cited
for the next thing it does not cover, and this one was already one row away from
refusing a capability that works.

**D-52 A size is reported with its provenance.** `R-ASSET-COST` splits its
answer into the asset's own size, what it pulls in, and whether each reference
is hard, soft or editor-only. Rejected: one total size. Why: the total names no
cause and cannot be acted on. The check that matters is "a heavy hard
reference where a soft one would do", and that fact does not exist inside a
total.

**D-53 A diff names what it did not compare.** `P-ASSET-DIFF` lists every
field or section it could not compare, with the reason. Rejected: omitting an
incomparable section. Why: a diff that omits a section reads as "nothing
changed there", which is the one sentence a diff must never say wrongly.

### Running and debugging

**D-54 A tool call must never block the thread the game runs on.** A tool call
runs on the editor's game thread and PIE is ticked on that same thread, so a
capability that must wait for the game gets a request half and a collect half
instead of one call. This is already how `R-PLAY-CHECK` works; the same split
is required for a scripted PIE scenario and for an automation test run if the
same blocking applies. Rejected: one call that waits, with a long timeout.
Why: the failure mode is not a slow answer, it is an answer that never comes.

**D-55 No pausing tool exists until the server is PROVEN to answer while
paused.** A Blueprint breakpoint stops the editor in a nested loop, and if the
MCP server does not answer there then an agent that armed one hangs until a
human resumes the game. Trace points were built first for that reason
(`D-36`). Rejected: building breakpoints and documenting the risk. Why: a
capability whose failure mode is "the agent stops forever" has to be gated on
a measurement, not on a warning. If the measurement says the server does not
answer, the breakpoint tools are NOT BUILT and the refusal goes in the
description of the tools an agent would reach for instead. If it answers but
only slowly, the tools are split like `R-PLAY-CHECK` and the split is stated
in their descriptions.

### Sessions and source control

**D-56 A work session is bounded, and it says when it ended itself.** One
`E-SESSION begin` makes a whole series of edits one undo. An unbounded one is
refused: a session older than its limit, or holding more edits than its limit,
ends itself and says so. Why: an agent that opens a session and then fails
leaves the editor holding one open transaction, and every later edit joins a
transaction nobody will ever end.

**D-57 Nothing checks an asset out automatically until the checkout tool is
proven.** `E-VCS-CHECKOUT` is built and tested before any write tool calls it.
Why: the engine's own checkout path already failed once, headless, and made
`E-SAVE` report a failure while nothing had been written. An automatic
checkout wired into every write would turn a source control problem into a
"every tool reports failure" problem, and the cause would be invisible from
any of those results.

**D-58 A scripted play session uses the engine's input path.** A PIE scenario
asserts a key or a click through the engine's own input handling. Rejected:
calling the handler the input would have called. Why: the capability exists to
check the input path, and a call straight into the handler proves nothing
about the part that is usually wrong.

### Build documents and who writes the spec

**D-59 The specification files are written by the build side, and the reading
order is normative.** `00_START_HERE.md` section 3 says an answer to a question
arrives as a new spec version, ***REMOVED*** .. In practice
the build side writes the new version into `Spec/`, and the owner reads it.
Rejected: waiting for the owner to author each capability. Why: the build side
is the only party that has measured the engine, and the alternative was that
capabilities were built with no specification at all.

**D-60 A plan runs in `Spec/WORKING_PLAN.md`, not in this file.** A multi-round
plan, the state of each capability and the next capability to pick up live in
`Spec/WORKING_PLAN.md`, which is a working document and is expected to change
every round. This file holds only what is DECIDED, and a decision changes only
through a question. Why: a working plan that lives in the decisions file turns
every progress edit into a decision edit, and then the decisions stop meaning
"do not undo this without asking".


