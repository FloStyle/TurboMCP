<!-- Imported field evidence and design proposals; not executed instructions or verified results. -->

# TurboMCP tidy scenario report: loop cascade, three compared outputs and clean-room recipe

**Report ID:** 2026-11-05_S1
**Date:** 2026-11-05
**Review state:** Pending human review
**Plugin version:** Unknown
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app; version Unknown
**Test case:** One Blueprint function library. One function graph with 94 nodes at the start, one entry, two loops in sequence, one loop nested in the second loop, one guard branch with a fallback path, and an execution merge before the return. No comment boxes at the start. At the end: 117 nodes, including 11 reroutes, and 6 comment boxes.
**Context:** Editor idle, no play session. Source control in use. The asset was a local writable copy. Checkout was refused because another user held an exclusive lock. Saves wrote the file locally.
**Tools used:** PreviewTurboMCPGraphTidy, ReadTurboMCPToolHelp, ReadTurboMCPToolIndex, ReadTurboMCPBlueprintLoad, ReadTurboMCPGraphLoad, ReadTurboMCPCommentList, ReadTurboMCPNodeResolve, ReadTurboMCPDirtyList, PreviewTurboMCPGraphSplitValue, PreviewTurboMCPNodeType, PreviewTurboMCPGraphAppend, EditTurboMCPGraphAppend, EditTurboMCPLocalVariableAdd, LookTurboMCPGraphPositions, EditTurboMCPGraphPositions, PreviewTurboMCPGraphKnots, EditTurboMCPGraphKnots, EditTurboMCPComment, LookTurboMCPLayoutDoneCheck, LookTurboMCPCapture, EditTurboMCPCompile, EditTurboMCPSave. A local script computed the positions for the agent layout. The user then edited the graph by hand in the editor.
**Outcome:** Three layouts of the same graph were produced and measured: the tidy default, an agent layout and the user's hand layout. The user accepted the hand layout as the reference. Compile clean. Saved locally. No crash.
**Session limits:** The tidy default was only previewed, never applied, so its crossing count was not measured. The clean-room fixture in Appendix F is fully specified but was not built or run, so its type IDs and pin names are unverified. Runtime behavior was not tested.
**Correction of:** None. This report merges the two earlier drafts of this session into one report, at the user's request. It is the only report for this session.

## How to use this report

This report is for an agent that rebuilds the scenario and improves the tidy tool. Read it in this order:
1. Appendix A: the graph structure, block by block, in generic terms.
2. Appendix B: the three outputs (tidy default, agent layout, user reference), with measures.
3. Appendix C: layout rules R1 to R9 taken from the user's hand edit. This is the target behavior.
4. Section 2: the items: defects, a rejected hypothesis, proposals and positive observations.
5. Appendix D: a proposed tidy design that would produce the reference.
6. Appendix E: machine-checkable acceptance criteria.
7. Appendix F: the complete build specification of the clean-room fixture: signature, locals, node table with links, build order, block map, scrambled start state, baseline and reference. Not built yet.
8. Appendix H: the scoring specification.
9. Appendix I: the proposed tool interface.
10. Appendix J: regression scenarios that protect current behavior.
11. Appendix K: the work loop for a coding agent.
12. Appendix L: known gaps and limitations, with the action that closes each gap.
13. Appendix G: how the session went, step by step, including the hypothesis the user rejected.

Everything needed is in this report. The fixture runs on a stock Unreal Editor with the TurboMCP plugin and its MCP server. No other file is needed.

## 1. Cross-cutting issues

**X-129: Compact tool help hides required parameters (low)**
- **Type:** Usability.
- **Scenario:** The client reads the compact help of a tool before its first call.
- **Reproduction status:** Observed on four tools in this session: the tidy edit and preview tools, the graph position reader, the node type search and the capture tool.
- **Steps to reproduce:**
  1. Call ReadTurboMCPToolHelp for EditTurboMCPGraphTidy.
  2. Call PreviewTurboMCPGraphTidy with only the parameters that the help shows.
- **Observed:** The compact help showed 5 of 9 parameters and said it was shortened. Three required parameters (the node list, the main path and the wrap points) were in the hidden part. The first call was refused for a missing required parameter. The same thing happened with the other three tools.
- **Expected:** Compact help always lists every required parameter, even when it shortens descriptions.
- **Impact:** One failed call per tool before the first useful result.
- **State after:** No change to the asset.
- **Workaround:** Read the schema in the refusal message and call again. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Print required parameters first. Shorten only the optional ones.

## 2. Tool observations

### 2.1 Tidy

**TD-62: Tidy spreads loop continuations as a staircase of rows across the full width (medium)**
- **Type:** Defect.
- **Scenario:** Appendix A. Two loops in sequence, a nested loop, a fallback that rejoins at a merge node.
- **Reproduction status:** Observed once in preview.
- **Steps to reproduce:**
  1. Call PreviewTurboMCPGraphTidy on the whole graph: empty node list, empty main path, empty wrap points, bForce false, default gaps.
  2. Open the before and after preview images.
- **Observed:** Each loop body and the branch True path stayed on the row of their control node. Each Completed path and the False path moved one row lower and far to the right. The real flow went down in a staircase over three rows that spanned the whole width, joined by long diagonal exec wires. The user marked three crossing exec paths on the image and rejected the layout.
- **Expected:** Rules R2 to R6 in Appendix C. Keeping the body on the row is correct. The defect is the global placement: blocks are not compact rectangles, Completed blocks are not placed below their loop, and handoff wires are diagonal.
- **Impact:** The layout could not be accepted. One agent pass and one user hand pass were needed.
- **State after:** Preview only. No change to the asset.
- **Workaround:** Exact positions with EditTurboMCPGraphPositions, then a hand correction by the user. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-149 and FT-150.

**TD-69: Rejected hypothesis: Completed on the main row (medium)**
- **Type:** Proposal, withdrawn.
- **Scenario:** Same as TD-62.
- **Reproduction status:** Tested once as the agent layout (output B in Appendix B).
- **Steps to reproduce:**
  1. Place every loop's Completed path on one straight main row and the loop bodies in lanes below.
  2. Show the result to the user.
- **Observed:** The agent first read the TD-62 crossings as "the wrong output stays on the row" and built output B on that idea. The user rated it better than the default but corrected it by hand. The user's reference keeps the body on the row and drops Completed below.
- **Expected:** An agent that improves the tool must not implement "Completed on the main row." FT-145 is withdrawn for the same reason.
- **Impact:** One full layout pass built on a wrong rule.
- **State after:** Superseded by the user reference.
- **Workaround:** Not applicable.
- **Cause:** The agent inferred the rule from the crossings without a reference layout.
- **Proposed behavior:** Before changing a layout rule, compare against a user reference layout. Appendix F gives a way to build one.

