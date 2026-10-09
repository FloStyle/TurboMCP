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
