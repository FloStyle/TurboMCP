# TurboMCP bulk search and inventory reads session report

**Report ID:** 2026-11-12_S2
**Date:** 2026-11-12
**Review state:** Pending human review
**Plugin version:** Unknown. The user updated the plugin before the session. The revision was not recorded.
**Engine version:** 5.8.4, licensee source build (public revision not known)
**Client:** Claude Code desktop app
**Test case:** Read-only audit of one project: about 4100 assets for a project-wide node search, about 650 widget assets for a widget inventory, and about 320 texture assets for a reference walk.
**Context:** No play session. Source control in use. The editor was restarted after the plugin update.
**Tools used:** ReadTurboMCPNodeFind, ReadTurboMCPWidgetInventory, ReadTurboMCPRefsBatch, BlueprintLoad, GraphLoad. Other tools: a local HTTP helper script and a local JSON parser.
**Outcome:** Task done. No edit, no compile, no save. No crash.
**Session limits:** Part of the session was lost to context compaction. Exact call counts and durations were not recorded.
**Correction of:** None

## 1. Cross-cutting issues

**X-138: Bulk search tools skip about one third of the assets (medium)**
- **Type:** Limitation.
- **Scenario:** Project-wide node search and widget inventory on a project where many assets depend on a map or a redirector.
- **Reproduction status:** Repeated (project scope and widget folder scope).
- **Steps to reproduce:**
  1. Run a node search with project scope.
  2. Run a widget inventory on all widgets.
- **Observed:** Each tool skipped every asset with a map or redirector dependency. The node search skipped 1289 of 4117 assets. The widget inventory skipped 98 of 647 widgets. The skip list was returned, but the skipped assets were not searched. The single-asset loaders (BlueprintLoad, GraphLoad) loaded the same assets without error.
- **Expected:** User expectation: a project-wide search covers all assets that the single-asset loaders can load, or it explains why each asset was skipped.
- **Impact:** The results are not complete. A caller list or a user list cannot be stated as complete.
- **State after:** No change to assets.
- **Workaround:** Load each skipped asset with BlueprintLoad or GraphLoad. It worked for the few assets that were checked. It does not scale to about 1300 assets.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Search the skipped assets in a second, slower pass, or give an option to include them.

**X-139: Large paged results needed a client script (low)**
- **Type:** Usability.
- **Scenario:** Reference walk and widget inventory with many pages.
- **Reproduction status:** Repeated.
- **Steps to reproduce:**
  1. Run a paged read with many pages.
  2. Collect all pages into one result.
- **Observed:** The pages were too large to keep in the client context. The session wrote each page to a file with a local HTTP helper, then parsed the files. Some outputs had several JSON objects one after the other. A standard JSON load failed until the parser read one object at a time.
- **Expected:** User expectation: one valid JSON document per output, and a way to get a compact full result.
- **Impact:** Manual scripting for each bulk read.
- **State after:** No change to assets.
- **Workaround:** File output plus an incremental JSON parse. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-172.

**X-140: A schema error when the arguments wrapper was missing (low)**
- **Type:** Usability.
- **Scenario:** One node search call.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Send the search fields at the top level instead of inside the arguments object.
- **Observed:** The call failed with a schema error. With the wrapper, the call worked. This was a client error.
- **Expected:** The error is correct. A hint about the wrapper would help.
- **Impact:** One extra call.
- **State after:** No change.
- **Workaround:** Send the call again with the wrapper. It worked.
- **Cause:** Client call format.
- **Proposed behavior:** None.

## 2. Tool observations

### Blueprint reader

**BL-20: Several assets in one call exceed the default token budget (low)**
- **Type:** Usability.
- **Scenario:** BlueprintLoad on 6 assets in one call.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Load 6 assets in one BlueprintLoad call with the default budget.
- **Observed:** The output went over the budget. A second call with a large token budget worked, and the output was written to a file.
- **Expected:** User expectation: a clear note on how to split the call or raise the budget.
- **Impact:** One extra call.
- **State after:** No change to assets.
- **Workaround:** Raise the token budget. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** None.

### References

**RF-14: Seed records are mixed with results in the pages (low)**
- **Type:** Usability.
- **Scenario:** ReadTurboMCPRefsBatch on a folder of textures, paged with the cursor.
- **Reproduction status:** Repeated over many pages.
- **Steps to reproduce:**
  1. Run the batch reference read on a folder.
  2. Read the pages.
- **Observed:** The pages contained records for the input assets next to the reference records. The client had to filter them out.
- **Expected:** User expectation: seed records are marked clearly or can be turned off.
- **Impact:** Client-side filtering.
- **State after:** No change.
- **Workaround:** Filter by record kind in the client. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** An option to omit seed records.

**RF-15: Batch references with depth, kinds and paging worked (positive)**
- **Scenario:** Folder-wide reference walk.
- **Observed:** Depth, kind filters and cursor paging gave a complete list for the assets that were not skipped. It replaced about 320 single-asset calls.

### Node find

**NF-15: OR mode, count-only and literal search work (positive)**
- **Scenario:** Node search with the new options.
- **Observed:** Match mode any, count only and literal search gave the expected results. They close the gaps of NF-09 and NF-11 in this session.

### Widget inventory

**WT-23: Rule-based widget inventory gave a full user list fast (positive)**
- **Scenario:** Inventory of all widgets with rules on class, call and node class, with match mode any.
- **Observed:** One paged run gave the user list for each rule, except for the skipped widgets (X-138).

## 3. Recurrence

- NF-15 and NF-09 (2026-11-09 S2): the OR mode and the count-only mode now exist. Resolution observed.
- NF-15 and NF-11 (2026-11-09 S2): literal pin values are now searched. Resolution observed.
- X-138 and NF-12 (2026-11-10 S1): the search is no longer one asset only, but the new scopes skip assets.
- X-139 and X-132 (2026-11-10 S1): bulk reads still need a client script.
- X-140 and AS-08 (2026-11-10 S1): the same kind of wrapper error on another tool.
- BL-20 and BL-15, BL-19: budget behavior again. This time the output went over the budget. It was not a silent fallback.
- RF-15 and RF-10 (2026-11-10 S1): batch reads remove the one-asset-per-call cost.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-172 | A compact full-result mode for bulk reads: all pages in one file, one JSON document | Page-by-page file writes and an incremental JSON parse |

## 5. Suggested priority

1. X-138: the skip makes project-wide results incomplete, with no simple workaround.
2. FT-172 and X-139: bulk reads need scripting.
3. RF-14, BL-20, X-140: minor friction.

## 6. Completeness and privacy review

All tools used and all item IDs are in this report. Call counts and durations are missing. The report was read back. The local lint result is given to the user. Review state stays Pending human review. Automated checking does not prove confidentiality.