**TD-63: Tidy reports no exec drift for a staircase flow (medium)**
- **Type:** Defect.
- **Scenario:** Same preview as TD-62.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Preview the tidy as in TD-62.
  2. Read the quality summary.
- **Observed:** Linear exec drift went from 208 to 0, with zero backward exec wires. The image showed the flow dropping about two rows, with long diagonal wires.
- **Expected:** A flow that changes rows outside a deliberate block handoff counts as drift. Crossings between exec wires count as a finding.
- **Impact:** A summary that showed only improvement for a rejected layout.
- **State after:** Preview only.
- **Workaround:** Visual review of the preview image. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-154.

**TD-64: Tidy accepts a layout with much worse input pin drift (low)**
- **Type:** Limitation.
- **Scenario:** Same preview as TD-62.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Preview the tidy as in TD-62.
- **Observed:** Input pin drift went from 8912 to 13442, about 51 percent worse. The preview said the edit would apply.
- **Expected:** A large regression in any measure gives at least a warning line.
- **Impact:** Unknown.
- **State after:** Preview only.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Warn when any measure gets worse by more than a set ratio.

**TD-65: Tidy routes a fallback exit diagonally across the graph to the merge node (medium)**
- **Type:** Limitation.
- **Scenario:** The guard branch False path rejoins the flow at a node with two exec inputs near the end.
- **Reproduction status:** Observed in all three outputs.
- **Steps to reproduce:**
  1. Preview the tidy as in TD-62.
- **Observed:** The fallback block stayed near the branch. Its exit wire crossed the whole graph diagonally. In output B the knot tool fixed it with one reroute. In output C the user routed it along the bottom and up through one reroute.
- **Expected:** Rule R6.
- **Impact:** One long crossing wire per layout.
- **State after:** Fixed by hand in output C.
- **Workaround:** Knot tool, then a hand move. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-151.

**TD-70: Tidy does not build compact blocks for control-flow regions (medium)**
- **Type:** Limitation.
- **Scenario:** Appendix A. 8 blocks, 3 loop levels, 1 fallback.
- **Reproduction status:** Observed once in preview.
- **Steps to reproduce:**
  1. Preview the tidy as in TD-62.
  2. Compare block positions with the box rectangles in Appendix B.3.
- **Observed:** Nodes of one block were spread across rows used by other blocks. No block had its own rectangle.
- **Expected:** Rules R1, R4 and R5.
- **Impact:** The user moved 5 boxes and their contents by hand.
- **State after:** Preview only.
- **Workaround:** Manual placement. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-149 and FT-150.

**TD-71: Tidy does not route long wires orthogonally (medium)**
- **Type:** Limitation.
- **Scenario:** Exec handoffs between blocks, the fallback exit, and data wires blocked by a node.
- **Reproduction status:** Observed in all three outputs.
- **Steps to reproduce:**
  1. Lay out the graph with tidy or with exact positions.
  2. Run PreviewTurboMCPGraphKnots, then EditTurboMCPGraphKnots.
- **Observed:** Tidy adds no reroutes. The knot tool placed 4 reroutes and skipped wires whose nodes sit in neighboring columns. The user added 7 more: 2 for exec drops at loop Completed outputs, 2 for a data wire routed over a branch, 2 for a handle wire routed under a setter, and 1 for a loop element wire.
- **Expected:** Rule R8.
- **Impact:** 7 reroutes placed by hand.
- **State after:** Reroutes saved.
- **Workaround:** Manual reroutes in the editor. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-151.

**TD-66: Tidy has no option to cache reused values in locals (medium)**
- **Type:** Proposal.
- **Scenario:** One pure call output feeds three consumers across two loop levels. One impure call output feeds three consumers at the far end, including the fallback.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Preview the tidy and read the getter-split count.
  2. Call PreviewTurboMCPGraphSplitValue on the pure call.
- **Observed:** The tidy split 0 getters and kept the long data wires. The split tool refused the pure call because it accepts only getters and literals.
- **Expected:** Rule R7.
- **Impact:** 2 locals, 2 setters and 6 getters created by the agent with one append.
- **State after:** Compiled clean.
- **Workaround:** EditTurboMCPLocalVariableAdd, then EditTurboMCPGraphAppend with explicit rewiring. This worked.
- **Cause:** Not applicable.
- **Proposed behavior:** FT-152.

**TD-72: Tidy and split tools ignore loop index outputs used by several consumers (medium)**
- **Type:** Limitation.
- **Scenario:** The nested loop's index feeds 3 consumers in different columns of its body's condition tree.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Lay out output B and look at the loop index wires.
- **Observed:** Three long index wires remained in output B. The user added a local, one setter as the first node of the loop body, and 3 getters.
- **Expected:** Rule R7, including loop outputs.
- **Impact:** One local, one setter and 3 getters added by hand.
- **State after:** Saved.
- **Workaround:** Hand edit. This worked.
- **Cause:** Not applicable.
- **Proposed behavior:** FT-152.

**TD-67: Tidy does not create comment boxes (low)**
- **Type:** Proposal.
- **Scenario:** No boxes at the start. Clear sections exist.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Preview the tidy as in TD-62.
- **Observed:** 0 boxes fitted. The tidy only fits existing boxes.
- **Expected:** Rule R1, with nesting as in R5.
- **Impact:** 6 boxes created by the agent. The user then nested one box in another.
- **State after:** Boxes saved.
- **Workaround:** EditTurboMCPComment create with explicit node lists. This worked.
- **Cause:** Not applicable.
- **Proposed behavior:** FT-153.

**TD-73: Tidy and layout measures do not rank layouts the way the user does (medium)**
- **Type:** Defect.
- **Scenario:** Appendix B.4.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Compare the tidy summary for output A with the user's verdict.
  2. Compare the done check measures for outputs B and C.
- **Observed:** Output A reported improvement on every exec measure and was rejected. Output C has more cross-box wires than B (9 against 8) and was accepted.
- **Expected:** The measures rank C above B, and B above A.
- **Impact:** An agent cannot tell improvement from regression with the current measures.
- **State after:** Not applicable.
- **Workaround:** Visual review by the user. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-154.

**TD-68: Tidy preview images are too small to read (low)**
- **Type:** Usability.
- **Scenario:** A preview of a graph with about 94 nodes.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Preview the tidy and open the before and after images.
- **Observed:** Both images were at a very small zoom. Node titles could not be read.
- **Expected:** The preview can show each block at a readable zoom.
- **Impact:** The user annotated an overview image by hand to explain the problem.
- **State after:** No change.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** FT-155.

**TD-74: Exact batch positions are a reliable base for scripted layouts (positive)**
- **Scenario:** 102 nodes moved in one call from a script-computed plan.
- **Observed:** One undo step. All 102 nodes moved. Later partial batches moved only the listed nodes. Positions plus sizes from LookTurboMCPGraphPositions were enough for an overlap-free plan on the first try.

### 2.2 Look and knot tools

