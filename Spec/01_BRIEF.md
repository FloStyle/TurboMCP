# Product brief

## The problem

Unreal Engine 5.8.3 ships an MCP server and a set of editor toolsets. An AI
agent connected to it can read and edit Blueprints, widgets and materials.
In real work, agents lose most of their time on five things:

1. **Reading is slow and has holes.** The engine's tools return a single graph,
   node or expression for each call. Reroute nodes break links into pieces.
   Reading one real Blueprint takes dozens of calls and the result still
   misses things.
2. **Read and write do not match.** The text an agent reads cannot be pasted
   back to write a graph. Node type IDs, pin names and asset paths must be
   guessed, and every wrong guess is a failed call.
3. **Writes are fragile.** A failed write can leave half a graph. Writes add
   nodes but cannot replace them. Some node kinds cannot be written at all.
   Saves report errors when they succeed.
4. **Agents work blind.** There is no good way to see a graph layout or a
   widget, and no way to check what was built while it runs.
5. **Graphs made by agents are messy.** Nodes land in one long row, on top
   of each other, or at the origin. Dead copies of logic stay behind.

## What the plugin is

An editor-only Unreal Engine 5.8.3 plugin that registers its own toolsets
with the MCP server built into the engine, plus one editor window. It does not replace the
engine's tools. It fills the gaps above and makes the engine's tools easier
to use correctly.

Nothing in it ships in a game build.

## Who uses it

- **AI agents** through the engine's MCP server.
  This is the main user. Every output is written for an agent with a limited
  context window: short, exact, and with the next step obvious.
- **Developers** through the editor window: to call any tool by hand, check
  the server, debug what an agent did, and test the tools.

## Tool kinds

Each tool has exactly one kind. Its name and the first word of its
description say which.

| Kind | What it does | Changes an asset? |
|---|---|---|
| Read | Returns text about assets | No |
| Look | Returns layout facts or images | No |
| Preview | Works out a change and reports it | No |
| Edit | Makes the change | Yes. One editor undo reverses all of it |

The order of work for an agent is always Read, then Preview, then Edit.

## Main goals

1. Read a whole Blueprint, widget or material in one call, with nothing
   missing and nothing repeated.
2. One graph text format for reading and writing. What a Read prints, a
   write tool accepts.
3. Node IDs that stay valid across edits and never point at the wrong asset.
4. Every Edit is safe: one undo, compile check, undo itself when the compile shows a new error,
   and a clear report of what changed.
5. Help agents use the engine's own tools: compact tool help, known traps,
   type ID and pin lookup, path checks.
6. Let agents see: layout reports, editor captures, widget previews, and a
   short play check.
7. Clean up graphs: tidy layout that respects the author's comment boxes,
   local repair of bad spots, removal of dead copies.
8. A record of every tool call, for later analysis.

## Not goals

- Changing or patching the engine's own MCP tools or toolsets.
- Runtime or shipping features.
- A general Blueprint editor UI. The window is for calling tools and
  debugging, not for editing graphs by hand.
- Reading or writing the project's own C++ source code. **Documenting the
  build steps for this plugin is not the same thing** and is a goal:
  `09_BUILD.md` exists because the engine's MCP support is experimental and
  building against a non-vanilla engine is a supported case, not a favour.

## Beyond the first delivery

The brief above describes the plugin as first specified. A later round added
capabilities that the same agent users asked for after using it. They are
specified in full in `03_TOOLS.md` sections 9 to 19, decided in
`05_DECISIONS.md` from `D-38` on, and ordered in `08_ROADMAP.md` section 2:

- **Seeing more**: captures of every asset editor, of the level viewport and
  of a running play session; neighbourhood framing; before and after image
  comparison. The first delivery captured Blueprint graphs only.
- **Materials end to end**: a material instance's overrides edited as
  overrides and not as a graph, material functions, material parameter
  collections, and the material graph written in the same text the reader
  prints.
- **Structure**: renaming a variable, function or event with its references
  updated; extracting a selection into a function or a macro; copying a block
  between graphs; adding and removing interfaces.
- **Widgets**: writing the widget tree, its properties, slots and style, its
  animations and its bindings. The first delivery could only read a tree and
  bind one event.
- **Data and assets**: data table rows, user structs, enums edited rather than
  only created, asset creation, rename, move and duplicate with redirectors
  fixed, curve assets and curve tables, a size and reference report, and a
  text diff between two versions of an asset.
- **Levels and actors**: reading a level and an actor with its components,
  placing and configuring actors, and editing a Blueprint actor's component
  hierarchy.
- **Running**: breakpoints, a debug state read, stepping, a scripted play
  session, and the automation test report. The first two are gated on a
  measurement, not on effort (`D-55`).
- **Quality and cost**: a Blueprint lint, one undo for a whole series of
  edits, batches in one transaction, the calls that cost the most, and a
  source control checkout before a write.

The rule that governs all of them is the one already in `05_DECISIONS.md`:
**a capability that pauses, deletes or blocks is gated on a measurement, and
its refusal is written before its feature is.**

## Target

- Unreal Engine 5.8.3, Windows editor. This is the **compilation goal**
  (`D-38`): it is the engine branch actually built, and it is the only engine
  this project has ever been built or measured on.
- The engine's `ToolsetRegistry` and `ModelContextProtocol` plugins
  (experimental, ship with the engine).
- Building against an engine that is not vanilla 5.8.3 is supported and
  documented in `09_BUILD.md`, because those two plugins are experimental
  and an engine that renamed one of their modules would fail this plugin's
  build in a way that reads like a bug in the plugin.
