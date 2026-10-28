# TurboMCP finding and tidying in large graphs session report

**Report ID:** 2026-10-27_S3
**Date:** 2026-10-27
**Review state:** Pending human review
**Plugin version:** Local install; exact commit not recorded in this session
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** One large widget event graph (about 390 nodes, 29 comment boxes), two small event graphs (about 24 nodes each), and several menu widgets searched for rows and handlers.
**Context:** Editor not in play mode during edits. Perforce source control in use.
**Tools used:** ReadTurboMCPWidgetTree, ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPNodeResolve, ReadTurboMCPGraphLoad, ReadTurboMCPBlueprintLoad, ReadTurboMCPCommentList, ReadTurboMCPDirtyList, ReadTurboMCPToolHelp, EditTurboMCPGraphAppend, EditTurboMCPPinDefault, EditTurboMCPGraphTidy, PreviewTurboMCPGraphTidy, EditTurboMCPGraphPositions, EditTurboMCPComment, PreviewTurboMCPAssetUndo, EditTurboMCPAssetUndo, LookTurboMCPLayoutDoneCheck, LookTurboMCPCapture, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPVcsCheckout. Other tools: the engine object property reader and writer.
**Outcome:** Two rows hidden, one option fix added, three graphs laid out. All assets compiled clean and saved. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-107: No tool reports node positions (medium)**
- **Type:** Limitation.
- **Scenario:** Nodes had to be placed by hand next to an existing node in a large graph.
- **Reproduction status:** Observed several times.
- **Observed:** GraphLoad, NodeContext, NodeResolve and the layout check do not print node positions. Comment boxes have rectangles, but nodes do not. Positions were estimated from region captures by converting pixels to graph units.
- **Expected:** A read of a node's position and size.
- **Impact:** Two extra captures and manual coordinate math per placement.
- **Workaround:** Region captures plus manual coordinates. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-120.

**X-108: Layout check and comment list overflow on a large graph (low)**
- **Type:** Usability.
- **Scenario:** A graph with about 390 nodes and 29 comment boxes.
- **Reproduction status:** Observed three times.
- **Observed:** The layout check output was cut off before the overlap list, so the overlaps caused by a new node could not be identified. The full-graph capture failed because the frame exceeded the zoom range. The comment list was cut off before the boxes of interest.
- **Expected:** A filter by node IDs, box, or region.
- **Impact:** Extra calls; the overlap check had to be done visually.
- **Workaround:** Region captures. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-121.

## 2. Tool observations

### Layout

**TD-44: Scoped tidy in a large graph made the layout worse while reporting improvement (medium)**
- **Type:** Defect.
- **Scenario:** Five nodes that sat thousands of units from their linked nodes, in a large graph with many comment boxes.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Tidy only those five nodes, with force off.
- **Observed:** The result said "longer wires; structural quality improved". Total wire length grew by about 30,000 units. Captures showed both groups still spread over thousands of units.
- **Expected:** Nodes placed next to the nodes they connect to, or a rollback.
- **Impact:** One undo and a manual placement.
- **State after:** Undone with AssetUndo; the graph returned to its saved state.
- **Workaround:** Manual GraphPositions from region captures. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Weight the distance to linked fixed nodes strongly for small scopes.

**TD-45: MainPath cannot start from a fixed node outside the scope (low)**
- **Type:** Limitation.
- **Scenario:** A chain of six new nodes after one existing node.
- **Reproduction status:** Observed once.
- **Observed:** A MainPath that included the fixed predecessor was refused: "MainPath requires distinct execution nodes within the selected scope".
- **Expected:** A way to lay a new chain in a row right after a fixed node.
- **Impact:** Manual positions instead.
- **Workaround:** GraphPositions. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Allow a fixed anchor node at the start of MainPath.

**TD-46: Whole-graph tidy of small event graphs worked well (positive)**
- **Scenario:** Two small event graphs messed up by spliced debug nodes.
- **Observed:** Whole-graph tidy removed all backward wires and overlaps. The layout check found no issues in one graph and only unrelated disconnected nodes in the other.

### Graph append

**GC-66: Append places spliced nodes far from their source in existing graphs (medium)**
- **Type:** Usability.
- **Scenario:** Single nodes or short chains spliced after an existing node.
- **Reproduction status:** Observed in four graphs.
- **Observed:** New nodes were placed 1,800 to 9,700 units from the node they follow, sometimes in a diagonal. Each case needed a later layout pass.
- **Expected:** New nodes placed beside the node they follow.
- **Impact:** Extra tidy or manual placement per edit.
- **Workaround:** Whole-graph tidy for small graphs; manual positions for large graphs.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Place spliced nodes to the right of their exec source by default.

**GC-67: A quoted struct literal in append text was stored with broken escaping (medium)**
- **Type:** Defect.
- **Scenario:** A gameplay tag struct pin set inline in append text with escaped quotes.
- **Reproduction status:** Observed once.
- **Observed:** Append reported the value written, but the pin held a tag name made of backslashes. The compile did not flag it.
- **Expected:** The struct value stored as written, or a refusal.
- **Impact:** Would have saved a wrong tag silently if not read back.
- **State after:** Fixed with PinDefault, which verified the value.
- **Workaround:** Leave the pin out of append, then set it with PinDefault. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Read back struct pin values after append and report a mismatch.

### Comments and undo

**CB-08: Comment set resized a box and kept its title (positive)**
- **Scenario:** A comment box widened to contain a new chain.
- **Observed:** The size changed and the title stayed, with an empty text argument.

**X-109: AssetUndo with a preview GUID cleanly reverted a bad tidy (positive)**
- **Scenario:** Undo of the TD-44 tidy.
- **Observed:** The preview named the transaction; the undo restored the saved state and cleared the dirty flag.

## 3. Recurrence

- TD-44 relates to TD-42 (2026-10-26_S9) and TD-43 (2026-10-27_S2): scoped tidy places nodes far from their consumers or rolls back.
- GC-66 relates to the far placements seen in 2026-10-26_S9.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-120 | Read node position and size by node ID | Region captures and pixel-to-graph conversion |
| FT-121 | Layout check and comment list filtered by node IDs, box, or region | Cut-off output on a large graph and visual overlap checks |
| FT-122 | Place nodes in a row after a given node | Manual coordinates for a six-node chain |

## 5. Suggested priority

1. GC-67: a silent wrong value in a saved asset.
2. TD-44 and GC-66: frequent layout rework after edits.
3. X-107, FT-120, FT-122: manual placement cost.
4. X-108, FT-121, TD-45: friction on large graphs.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs: X-107, X-108, X-109, TD-44, TD-45, TD-46, GC-66, GC-67, CB-08, FT-120, FT-121, FT-122. Missing evidence: plugin commit, engine revision and client version. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
