# TurboMCP merge tools first check session report

**Report ID:** 2026-10-26_S3
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** a5775da
**Engine version:** Unknown
**Client:** Claude Code desktop app, version Unknown
**Test case:** Preview-only check of the new merge tools on a real case: two widget blueprints to merge into a third (an unsaved duplicate of one of them).
**Context:** No play session. Source control was in use. Only Preview tools were called. No asset changed.
**Tools used:** ReadTurboMCPToolIndex, ReadTurboMCPToolHelp, PreviewTurboMCPInterfaceAdd, PreviewTurboMCPMergeMembers, PreviewTurboMCPGraphTransfer, PreviewTurboMCPWidgetSubtreeCopy, PreviewTurboMCPLocalVariableAdd, PreviewTurboMCPDispatcherAdd. Other tools by role: engine asset existence check.
**Outcome:** Tool check complete. The merge was not started. No compile. No save. No crash.
**Session limits:** EditTurboMCPMenuActivate was not tested because no play session ran.
**Correction of:** None

## 1. Cross-cutting issues

**X-100: The merge is still blocked by a name collision that no tool can resolve (high)**
- **Type:** Limitation.
- **Scenario:** The merge target has a Blueprint function with the same name as an event of the interface to add.
- **Reproduction status:** Observed once, in preview.
- **Steps to reproduce:**
  1. Call PreviewTurboMCPInterfaceAdd on a Blueprint that has a function with the same name as an interface member.
- **Observed:** The preview refused with a clear message: rename or migrate the existing graph first. No TurboMCP tool renames a function graph and updates its call sites in other Blueprints.
- **Expected:** A rename tool exists, so the refusal has a TurboMCP next step. This is a user expectation.
- **Impact:** The whole merge waits on this one step.
- **State after:** Nothing changed.
- **Workaround:** None observed. A manual rename in the editor is possible but not tried.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-107.

## 2. Tool observations

### Interface

**FA-11: InterfaceAdd refuses a member name collision before any write (positive)**
- **Scenario:** Same as X-100.
- **Observed:** The refusal named the conflicting graph and said that nothing changed.

### Member merge

**FA-12: MergeMembers does not report custom events and has no exclude list (medium)**
- **Type:** Limitation.
- **Scenario:** The source Blueprint has 5 functions, 2 custom events and 4 functions with no callers.
- **Reproduction status:** Observed once, in preview.
- **Steps to reproduce:**
  1. Call PreviewTurboMCPMergeMembers from the source to the target.
  2. Call PreviewTurboMCPGraphTransfer for a source function that calls a custom event.
- **Observed:** The merge preview listed 5 function declarations only. It did not list the custom events. The transfer preview then refused on a missing custom event. The merge preview also listed 4 functions that have no callers; the tool has no way to leave them out.
- **Expected:** Custom events are listed and can be declared. An exclude list skips members that the merge should drop.
- **Impact:** A merge with cleanup needs extra manual steps: create the event some other way, then delete the dead functions after the merge.
- **State after:** Nothing changed.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add custom events to the declaration set. Add an exclude parameter.

### Graph transfer

**GC-50: GraphTransfer cannot merge into an existing event graph (low)**
- **Type:** Limitation.
- **Scenario:** Transfer of an event graph into a target that already has an event graph.
- **Reproduction status:** Observed once, in preview.
- **Observed:** The tool refused the existing name. With a new graph name, the preview passed this check and moved on to dependency checks.
- **Expected:** An option to append the event chains into the existing event graph.
- **Impact:** Low. A second event graph page works.
- **Workaround:** Use a new graph name. The preview accepted it.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add an append mode for event graphs.

**GC-51: GraphTransfer dependency checks are clear, and the preview does not list the planned nodes (low)**
- **Type:** Usability.
- **Scenario:** Three transfer previews with missing members and one with a non-empty target function.
- **Reproduction status:** Observed four times.
- **Observed:** Each refusal named the missing variable, widget or function, or said that the target function has logic. This was useful. No preview reached the point of listing nodes, so it is not known whether a passing preview lists them.
- **Expected:** A passing preview lists the nodes, links and locals it will create.
- **Impact:** Unknown.
- **Workaround:** None needed yet.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Confirm that a passing preview gives a node list.

### Widget tree

**WT-11: WidgetSubtreeCopy refuses on an inner widget name collision (medium)**
- **Type:** Limitation.
- **Scenario:** Copy of a subtree whose inner panel has the same generated name as a panel in the target.
- **Reproduction status:** Observed once, in preview.
- **Observed:** The copy of the top widget refused on the collision. A copy of a deeper widget in the same subtree passed: 8 widgets, with names, properties, slots and localized text preserved.
- **Expected:** An option to rename colliding inner widgets automatically.
- **Impact:** The outer containers must be rebuilt by hand, or the copy starts deeper than intended.
- **Workaround:** Start the copy at a deeper widget. The preview accepted it.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add an auto-rename option and report the new names.

### Locals and dispatchers

**VA-08: LocalVariableAdd and DispatcherAdd previews were clear (positive)**
- **Scenario:** One int local in a function, and one dispatcher with a bool and an object input.
- **Observed:** Both previews used the shared type syntax and said what is not done.

### Tool help

**TH-13: Several new tools have no parameter descriptions (low)**
- **Type:** Usability.
- **Scenario:** Help reads for the new tools.
- **Reproduction status:** Observed six times.
- **Observed:** The help for GraphTransfer, WidgetSubtreeCopy, MergeMembers, DispatcherAdd, InterfaceRemove and MenuActivate lists parameter names with no description. The expected value formats were guessed. The confirm and target formats are not known.
- **Expected:** Each parameter has a description and an example, as in the older tools.
- **Impact:** Trial calls are needed.
- **Workaround:** Guessing. It worked for the four tools that were tried.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add descriptions and examples.

## 3. Recurrence

- X-100 follows X-99 (report 2026-10-26_S2). The interface add from FT-102 now exists. The new blocker is the function rename.
- WT-11 follows WT-10 and FT-103 (report 2026-10-26_S2). The subtree copy now exists, with the collision limit.
- FA-12 follows FT-105 (report 2026-10-26_S2).
- GC-50 and GC-51 follow GC-49 and FT-104 (report 2026-10-26_S2).

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-107 | Rename a function or graph and update its call sites in other Blueprints, with a preview of affected nodes | The blocked interface add; a manual editor rename |
| FT-108 | Exclude list and custom-event support in MergeMembers | Manual event creation and deletion of dead functions after the merge |
| FT-109 | Auto-rename option in WidgetSubtreeCopy | Manual rebuild of the outer containers |

## 5. Suggested priority

1. FT-107 and X-100: the only blocker for the merge.
2. FA-12 and FT-108: the transfer fails without the custom events.
3. WT-11 and FT-109.
4. TH-13, GC-50 and GC-51.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-100, FA-11, FA-12, GC-50, GC-51, WT-11, VA-08, TH-13, FT-107 to FT-109. MenuActivate and InterfaceRemove were not called. The engine version and client version were not observed. The report was read back. Local lint passed. The glossary is in the archive folder and was not updated. Automated checking does not prove confidentiality.
