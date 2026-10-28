# TurboMCP function flags, non-Blueprint asset writes and tool toggles session report

**Report ID:** 2026-10-27_S5
**Date:** 2026-10-27
**Review state:** Pending human review
**Plugin version:** Local install; exact commit not recorded in this session
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** Two widget Blueprints edited (one new pure helper function, one new input event lane). Three new input data assets created and configured. One shared input metadata data asset edited through the engine object writer.
**Context:** Editor not in play mode for edits. Perforce source control in use. The editor restarted once during the session.
**Tools used:** ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPGraphLoad, ReadTurboMCPBlueprintLoad, ReadTurboMCPWidgetTree, ReadTurboMCPRefs, ReadTurboMCPToolHelp, EditTurboMCPFunctionAdd, EditTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphRemove, EditTurboMCPGraphTidy, EditTurboMCPGraphPositions, EditTurboMCPPinDefault, EditTurboMCPBindNewEvent, EditTurboMCPWidgetAdd, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPVcsCheckout, PreviewTurboMCPAssetUndo, PreviewTurboMCPAssetRevert, LookTurboMCPLayoutDoneCheck, LookTurboMCPCapture. Other tools: the engine data asset creator, the engine object property reader and writer, the engine node-type search and node creation tool.
**Outcome:** Both Blueprints compiled clean and were saved. One shared data asset was corrupted in memory, not saved, and the user restarted the editor without saving. No crash observed.
**Session limits:** The MCP server connection dropped once (fetch failed, then server unavailable). The user restarted the editor. No other limits observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-113: Engine object writer replaced a whole map in memory while reporting failure (high)**
- **Type:** Defect (engine tool, not TurboMCP).
- **Scenario:** A shared metadata data asset with a map keyed by object references. One entry was to be added.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Read the map property with the engine object reader.
  2. Write the full map back with one extra entry, using the same key format the reader printed.
- **Observed:** The writer refused every key ("is not a subobject of the property owner") and reported that the property could not be set. A read-back then showed the map reduced to one entry with a None key. The asset was dirty in memory.
- **Expected:** A refused write leaves the property unchanged.
- **Impact:** A shared asset was corrupted in memory. The user had to restart the editor without saving.
- **State after:** Not saved. Disk file unchanged. Checkout reverted.
- **Workaround:** Revert the checkout and restart without saving. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Out of TurboMCP scope for the writer itself. See X-114 for recovery.

**X-114: TurboMCP undo and revert do not cover non-Blueprint assets (high)**
- **Type:** Limitation.
- **Scenario:** Recovery of the data asset damaged in X-113.
- **Reproduction status:** Observed once.
- **Observed:** PreviewTurboMCPAssetUndo refused: undo supports Blueprint assets only. PreviewTurboMCPAssetRevert refused: revert requires a Blueprint.
- **Expected:** Reload or revert of any dirty asset from disk.
- **Impact:** No in-session recovery. Editor restart needed.
- **Workaround:** Editor restart without saving. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-129.

**X-115: MCP connection dropped mid-edit (medium)**
- **Type:** Limitation.
- **Scenario:** A checkout and an append sent together.
- **Reproduction status:** Observed once.
- **Observed:** The first call failed with a fetch error. The second reported the server unavailable. Neither call changed anything. The user restarted the editor, after which calls worked.
- **Expected:** A clear reason, or a reconnect.
- **Impact:** One editor restart.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** None.

**X-116: Engine object writer cannot create an instanced sub-object (medium)**
- **Type:** Limitation (engine tool).
- **Scenario:** An input action needed a new instanced key-settings sub-object.
- **Reproduction status:** Observed twice with two value formats.
- **Observed:** Both writes returned success. A read-back showed the property still None.
- **Expected:** The sub-object is created, or the write reports failure.
- **Impact:** The user set it by hand.
- **Workaround:** Manual editor step.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-130.

## 2. Tool observations

### Members

