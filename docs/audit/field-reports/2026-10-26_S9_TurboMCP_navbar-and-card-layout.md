# TurboMCP navigation bar and card layout session report

**Report ID:** 2026-10-26_S9
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** Local build compiled 2026-10-26 15:36 (exact commit not recorded in this session)
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** Three widget Blueprints and one shared card widget. A breadcrumb widget was copied into one menu and added to one detail screen. Two functions received new nodes. Card size, padding, spacing and visibility defaults were changed.
**Context:** Edits were made outside play mode, except where noted in X-105. Perforce source control in use.
**Tools used:** EditTurboMCPWidgetSubtreeCopy, EditTurboMCPWidgetsAsVars, EditTurboMCPOverrideAdd, ReadTurboMCPOverrides, EditTurboMCPGraphAppend, PreviewTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphSwapCall, EditTurboMCPGraphRemove, EditTurboMCPGraphSplitValue, EditTurboMCPGraphTidy, LookTurboMCPLayoutDoneCheck, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPVcsCheckout, ReadTurboMCPVcsState, ReadTurboMCPGraphLoad, ReadTurboMCPNodeFind, ReadTurboMCPNodeResolve, ReadTurboMCPBlueprintLoad, ReadTurboMCPWidgetTree, ReadTurboMCPRefs, ReadTurboMCPAssetSearch, ReadTurboMCPDataTable, ReadTurboMCPDirtyList. Other tools: the engine widget toolset (add and remove widget), the engine object property reader and writer, and the engine Blueprint node-type search.
**Outcome:** All requested changes are complete. Every edited asset compiled with no errors and no warnings. All saved. No crash. The user tested the result in play mode.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-105: TurboMCP edits refuse during play mode, but engine property edits apply (medium)**
- **Type:** Usability.
- **Scenario:** Play mode was running while widget layout properties were changed and the asset was then compiled.
- **Reproduction status:** Observed twice.
- **Steps to reproduce:**
  1. Start play mode.
  2. Set widget properties with the engine object property writer.
  3. Compile or save with the TurboMCP tools.
- **Observed:** The property writes succeeded. Compile and save were refused because play mode was active. The asset held unsaved changes until play mode ended.
- **Expected:** A consistent rule across tools, or a clear warning that part of the edit is pending.
- **Impact:** A partly applied edit that needed a second pass.
- **State after:** After play mode ended, compile and save succeeded and the values read back.
- **Workaround:** Wait for play mode to end, then compile and save. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Refusal messages could list unsaved packages that hold edits made by other tools.

## 2. Tool observations

### Graph append

**GC-61: Append cannot reference a variable whose name contains a space (medium)**
- **Type:** Defect.
- **Scenario:** A widget Blueprint variable with a two-word name.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Append a getter form for that variable, using its type ID.
- **Observed:** Refused. The parser cut the type ID at the space. The suggested closest IDs contained the full name.
- **Expected:** The type ID from the tool's own suggestion is accepted, with quoting if needed.
- **Impact:** A getter next to its consumer could not be created. One long wire remains instead.
- **State after:** Unchanged by the refused call.
- **Workaround:** Connect from the output pin of the existing setter node for that variable. This worked and compiled.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept quoted type IDs, or name forms with spaces.

**GC-62: Append does not accept branch output sections (low)**
- **Type:** Limitation.
- **Scenario:** A new Branch with one new call on each output.
- **Reproduction status:** Observed twice.
- **Steps to reproduce:**
  1. Append a Branch followed by output sections for its true and false pins, each with a new call inside.
- **Observed:** Refused. The section was read as an unknown pin.
- **Expected:** The section syntax that the reader prints is also accepted as input.
- **Impact:** One retry.
- **State after:** Unchanged.
- **Workaround:** Give each new call an execute link to the Branch type ID with the pin name. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept output sections, or document that they are reader-only.

**GC-63: Append creates break-struct nodes that the reader marks as not writable (positive)**
- **Scenario:** A break node for a project struct was needed to read one text field.
- **Observed:** The readers show break-struct nodes with no type ID and the note that they cannot be written back. The engine node-type search returned a type ID. Append accepted it and created the node, which connected and compiled.
- **Note:** The reader message suggests this is impossible. A reader hint with the type ID would avoid the detour.

### Layout

**TD-42: Scoped tidy placed split getter copies far from their consumers (medium)**
- **Type:** Defect.
- **Scenario:** A long linear function. Shared getters were split with GraphSplitValue, and then only the new nodes were tidied.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Split two shared getters in a long function.
  2. Tidy only the new copies and the nodes added in this session.
- **Observed:** The tidy reported improved structure, but the layout check then found getter copies about 3000 to 5000 units from their consumers, and backward data wires.
- **Expected:** Copies are placed next to the consumer, as the split tool describes.
- **Impact:** One extra split and a whole-graph tidy.
- **State after:** A whole-graph tidy with the automatic main path fixed the layout. Compiled clean.
- **Workaround:** Whole-graph tidy. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Scoped tidy keeps each getter beside its single consumer.

### Widget tree and other edits

**WT-14: WidgetSubtreeCopy cloned a configured breadcrumb into another menu (positive)**
- **Scenario:** A breadcrumb subtree with instance settings, copied between two menu widgets with the same parent class.
- **Observed:** Three widgets were copied with their properties and slots. Only one setting had to change afterward.

**GC-64: SwapCall with a pin map, OverrideAdd, SplitValue and the layout check worked as described (positive)**
- **Scenario:** A lookup call was swapped for another call with a renamed input. An activation event was added. Shared getters were split.
- **Observed:** Each call succeeded. The swap moved four links and kept defaults. The connect tool reported the pure node it left unused.

## 3. Recurrence

- X-105 relates to the play-mode refusals noted in 2026-10-26_S1. Conditions differ: this time, engine edits were applied during play mode.
- The need for a TurboMCP widget property writer and a widget add or remove tool recurs from WT-13 (2026-10-26_S8) and 2026-10-26_S2. This session used the engine tools for every widget property change and for one widget add and one widget remove.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-116 | Widget property set with slot support and read-back | About ten engine property calls for size, padding, spacing and visibility |
| FT-117 | Widget add of a user widget class into a named parent | One engine add-widget call, then a separate property pass |
| FT-118 | Reader hint with the writable type ID for break and make struct nodes | An engine node-type search |

## 5. Suggested priority

1. GC-61: names with spaces are common in project Blueprints and block the getter-per-consumer layout style.
2. TD-42: the tidy reported success but left a poor layout.
3. FT-116 and FT-117: every widget layout change needed engine fallbacks.
4. X-105, GC-62, FT-118: friction with working workarounds.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs in this report: X-105, GC-61, GC-62, GC-63, GC-64, TD-42, WT-14, FT-116, FT-117, FT-118. Missing evidence: plugin commit, engine revision and client version. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