**LR-21: The done check repeats intended lane drops as defects (low)**
- **Type:** Usability.
- **Scenario:** Output B.
- **Reproduction status:** Observed twice.
- **Steps to reproduce:**
  1. Run LookTurboMCPLayoutDoneCheck on output B.
- **Observed:** One backward data wire and one getter under its consumer were found. Both were real and were fixed. Each intended loop lane drop was also listed as exec-not-straight, 8 times.
- **Expected:** Intended drops at loop body and branch outputs are listed apart from defects.
- **Impact:** A long findings list.
- **State after:** No change.
- **Workaround:** Manual triage. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See LR-22.

**LR-22: The done check flags intended structure in the accepted layout (low)**
- **Type:** Usability.
- **Scenario:** Output C, which the user accepted.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Run LookTurboMCPLayoutDoneCheck on output C.
- **Observed:** 53 findings: 41 pin-not-level, 5 exec-not-straight, 9 cross-box wires and 5 sparse-box warnings. All exec-not-straight findings and all cross-box wires are deliberate block handoffs through reroutes (R3, R6). The sparse warnings come from parent boxes that hold nested blocks.
- **Expected:** Findings are classed as intended (by rules R2 to R6) or as defects. Only defects go into the main count. A parent box counts its child boxes as filled area.
- **Impact:** An agent that treats findings as defects would undo the user's layout.
- **State after:** No change.
- **Workaround:** Manual triage. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add a finding class field: intended or defect.

**KN-02: The knot pass lowered crossings after a manual layout (positive)**
- **Scenario:** Output B.
- **Observed:** The preview explained each skipped wire. The edit placed 4 reroutes and reported crossings down from 34 to 13. It keeps a pass only when the count goes down.

**KN-03: The user kept all knot-tool reroutes (positive)**
- **Scenario:** Output C.
- **Observed:** The user moved the 4 tool reroutes but deleted none. The user added 7 more of the same kind (TD-71).

### 2.3 Edit, comment and save tools

**ED-25: Append with explicit rewiring of existing inputs worked (positive)**
- **Scenario:** 2 setters inserted into the exec chain and 6 getters wired to existing inputs, in one call.
- **Observed:** The preview listed all 11 wires and the 8 new nodes. The edit made all 11 wires and read them back. Compile clean.

**ED-26: Local declaration plus append is a complete cached-value pattern (positive)**
- **Scenario:** Two calls to EditTurboMCPLocalVariableAdd (an integer and an engine struct), then one append.
- **Observed:** Both declarations worked on the first try with a plain struct path. Compile clean after the append.

**CB-12: Comment creation from an explicit node list worked (positive)**
- **Scenario:** 6 boxes created around node lists of 6 to 37 nodes.
- **Observed:** Each box enclosed its nodes and reported its rectangle. One refit after a node move correctly reported that nothing needed fitting.

**SV-16: Save reported a refused checkout clearly (positive)**
- **Scenario:** A save of a package that another user held with an exclusive lock.
- **Observed:** The package was written. The provider refusal was reported word for word. The report said that the checkout does not gate the write.

## 3. Recurrence

- TD-62, TD-70 relate to TD-49 (a lane placed far from its target).
- TD-63, TD-73 recur TD-44 (layout worse while the summary reports improvement). Changed condition: whole graph, not a scoped selection.
- TD-66, TD-72 recur TD-35 (shared values not split) and TD-36 (long wires from the entry). New: pure call outputs, impure call outputs and loop index outputs.
- TD-67 relates to TD-54 (comment boxes in scoped tidy). Changed condition: no boxes at the start.
- TD-69 withdraws FT-145.
- LR-22 recurs LR-21.

## 4. Features that would have helped

FT-145 (Completed on the main row) is withdrawn by TD-69. FT-146 to FT-148 are replaced by FT-151 to FT-153 and are not listed again.

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-149 | Tidy block tree: split the exec flow into blocks for loops, branches and sequences | The agent's scripted layout and the user's hand pass that moved 110 nodes |
| FT-150 | Cascade block placement with nested Completed blocks (R2 to R6) | The user's move of 5 boxes and their contents |
| FT-151 | Orthogonal routing with reroutes for exec handoffs, merge exits and blocked data wires | 7 hand-placed reroutes and 1 knot pass |
| FT-152 | Local promotion for reused values, including loop index outputs, with a candidate list in the preview | 3 locals, 3 setters and 9 getters created by append or by hand |
| FT-153 | Box creation for each block, nested like the block tree | 6 box create calls and 1 hand nesting |
| FT-154 | New quality measures: unrelated wires crossing boxes, diagonal length, row breaks inside a block, bounding area, and a regression warning | Visual review to reject output A and to rank B and C |
| FT-155 | Per-block readable preview images and a block table | A hand annotation on an unreadable overview image |
| FT-156 | A scenario replay harness: build a fixture from graph text, scramble it with a seed, run tidy and score the result | Manual comparison of three outputs in this session |

## 5. Suggested priority

1. FT-149 and FT-150 (TD-62, TD-70). Block cascade placement. This is most of the gap between the default and the reference.
2. FT-154 (TD-63, TD-64, TD-73). The measures must rank layouts the way the user does. Otherwise an agent cannot tell improvement from regression.
3. FT-156. The replay harness, so an agent can repeat this scenario without the proprietary graph.
4. FT-151 (TD-65, TD-71). Orthogonal routing.
5. FT-152 (TD-66, TD-72). Local promotion, including loop indexes.
6. FT-153, FT-155, LR-22, X-129. Boxes, preview images, finding classes and help text.

## 6. Completeness and privacy review

Every tool used is listed. Item IDs: X-129, TD-62 to TD-74, LR-21, LR-22, KN-02, KN-03, ED-25, ED-26, CB-12, SV-16 and FT-149 to FT-156. FT-145 to FT-148 were allocated in an earlier draft of this report and stay allocated; FT-145 is withdrawn and FT-146 to FT-148 are replaced as described in section 4. Recurrence links use report-format IDs only. Missing evidence: the crossing count of output A on the original graph, any build of the fixture in Appendix F, and a user-approved reference for the fixture. Appendix L lists each gap and the action that closes it. Local lint passed. The report was read back. Review state stays Pending human review. Automated checking does not prove confidentiality.

## Appendix A: Scenario structure

The function has one entry and one return. "Pure tree" means the side-effect-free nodes that feed one exec node. Node counts are from the original graph before any edit.

