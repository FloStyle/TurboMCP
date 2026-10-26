# TurboMCP merge completion session report

**Report ID:** 2026-10-26_S8
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** Local build compiled 2026-10-26 15:36 (exact commit not recorded in this session)
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** Merge of two widget Blueprints into a third widget Blueprint. One event graph (44 nodes), one function (14 nodes) and one renamed function (17 nodes) were transferred. One menu data table row was repointed.
**Context:** Editor not in play mode during edits. Perforce source control in use.
**Tools used:** PreviewTurboMCPGraphTransfer, EditTurboMCPGraphTransfer, EditTurboMCPCompile, EditTurboMCPSave, ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPGraphLoad, ReadTurboMCPAssetDescribe, ReadTurboMCPBlueprintLoad, ReadTurboMCPWidgetTree, ReadTurboMCPRefs, ReadTurboMCPDataTable, PreviewTurboMCPGraphSwapCall, EditTurboMCPGraphSwapCall, EditTurboMCPGraphRemove, EditTurboMCPGraphAppend, PreviewTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphTidy, EditTurboMCPGraphPositions, LookTurboMCPLayoutDoneCheck, EditTurboMCPFunctionRemove, EditTurboMCPPinDefault, EditTurboMCPVarsAdd, EditTurboMCPDataTableRows, EditTurboMCPVcsCheckout. Other tools: the engine widget toolset (widget removal) and the engine object property reader.
**Outcome:** The merge task is complete. The merged asset compiles with no errors and no warnings. All edited assets saved. No crash. Runtime behavior was not tested in this session.
**Session limits:** The client lost the call format after a context compaction. The first three calls failed with tool not found until the toolset name was passed again. This is client context loss, not a plugin defect.
**Correction of:** None

## 1. Cross-cutting issues

**X-103: Failed previews leave transient widget Blueprints with compile errors (medium)**
- **Type:** Defect.
- **Scenario:** Several append previews in one session were refused during scratch execution.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Run append previews or edits on a widget Blueprint function that are refused during scratch execution.
  2. Start Play In Editor.
- **Observed:** The editor warned that two transient widget Blueprints with generic names had unresolved compiler errors. Every real asset compiled clean when checked right after.
- **Expected:** Scratch copies are removed or kept out of the editor's compile-error list after a preview ends.
- **Impact:** The user saw a false error prompt before play and needed an explanation to continue.
- **State after:** Real assets compiled and saved. The transient objects stayed in memory. Unknown whether they persist after an editor restart.
- **Workaround:** Continue past the prompt. Not tested further.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Destroy or mark scratch Blueprints transient-for-compile when the preview finishes.

**X-104: Transferred nodes get new IDs, so preview IDs are stale after the edit (low)**
- **Type:** Usability.
- **Scenario:** A graph transfer preview listed node IDs. Later appends used those IDs.
- **Reproduction status:** Observed three times.
- **Steps to reproduce:**
  1. Preview and apply a graph transfer.
  2. Use an ID from the transfer output in an append link.
- **Observed:** Append refused because the ID was not in the graph. The edit output repeats the source IDs, not the new target IDs.
- **Expected:** The edit result reports the IDs created in the target.
- **Impact:** One extra graph read per graph to get the new IDs.
- **State after:** No change was made by the refused calls.
- **Workaround:** Read the target graph again for fresh IDs. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Print a source-to-target ID map in the edit result.

## 2. Tool observations

### Graph transfer

**GC-58: GraphTransfer moved an event graph, a function and a renamed function cleanly (positive)**
- **Scenario:** An event graph with custom events, an eligibility flow and a message listener. A function that calls those custom events. A function transferred under a new name to avoid an interface name collision.
- **Observed:** All three transfers passed. Each target compiled clean right after. The preview listed every planned node and link.

### Graph edits

**ED-21: GraphSwapCall cannot change a call target from another object to self (medium)**
- **Type:** Limitation.
- **Scenario:** A call to a function on a child widget, with the child widget getter on its target pin. The replacement is a function on the Blueprint itself.
- **Reproduction status:** Observed twice.
- **Steps to reproduce:**
  1. Swap the call to the self function while the getter is linked. The tool refuses because the target link has no compatible pin.
  2. Remove the getter and swap again. The tool refuses because it cannot preserve the target pin default.
- **Observed:** Both attempts refused. No change.
- **Expected:** A swap to a self call drops the old target link and uses self.
- **Impact:** Manual remove, append and wire steps.
- **State after:** Unchanged by the refused calls.
- **Workaround:** Remove the old call, append a new self call and wire exec and data by hand. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** An option to drop or retarget the target pin, for example a self retarget flag.

**GC-59: GraphAppend cannot splice a Branch into an existing exec wire (medium)**
- **Type:** Limitation.
- **Scenario:** A function entry linked to the first node. A guard Branch must go between them, with else continuing to the old target.
- **Reproduction status:** Observed three times.
- **Steps to reproduce:**
  1. Append a Branch whose execute links to the entry output, with continuation set to the Branch else pin by label.
  2. Try again with the label written inline after the closed form.
