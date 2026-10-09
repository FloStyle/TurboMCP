# Questions

Questions for the owner about anything the specification leaves open, where a
guess would change how a tool behaves.

Format: a number, the date, the spec file and section, the question, and the
answer when it arrives. Answers come back as a new spec version, ***REMOVED***
. (`Spec/00_START_HERE.md` section 3).

---

## Q-1 2026-10-07 Spec/01_BRIEF.md target

The brief targets Unreal Engine 5.8.1. The owner chose branch `release`, which is
5.8.3, for the engine build. Every engine fact in `Spec/07_ENGINE_NOTES.md` was
checked against 5.8.1.

Question: should `Spec/07_ENGINE_NOTES.md` be re-verified against 5.8.3, and
should the brief's target line be updated to 5.8.3?

Answer: not yet answered.

---

## Q-2 2026-10-07 Spec/03_TOOLS.md section 1.3

Section 1.3 asks for one pair of action words used for every Preview/Edit pair,
and gives "Suggest X" and "Apply X" as an example.

Question: is "Suggest" and "Apply" the pair to use, or does the owner want
another pair?

Answer: not yet answered.

---

## Q-3 2026-10-09 Spec/03_TOOLS.md R-NODE-RESOLVE

`R-NODE-RESOLVE` must print "the node type ID (the one the engine's node creation
tool accepts)". That ID is `<menu category>|<node title>` with whitespace
removed, and **the title half follows the editor's language**. On this French
editor the engine's own `find_node_types` offers `Affichage|Événement|...` and
`Utilitaires|Casting|...`, so the localised form is genuinely what the engine's
writer accepts here.

This plugin now prints the un-localised category (`AddEvent` rather than
`Ajouterunévénement`) but cannot un-localise the title, because Epic builds a node
title at run time from separate localised fragments. It also prints the node's
C++ class (`K2Node_Event`) beside it as the one locale-independent name.

Question: is that acceptable, or does the specification want something stronger —
for example a type ID that is stable across languages even when the engine's own
writer would not accept it? A stable ID would have to come from somewhere other
than the title, and would then need translating before it could be handed to the
engine's writer.

Answer: not yet answered.

---

## Q-4 2026-10-09 Spec/03_TOOLS.md E-SAVE

`E-SAVE` is classified as an **Edit** tool, but it does not make a change: it
writes to disk changes that were already made. Its own spec even has it report
"had no changes" as a normal outcome.

This plugin follows the spec and puts it in `UTurboMCPEditToolset`, and it opens
its result with `unchanged:` when nothing was saved and `changed: saved N` when
something was, which satisfies `P4`.

Question: is `Edit` the intended class for a save, or should `E-SAVE` sit outside
the four kinds? The four kinds are described as Read, Look, Preview and Edit, and
"an Edit is one transaction" (`P4`) does not obviously describe a save.

Answer: not yet answered.

---

## Q-5 2026-10-09 Spec/03_TOOLS.md R-COMMENT-LIST, nesting and overlap

`R-COMMENT-LIST` must report a box's "parent box if nested", and a node can fall
inside two overlapping boxes. Both are computed from the rectangles, because the
engine stores neither.

**Neither path has been exercised against real content.** Every engine asset
reachable from this project reports `0 nested` and `0 node(s) inside more than one
box`. The arithmetic is checked against a model in `Tools/test-comments.py`, which
proves the geometry and does not prove the rendering. `TestRecord.md` section 2k
records this as unverified.

Question: is there a piece of content the owner knows of that has nested or
overlapping comment boxes, so the two paths can be verified against something real
rather than a model? Failing that, is a hand-made nested box acceptable as part of
the test kit, given `06_ACCEPTANCE.md` requires the kit's structure to be built by
hand anyway?

Answer: not yet answered.

---

## Q-6 2026-10-09 Spec/03_TOOLS.md R-DATATABLE, project-owned tables

`R-DATATABLE` is verified against an engine DataTable with a known struct, fields and
rows. It is **not** verified against a table whose row struct is a project asset,
because no such table could be built and four attempts failed on engine rules rather
than on anything in this plugin:

- a `UserDefinedEnum` cannot be a row struct, because it is not a `UScriptStruct`;
- `BlueprintEditorLibrary.add_member_variable` only accepts a **Blueprint**, so a
  `UserDefinedStruct` cannot be given fields that way;
- `DataTable::RowStruct` is **read-only** once the asset exists;
- a CSV import through `AssetImportTask` refuses with "A Data table row type must be
  specified in the import settings json file for automated import".

Question: should a project-owned table be built, and if so how? Two routes are known
and both cost more than the check is worth right now: an import-settings JSON beside
the CSV, or a small C++ commandlet inside the plugin that creates the struct with
`FStructureEditorUtils` and the table with `NewObject`.

The second may be worth preferring, because a commandlet in the plugin is also the way
to build the hand-made acceptance kit that `06_ACCEPTANCE.md` requires. If so, that
kit and this check share one piece of machinery rather than two.

Answer: not yet answered.

---

## Q-7. Do the node widgets really not draw into an off-screen render?

