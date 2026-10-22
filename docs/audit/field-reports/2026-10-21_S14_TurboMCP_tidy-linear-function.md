# TurboMCP tidy on a linear function session report

**Report ID:** 2026-10-21_S14
**Date:** 2026-10-21
**Review state:** Pending human review
**Plugin version:** Version with the fixes for S10 to S12. The exact commit is Unknown.
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One widget Blueprint function graph with 38 nodes, one long exec chain, one latent node with a completion lane, and one note box that holds no node.
**Context:** Not in play. Source control in use. No submit.
**Tools used:** ReadTurboMCPAssetSearch, ReadTurboMCPBlueprintLoad, PreviewTurboMCPGraphTidy, EditTurboMCPGraphTidy, EditTurboMCPCompile, LookTurboMCPLayoutReport, LookTurboMCPCapture.
**Outcome:** Tidy applied in one edit. Compile clean. Not saved; the user reviews first. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-85: Tidy worked in one preview and one edit (positive)**
- **Type:** Positive.
- **Observed:** The whole task took 7 TurboMCP calls. The measured length matched the preview (33279 to 26107). Overlaps went to zero. Only one exec finding stayed.

## 2. Tool observations

### Tidy

**TD-31: One very long row for a long chain (low)**
- **Type:** Usability.
- **Observed:** The result is one row about 10100 units wide and 470 units tall. It is straight, but it needs much horizontal scroll.
- **Proposed behavior:** An option to wrap a long chain into rows at a natural break, for example after a latent node.

**TD-32: Shared self reference gives a 3000-unit wire (low)**
- **Type:** Limitation.
- **Observed:** One self node near the start feeds a node far down the chain. The longest wire is 2997 units, and its target input is 278 units off level.
- **Expected:** One self copy per consumer, as in the user layout rules.
- **Proposed behavior:** Split shared self and getter nodes when a wire passes a set length.

### Comment boxes

**CB-03: A note box with no node is left behind (low)**
- **Type:** Limitation.
- **Observed:** The graph has a note box with no node in it. The tidy kept its corner. After the tidy, the box sits far to the left and below the chain, away from the part of the chain that the note is about.
- **Expected:** A note box moves with the nodes it describes, or the tool reports that the box is now far from the chain.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** List empty boxes in the preview and ask which node to anchor each one to.

### Capture

**CP-14: Graph looks wrong in the live editor after a TurboMCP edit (low)**
- **Type:** Defect.
- **Scenario:** The live graph editor, right after a tidy or position edit on a graph with a comment box. Also seen on compact pure nodes.
- **Reproduction status:** Observed by the user, sometimes. The exact conditions are not known.
- **Observed:** After the TurboMCP edit, the comment box body is a light, almost opaque grey. Its title text has a heavy dark shadow and looks bold and blurred. Compact pure nodes show a hard dark band around the body, like a second outline. The user describes it as a node refresh glitch that over-contrasts the drawn drop shadows. After the user makes one manual edit in the graph, the box goes back to its normal darker, see-through look and the text is sharp.
- **Expected:** The graph looks the same after a TurboMCP edit as after a manual edit.
- **Impact:** The user cannot judge the result until the graph redraws. It can look like the edit changed the box colour.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Refresh the open graph view in the same way as a manual edit does, after each layout edit.

### User reference layout

**TD-33: Measured gap between the tidy and the user's final layout (data)**
- **Type:** Reference data for the tidy planner.
- **Scenario:** The user started from the tidy result and finished the graph by hand.
- **Observed:** The same graph, measured with the layout report:

| Measure | Tidy result | User final |
| --- | --- | --- |
| Nodes | 38 | 40 |
| Comment boxes | 1 | 0 |
| Width | 10130 | 8122 |
| Height | 474 | 342 |
| Total wire length | 26107 | 16925 |
| Longest wire | 2997 | 1152 |

- **What the user changed:** The user added two self reference copies, so each consumer has its own copy. The user removed the stranded note box. The user moved the nodes closer together. Inputs sit just below the main row, and the main row has no separate lanes.
- **Expected:** The tidy result is close to the user's final layout. The user's layout is 35 percent shorter in wire length and 20 percent narrower.
- **Proposed behavior:** Use this pair as a test case. Split shared self and getter nodes (TD-32) and use tighter horizontal spacing for a linear chain.

**LR-19: Layout report finds 38 issues on the accepted layout (low)**
- **Type:** Usability.
- **Observed:** On the user's final layout, the report gave 10 exec-not-straight, 26 pin-not-level and 1 overlap. Most offsets were 16 to 56 units. The user accepted this layout.
- **Expected:** An accepted layout gives few or no findings.
- **Proposed behavior:** A tolerance for small offsets, as LR-18 also proposes. Use the user's layout as the calibration case.

## 3. Recurrence

- TD-32 is like TD-24 and the self-copy rule from earlier reports.
- CB-03 is like LR-17: a box with no or few nodes is not handled.

## 4. Features that would have helped

None observed.

## 5. Suggested priority

1. TD-32, because it breaks a user layout rule.
2. CB-03.
3. TD-31.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-85, TD-31, TD-32, CB-03, CP-14, TD-33, LR-19. CP-14, TD-33 and LR-19 come from the user review. The report was read back. The local lint was run. Review state stays Pending human review.
