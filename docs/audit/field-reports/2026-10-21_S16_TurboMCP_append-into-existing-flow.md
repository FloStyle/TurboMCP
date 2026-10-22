# TurboMCP append into an existing flow session report

**Report ID:** 2026-10-21_S16
**Date:** 2026-10-21
**Review state:** Pending human review
**Plugin version:** Version with the fixes for S10 to S12. The exact commit is Unknown.
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One widget Blueprint. A function graph with a switch on an enum and two lanes (19 nodes). A new third lane had to reuse the two existing end nodes. One event graph got a 4-node block.
**Context:** Not in play. Source control in use. No submit.
**Tools used:** ReadTurboMCPGraphLoad, ReadTurboMCPNodeFind, ReadTurboMCPNodeResolve, ReadTurboMCPToolHelp, ReadTurboMCPToolSchema, PreviewTurboMCPGraphAppend, EditTurboMCPGraphAppend, PreviewTurboMCPGraphPatch, EditTurboMCPGraphRepoint, EditTurboMCPGraphTidy, EditTurboMCPGraphPositions, EditTurboMCPCompile, EditTurboMCPSave, LookTurboMCPLayoutReport, LookTurboMCPCapture. Engine graph tools for connect and delete.
**Outcome:** Final graph correct and laid out, compile clean, saved. The first result was rejected by the user as ugly. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-87: The first fix was ugly because no tool adds a new lane into existing nodes (high)**
- **Type:** Limitation.
- **Scenario:** A new branch lane must end in two existing nodes that already have exec links from other lanes.
- **Observed:** The append tool can only link inputs of nodes it creates. The patch tool refuses links ("edits literal defaults only"). The connect tool replaces data inputs only. So the agent duplicated the two end nodes and their getter. The append placed all new nodes in one column at the graph edge with estimated sizes. The tidy then made the graph longer and rolled back. The user called the result ugly.
- **Workaround:** The engine connect tool added the exec links into the existing nodes (an exec input accepts many links). The engine delete tool removed the duplicates. Then one position call placed the lane. This worked, but it took about 12 extra calls and needed engine tools.
- **Proposed behavior:** Let append link a new node output to an existing node input, by node ID and pin name, with read-back. Let append place the new block next to the node it attaches to, as a lane in the user layout style.

**X-88: Engine connect tool ignores an unknown pin field and wires the wrong pin, silently (high)**
- **Type:** Defect (engine tool), with no TurboMCP guard.
- **Observed:** The agent passed a pin by name. The engine tool ignored the name field, used index 0, and returned no error. The branch true and false outputs both went to the wrong node. The agent found it only by re-reading the graph. With an index field the call worked.
- **Expected:** An unknown field is refused, or the call reads back the made link.
- **Proposed behavior:** A TurboMCP exec-connect tool that takes node IDs and pin names, and reads back both ends (see FT-91).

**X-89: Event rows were placed far from the boxes they continue in (high)**
- **Type:** Root cause of 3 extra user prompts.
- **Scenario:** Five unboxed event chains at the top of a large event graph. Three of them continue in nodes inside lower boxes (one shares its tail with another chain).
- **Observed:** The agent laid out each chain as a clean row at the top and checked each row with a local capture. The rows looked good, so the agent reported done. A full-graph view then showed three wires of 5000 to 9000 units that crossed every box. The user had to point this out. The fix moved one box next to its continuation, and two events into the boxes they feed. Total: about 25 extra calls and 3 extra user prompts.
- **Cause, agent side:** The agent planned from each event's own chain and did not ask where the chain ends. It checked only local captures, not the whole graph.
- **Cause, tool side:** No tool reports where a chain leaves its region. The layout report gives one longest wire but does not list long cross-box wires. The graph reader shows the shared tail only as a goto, which is easy to miss.
- **Proposed behavior:**
  1. Layout report: a "cross-box wire" finding that lists each wire between two boxes, or between a box and unboxed nodes, with its length. Sort by length.
  2. A placement helper: for a set of unboxed nodes, propose the box or free area closest to the nodes they link to, and say which wires would become short.
  3. A "shared tail" note in the graph reader: name the nodes reached from two or more entry events.
  4. The tidy and group-move results report the change in total cross-box wire length, so an agent sees the regression without a full capture.

