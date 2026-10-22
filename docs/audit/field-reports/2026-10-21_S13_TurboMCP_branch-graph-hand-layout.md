# TurboMCP branch graph hand layout session report

**Report ID:** 2026-10-21_S13
**Date:** 2026-10-21
**Review state:** Pending human review
**Plugin version:** Version with the fixes for S10 to S12. The exact commit is Unknown.
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One widget Blueprint function graph with 54 nodes and no comment box. Two nested branches, one execution merge, and many single-use getters.
**Context:** Not in play. Source control in use. No submit.
**Tools used:** ReadTurboMCPNodeFind, ReadTurboMCPGraphLoad, PreviewTurboMCPGraphTidy, LookTurboMCPCapture, LookTurboMCPLayoutReport, EditTurboMCPGraphPositions, EditTurboMCPCompile.
**Outcome:** Layout done in memory with 3 position calls. Compile clean. Not saved; the user reviews first. No crash.
**Session limits:** The user did not review the result in this session.
**Correction of:** None

## 1. Cross-cutting issues

**X-84: Short asset names work everywhere (positive)**
- **Type:** Positive.
- **Observed:** Every tool accepted the short asset name. No path search was needed.

## 2. Tool observations

### Tidy

**TD-28: Tidy doubles wire length on a nested branch graph (medium)**
- **Type:** Limitation.
- **Scenario:** A graph with an outer branch, an inner branch, and an execution merge after the outer branch.
- **Reproduction status:** Observed once.
- **Observed:** The preview predicted 14391 to 27918 and rejected the change. The after image showed one long row with the branch lanes pushed far to the side. A hand layout reached 15480 with no exec staircase and 1 overlap.
- **Expected:** Branch lanes stacked under their branch, as in the user layout rules.
- **Impact:** The whole layout was done by hand. It needed 54 positions in one call.
- **Workaround:** Hand positions from the graph text and one capture. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Treat each branch output as a lane. Put the merge node on the main row.

**TD-29: The wire-length gate rejects the accepted layout (high)**
- **Type:** Defect in the acceptance rule.
- **Scenario:** The same graph as TD-28.
- **Reproduction status:** Observed once, by measurement.
- **Observed:** The original messy layout measured 14391. The hand layout that the user accepted measured 15480. The tidy accepts a result only when the wire length goes down. So the tidy gate would reject the layout the user wants, even if the planner made it.
- **Expected:** The gate scores what the user rules value: a straight main exec row, stacked lanes, no overlap, inputs level with their pins. Total wire length is a secondary score.
- **Impact:** No change to the planner can pass the gate on graphs like this one.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Score in this order: overlaps, exec rows straight, backward wires, inputs level, then wire length. Accept a result that improves the first four, even with longer wires.

**TD-30: Reference recipe from the accepted hand layout (proposal)**
- **Type:** Proposal.
- **Observed:** The hand layout used these steps. The user said the result was not too bad.
  1. Put the entry and every exec node of the main path on one row.
  2. For each branch, keep the first output on the main row. Put the other output lane one band below the main row data inputs.
  3. Put a merge target on the main row, after both lanes.
  4. Put each pure input in a column to the left of its consumer, level with the target pin. Nested inputs go one more column to the left.
  5. Leave about 120 to 200 units between exec nodes, and more when the inputs need columns.
  6. Size each band by its tallest data input cluster, not by a fixed pitch.
- **Proposed behavior:** Use this recipe as the default for graphs with no comment box.

### Capture

**CP-13: Native graph capture is a good check (positive)**
- **Observed:** The graph capture returned a new native mode at the requested size. Node text was readable at that size. CP-11 did not occur again.

### Layout report

**LR-18: Pin-level findings are too noisy after a hand layout (low)**
- **Type:** Usability.
- **Observed:** After the hand layout, the report gave 34 pin-not-level findings. Most were offsets of 10 to 46 units. On the capture these looked level enough. The real fault, one overlap, was easy to miss among them.
- **Proposed behavior:** A tolerance value, or sort the findings by size.

## 3. Recurrence

- TD-28 is like TD-27. A tidy rejects a graph without a usable layout.
- FT-88 still applies: hand positions need pixel estimates from a capture.

## 4. Features that would have helped

None observed beyond FT-88.

## 5. Suggested priority

1. TD-29, because the gate blocks the accepted result.
2. TD-28 and TD-30, because branch graphs are common.
3. LR-18.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-84, TD-28, TD-29, TD-30, CP-13, LR-18. The report was read back. The local lint was run. Review state stays Pending human review.