**CB-10: FunctionAdd has no pure or const option (medium)**
- **Type:** Limitation.
- **Scenario:** A helper function that builds a string was requested as pure.
- **Reproduction status:** Observed once.
- **Observed:** FunctionAdd has no flag parameter. No other tool sets function flags. The engine reader could not read the entry node flags.
- **Expected:** A pure and const option, or a function-flags tool.
- **Impact:** The user ticked Pure by hand.
- **Workaround:** Manual editor step. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-128.

**GC-74: GraphCollapse is inspection only (low)**
- **Type:** Limitation.
- **Scenario:** A three-node string chain was to move into a new function.
- **Reproduction status:** Observed once.
- **Observed:** The help states collapse is not implemented. The nodes were rebuilt in the new function by hand and the originals removed.
- **Expected:** Collapse to a function.
- **Impact:** Several extra calls.
- **Workaround:** FunctionAdd, append, remove, connect. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Implement collapse to function.

**GC-75: BindNewEvent refuses short node IDs (low)**
- **Type:** Usability.
- **Scenario:** Binding a new custom event to a delegate pin.
- **Reproduction status:** Observed twice.
- **Observed:** Short IDs were refused with "target must be an exact delegate input pin". The full ID worked.
- **Expected:** Short IDs accepted, or a refusal that names the ID problem.
- **Impact:** Two retries.
- **Workaround:** Full ID. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Same as GC-72.

**CB-11: WidgetAdd placed a designer button and BindNewEvent wired a delegate cleanly (positive)**
- **Scenario:** An invisible button for an action-bar prompt, and a handler for an input-method-changed delegate.
- **Observed:** Both worked on the first valid call. The handler signature came from the delegate.

### Layout

**TD-49: Scoped tidy placed a new four-node lane about 3,000 units from its target (medium)**
- **Type:** Usability.
- **Scenario:** A new input event lane linked to an existing chain inside a comment box, in a graph with seven boxes.
- **Reproduction status:** Observed once.
- **Observed:** The tidy reported improved structure. A capture showed the lane far from the chain it feeds, with a long backward wire across boxes.
- **Expected:** The lane placed beside its target.
- **Impact:** One extra capture and manual placement.
- **Workaround:** Region capture and GraphPositions. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Same as TD-44.

**X-117: Neighbour and region captures made manual placement reliable (positive)**
- **Scenario:** Placement checks after each layout change.
- **Observed:** Neighbour mode framed the right nodes. Region mode gave a stable scale for coordinate math.

## 3. Recurrence

- GC-75 relates to GC-72 (2026-10-27_S4): short IDs refused by an edit tool.
- TD-49 relates to TD-44 (2026-10-27_S3): scoped tidy places nodes far from their links.
- The input action event entry issue from GC-68 (2026-10-27_S4) was seen again. The engine node creation tool was again the workaround.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-126 | Turn each TurboMCP tool on or off one by one | User request; no observed workaround |
| FT-127 | Turn tool presets on or off by domain, for example all vision or capture tools, or one tool family | User request; no observed workaround |
| FT-128 | Set function flags (pure, const) on create or later | A manual editor step |
| FT-129 | Reload or revert any dirty asset from disk, not only Blueprints | An editor restart after X-113 |
| FT-130 | Create and edit instanced sub-objects and object-keyed maps on data assets safely | Two manual editor steps and the X-113 corruption |

## 5. Suggested priority

1. X-113 and X-114, FT-129: a failed write damaged a shared asset with no in-session recovery.
2. FT-126 and FT-127: user request for tool control.
3. CB-10 and FT-128, X-116 and FT-130: common edits that need manual steps.
4. TD-49, GC-74, GC-75, X-115: friction.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs: X-113, X-114, X-115, X-116, X-117, CB-10, CB-11, GC-74, GC-75, TD-49, FT-126, FT-127, FT-128, FT-129, FT-130. Missing evidence: plugin commit, engine revision and client version. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