**X-90: Agent check rule for graph layout (process)**
- **Type:** Process note for agents using these tools.
- **Observed:** A local capture of each lane hid the global problem.
- **Proposed behavior:** Before reporting a layout as done, run one full-graph capture and the layout report, and fix every wire that crosses a box. The tool could do this as one call: a done-check that returns the full capture plus the top 10 longest wires.

## 2. Tool observations

### Graph check, append, replace, patch

**GC-31: Append made a getter for the wrong class (high)**
- **Type:** Defect.
- **Scenario:** A member variable getter written by type ID. Another widget class has a variable with the same name.
- **Reproduction status:** Observed once.
- **Observed:** The append created a getter whose self pin is the other class. Its read-back said every node matched the plan. The compile failed with two errors. The agent repointed the readers to an existing getter of the right class.
- **Expected:** A variable type ID resolves to the class that owns the graph first, or the append refuses the ambiguity.
- **Cause:** The cause is not confirmed.

**GC-32: A label on a getter with no input pins is refused (low)**
- **Type:** Usability.
- **Observed:** A closing parenthesis on its own line is read as a pin name. A label inside the comment eats the parenthesis. Writing the self pin as "self self" passed the preview but failed the edit with an invalid default. So preview and edit disagreed. The workaround was to refer to the getter by its type ID, which only works when the type is unique in the block.
- **Proposed behavior:** Accept a label on any form. Make the preview run the same pin default checks as the edit.

### Tidy

**TD-37: Tidy rolled back on a 27-node switch function (low)**
- **Observed:** Wire length would go from 16008 to 17923, so the tidy rolled back. This repeats TD-29: the gate rejects layouts that add lanes.

### Comment boxes and group move

**CB-04: Comment create and group move worked well (positive)**
- **Observed:** Four new boxes were made in four calls, each sized to its listed nodes. Three group moves shifted whole boxes with their nodes in one undo step each. This is the right tool for "make room between boxes".

**TD-38: Tidy refuses a selection of event chains that end in other boxes (medium)**
- **Scenario:** Five unboxed event chains at the top of a large event graph. Their tails continue into nodes inside other boxes.
- **Observed:** The tidy refused twice with "a node would leave its comment box", without naming the node. The agent laid out 47 nodes by hand in 2 position calls, using the graph text to find the chains.
- **Proposed behavior:** Name the node that blocks the tidy. Offer to tidy only the unboxed part of each chain and leave boxed nodes fixed.

## 3. Recurrence

- TD-37 repeats TD-29.
- FT-88 still applies: the lane placement used capture estimates.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-91 | Exec connect into an existing node input, by node ID and pin name, with read-back | 2 engine connect calls, 2 wrong links, 2 deletes, 1 re-read |
| FT-93 | Cross-box wire finding and a placement helper that puts unboxed chains next to their continuation (see X-89) | About 25 calls and 3 user prompts in this session |
| FT-94 | Layout done-check: full capture plus the 10 longest wires in one call | Repeated local captures that missed the problem |
| FT-92 | Append a lane: attach a new block to a switch or branch output, reuse named end nodes, and place it as a lane | The duplicate nodes, the column placement and the hand position pass |

## 5. Suggested priority

1. X-89 and FT-93, because they cost the most user prompts.
2. GC-31, because it made a false-success read-back and a compile error.
3. X-87, FT-91 and FT-92, because adding a case to an existing flow is a common edit.
4. X-88.
5. GC-32, TD-37.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-87, X-88, GC-31, GC-32, TD-37, TD-38, CB-04, X-89, X-90, FT-91, FT-92, FT-93, FT-94. The user reviewed the first result. The report was read back. The local lint was run. Review state stays Pending human review.
