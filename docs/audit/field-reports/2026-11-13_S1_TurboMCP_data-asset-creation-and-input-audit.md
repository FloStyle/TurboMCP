# TurboMCP data asset creation and input audit session report

**Report ID:** 2026-11-13_S1
**Date:** 2026-11-13
**Review state:** Pending human review
**Plugin version:** Unknown (same install as 2026-11-12 S2)
**Engine version:** Unreal Engine 5, source build. Exact revision not recorded.
**Client:** Claude Code desktop app, through an MCP relay and a small HTTP helper for paged calls
**Test case:** About 300 input assets (input actions, input mapping contexts, user interface metadata data assets), a project-wide Blueprint search, 3 large Blueprints, about 650 widget Blueprints
**Context:** Editor open, no play session. Source control in use. Read only: no edit, compile or save in this session.
**Tools used:** ReadTurboMCPRefsBatch, ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPBlueprintLoad, ReadTurboMCPWidgetInventory, ReadTurboMCPDataAssetProperty, describe_toolset (schema reads of the read and edit toolsets). Other tools: an engine object property reader and the engine asset toolset listing.
**Outcome:** Audit complete. No compile, no save, no crash.
**Session limits:** The edit operations were read from the schema only. They were not run.
**Correction of:** None

## 1. Cross-cutting issues

**X-141: No tool creates a new data asset (medium)**
- **Type:** Limitation.
- **Scenario:** The plan needs 7 new data assets of one engine class (a subclass of the engine data asset class). Each one starts empty, then gets map entries.
- **Reproduction status:** Observed once, from the tool lists.
- **Steps to reproduce:**
  1. List the TurboMCP edit tools and the engine asset tools.
  2. Look for an operation that creates an asset of a given class at a content path.
- **Observed:** The TurboMCP edit toolset has asset undo, revert and delete, but no create. The engine asset toolset has duplicate, move and delete, but no create.
- **Expected:** One call creates an empty typed data asset (user expectation).
- **Impact:** Two routes only. Route 1: duplicate an existing asset, then remove each copied map entry. Copied entries can stay by mistake. Route 2: the user creates the assets by hand.
- **State after:** No change. Not run.
- **Workaround:** None used. The plan asks the user to choose a route.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-173.

**X-142: Bulk scans still skip assets with a map or redirector dependency (medium)**
- **Type:** Limitation.
- **Scenario:** A project-wide node search for 11 names, and a widget inventory by class.
- **Reproduction status:** Repeated (see X-138).
- **Steps to reproduce:**
  1. Run a project-scope node search with literals included, and follow every cursor.
  2. Run a widget inventory with one class rule on the project.
- **Observed:** The node search took 258 calls and scanned about 2830 assets. It skipped 1289 assets (map or redirector dependency). The widget inventory also skipped widgets for the same reason. A Blueprint read loaded 2 of the skipped assets without problem.
- **Expected:** The scan covers the skipped assets, or gives a second pass that loads them.
- **Impact:** The search result is not exhaustive. The reference search had to cover the gap.
- **State after:** No change.
- **Workaround:** A Blueprint read on each skipped asset that mattered. It worked for 2 assets. It does not scale.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** An option to load the skipped assets in the same paged scan.

## 2. Tool observations

### Node find

**NF-16: Text output needs parsing to follow the cursor (low)**
- **Type:** Usability.
- **Scenario:** A 258-page project scan from a script.
- **Reproduction status:** Repeated.
- **Steps to reproduce:**
  1. Call the node search from a script and follow the cursor.
- **Observed:** The result is text. The script had to find the next cursor and the result rows with pattern matching.
- **Expected:** A JSON format option, as other read tools have.
- **Impact:** Extra script work.
- **State after:** No change.
- **Workaround:** Pattern matching on the text. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** A format option with a JSON result.

**NF-17: Node search does not see references inside a mapping context (low)**
- **Type:** Limitation (scope note).
- **Scenario:** An input action is used only as the chord action of a trigger inside a mapping context.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Search for the action name in graphs.
- **Observed:** The search finds no use, because the use is in a data asset, not a graph. The reference search and a raw property read found it.
- **Expected:** This is the documented scope of the tool.
- **Impact:** The user can think that the action is unused.
- **State after:** No change.
- **Workaround:** Reference search. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** A note in the result: data asset references are outside the scope, use the reference search.

### Blueprint reader

**BL-21: Large Blueprint read refused, with a clear size (positive)**
- **Scenario:** A Blueprint with a full document of about 676000 characters.
- **Observed:** The read was refused at the default budget and at 400000. The refusal gave the required size. The read worked with a budget of 800000.

### Data asset property reader and writer

**DA-04: Reader accepts owned instanced child paths (positive)**
- **Scenario:** Read a list property of a configuration data asset in values mode.
- **Observed:** The read gave each array entry with its object reference and tag. The schema says that paths can go through owned instanced children. An earlier session saw a refusal for a subobject object path. See Recurrence.

**DA-05: Writer schema covers instanced children and map children (positive, not run)**
- **Scenario:** Schema read of the data asset property writer.
- **Observed:** The schema gives create_child for an instanced struct object on an input action, and for an instanced map value. It gives child_set for fields of an existing child. This matches the planned edits. The operations were not run in this session.

### References

**RF-16: Batch referencer search is reliable for dead-asset checks (positive)**
- **Scenario:** Referencer search with all kinds at depth 1 on 60 seeds, then on 22 seeds, then on 18 seeds.
- **Observed:** Each run paged without error. The seed records were easy to filter. The results matched later Blueprint reads.

### Widget inventory

**WT-24: Class rule finds the users of a widget class (positive)**
- **Scenario:** Find all widget Blueprints that contain one engine widget class.
- **Observed:** 43 calls gave 6 users. Skipped widgets are reported (see X-142).

## 3. Recurrence

- X-142 and X-138 (2026-11-12 S2): same skip reason, same count of skipped assets (1289). No change in conditions.
- DA-04 and earlier DA reader refusal of subobject paths (2026-11-12 S2 session notes, no item ID). The schema now documents child paths. The refused input was an object path, not a property path. The two observations do not prove a change in the tool.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-173 | Create a data asset: class, content path, optional initial fields. Mark it for add in source control. Return the path and the reader revision. Refuse an existing path. | 7 assets to create by duplicate and cleanup, or by hand |

## 5. Suggested priority

1. FT-173 / X-141: blocks a common step (new data assets) for a planned data integration.
2. X-142: makes bulk searches not exhaustive.
3. NF-16 and NF-17: friction and scope notes.

## 6. Completeness and privacy review

All tools used are listed. All 8 observations from the session have an item: X-141, X-142, NF-16, NF-17, BL-21, DA-04, DA-05, RF-16, WT-24, FT-173. The edit operations were not run, so DA-05 is a schema observation only. The report was read back. Local lint: see the result below. No asset, project or path name is in this report. Review state stays Pending human review.