1. **Setup block.** Entry, one call that creates a mesh attribute handle, then a guard branch. Pure trees: an array length of a function input, and an integer compare with the literal 2 that feeds the branch. 7 nodes. After the edits it also holds two local setters (a count cache and the handle cache).
2. **Loop 1 (for-each over an input vector array).** An array clear before the loop. Body: one array add. Its pure tree transforms each element with a function-input transform, breaks it, and makes a vector with Z set to 0. 11 nodes with getters.
3. **Mesh read block.** Two calls that read the mesh vertices and convert them to an array, then one array clear. 5 nodes.
4. **Loop 2 (for-each over the vertex array).** Body: a setter that resets a running minimum to a large value, then loop 3.
5. **Loop 3 (indexed for-loop from 0 to count minus 1, nested in loop 2).** Body: a branch on "candidate squared distance is less than the running minimum," then one setter on True. The condition tree has about 30 pure nodes: vector break and make, a closest-point-on-segment query, two array reads, an index wrap-around (index plus 1, or 0 at the last index), and squared 2D distance math. The loop index feeds 3 consumers in different columns.
6. **Falloff block (loop 3 Completed, still inside the loop 2 body).** One array add of a weight. Its pure tree of 14 nodes takes the square root of the running minimum, applies a clamped map range between two function inputs, then a smoothstep polynomial.
7. **Write and finish block (loop 2 Completed).** One conversion call and one attribute write, then the final mesh operation and the return. The final operation takes two option structs with pure trees of about 12 nodes. Its exec input is a merge: two sources.
8. **Fallback block (guard branch False).** Two calls that select mesh elements and write the attribute from the selection. Its exit goes to the merge node in block 7.

Values reused across blocks, which cause long data wires:
- The array length: 3 consumers, in blocks 1 and 5.
- The attribute handle: 3 consumers, in blocks 7 and 8.
- The loop 3 index: 3 consumers in the block 5 condition tree.
- A function input (the target mesh), used in blocks 1, 3, 7 and 8. Separate input getters already existed for each consumer.

## Appendix B: The three outputs

### B.1 Output A: tidy default (preview only)

- **Call:** PreviewTurboMCPGraphTidy, whole graph, no main path, no wrap points, bForce false, default gaps (horizontal 120, vertical 100, lane pitch 280).
- **Reported measures:** overlaps 0 to 0. Linear exec drift 208 to 0. Fan-out row conflicts 0 to 0. Backward exec 0 to 0. Backward data 2 to 2. Pure input row conflicts 0 to 0. Input pin drift 8912 to 13442. Total wire length 118930 to 109833 across 120 wires. 94 nodes would move, 0 boxes fitted, 0 getters split. Every node had a measured size.
- **Shape:** see TD-62.
- **User verdict:** rejected.

### B.2 Output B: agent layout

- **Rules used:** one straight main row from entry to return with every loop Completed on it, loop bodies in lanes below, the fallback at the bottom. This rule set was wrong (TD-69).
- **Structural edits:** 2 locals (count cache and handle cache), 2 setters and 6 getters. 4 reroutes from the knot tool. 6 comment boxes, none nested.
- **Measures (LookTurboMCPLayoutDoneCheck):** 106 nodes, 128 wires, total wire length 88039, longest wire 10348, 42 straight-segment crossings, 8 cross-box wires totaling 28653 units. Bounds about 14455 wide and 3052 tall.
- **User verdict:** "better, but several problems remain."

### B.3 Output C: user reference (hand edit of output B)

- **Rules seen:** Appendix C.
- **Structural edits beyond output B:** 1 local for the loop 3 index, 1 setter as the first node of the loop 3 body, 3 getters. 7 more reroutes, 11 in total. One box nested in another.
- **Measures (LookTurboMCPLayoutDoneCheck):** 117 nodes, 137 wires, total wire length 76539, longest wire 6160, 15 straight-segment crossings, 9 cross-box wires totaling 20831 units. Bounds about 10689 wide and 4715 tall. 110 of the 112 nodes shared with output B moved.
- **Box rectangles in graph units (x, y, width, height):**
  - Setup: -304, -80, 2432, 432.
  - Loop 1 and mesh read: 2210, -82, 2160, 1040.
  - Loop 2 with loop 3 body: 4448, 496, 5552, 2416.
  - Falloff, nested in the loop 2 box: 7104, 1840, 1664, 794.
  - Write and finish: 6928, 2992, 3457, 805.
  - Fallback: 2272, 4176, 1396, 457.
- **Exec anchors (top-left of the node, graph units):** entry -250, 7. Guard branch 1872, 0. Loop 1 2680, 0, with its body add at 4128, 16 (same row). Mesh read calls at 3104, 576 and 3499, 576 (below the loop 1 body). Loop 2 at 4488, 580. Its body setter at 4928, 592 and loop 3 at 5607, 580 (same row). Loop 3 body: index setter 5895, 596, branch 9038, 584, minimum setter 9406, 600 (same row). Loop 3 Completed reroute at 6368, 2032, then the falloff add at 8593, 2014 (below). Loop 2 Completed reroute at 5024, 3152, then the write block from 7140, 3076 to the return at 10208, 3076 (below). Fallback calls at 2565, 4260 and 3345, 4260. Fallback exit reroute at 9504, 4352, then up into the merge node at 9820, 3076.

### B.4 Comparison

| Measure | A tidy default | B agent | C user reference |
| --- | --- | --- | --- |
| Total wire length | 109833 (preview) | 88039 | 76539 |
| Longest wire | Unknown | 10348 | 6160 |
| Straight-segment crossings | Unknown | 42 | 15 |
| Cross-box wires | No boxes | 8 | 9 |
| Locals added | 0 | 2 | 3 |
| Reroutes | 0 | 4 | 11 |
| Boxes | 0 | 6, flat | 6, 1 nested |
| Width x height | Unknown | 14455 x 3052 | 10689 x 4715 |
| Accepted by user | No | No | Yes |

C has more cross-box wires than B and was still accepted. In C each cross-box wire is a deliberate handoff between blocks through reroutes. Cross-box count alone is not a quality measure.

## Appendix C: Target layout rules

R1. **Blocks.** Each control-flow region is a block with its own comment box: setup, each loop with its body, each Completed continuation, and each branch path.

R2. **Loop body continues on the row.** The body of a loop continues to the right on the loop node's row. This matches the current tidy rule "first output stays on the row." Branch True also stays on the row.

R3. **Completed drops below.** The Completed block starts below the body block's rectangle. Its exec wire drops vertically through a reroute just after the loop node, then runs horizontally to the next block.

R4. **Cascade.** Blocks form a diagonal staircase from top left to bottom right. Each lower block shifts right only as far as needed. There is no single long main row.

R5. **Nesting.** The Completed block of an inner loop sits inside the outer loop's block and box, because it runs inside the outer loop body.

R6. **Fallback lane.** The branch False block sits below all other blocks, under its branch. Its exit runs along the bottom, then up into the merge node through a reroute.

R7. **Locals for reused values.** A value used in another block, or by several far-apart consumers, goes in a local variable: one setter after the producer, one getter next to each consumer. This includes a loop index used by several consumers. Its setter is the first node of the loop body.

R8. **Orthogonal wires.** Long wires, exec and data, are horizontal and vertical segments joined by reroutes. Two reroutes route a data wire around a node that blocks it.