Status: **open, and it limits what `L-CAPTURE` can be claimed to do.**

`L-CAPTURE` moves the graph view and renders the live `SGraphEditor` widget into a render
target. What comes out is the **real graph editor chrome** — toolbar, back and forward
arrows, the breadcrumb `BP_NodeTest > EventGraph`, the `BLUEPRINT` watermark — and **no
nodes**.

So the widget really is the graph editor, the render really ran, and the image size is
exactly what was asked for. The nodes are simply not in it.

What is already known, so nobody repeats it:

- The view move **is** applied. `GetViewLocation` read back after `SetViewLocation` gives
  `applied 0.750 at x 980 y 208` against `asked 0.729`. The engine accepted the centre and
  clamped the zoom to its own limit, which is expected and fine.
- `FSlateApplication::ForceRedrawWindow` on the widget's window, to make the node panel
  recompute which nodes are on screen, **did not change the outcome**.
- `FWidgetRenderer::DrawWidget` renders the composed widget as it is. A node panel culls
  nodes outside its geometry, so an empty canvas is what a panel with no laid-out geometry
  would produce — but the chrome around it renders at the right size, so the outer widget
  does have geometry.

Untried routes, cheapest first:

1. `FWidgetRenderer::DrawWidget` with `bDeferRenderTargetUpdate` and an explicit geometry
   pass, or feeding `SNodePanel` its own `Tick` and `Paint` with a synthetic `FGeometry` of
   the requested size, so the panel believes it has the room the render target gives it.
2. Render at the panel's **actual** on-screen size rather than the requested size, which
   removes the mismatch between what the panel thinks it has and what is being drawn, then
   scale the image.
3. Give up on off-screen rendering for this tool and call the engine's own
   `USlateInspectorToolset::Screenshot`, which captures the on-screen window. That works by
   construction but loses the framing: the image is whatever is visible, so the frame has
   to be achieved by moving the view and hoping the window is the right shape.

Question: which of these actually makes the nodes appear, and is the answer the same for
the widget Designer and for materials?

Answer: **partly answered.** A follow-up session found and fixed a real root cause and
ruled out a route. What is now known:

FOUND AND FIXED. A newly opened tab has no geometry until Slate has ticked MORE THAN ONCE.
One tick was not enough; up to eight are now pumped, stopping as soon as the panel reports
a size. The panel went from `0x0` to **988x742**, the tab went from greyed out to active,
and the frame stopped being clipped. Also fixed: Slate geometry is in slate units and the
picture is in pixels, so the panel origin is divided by the widget scale. On this machine
the scale is 1.00, so it changed nothing here, but it is a real bug elsewhere.

RULED OUT. `FWidgetRenderer::DrawWidget` on the graph editor alone is the wrong route: it
does `Window->SetContent(Widget)` and restores the original parent afterwards, so the panel
paints against a hierarchy it did not grow up in. `DrawWindow` on the real window is the
right shape and does paint through the engine's own `SWindow::Paint`.

STILL OPEN. With the geometry correct, the computed crop (`x 17 to 1488, y 344 to 657` of a
1670x911 window) lands on the LEFT SIDEBAR — the `My Blueprint` panel — not the graph
canvas. So the remaining defect is in mapping the frame to window pixels, not in the view
move and not in the panel geometry.

NEXT, IN ORDER:
1. Check whether the render target's content origin matches the window's screen origin at
   all. `FWidgetRenderer::DrawWindow` derives its geometry from `DrawSize` and `Scale`, and
   the render thread update it schedules uses `FApp::GetDeltaTime()` rather than the
   `DeltaTime` argument the caller passes. Verify where widget (0,0) actually lands in the
   picture by rendering the whole window scaled to the requested size and measuring a known
   widget's position in the output.
2. If the offset is not a constant, crop by SCANNING rather than by arithmetic: render the
   whole window, and cut the graph canvas out by finding the panel's own background colour
   instead of computing its position.
3. Only then decide whether the engine-screenshot fallback is still needed. It works today
   — it returns a real 1670x911 PNG of the editor window — and the result labels it in
   words so it can never be mistaken for the frame that was asked for.

THE TEST LESSON, which outlives the bug. Every check in `test-capture.py` passed while the
canvas was empty: existence, size, PNG header and file name are all true of a picture of
nothing. The suite now counts distinct colours and requires at least eight, which a flat
fill cannot reach. A check that cannot fail the way reality fails proves nothing.

---

## Q-7b. The graph panel uses a different vertical origin than the window

Status: **open, and the defect is now located rather than guessed.**

Route 1 from Q-7 was taken: a `debug-window` mode renders the whole editor window with NO
cropping, at the requested size, so the computed frame's position can be MEASURED instead
of assumed. That mode is kept in the tool, because it is the only way to answer this
question and re-deriving it costs more than one mode costs.

**The render pipeline is proven to work.** The uncropped 1600x900 picture shows the real
graph canvas with the real nodes in it: `Print String`, `In String` and `Development Only`
are all visible, clustered in the upper right of the canvas, which is where a view centred
on x 980 y 208 at zoom 0.75 puts them.

