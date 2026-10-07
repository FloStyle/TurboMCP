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