R9. **Pure trees.** Pure nodes sit in columns to the left of their consumer, inside the consumer's block, with the output pin level with the target pin.

## Appendix D: Proposed tidy design

This is a design suggestion only.

**Phase 1: block tree.** Split the exec graph into blocks. A block is a maximal straight exec run. Loops make Body and Completed children. Branches make True and False children. Sequences make one child per output. Each pure node goes to the block of its earliest consumer in exec order.

**Phase 2: local promotion (opt-in).** List values whose consumers sit in more than one block, or whose estimated wire is longer than a threshold. Include loop index and element outputs. For each accepted value, add a local, a setter after the producer (or as the first node of the loop body for a loop output), and one getter per consumer. Show the candidates in the preview with consumer count and wire length. Suggested parameter: a promotion mode of off, list or a list of value IDs.

**Phase 3: layout inside each block.** Exec nodes on one row. Pure trees in columns to the left of their consumer, output pin level with the target pin. Work from the innermost block outward, so each block has a known rectangle and known entry and exit pins.

**Phase 4: cascade placement.** A loop Body or branch True block continues right on the parent row. A loop Completed block goes below the body rectangle, starting near the loop node's x. A branch False block goes below all placed blocks, under its branch. An inner-loop Completed block stays inside the outer block's rectangle. Keep a routing channel of about 60 units between rectangles.

**Phase 5: orthogonal routing.** Exec handoff to a lower block: a reroute under the source pin, a vertical drop, then a horizontal run. Exit to a merge node: along the bottom edge, then up in the channel left of the merge node. Data wire blocked by a node: one reroute level with the source and one level with the target.

**Phase 6: boxes (opt-in).** One box per block, nested like the block tree, with a placeholder title from the control node and output name. Keep the title of an existing box when most of its nodes belong to one block.

**Phase 7: measures and acceptance.** Score with FT-154: unrelated wires crossing boxes (heavy weight), diagonal segment length, row breaks inside a block, wire crossings, then bounding area. Warn or refuse when any measure gets worse by more than a set ratio. Treat block handoffs through reroutes as intended, not as drift.

**Phase 8: preview.** One overview image, one readable image per block, a block table (kind, node count, rectangle, parent), the local candidates and the before and after measures.

## Appendix E: Acceptance criteria for an automated check

An implementation passes the scenario when all of these hold on the clean-room fixture:
1. Each loop Body block's first exec node is on its loop node's row, within 16 units in y.
2. Each loop Completed block's top edge is below the bottom edge of the Body block of the same loop.
3. Each inner-loop Completed block lies inside the outer loop block's rectangle.
4. No wire segment crosses a box that holds neither of its endpoints.
5. No exec wire longer than 800 units has a diagonal segment longer than 200 units.
6. Each value with consumers in two or more blocks is offered as a local candidate, or promoted when promotion is on. This includes a loop index with 3 or more consumers.
7. One box per block, nested like the block tree, with no partial overlap between boxes.
8. Total wire length is at most 1.10 times the reference. Straight-segment crossings are at most the reference count plus 5.
9. The tool's own summary ranks the reference above output B, and output B above output A.

These criteria come from one user reference. The thresholds are suggestions.

## Appendix F: Clean-room fixture, complete build specification (not built)

The fixture is a graph with the same structure as Appendix A. It uses stock engine nodes only and neutral names. It runs on the standard pipeline: a stock Unreal Editor with the TurboMCP plugin and its MCP server. Nothing in this appendix was built or run in this session. Type IDs and pin names in the tables come from this session's reader output on the original graph. Before the first build, check each one with PreviewTurboMCPNodeType and PreviewTurboMCPNodePins. Reader IDs and creation IDs can differ.

### F.1 Prerequisites

- A stock Unreal Editor 5.8 project with the TurboMCP plugin enabled and its MCP server running. The editor is idle, with no play session.
- A test content folder that is never added to source control.
- Check the toolsets first with ReadTurboMCPStatus and ReadTurboMCPToolIndex. For every tool, read the full schema with describe_toolset before the first call. Compact help can hide required parameters (X-129).

### F.2 Asset and function

1. Preview, then create one Blueprint function library in the test folder with PreviewTurboMCPFunctionLibraryCreate and EditTurboMCPFunctionLibraryCreate. Name it TidyFixture_LoopCascade.
2. Create one function with EditTurboMCPFunctionAdd. Name it LoopCascade.

| Kind | Name | Type |
| --- | --- | --- |
| Input | PathPoints | array of vector |
| Input | PathTransform | transform |
| Input | SamplePoints | array of vector |
| Input | InnerDistance | float |
| Input | FalloffDistance | float |
| Input | FallbackWeight | float |
| Input | Scale | float |
| Output | Weights | array of float |

3. Declare locals with EditTurboMCPLocalVariableAdd, one call each, with an empty default.

| Local | Type | Used in |
| --- | --- | --- |
| FlatPoints | array of vector | Loop 1, loop 3 |
| WorkWeights | array of float | Read, falloff, fallback, return |
| BestDistSq | float | Read, loop 2, loop 3, falloff |
| ResultScale | float | Setup, finish |

Do not declare PointCount or CurrentIndex in the fixture. The tool under test must propose them (criterion E.6).

4. Compile with EditTurboMCPCompile.

### F.3 Node table

Labels N01 to N92 are local labels for this report. When you build with EditTurboMCPGraphAppend, put each label on its node, and record the node ID that the edit returns for each label. The scoring script uses that label-to-ID map. Gaps in the numbering are deliberate.

Type ID patterns:
- Variable getter and setter: Variable|(fixture class path)|Get|(name) and Variable|(fixture class path)|Set|(name).
- Function input getter: discover with PreviewTurboMCPNodeType. Use one getter node per consumer.
- Math and array nodes: as listed. The reader showed them in the form "Utilities|Operators|Add" and "Function|/Script/Engine.KismetMathLibrary:BreakVector."

Setup block (S):

| Label | Node | Inputs and links |
| --- | --- | --- |
| N01 | Function entry | Exists already |
| N03 | Input getter PathPoints | |
| N02 | Utilities, Array, Length | TargetArray from N03 |
| N05 | KismetSystemLibrary MakeLiteralInt | Value 2 |
| N04 | Operators GreaterEqual, integer | A from N02 ReturnValue, B from N05 |
| N08 | Input getter Scale | |
| N07 | Set ResultScale | execute from N01 then; ResultScale from N08 |
| N06 | FlowControl Branch | execute from N07 then; Condition from N04 |

Loop 1 block (L1) and its body (L1B):

