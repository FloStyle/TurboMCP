# TurboMCP merge run session report

**Report ID:** 2026-10-26_S4
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** build compiled 2026-10-26 14:50, source revision Unknown
**Engine version:** 5.8.3, project build
**Client:** Claude Code desktop app, version Unknown
**Test case:** Merge of a menu widget blueprint (about 15 graphs, a widget subtree of 11 widgets) into a duplicate of a second menu widget blueprint that holds an interface.
**Context:** No play session. Source control was in use. Only a new duplicate asset was changed. Originals were not edited.
**Tools used:** ReadTurboMCPStatus, ReadTurboMCPToolIndex, ReadTurboMCPToolHelp, ReadTurboMCPBlueprintLoad, ReadTurboMCPGraphLoad, PreviewTurboMCPInterfaceAdd, PreviewTurboMCPMergeMembers, EditTurboMCPMergeMembers, PreviewTurboMCPWidgetSubtreeCopy, EditTurboMCPWidgetSubtreeCopy, PreviewTurboMCPGraphTransfer, EditTurboMCPGraphTransfer, EditTurboMCPFunctionRemove, EditTurboMCPOverrideAdd, EditTurboMCPGraphRemove, EditTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphTidy, EditTurboMCPCompile, EditTurboMCPSave. Other tools by role: engine asset duplicate and delete, engine widget rename.
**Outcome:** Partial merge. Widgets, members and 5 function bodies moved. Two functions and the event graph are blocked. Compile clean. Saved as a checkpoint. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-101: Graphs that call custom events cannot be transferred, and custom events cannot be created by the merge tools (high)**
- **Type:** Limitation.
- **Scenario:** The source event graph defines three custom events and calls one of them. Two source functions call a custom event.
- **Reproduction status:** Observed three times.
- **Steps to reproduce:**
  1. Run MergeMembers from source to target.
  2. Call GraphTransfer for the source event graph with a new target graph name.
  3. Call GraphTransfer for a function that calls a custom event.
- **Observed:** MergeMembers did not declare the custom events. The event graph transfer refused on a custom event that the same graph defines. The function transfer refused on a missing custom event.
- **Expected:** Custom events defined inside a transferred event graph satisfy the dependency check. MergeMembers declares custom events, or the transfer of the defining event graph comes first.
- **Impact:** The merge stops with two functions and the event graph not moved.
- **State after:** The target compiles clean without these graphs. Saved as a checkpoint.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Count custom events defined in the transferred graph as satisfied. Add custom events to MergeMembers.

## 2. Tool observations

### Graph transfer

**GC-52: GraphTransfer of an inherited override created a plain function with the same name (high)**
- **Type:** Defect.
- **Scenario:** The source overrides a parent class function. The target has no override yet.
- **Reproduction status:** Observed once.
- **Observed:** The preview said the target had no existing declaration. The transfer created a plain function with the parent function name. The next compile failed with a name-already-used error.
- **Expected:** The transfer creates an override, or refuses and points to OverrideAdd.
- **Impact:** A compile error and a manual repair.
- **State after:** Repaired by removing the function, adding the override, and rebuilding the 3-node body with append and connect. Compile clean.
- **Workaround:** As above. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Detect parent-declared functions and create an override.

**GC-53: GraphTransfer refuses a fresh override because of its default parent call (medium)**
- **Type:** Limitation.
- **Scenario:** An override created by OverrideAdd contains the default parent call and a return link.
- **Reproduction status:** Observed once.
- **Observed:** The transfer refused because the target function contains logic.
- **Expected:** The default parent call of a fresh override counts as empty, or an option replaces it.
- **Impact:** The body was rebuilt by hand.
- **Workaround:** Manual rebuild. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Treat the untouched default override body as empty.

**GC-54: GraphTransfer moved four function bodies cleanly (positive)**
- **Scenario:** Functions of 4 to 30 nodes with locals, a data-table loop macro and delegate binds, into empty declarations made by MergeMembers.
- **Observed:** All four transfers succeeded. The target compiled clean.

### Member merge

**FA-13: MergeMembers skipped a name that collides with an interface event (positive)**
- **Scenario:** The source has a function with the same name as an event of the target's interface.
- **Observed:** The function was left out without an error. The other 9 declarations were created.

### Widget tree

**WT-12: WidgetSubtreeCopy worked after the collision was removed (positive)**
- **Scenario:** An 11-widget subtree.
- **Observed:** After the target root panel was renamed, the copy kept names, properties, slots and localized text. The rename needed an engine tool; no TurboMCP widget rename exists.

## 3. Recurrence

- X-101 follows FA-12 (report 2026-10-26_S3). Still observed on the new build.
- X-100, WT-11 (report 2026-10-26_S3): still observed. The function rename was avoided by merging in the other direction.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-110 | TurboMCP widget rename | One engine fallback call |

## 5. Suggested priority

1. X-101: the last blocker for the merge.
2. GC-52: it produced a compile error.
3. GC-53, FT-110.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-101, GC-52, GC-53, GC-54, FA-13, WT-12, FT-110. The report was read back. Local lint passed. The glossary is in the archive folder and was not updated. Automated checking does not prove confidentiality.