**The numbers that do not agree.** The frame the tool computes is `x 17 to 1488,
y 344 to 657` in a 1670x911 window. Reading the node positions back out of the uncropped
picture gives a different answer:

| Quantity | Value |
|---|---|
| Graph nodes visible at | image x 950-1100, y 165-230 |
| Canvas in the window | x 259 to 1247, y 130 to 872 |
| `GetViewLocation` reads back | x 980, **y 208**, zoom 0.750 |
| Back-solving the node's y from the picture | view centre **y ≈ -181** |

The vertical difference between the applied view centre and the one the picture was drawn
with is about **390 pixels**, which is a little over half the panel height (742). A
half-panel offset is the signature of the panel measuring itself from its own top-left
while the transform assumes the window's, so the next thing to check is what
`SGraphEditor::GetTickSpaceGeometry()` and `SNodePanel`'s own view transform each use as
their origin — not what the frame arithmetic does with them.

What is already ruled out, so it is not retried: the view move itself (read back and
applied), the panel geometry (`988x742`, correct and non-zero), DPI scaling (1.00 here, and
the origin is divided by it anyway), and `SWindow::GetPositionInScreen` versus
`GetWindowGeometryInScreen` (same origin, so not the cause).

The horizontal placement is right — nodes fall inside the crop's x range — so this is
specifically a vertical origin mismatch, not a general mapping error.

Three further hypotheses were tested and are also ruled out:

- `SWindow::GetSizeInScreen()` differing from the window's display size. They are the same
  property; the check was a no-op and was removed rather than left in.
- `FWidgetRenderer::DrawWindow` producing a render target whose size differs from the size
  asked for. It does not: the target is created at `WindowWidth x WindowHeight` and the read
  back asserts `Pixels.Num() == WindowWidth * WindowHeight`, which passes.
- `FGeometry::GetAbsolutePosition()` returning layout space rather than render space. It uses
  `AccumulatedRenderTransform`, so with a layout scale of 1.00 it returns screen pixels,
  which is what the mapping assumes.

**This has now resisted three successive hypotheses, so it is being left open rather than
chased further.** The roadmap has nine phases and this is one tool's crop rectangle; every
hour on it is an hour phases 4 to 9 do not have. `debug-window` captures the window
correctly today, and `L-CAPTURE`'s framing modes are recorded as NOT working rather than
half-working.

The next attempt should start from the route below rather than from arithmetic, because
arithmetic has been wrong three times.

RECOMMENDED NEXT ROUTE — measure, do not derive:

1. Put a known node at a known graph position, capture with `debug-window`, and locate that
   node in the output by its title text. One measurement gives the real transform and
   replaces every assumption above.
2. If the offset is constant, subtract it and be done.
3. If it is not constant, do NOT derive it. Crop by SCANNING the picture for the canvas,
   which needs no window geometry at all and therefore cannot be wrong about it. The canvas
   is the largest region bounded by the editor's panel gutters, and the frame's position
   within it follows from the view location and zoom alone, which are known correct.

Route 3 is preferred even if route 2 works, because it removes the dependency on window
geometry entirely.

NOT VERIFIED: whether the offset is constant or depends on the panel's size, the window's
position, or the zoom.

Answer: not yet answered. `L-CAPTURE`'s `debug-window` mode captures the window correctly
today, and the framing modes still need this fixed before they can be claimed to work.

---

## Q-8. Can the UHT freshness check be made content-based instead of timestamp-based?

Status: **open. A content-based version was written, could not be made to fail, and was
withdrawn.**

`Tools/check-plugin.py` compares the mtime of each reflection header against
`TurboMCPToolsets.generated.h`. Editing any reflection header makes it newer, so the check
reports `UHT must re-run` — but UHT **does** run, and when the reflected declarations are
unchanged its output is **byte-identical**, so the generated file keeps its old mtime and
the check reports staleness that is not there.

This produced a false positive this session. The check said the reflection was stale while
`LookTurboMCPCapture` was loading and answering MCP calls, and the generated header was
confirmed to contain the declaration:

```
LookTurboMCPCapture dans le generated.h ? True
LookTurboMCPWidgetPreview dans le generated.h ? True
```

A content-based replacement was written: for each reflection header, require every
reflected name it declares to appear in its `.generated.h`. On the real tree it behaves
correctly and reports `generated code carries every declaration from 1 reflection header`.
The name extraction was verified separately against the real header and finds all 27
declarations, with 0 missing.

**It was withdrawn because it could not be made to fail.** Breaking the generated header on
purpose — replacing `LookTurboMCPCapture` with `XXLookTurboMCPCapture` — left the check
green, three times, including after the first attempt was found to have written through
the `Marketplace` junction instead of the real path. A check whose failure cannot be
demonstrated is worth less than a check whose false positive is written down, so the
original was restored with the false positive documented in place and here.

Question: what is wrong with the break test, and once it genuinely fails on a broken
generated header, should the content-based version replace the timestamp one?

Two candidate explanations for the break test, neither confirmed:

- `check_uht_freshness` may not be the function the script actually runs, though
  `def check_uht_freshness` appears exactly once.
- The generated header the check reads and the one the break test writes may not be the
  same file, despite both naming
  `<REPO>\Intermediate\Build\Win64\UnrealEditor\Inc\TurboMCP\UHT\TurboMCPToolsets.generated.h`.

Answer: not yet answered. The timestamp check stands, and its failure message now tells the
reader to confirm before acting.

---

## Q-9. Is the `full` level's knot handling exercised by any test?

Status: **open. The branch exists and no test in this repository reaches it.**

`Spec/03_TOOLS.md` says `full` adds "the editing details: node type IDs, knots on links, pin
indexes". Three of those are now verified against a real Blueprint:

| Detail | Verified | Evidence |
|---|---|---|
| node type IDs, long form | **yes** | `full` carries `/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:` while `bodies` carries the short form |
| each node's C++ class | **yes** | 8 occurrences of `; class ` in `full`, 0 in `bodies` |
| linked outputs | **yes** | 7 occurrences of `; out ` in `full`, 0 in `bodies` |
| **a knot named on a link** | **NO** | `BP_NodeTest` has no knot node, so the branch that prints `(-> ... ; via knot)` never runs |

`TurboMCPGraphText.cpp` has the branch:

```cpp
if (IsKnot(Target) && bFull)
{
    return FString::Printf(TEXT("(-> %s ; via knot)"), *TurboMCPNodeId::MakeForNode(Target));
}
```

and it is **unreached by every suite in `Tools/`**. The JSON writer forwards the level to that
same writer, so a knot would flow through correctly — but "would" is not evidence, and this
project has already shipped four checks that passed while the thing they claimed to verify was
broken.

The pin INDEXES named in the spec are also not visible in the output: `full` prints `; out NAME
-> N link(s)` but no numeric index. That may be a deliberate difference from the spec's wording
or a gap. It is not claimed either way here.

Question: should a test asset with a knot be built, and should the pin index be printed?

Answer: **the knot half is answered, and the answer is that it cannot be built with the tools
available.** The pin index half is still open.

WHAT WAS TRIED, and what each attempt returned:

1. **The engine's DSL cannot create a reroute node.** `blueprint_dsl.py` recognises one — it
   defines `_REROUTE_NODE = '|RerouteNode'` and has a `_follow_reroutes` function that walks
   through knots while traversing a graph, and it even raises `Cycle detected in reroute (knot)
   nodes` on a malformed chain. So the type is known. But **writing** one through
   `write_graph_dsl` is refused:

   ```
   AssertionError: The node could not be created / |RerouteNode does not exist
   in: (|RerouteNode)
   ```

   The engine's own writer can traverse a knot and cannot make one. The test graph was checked
   afterwards and is unchanged, so the refusal happens before anything is written.

2. **A knot is not a menu node type**, so it cannot be written by name the way every other node
   can. A sweep of `find_node_types` filtered to `Utilities|FlowControl` returns 914 types and
   none is a knot. That is consistent with the refusal above rather than a second problem.

3. **`UK2Node_Knot` is not reachable from Python.** It is declared `UCLASS(MinimalAPI)`, and the
   programmatic toolset the MCP server exposes permits only `copy`, `re`, `math`, `json`,
   `datetime` and `time` as imports, so `unreal` is not available to a script at all.

So the branch that prints `(-> <node id> ; via knot)` at `full` is **reachable only from a graph
that already contains a knot**, and nothing in this repository's toolchain can put one there.

WHAT THIS MEANS, stated plainly: the branch is **not wrong**, and it is **not verified**. The JSON
forwards the level to the same writer, so a knot would flow through, but that is reasoning rather
than evidence. TestRecord 3h records three of the four details as verified; this is the fourth.

HOW IT COULD STILL BE CLOSED, for whoever picks it up: a knot could be added by an engine tool
that spawns nodes directly rather than through the DSL, or by a small commandlet inside this
plugin using `FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Knot>` the way
`EdGraphSchema_K2.cpp` does. The second is also the route to the hand-made acceptance kit that
`06_ACCEPTANCE.md` requires, so it would pay for itself twice.

THE PIN INDEX half is untouched and still ambiguous: `full` prints `; out NAME -> N link(s)` and
no numeric index, while the spec names "pin indexes". Deliberate difference or gap has not been
decided, and it is not claimed either way.

---

## Q-10 RESOLVED. The engine's tool-call event does not carry enough to name the tool

**Status: the log works. The ENGINE's tool names resolve only partly; the plugin's own tools are
logged from inside themselves with full arguments, which is the case the spec actually cares about.**

### What was measured, not assumed

`ModelContextProtocolAnalytics.cpp` records one event per tool call, named `<namespace>.ToolCall`,
carrying exactly five attributes:

| Attribute | Value |
|---|---|
| `SessionId` | the MCP session id |
| `ToolsetNameHash` | `LexToString(FBlake3::HashBuffer(utf8(toolset name)))` |
| `ToolNameHash` | the same hash over the tool identifier |
| `Duration` | seconds, as a string |
| `ResultStatus` | `Success` or `Error` |