| Label | Node | Inputs and links |
| --- | --- | --- |
| N11 | Get FlatPoints | |
| N10 | Array Clear | execute from N06 then (True); TargetArray from N11 |
| N13 | Input getter PathPoints | |
| N12 | Array ForEachLoop | Exec from N10 then; Array from N13 |
| N20 | Input getter PathTransform | |
| N19 | KismetMathLibrary TransformLocation | T from N20; Location from N12 Array Element |
| N17 | KismetMathLibrary BreakVector | InVec from N19 ReturnValue |
| N18 | KismetMathLibrary BreakVector | InVec from N19 ReturnValue |
| N16 | KismetMathLibrary MakeVector | X from N17 X; Y from N18 Y; Z 0 |
| N15 | Get FlatPoints | |
| N14 | Array Add | execute from N12 LoopBody; TargetArray from N15; NewItem from N16 |

Read block (R), on loop 1 Completed:

| Label | Node | Inputs and links |
| --- | --- | --- |
| N23 | Set BestDistSq | execute from N12 Completed; value 0 |
| N22 | Get WorkWeights | |
| N21 | Array Clear | execute from N23 then; TargetArray from N22 |

Loop 2 block (L2) and its body (L2B):

| Label | Node | Inputs and links |
| --- | --- | --- |
| N25 | Input getter SamplePoints | |
| N24 | Array ForEachLoop | Exec from N21 then; Array from N25 |
| N26 | Set BestDistSq | execute from N24 LoopBody; value 1e12 |
| N29 | MakeLiteralInt | Value 1 |
| N28 | Operators Subtract, integer | A from N02 ReturnValue (second consumer of the shared length); B from N29 |
| N27 | FlowControl ForLoop | execute from N26 then; FirstIndex 0; LastIndex from N28 |

Loop 3 body (L3B), on N27 LoopBody:

| Label | Node | Inputs and links |
| --- | --- | --- |
| N35 | BreakVector | InVec from N24 Array Element |
| N36 | BreakVector | InVec from N24 Array Element |
| N34 | MakeVector | X from N35 X; Y from N36 Y; Z 0 |
| N38 | Get FlatPoints | |
| N37 | Array Get (a copy) | Array from N38; Dimension 1 from N27 Index |
| N44 | MakeLiteralInt | Value 1 |
| N43 | Operators Add, integer | A from N27 Index; B from N44 |
| N48 | MakeLiteralInt | Value 1 |
| N47 | Operators Add, integer | A from N27 Index; B from N48 |
| N46 | Operators Equal, integer | A from N47; B from N02 ReturnValue (third consumer of the shared length) |
| N45 | MakeLiteralInt | Value 0 |
| N42 | Utilities Select | Option 0 from N43; Option 1 from N45; Index from N46 |
| N41 | Get FlatPoints | |
| N39 | Array Get (a copy) | Array from N41; Dimension 1 from N42 |
| N49 | KismetMathLibrary FindClosestPointOnSegment | Point from N34; SegmentStart from N37; SegmentEnd from N39 |
| N50 | BreakVector | InVec from N34 |
| N51 | BreakVector | InVec from N49 |
| N52 | Operators Subtract, double | A from N50 X; B from N51 X |
| N53 | Operators Multiply, double | A from N52; B from N52 |
| N54 | Operators Subtract, double | A from N50 Y; B from N51 Y |
| N55 | Operators Multiply, double | A from N54; B from N54 |
| N40 | Operators Add, double | A from N53; B from N55 |
| N32 | Get BestDistSq | |
| N31 | Operators Less, double | A from N40; B from N32 |
| N30 | FlowControl Branch | execute from N27 LoopBody; Condition from N31 |
| N33 | Set BestDistSq | execute from N30 then (True); value from N40 |

The loop index N27 Index has 3 consumers: N37, N43 and N47. This is the loop-index case for R7.

Falloff block (F), on N27 Completed:

| Label | Node | Inputs and links |
| --- | --- | --- |
| N66 | Get BestDistSq | |
| N65 | KismetMathLibrary Sqrt | A from N66 |
| N67 | Input getter InnerDistance | |
| N69 | Input getter InnerDistance | |
| N70 | Input getter FalloffDistance | |
| N68 | Operators Add, double | A from N69; B from N70 |
| N64 | KismetMathLibrary MapRangeClamped | Value from N65; InRangeA from N67; InRangeB from N68; OutRangeA 0; OutRangeB 1 |
| N59 | Operators Multiply, double | A from N64; B from N64 |
| N61 | MakeLiteralDouble | Value 3 |
| N63 | MakeLiteralDouble | Value 2 |
| N62 | Operators Multiply, double | A from N63; B from N64 |
| N60 | Operators Subtract, double | A from N61; B from N62 |
| N58 | Operators Multiply, double | A from N59; B from N60 |
| N57 | Get WorkWeights | |
| N56 | Array Add | execute from N27 Completed; TargetArray from N57; NewItem from N58 |

Finish block (E), on N24 Completed:

| Label | Node | Inputs and links |
| --- | --- | --- |
| N85 | Input getter FallbackWeight | |
| N78 | Operators Multiply, double | A from N07 output value (first far consumer of the setter output); B from N85 |
| N74 | MakeLiteralDouble | Value 0 |
| N75 | MakeLiteralDouble | Value 1 |
| N73 | KismetMathLibrary FClamp | Value from N78; Min from N74; Max from N75 |
| N71 | Set ResultScale | execute from N24 Completed; value from N73 |
| N81 | Get WorkWeights | |
| N80 | Function result (return) | execute from N71 then, and from N88 then (merge); Weights from N81 |

Fallback block (X), on N06 else:

| Label | Node | Inputs and links |
| --- | --- | --- |
| N87 | Get WorkWeights | |
| N86 | Array Clear | execute from N06 else; TargetArray from N87 |
| N91 | Input getter FallbackWeight | |
| N90 | Operators Multiply, double | A from N91; B from N07 output value (second far consumer of the setter output) |
| N89 | Get WorkWeights | |
| N88 | Array Add | execute from N86 then; TargetArray from N89; NewItem from N90 |

Totals: about 85 nodes, 1 entry, 1 return with a two-source exec merge, 3 loops (2 for-each, 1 for-loop nested in a for-each body), 2 branches, and 3 reused values: N02 (3 consumers in two blocks), N07 output (2 consumers in two blocks far from the producer) and N27 Index (3 consumers). No reroutes, no boxes.

If the setter output pin of N07 cannot be linked, use one getter of ResultScale per consumer instead. Then the reused-value case for N07 is lost. Note that in the scenario record.

### F.4 Build order and checks

1. Append one block per EditTurboMCPGraphAppend call, in the order S, L1, L1B, R, L2, L2B, L3B, F, E, X. Run PreviewTurboMCPGraphAppend first each time. Use placement local.
2. To link an exec input that already has a source, such as N80, write the second link on the existing node form. Then read the node back with ReadTurboMCPNodeResolve and confirm that both sources are present. An explicit link can replace an existing one.
3. After each block, run EditTurboMCPCompile. Stop on any error.
4. At the end, check the structure with ReadTurboMCPGraphLoad at detail bodies. Expected: one entry point, no unreachable nodes, the exec merge reported on N80, and the 3 loops in the nesting order above.
5. Save with EditTurboMCPSave.

