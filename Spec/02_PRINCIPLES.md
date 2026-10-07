# Principles

These rules apply to every tool. A tool that breaks one is not done. Each
rule has the reason it exists. Most reasons are real failures seen when
agents used MCP tools on real projects.

## P1. Never return nothing

An empty result says why it is empty and what the agent can do next.

- A filter that matches nothing says so and lists what exists.
- "No problems found" is printed only after the check really ran. If a
  lookup inside the check failed, say that instead.
- A section with zero items is still printed with its count of zero, when
  the agent would otherwise not know the section was checked.

Why: a layout check once reported a clean graph while its own data
lookup had failed. A graph filter once returned only a header, and the
agent could not tell if the graph was empty or the filter was wrong.

## P2. Refuse rather than guess

When an input fits more than one thing, or fits nothing, refuse. Name what
was refused, why, and the exact inputs that would work.

Why: a short name that fitted two assets was resolved silently to the
wrong one. Two agents lost their work to it.

## P3. Read, Look and Preview never change anything

Not the asset, not its dirty flag, not the graph's node count, not the
editor selection that the user can see. A Preview may build things in a
transient package and must destroy them before it returns.

Why: an engine tool that reads the pins of a node type does it by adding a
real node to the graph and does not remove it. Agents removed these probe
nodes by hand.

## P4. Every Edit is one safe unit

- One editor transaction. One undo reverses the whole call.
- Compile after the change when the asset kind can be compiled. If the
  compile has a NEW error or warning that was not there before, roll back
  and report it.
- The result opens with either `unchanged` and the reason, or a summary
  of what changed.
- An Edit that improves something measurable (layout, wire length) measures
  before and after, and undoes the change when the measure got worse,
  unless the agent asked to force it. The numbers are always printed.
- Each Edit tool is paired with a Preview tool whose name uses the same
  target words, unless the spec says it has none. The Preview and the Edit run the same code up to
  the point of writing, so they cannot disagree.

Why: an engine write tool wrote first and compiled after. A compile error
left broken nodes in the graph, and the agent had to find and remove them.

## P5. Any asset path form works

Every tool that takes an asset accepts: a short name, a package path, an
object path, and a generated class path. All resolve to the same asset. An
asset made in this session but never saved is found. An
asset that is not loaded yet is found. A short name that fits more than one
asset is refused with the full list (P2).

Why: the engine's tools want different path forms, and some report "does
not exist" for an asset that is not loaded yet.

## P6. Outputs are written for a context window

- Every output has a token estimate. Large outputs take a budget and get
  smaller in steps (see `04_FORMATS.md`), never cut in the middle.
- No engine log text in a result. Count warnings and name their source
  instead.
- Images go to disk. A tool returns the file path, not the image data,
  unless the agent asks for inline data.
- No full dumps. Every list has a cap and says how many were left out and
  how to get them.

Why: engine tool help runs to 60k to 70k characters. A type search on a
common word returned 40k characters. A save returned hundreds of identical
log lines and hid the one that mattered. An inline image filled an agent's
whole context.

## P7. Results lead to the next step

When a result names a thing, it names it in the exact form another tool
accepts: full asset path, node ID, pin name, node type ID. An error names
the closest valid inputs.

Why: agents paid one failed call for every guessed name.

## P8. Never leave the user worse off

- A layout tool never moves a comment box the user drew.
- Work that is not connected to anything is never moved into a layout,
  because that would make it look connected.
- Nothing that an agent sets for debugging (trace points, breakpoints) is
  left without a way for a human to see it and clear it.
- No tool deletes anything that could be triggered from other graphs or
  objects (events, custom events, functions) unless the agent named it.

## P9. One code path

The editor window runs exactly the code path the MCP runs. What a developer
sees in the window is what the agent gets.

## P10. Describe the tool for the agent

The tool description is the agent's only manual. It says the kind first,
then what the tool does, what it does not do, what it costs, and which tool
to call next. Parameter descriptions say the accepted forms and the default.
A Preview tool without an Edit twin says so, and the reverse.