**There are no names and no arguments.** A reader of the engine's event has a 64-character hash and
nothing else.

### What resolves

`FTurboMCPAnalyticsProvider` reproduces the engine's own hash — the UTF-8 step matters, because
hashing the `TCHAR` buffer would differ for any non-ASCII name — and holds a hash-to-name table.
This plugin's own four TOOLSET names resolve. Measured:

```
2026-10-09T17:07:20.229Z  end  engine  TurboMCP.TurboMCPEditToolset...
```

### What does not, and why

**The tool identifier the engine hashes is not the string this plugin registers**, and the exact
string it does hash was not pinned down in this round. Measured symptoms:

- With only the toolsets registered, the line read
  `TurboMCP.TurboMCPReadToolset.(engine tool ending 43b186416e61)` — a name beside a hash.
- With the tool registered as `<Toolset>.<Tool>`, the hash still did not match.
- With the tool tried against EVERY toolset prefix (a bug), the log showed
  `TurboMCP.TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset.ReadTurboMCPBlueprintLoad` — the
  toolset repeated inside the tool name. That is **worse than a hash, because it looks correct**.
  The pairing is now data rather than a loop, so that wrong answer cannot come back.

### The honest position

Engine tool names are written as `(engine tool ending <12 hex>)`. That is **labelled as what it is**
rather than invented, and twelve hex characters tell two engine tools apart inside one session.
`07_ENGINE_NOTES.md` section 2.4 anticipated exactly this and gave the shape of the answer the
plugin took: *"When arguments are missing from the event, record the fields it has for engine tools,
and record full detail for the plugin's own tools from inside those tools."*

### The route that would close it

Read the identifier the engine ACTUALLY hashes, from the engine source that builds it, rather than
inferring it from the call site. `RecordToolCallEvent(SessionId, ToolName, ...)` receives `ToolName`
from the server's dispatch and the string it holds at that point has not been read. One grep of the
caller, and the registration table would be exact.

### The other measured fact: two events per call

Every plugin tool call produced **two** log lines with the same duration, one whose toolset resolved
and one whose toolset hash did not — `(engine tool ending af1349b9f5f9)` appears on the second line
of every pair. So the server records the event twice, or records it once from the handler and once
from the transport. Not investigated; recorded rather than guessed at.

---

---

## Q-10 RESOLVED (round 25). The engine's identifiers, found by reading where they are built

**The question is closed. Every name in the call log resolves; a check now fails if any hash
placeholder survives.**

### What was wrong

The first answer accepted that engine tool names could not be resolved, because
`UToolsetRegistry` exposes no list of served toolsets or tools. **That was the wrong place to
look.** The engine already solves this exact problem for itself:
`ModelContextProtocolToolHashMappingCommandlet` builds a hash-to-name mapping, and reading it gave
the whole answer.

### The three facts that closed it

1. **`IModelContextProtocolModule::GetTools()`** returns every registered tool with its identifier.
   The plugin's provider now reads it rather than keeping a hand-written table, so a tool added
   anywhere resolves without any file being touched.

2. **`UE::ModelContextProtocol::Analytics::HashToolIdentifier` and `ParseToolsetName` are exported
   `UE_API`.** The first version *reimplemented* the hash — UTF-8 through `FBlake3::HashBuffer`,
   then `LexToString` — and that was a liability: a reimplementation is correct only while it
   matches, and **nothing would have said so when it stopped.** The engine's own function is now
   called, which makes the lookup exact by construction.

3. **In tool-search mode, `GetTools()` returns exactly THREE identifiers** — `call_tool`,
   `describe_toolset`, `list_toolsets`, all with an **empty** toolset prefix. Measured, not assumed.
   The toolset's own tools are *"dispatched through `call_tool` without being registered"*, as
   `ModelContextProtocolToolsetRegistryAdapter.h` says. So the inner event's toolset hash is over
   the string the caller passed, and the **tool slot holds the hash of the whole qualified name**:
   `ModelContextProtocolServer.cpp:904` builds `ResolvedFullName = toolset_name + "." + tool_name`
   and passes *that*. Registering the bare tool name was not enough.

### The two events per call are the engine's own design, not a bug

`ModelContextProtocolServer.cpp:887` emits one event for `call_tool` and `:907` emits a **second**
for the inner tool, with the comment:

> *Tool-search mode routes every actual toolset invocation through `call_tool`; without a second
> emission every routed call collapses onto the empty-prefix toolset hash and per-toolset analytics
> for the real targets stay invisible.*

So two lines per call is correct. The earlier note in this file calling it an unexplained
duplication is **withdrawn**.

### A bug the fix exposed: resolved-and-empty is not unresolved

The empty toolset prefix is a **legitimate key** — the engine's own commandlet registers it — so
`call_tool` resolves to the empty string. The lookup then tested `IsEmpty()` to decide whether
resolution had happened, and printed:

```
(engine tool ending af1349b9f5f9)  .  call_tool
```