### F.5 Block map

The scoring script needs this map. Replace labels with the recorded node IDs.

| Block | Kind | Parent | Control node and output | Exec nodes in order |
| --- | --- | --- | --- | --- |
| S | setup | none | entry | N01, N07, N06 |
| L1 | straight | S | N06 then | N10, N12 |
| L1B | loop body | L1 | N12 LoopBody | N14 |
| R | loop completed | L1 | N12 Completed | N23, N21, N24 |
| L2B | loop body | R | N24 LoopBody | N26, N27 |
| L3B | loop body | L2B | N27 LoopBody | N30, N33 |
| F | loop completed (inner) | L2B | N27 Completed | N56 |
| E | loop completed | R | N24 Completed | N71, N80 |
| X | branch false | S | N06 else | N86, N88 |

Each pure node belongs to the block of its earliest consumer in exec order. Expected boxes: S, L1 with L1B, R, L2B with L3B and F nested, E, X. Box F must be inside box L2B (rule R5).

### F.6 Scrambled start state

1. Read all node IDs and sizes with LookTurboMCPGraphPositions, empty node list, maxNodes 256.
2. Make positions with a linear congruential generator: seed 20261105, multiplier 1103515245, increment 12345, modulus 2 to the 31st. Take nodes sorted by label. For each node take two draws: x is draw modulo 9000, y is draw modulo 3000. Then remove overlaps: sort by x, and move each node down by its height plus 40 until it overlaps nothing.
3. Apply all positions in one EditTurboMCPGraphPositions call. Compile and save. Record the full position list in the scenario record. This is the start state.

### F.7 Baseline (output A on the fixture)

1. Run PreviewTurboMCPGraphTidy: whole graph (empty node list), empty main path, empty wrap points, bForce false, default gaps. Record the reported measures and images.
2. Apply with EditTurboMCPGraphTidy, same options. If it refuses or rolls back, record the refusal text as the baseline result.
3. Run LookTurboMCPLayoutDoneCheck and LookTurboMCPGraphPositions. Record positions, wires, box rectangles and findings.
4. Score with Appendix H. Expected: the baseline fails criteria E.2 to E.7.

### F.8 Reference (output C equivalent on the fixture)

1. Start from the scrambled state.
2. Add PointCount (integer) with a setter after N06 on the True path, before N10, fed by N02. Replace the 3 links from N02 with 3 getters. Add CurrentIndex (integer) with a setter as the first node of L3B, fed by N27 Index. Replace the 3 index links with 3 getters. Use EditTurboMCPLocalVariableAdd, then EditTurboMCPGraphAppend.
3. Lay out every block as a compact rectangle (rule R9 inside each block). Then place the blocks:

| Block | x of the first exec node | y of the exec row |
| --- | --- | --- |
| S | 0 | 0 |
| L1 | right edge of S plus 120 | 0 |
| L1B | right edge of N12 plus the width of its pure tree plus 120 | 0 (same row as N12) |
| R | x of N12 plus 120 | bottom of the L1 and L1B rectangle plus 200 |
| L2B | right edge of N24 plus 120 | same row as N24 |
| L3B | right edge of N27 plus its pure tree width plus 120 | same row as N27 |
| F | x of N27 plus 400 | bottom of L3B plus 200, inside the L2B box |
| E | x of N24 plus 400 | bottom of the L2B box plus 200 |
| X | x of N06 plus 400 | bottom of all other boxes plus 300 |

4. Add reroutes. One under N12 Completed, one under N27 Completed and one under N24 Completed, each with a vertical drop and then a horizontal run. One at the bottom of X for its exit, under the x of N80 minus 100, then up into N80. One or two for each data wire that crosses a node. Run EditTurboMCPGraphKnots first, then append the rest.
5. Create one box per block with EditTurboMCPComment, with titles "Setup", "Loop 1", "Read", "Loop 2 body", "Closest segment", "Falloff", "Finish" and "Fallback". Make the Falloff and Closest segment boxes lie inside the Loop 2 body box.
6. Compile and save. Score with Appendix H. Expected: all criteria pass except E.8 and E.9, which need this reference as their input.
7. A user must review the reference in the editor and correct it by hand before it is stored. The rules in Appendix C leave room for judgment. Until a user approves a reference, use only criteria E.1 to E.7 as the gate.

## Appendix H: Scoring specification

Inputs, all read with stock TurboMCP tools:
- Node positions and sizes: LookTurboMCPGraphPositions.
- Wires with pin endpoints and wire lengths: LookTurboMCPLayoutReport for the whole graph. Reroutes are nodes; a wire through reroutes is a chain of segments.
- Box rectangles and membership: ReadTurboMCPCommentList.
- The label-to-ID map from F.3 and the block map from F.5.

Definitions:
- A segment is one straight line between two pin positions, or between a pin and a reroute.
- A segment is diagonal when its angle is more than 10 degrees from both horizontal and vertical.
- An unrelated box crossing is a segment that crosses a box rectangle holding neither of its endpoint nodes.
- A row break is an exec link inside one block whose target exec pin y differs from the source exec pin y by more than 16 units.
- A block rectangle is the union of its exec nodes, its pure nodes and its reroutes.

Measures, in priority order (lexicographic):
1. Unrelated box crossings (count).
2. Exec-exec segment crossings (count).
3. Diagonal exec length (sum of the lengths of diagonal exec segments).
4. Row breaks inside blocks (count).
5. Data segment crossings (count).
6. Total wire length.
7. Bounding area.

Ranking: compare layouts measure by measure, in that order. A difference smaller than 5 percent, or smaller than 1 for counts, counts as a tie, and the next measure decides.

Regression warning: report any measure that gets worse by more than 10 percent, even when the overall ranking improves.

Criteria E.1 to E.9 are checked as written in Appendix E, with these definitions. The output is one pass or fail per criterion, the seven measures, and the ranking of baseline, candidate and reference.

## Appendix I: Proposed tool interface

This is a design suggestion only. The current default behavior must stay available until the regression scenarios in Appendix J pass.

New optional parameters for PreviewTurboMCPGraphTidy and EditTurboMCPGraphTidy:

| Parameter | Values | Default | Effect |
| --- | --- | --- | --- |
| layoutMode | rows, blocks | rows | blocks turns on phases 1, 3, 4 and 7 of Appendix D |
| routeWires | off, orthogonal | off | orthogonal turns on phase 5 and may add reroutes |
| createBoxes | false, true | false | true turns on phase 6 |
| promoteLocals | off, list, or a comma-separated list of value IDs | off | list only reports candidates; IDs promote those values |
| channelWidth | number from 20 to 400 | 60 | Gap between block rectangles for routing |