- **Observed:** Refused. The message asks to name the old target and branch output in the text. A branch section with a link to the existing node was refused as not a pin. Without an execute link, the preview refused because the new node had no path from an entry.
- **Expected:** Continuation by label and pin reconnects the displaced wire to that pin, as documented.
- **Impact:** The guard could not be added as designed.
- **State after:** Unchanged.
- **Workaround:** Combine the guard flag with an existing branch condition through a boolean OR node, then reconnect the condition with GraphConnect. This worked and compiled.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept continuation label and pin for a Branch, or document a syntax that names the old target.

**GC-60: GraphConnect, GraphRemove, PinDefault, VarsAdd, FunctionRemove and DataTableRows worked as described (positive)**
- **Scenario:** Cleanup after the merge: three dead functions removed, one variable added, one log text changed, one condition rewired, one data table class field changed.
- **Observed:** Each call succeeded on the first valid attempt. Results read back the change. Data table checkout and save also worked.

### Graph reader

**GL-22: Readers do not show which function a Create Event node binds (medium)**
- **Type:** Limitation.
- **Scenario:** A function binds card click and hover delegates with two Create Event nodes.
- **Reproduction status:** Observed in three readers.
- **Steps to reproduce:**
  1. Read the function with GraphLoad. The bind nodes show as unwritable, with no bound name.
  2. Read the Create Event nodes with NodeContext. The node shows only its type and self pin.
- **Observed:** No reader shows the bound function name.
- **Expected:** The bound function name is shown, so a caller check can include delegate bindings.
- **Impact:** Two functions could not be proved unused and were kept.
- **State after:** No change.
- **Workaround:** BlueprintLoad at signatures detail reports delegate binding sites per asset. It answered the question for one of the two assets.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Print the bound function name on Create Event nodes in GraphLoad and NodeContext.

**GL-23: NodeContext refuses short IDs that NodeFind returned (low)**
- **Type:** Usability.
- **Scenario:** NodeFind printed a full path ID. NodeContext was given the short hex suffix.
- **Reproduction status:** Observed twice.
- **Steps to reproduce:**
  1. Pass the short hex part of a NodeFind result to NodeContext.
- **Observed:** Refused as not resolving uniquely. The asset-graph-hex form worked. A comma list of two IDs was also refused.
- **Expected:** Short IDs accepted when unique, or a clear note on the required form.
- **Impact:** One retry each.
- **State after:** No change.
- **Workaround:** Pass the asset-graph-hex form, one node per call. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept the short form, or say which form is expected in the refusal.

### Layout

**TD-41: Tidy of one inserted node rolled back with no other option offered (low)**
- **Type:** Usability.
- **Scenario:** One setter node spliced into a long event graph inside a large comment box.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Tidy only the new node with force off.
- **Observed:** The layout made wires longer, so the tool rolled back. The node stays slightly behind its source. Whole-graph tidy worked well for two smaller graphs.
- **Expected:** A placement that keeps the node forward of its source, or a suggestion.
- **Impact:** One backward wire left in the merged graph.
- **State after:** Rollback restored the original positions.
- **Workaround:** None used.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** For a single spliced node, offer a shift of downstream nodes.

### Widget tree

**WT-13: No TurboMCP tool removes a widget (low)**
- **Type:** Limitation.
- **Scenario:** A child widget instance had to be removed from the merged widget tree.
- **Reproduction status:** Observed once.
- **Observed:** No TurboMCP tool for removal. The engine widget toolset removed it, and the asset compiled clean.
- **Expected:** A TurboMCP remove with a reference check.
- **Impact:** One engine fallback call.
- **State after:** Removed, compiled, saved.
- **Workaround:** Engine widget removal. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-113.

## 3. Recurrence

- GC-58 follows X-101 (2026-10-26_S4). The custom-event transfer block from that report did not occur with this build.
- GC-58 follows GC-51 (2026-10-26_S3). The preview now lists planned nodes.
- WT-13 matches the missing widget removal noted in 2026-10-26_S2. Conditions unchanged.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-113 | Widget remove with a check for graph references to that widget | One engine fallback call and a manual reference check |
| FT-114 | Insert a guard Branch on an existing exec wire | Three refused appends and a workaround through a boolean condition |
| FT-115 | Swap a call target to self | Two refused swaps, then remove, append and two wire steps |

## 5. Suggested priority

1. X-103: false compile errors shown before play can make users distrust valid assets.
2. GC-59 and FT-114: guard insertion is a common edit and had no direct path.
3. ED-21 and FT-115: retargeting calls to self is common in merges.
4. GL-22: delegate targets are needed for safe dead-code removal.
5. X-104, GL-23, TD-41, WT-13: friction with working workarounds.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs in this report: X-103, X-104, GC-58, GC-59, GC-60, ED-21, GL-22, GL-23, TD-41, WT-13, FT-113, FT-114, FT-115. Missing evidence: plugin commit, engine revision and client version. Runtime behavior after the merge was not observed. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