**a line saying it could not name a toolset it had just named.** Resolution is now tracked by
whether the key was **found**, not by whether the value is empty, and an empty prefix is written as
`(top-level)` so a reader sees a name rather than a blank column.

### Measured, after the fix

```
(top-level)                   .  call_tool
TurboMCP.TurboMCPEditToolset  .  TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog
(top-level)                   .  call_tool
TurboMCP.TurboMCPReadToolset  .  TurboMCP.TurboMCPReadToolset.ReadTurboMCPBlueprintLoad
```

### The check that would have caught the earlier half-state

The suite previously *tolerated* a line with a name in one slot and a hash in the other, which
**looks finished**. It now refuses any line containing a hash placeholder, so the half-resolved
state cannot pass again.

---

---

## Q-11. The panel's checkbox has never been SEEN

**Status: open, and deliberately recorded rather than worked around.**

`03_TOOLS.md` requires the call log to be turned on by a checkbox in the panel. The checkbox exists,
compiles, and is constructed in the panel's Slate tree; the state it shows and the toggle it fires go
through the same code paths the tools use, and the tool side is covered by `test-call-log.py`.

**What is missing is any check on the WIDGET ITSELF.** This plugin has no Slate inspection tool, so
the panel's widget tree cannot be read from here, and `test-agent-help` does not reach it either. So
these are unverified:

- that the checkbox's label reads as intended and is not clipped;
- that the summary line fits and is readable at the default panel width;
- that ticking the box actually repaints the summary, rather than showing a stale figure until the
  next tool call refreshes it. **This one is a real risk**: `GetCallLogSummary` is a bound attribute,
  which Slate re-evaluates on paint, but paint is not guaranteed to happen just because the setting
  changed, and no code calls `Invalidate` after a tick.

**The route that would close it:** a Slate inspection capability over the panel's widget tree, which
is the same gap the engine's own `SlateInspectorToolset` fills for the editor's other windows. Adding
one is a candidate for phase 6, where the layout tools live.

Until then, a human opening the Window menu and looking at the TurboMCP tab is the only proof.

---

---

## Q-12. The dead-node REMOVAL path has never run

**Status: open, deliberately recorded rather than worked around.**

`P/E-GRAPH-DEAD` finds disconnected groups that nothing reaches and whose writes are never read, and
`E-GRAPH-DEAD` removes them. **The finding half is verified. The removal half is not, and cannot be
with the writers available.**

### Why it cannot be tested here

This plugin's tools read, preview and edit; they do **not** create graph nodes. The engine's
`write_graph_dsl` does, and it **refuses to build a disconnected group**: it needs a top-level `event`
or `fn` form, and every node it makes is wired into that form's exec chain by construction. Measured
attempts:

- `(Development|PrintString "orphan probe")` alone →
  `Top-level form must start with "event" or "fn"`.
- `(fn OrphanProbe (Development|PrintString "orphan probe"))` →
  `The node could not be created / AddEvent|OrphanProbe does not exist`.

So the test graph contains **no dead node**, and no dead node could be made. The same wall `Q-9` hit
with reroute knots.

### What IS verified

| Claim | Evidence |
|---|---|
| a live graph is reported as having **nothing** dead | `11 node(s) in the graph, 0 unreachable group(s)` |
| the walk follows **data** wires, not exec alone | the graph's pure operator nodes have no exec chain of their own and none is called dead |
| the preview **changes nothing** | the node set is compared by identity before and after |
| a no-op edit says `unchanged:` | `unchanged: EventGraph has 0 unreachable group(s) and none is safe to remove` |
| the request is validated | no asset, a non-Blueprint, and a graph that does not exist are each refused by name |

The **generous reachability rule is the point of the verified half**: a walk that over-reports would
call **live** code dead, and that is the failure that destroys a user's work. A live graph answering
"nothing" is the single most important result to get right, and it is got right.

### What is NOT verified

- that a group IS found when one exists;
- that the three KEEP rules fire in practice — a group holding an entry, a group whose write is read
  by live code, and a group that repeats live logic;
- that removal takes only the intended nodes, and that the node counts before and after agree;
- that the entry re-check on the node about to go actually protects anything.

### The route that would close it

An asset that SHIPS with a dead group, committed once and never rebuilt — the same approach
`TA-1` to `TA-13` in `06_ACCEPTANCE.md` take for the hand-made kit. A graph written by the editor's
own UI, or by a C++ commandlet that wires a group and then breaks the one link that reaches it, would
give the tool something real to find. Until then this capability's write half rests on code review.

---

---

## Q-13. The local-variable PROMOTION path has never run

**Status: open, deliberately recorded rather than worked around.**

`P/E-LOCAL-VARS` finds pure values read more than once and promotes the SAFE ones. **The finding half
is verified. The promotion half is not, and cannot be with the writers available.**

### Why it cannot be tested here

A candidate needs a **pure node whose value is read more than once**. This plugin's tools do not create
graph nodes, and the engine's `write_graph_dsl` **cannot express one**: a DSL expression is a **tree**,
and a tree has no way to name an intermediate value, so every sub-expression it writes is consumed by
exactly one parent. Measured against the DSL's own examples:

```
(fn DoubleIt (Value)
  (return (* Value 2)))

(fn ClampedScale (Value Multiplier)
  (return (* (select (< Value 0.0) 0.0
               (select (> Value 100.0) 100.0 Value))
             Multiplier)))
```

Every value in these is used exactly once. There is no form for "compute this once, read it twice".

**So the test graphs contain no candidate, and none can be made.** The same wall `Q-9` hit with reroute
knots and `Q-12` hit with disconnected groups — three capabilities now blocked by the same gap.

### What IS verified

| Claim | Evidence |
|---|---|
| a graph that cannot hold locals is refused **with that reason** | `'EventGraph' is not a function graph … An event graph has no local variables.` |
| the refusal **lists the functions that do exist** | `Its functions are: UserConstructionScript.` |
| a **function** graph is accepted | the same tool answers a function graph without an error |
| a function with no candidate says **why** | `0 value(s) used more than once … no pure node's value is read more than once` |
| the preview changes nothing | the asset manifest is compared before and after |
| the edit says `unchanged:`, not `changed: promoted 0` | `unchanged: no value in … was promoted` |
| the **event graph is refused by the EDIT too**, with the same reason | `D-02`: a preview and its edit twin must refuse the same input |

**The refusal checks are the valuable half.** The difference between *"this graph cannot hold a local"*
and *"nothing found"* is a reason a caller acts on, and getting it wrong would send an agent to the
wrong function.

### What is NOT verified

- that a candidate IS found when one exists;
- that the **transitive safety rule** classifies anything — neither a safe chain nor an unsafe one has
  ever been walked;
- that an unsafe value is actually REFUSED by the edit;
- that promotion works at all: `AddLocalVariable`, the getter node, the pin-by-pin rewire, and the
  removal of the original last.

### The route that would close it

An asset that **ships with a candidate** — a function graph containing a pure node read twice, with one
safe feed and one unsafe feed — built once in the editor's own UI and committed. That is the same
approach `Q-12` needs and the same one `TA-1` to `TA-13` in `06_ACCEPTANCE.md` take for the hand-made
kit. **One committed asset would close Q-12 and Q-13 together**, and would unblock `P/E-GRAPH-KNOTS`
(Q-9) if it also carried a knot.

---

---

## Q-14. A commandlet in this plugin cannot be found, and the route that works

**Status: the commandlet is written and compiles; it cannot be DISCOVERED. The working route is
Python, and it is measured.**

### Wall 1: the engine does not load this plugin to look for a commandlet

Running `-run=TurboMCPAcceptanceAsset` answers:

```
LogInit: Error: TurboMCPAcceptanceAssetCommandlet looked like a commandlet,
but we could not find the class.
```

The class is in the binary. The engine's lookup **infers a module name from the commandlet name**
(`TurboMCPAcceptanceAsset`), calls `LoadModule` for it, finds nothing, and gives up — it does not know
to load **this** plugin. The `.uplugin` loads the module at `PostEngineInit`, which is **later than
commandlet discovery**, so the class was present and **unregistered**. That is the same shape as phase
0's toolset registration, and the same lesson: a class present and unregistered looks exactly like a
class that is missing.

### Wall 2: a static initialiser that loads the module CRASHES

Measured, so nobody tries it again:

```
LogWindows: Error: Fatal error!
LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000011e05c975a8
[Callstack] UnrealEditor-Core.dll!FModuleManager::LoadModuleWithFailureReason()
            [ModuleManager.cpp:1222]
```

A static initialiser inside a module **that is being loaded** runs **during** that load, so asking the
manager to load the same module again **re-enters it before it is on the list**. The initialiser has
been removed, and this note is in the source file too.

### The route that WORKS, measured

Python's commandlet runs, and **Python can build every structure the acceptance asset needs**:

```
add_function_graph -> <Object '/Game/Tests/BP_NodeTest.BP_NodeTest:PyProbe' Class 'EdGraph'>
new_object K2Node_Knot -> <Object '/Engine/Transient.K2Node_Knot_0' Class 'K2Node_Knot'>
```

And a K2Node's pins are reachable **without** `unreal.EdGraphPin`, which is **not exposed to Python**,
through the node's own accessors:

| Accessor | Available |
|---|---|
| `find_input_pin` | yes |
| `find_output_pin` | yes |
| `find_execute_pin` | yes |
| `find_data_input_pin` | yes |
| `find_result_pin` | yes |
| `find_self_pin` | yes |

So the acceptance asset can be built by a **Python commandlet script** — the route phase 2 already
proved with `make-test-blueprint.py` — and the C++ commandlet stays as the **record of the structure**
and of the wall it hit.

### What this means for Q-9, Q-12 and Q-13

All three stay open, but they are now **one script away** rather than blocked by an unknown. The script
needs to build, in `/Game/Tests/TurboMCPAcceptance`:

1. **KnotProbe** — three `K2Node_Knot`s between two distant calls, for `P/E-GRAPH-KNOTS` (Q-9);
2. **DeadProbe** — a node group with **every link broken**, for `P/E-GRAPH-DEAD` (Q-12);
3. **LocalVarProbe** — a pure node whose value is read by **two** consumers, one fed by a function
   input (SAFE) and one by a member variable (UNSAFE), for `P/E-LOCAL-VARS` (Q-13);
4. **SaneProbe** — an ordinary chain that **should not be disturbed**, which is what a rollback test
   needs.

### A Python probe failure worth recording

A probe script that called `dir(unreal.EdGraphPin)` **wrote no file and exited -1**, because the class
does not exist and the exception aborted the script before the write. **Silent**: the commandlet still
reported success in one variant. A probe that means to report must write **before** the risky call, or
a failure looks like a success with no output.

---

---

## Q-12 RESOLVED (round 31). The dead-group path runs

**The acceptance asset closed it.** `UTurboMCPAcceptanceAssetCommandlet` builds
`/Game/Tests/TurboMCPAcceptance:DeadProbe`, a function graph whose two node groups have **every link
broken** — which is the state a user leaves behind when they delete the node that used to call a
chain, and which `write_graph_dsl` cannot make because it needs a top-level `event`/`fn` and wires
every node into that form's exec chain.

Measured, and both halves now run:

```
preview: dead nodes in ... : DeadProbe
4 node(s) in the graph, 2 unreachable group(s)

WOULD REMOVE  1 node(s): K2Node_CallFunction
    nodes: 939233
    REPEATS LIVE LOGIC: calls PrintString

WOULD REMOVE  1 node(s): K2Node_CallFunction
    nodes: 9aa718
    REPEATS LIVE LOGIC: calls PrintString

the edit would remove 2 group(s) and leave 0, and would then report the node counts before and after
```

So the finding half fires on a real dead group, the **REPEATS LIVE LOGIC** report works, and the three
KEEP rules are exercised in the negative. The **removal** is not yet run against this asset; extending
`test-graph-dead.py` to do so is the remaining step, and it is now a test change rather than a missing
artifact.

### The route that closed it, for the record

`LaunchEngineLoop.cpp` calls `LoadModule` **only when the `-run=` token contains a period**, so the
commandlet must be named `-run=TurboMCP.TurboMCPAcceptanceAssetCommandlet`. The previous round's
static-initialiser workaround was unnecessary and crashed.

---

---

## Q-11 PARTLY CLOSED (round 34): the runner is testable, the Slate LAYOUT is not

`Q-11` recorded that the panel's checkbox had never been SEEN, because this plugin has no Slate
inspection tool and the panel's widget tree cannot be read. The requirement that every MCP feature be
testable in the panel turned that into two separate questions, and **only one of them is now answered**.

### The runner half IS closed

`Source/TurboMCP/TurboMCPPanelProbeCommandlet.cpp` is a COMMANDLET rather than a tool, because a panel
feature does not belong in the MCP API. Invoked with the module prefix the engine needs -
`-run=TurboMCP.TurboMCPPanelProbeCommandlet`, because `LoadEngineLoop.cpp` calls `LoadModule` only when
the token contains a period - it drives `FTurboMCPPanelToolRunner`, which is plain C++ with no Slate.

Measured on a clean run:

```
panel-registry: available
panel-toolsets: 27 published
panel-toolset: present  TurboMCP.TurboMCPReadToolset / LookToolset / PreviewToolset / EditToolset
panel-toolset-tools: Read 21, Look 4, Preview 10, Edit 17 - 0 partial, 0 with no sample
panel-tools: 52 total, 0 with a partial sample
panel-summary: registry=available toolsets=27 missing=0 tools=52 partial=0 failedtoolsets=0
```

`Tools/test-panel.py` asserts each count against **what the header declares**, not a literal, so adding
a tool cannot make it wrong.

### TWO THINGS THIS MEASURED THAT WERE ONLY FEARED BEFORE

**The deadlock does not happen.** `CallAndWait` sleeps on the calling thread while the future is
completed, and a `-run=` commandlet has no editor tick to do that, so a hang was expected. Measured:
**it returns an ANSWER in 0.00 s**, because `ExecuteTool` completes the future SYNCHRONOUSLY for a
synchronous C++ tool. The runner's live path is verifiable without an editor, and the TIMEOUT branch is
now the only unrun path in the runner.

**The panel would have shown a JSON blob.** `UToolCallAsyncResultString::Value` is declared
`FString Value` and reads like the tool's text; measured, it is the text SERIALISED as
`{"returnValue":"..."}` with ESCAPED newlines. Reading the field directly compiles and leaves the
wrapper on, which is **one wrong decoding replaced by another**. The field is now parsed and
`returnValue` extracted. Proven by the character count: the live call reported **192 characters before
and 173 after**, exactly the wrapper removed.

### The Slate half is STILL OPEN and cannot be closed by this

The probe proves nothing about the panel's **layout**, whether the combo boxes show their selection,
whether a SAMPLE press visibly refills the arguments box, or whether the result pane repaints. `/`Q-11`
stays open for those, and they need a human eye or a Slate inspection tool.