Rules for the interface:
- The preview and the edit reach the same plan from the same inputs. The preview reports the block table, the local candidates, the reroutes and boxes it would add, and the before and after measures from Appendix H.
- Moves, new reroutes and new boxes go in one undo transaction.
- Local promotion changes the graph's structure and requires a compile. Do it as a separate step, either in a separate call or as the first step with its own compile, before the layout. If promotion fails, no layout change is applied.
- Every finding in the result carries a class: defect, or intended with the rule number (R2 to R6). This also fixes LR-21 and LR-22.
- A result that is worse on any measure by more than the regression ratio is refused unless bForce is true. The refusal names the measure.

## Appendix J: Regression scenarios

These protect behavior that works today. Each one is small, so it can be built with one append call. All were described, not built.

| ID | Graph | Expected after tidy in blocks mode |
| --- | --- | --- |
| J1 | Event graph: one start event, then 3 exec calls in a row with one getter each | One straight row. Same node order. No reroutes. No boxes unless asked. Wire length not more than 5 percent above the current default result. |
| J2 | Event graph: one Sequence with 3 outputs, 2 exec calls on each | The first output stays on the row. The other outputs are stacked below in order, as 3 blocks. No crossing exec wires. |
| J3 | Function: for-each with a Break input wired back from inside the body | No refusal. The Break wire is kept as feedback, not treated as a cycle. Body on the row, Completed below. |
| J4 | Function: a graph that already has 2 titled comment boxes | Box titles are kept. Boxes are fitted around their blocks. No box becomes empty. |
| J5 | Scoped tidy: select one block of the main fixture only | Unselected nodes do not move. The selected block becomes a compact rectangle next to its connections. |
| J6 | Two event graphs side by side with 3 entry points each | Each entry point and its chain is laid out as its own block. Entry chains do not interleave. |

Earlier reports cover related behavior: TD-46 and TD-48 (small graphs that tidy well today), TD-51 and TD-53 (loop break feedback), TD-54 (boxes in scoped tidy) and TD-57 (interface graphs).

## Appendix K: Work loop for a coding agent

1. Build the fixture (F.2 to F.4), the start state (F.6) and the baseline (F.7). Store everything in a scenario record outside the plugin's source.
2. Build the reference (F.8). Stop and ask a user to review and correct it. Store the approved reference.
3. Build J1 to J6 and record their current default results.
4. Change the tool. After each change:
   - Restore the fixture start state from the recorded positions.
   - Run the tidy preview and edit with layoutMode blocks and the other new options.
   - Score with Appendix H.
   - Run J1 to J6 and compare with their recorded results.
5. Keep a change only when the fixture score improves, no criterion that passed before now fails, and J1 to J6 still pass.
6. When E.1 to E.9 all pass, ask the user to check the result visually. Report it with a new session report.

Known limits of this loop:
- The fixture is one structure. A pass does not prove that other graph shapes work. J1 to J6 reduce, but do not remove, that risk.
- The reference depends on one user's judgment.
- Measures use straight segments, not rendered curves.
- Every step needs a running editor with the MCP server. There is no offline mode.

## Appendix L: Known gaps and limitations

This report is a complete specification. Nobody has built or tested it. The table gives each gap and the action that closes it. Close gaps 1 to 4 in the first build session. Gap 5 needs one review pass by a user.

| No. | Gap | Effect | Action that closes it |
| --- | --- | --- | --- |
| 1 | Nobody has built the fixture in Appendix F. | The fixture can contain errors that only a build shows. | Do steps F.2 to F.4. Compile after each block. Record each error and its correction in the scenario record. |
| 2 | Type IDs and pin names come from reader output, not from creation. Creation IDs can differ. | An append can refuse a node or a pin. | Before the first append, look up each node kind with PreviewTurboMCPNodeType and each pin with PreviewTurboMCPNodePins. Use the returned values. |
| 3 | The link from the output value of setter N07 to two far consumers is not tested. | If the link fails, the fixture loses one reused-value case. | Try the link in the E block append. If it fails, use one getter per consumer and record that the case is lost (F.3). |
| 4 | Nobody has applied the default tidy to the fixture. | There are no baseline numbers. | Do step F.7. Record the measures, the positions and any refusal text. |
| 5 | No approved reference exists for the fixture. Rules R1 to R9 leave room for judgment. | Criteria E.8 and E.9 cannot operate. | Do step F.8. Then stop and ask a user to examine and correct the reference in the editor. Until a user approves it, use only criteria E.1 to E.7. |
| 6 | The scoring script is a specification only. | Codex must write the script. | Write the script from Appendix H. Then make sure that the baseline fails criteria E.2 to E.7 and the approved reference passes all criteria. |
| 7 | The thresholds come from one reference layout: 16 units for a row, 10 degrees for a diagonal, 5 percent for a tie and 10 percent for a regression. | The limits can be too strict or too loose for other graphs. | Keep them as defaults. After J1 to J6 run, adjust a threshold only when a user rejects a result that passes, or accepts a result that fails. Record each change. |
| 8 | There is one main scenario. J1 to J6 are small, and nobody has built them. | A change can fit this fixture and break other graph shapes. | Build J1 to J6 before the first tool change. Record their current results. Run them after each change (Appendix K). |
| 9 | There is no offline mode. Each step needs a running editor with the MCP server. | An agent without editor access cannot do the work loop. | Give the agent access to a stock editor with the TurboMCP plugin and its MCP server. Without this access, the agent can only write code and cannot test it. |
| 10 | The privacy rules prevent images in this report. | The agent cannot see the user reference of the original graph. | Use the scores as the main check. Use LookTurboMCPCapture on the fixture for visual review. A user makes the final visual check (Appendix K, step 6). |
| 11 | The original graph had a mesh object. The fixture uses plain arrays. | Node sizes and pin counts differ from the original. Absolute positions from Appendix B.3 do not transfer. | Use the relative placement table in F.8, not the absolute coordinates of Appendix B.3. |
| 12 | The crossing count of the default tidy on the original graph is unknown. It was only previewed. | Appendix B.4 has gaps for output A. | No action is possible without the original graph. The fixture baseline (gap 4) replaces it. |

## Appendix G: Session timeline

1. The asset was taken from a shelved change into a local writable copy. Normal unshelve was refused because of an exclusive lock held by another user.
2. The editor was started. The tidy preview (output A) was run after one failed call caused by X-129.
3. The user annotated the preview image with three crossing paths and asked about crossings, locals and boxes.
4. The agent proposed the Completed-on-main-row rule (TD-69) and built output B: 2 locals, about 100 nodes placed by script, 4 reroutes, 6 boxes. Compile clean, saved.
5. The user corrected output B by hand into output C.
6. The agent compared B and C node by node and extracted rules R1 to R9.
7. The agent wrote this report. The clean-room fixture was specified (Appendix F) but not built.
