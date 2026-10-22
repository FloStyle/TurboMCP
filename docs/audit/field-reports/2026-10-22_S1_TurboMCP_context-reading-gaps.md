# TurboMCP context reading gaps session report

**Report ID:** 2026-10-22_S1
**Date:** 2026-10-22
**Review state:** Pending human review
**Plugin version:** Version with the fixes for S10 to S12. The exact commit is Unknown.
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** First part: two questions about a large HUD widget Blueprint (43 graphs, about 480 nodes) and one save data table. Later parts: a full working day of options menu edits in about 8 widget Blueprints and 1 function library (new functions, appends into existing flows, delegate binding, hand layouts, comment boxes).
**Context:** Not in play during edits. Source control in use. No submit by the agent.
**Tools used:** ReadTurboMCPNodeFind, ReadTurboMCPNodeResolve, ReadTurboMCPBlueprintLoad, ReadTurboMCPGraphLoad, ReadTurboMCPAssetSearch, ReadTurboMCPDataTable, ReadTurboMCPRefs, ReadTurboMCPToolIndex, ReadTurboMCPToolHelp, ReadTurboMCPToolSchema, PreviewTurboMCPGraphAppend, PreviewTurboMCPAssetUndo, EditTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPPinDefault, EditTurboMCPGraphPositions, EditTurboMCPGraphGroupMove, EditTurboMCPGraphTidy, EditTurboMCPComment, EditTurboMCPFunctionAdd, EditTurboMCPFunctionRemove, EditTurboMCPAssetUndo, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPSaveWithoutThumbnail, LookTurboMCPCapture, LookTurboMCPLayoutReport. Engine tools for node create, delete, connect, node types, object properties, widget tree, tags, logs and console variables.
**Outcome:** All requested changes compiled clean and were saved. The agent fell back to images for one logic question, and the user rejected that as the wrong method. Two comment boxes were changed by mistake and restored by hand (CB-05).
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-91: No fast "full context" read for one node in a large graph (high)**
- **Type:** Limitation.
- **Scenario:** A function is called from two places in a large event graph. The question is what triggers each call.
- **Observed:** The node find gives the two call node IDs and their graph, but not the event or chain around them. The event graph text read is cut at about 22000 characters and does not reach these chains. A graph-filtered whole-Blueprint read lists callers only inside the filtered graphs. So the agent used the neighbours capture twice and read the answer from the images: one call runs at widget init, the other runs on a gameplay message. The user rejected images for logic reading: a full context, fast, is the job of TurboMCP.
- **Expected:** One text call answers "show me the chain around this node": the entry event, the exec path to the node, the data inputs, and what runs after it.
- **Impact:** 2 image calls and a user correction. Images can hide pin values and wire targets.
- **Proposed behavior:** See FT-95. The neighbours capture already computes the right node set; give the same selection as text.

**X-92: "Functions nothing calls" is misleading under a graph filter (medium)**
- **Type:** Usability.
- **Observed:** With a graph filter, the whole-Blueprint read listed two functions under "functions nothing calls". Both are called from the event graph, which the filter excluded. A note says that the filter limits the relations, but the heading still reads as a fact.
- **Expected:** The caller list is computed over the whole Blueprint, even when only some bodies are printed.
- **Proposed behavior:** Always compute callers globally. Print "called from: (graph, node ID)" next to each listed function.

## 2. Tool observations

### Graph reader

**GL-19: Large event graph text is cut without a way to continue (medium)**
- **Type:** Limitation.
- **Observed:** The event graph read stops at about 22000 characters and suggests a smaller detail level. There is no offset or "events matching X" parameter, so the missing events cannot be read as text.
- **Proposed behavior:** Add an entry filter (event or function names) and a continuation offset, like the asset search paging.

**GL-20: Empty function body detected (positive)**
- **Observed:** The reader said a function entry "has no exec chain and no body" although it holds nodes. That was the key fact: the setting was never applied. It answered "is it wired" in one call.

### Asset search

**AS-04: A type filter with a search word is refused (low)**
- **Type:** Defect.
- **Observed:** One search word plus "type:DataTable" was refused as an unsupported filter, although the help says filters can appear before or after the words. The same search without the filter worked.
- **Expected:** Words and the type filter combine.

### Data table reader

**DT-11: Row values are not printed (low)**
- **Type:** Limitation (documented).
- **Observed:** To see which tag each row holds, the agent needed the engine row tool. For save-setting tables, the row tag and type are the main facts.
- **Proposed behavior:** Print the key columns (tag, type, default) for save descriptor tables, or a "columns" parameter.

### Later in the same session (feature build)

**X-93: A tag referencer tool existed and the agent did not find it (medium)**
- **Type:** Usability (tool discovery).
- **Observed:** The engine tag toolset has "find referencers by tag". It would have answered "is this setting wired" in one call. The agent found it only later, while adding a tag. The TurboMCP tool index does not point to it from the asset or node search help.
- **Proposed behavior:** In the node find and asset search help, name the tag referencer tool for tag questions.

**GL-21: Save-value getter nodes are not printed (medium)**
- **Type:** Limitation.
- **Observed:** The project's custom save-value getter node prints as "no node type ID, cannot be written back", with no slot or tag. Those two pins are the whole point of the node. The agent had to resolve each node separately.
- **Proposed behavior:** Print the pin defaults of nodes that cannot be written back, even if the node itself cannot be written.

**GC-38: Append moved an existing wire without the preview saying so (medium)**
- **Type:** Defect (preview and edit disagree).
- **Observed:** The preview of an append that linked from an already connected exec output said "0 forms link to existing". The edit then reported "moved existing wire": the new async node was inserted between two existing nodes. The result was correct here, but the preview did not warn.
- **Proposed behavior:** The preview lists every existing wire it will move.

**SV-13: Editor save fails on a HUD widget, the no-thumbnail save works (low)**
- **Type:** Limitation (engine path).
- **Observed:** The normal save failed with "Accessed None" errors from the widget's design-time code during the thumbnail draw. The save without thumbnail worked at once. The failure message did not suggest it.
- **Proposed behavior:** When a save fails in the thumbnail step, name the no-thumbnail save in the error.

**TD-39: Tidy rejected all 4 new functions (low)**
- **Observed:** All four graphs grew in wire length and rolled back (6418 to 8399, 11576 to 15221, 4377 to 6473, 884 to 1296). Hand layout again. Same cause as TD-29.

**TD-40: Tidy rolled back on a 23-node library function (low)**
- **Observed:** Wire length would go from 25018 to 29553, so the tidy rolled back. Hand layout in 2 position calls. Same cause as TD-29.

**GC-39: Append cannot make a function input getter (medium)**
- **Type:** Limitation.
- **Observed:** The layout rules ask for one function input getter per consumer. The append refused every getter form for a function parameter and listed no matching type ID. So six long wires from the entry node stay in the graph.
- **Proposed behavior:** Accept a function parameter getter form in append, and name it in the refusal hint.

**GC-47: Append made widget getters for the wrong class again (high)**
- **Type:** Defect. Recurrence of GC-31.
- **Observed:** A getter for a designer widget variable, written by type ID, was created with its self pin typed to another widget class that has a widget of the same name. The read-back said every node matched the plan. The compile failed with 4 errors. The engine node finder lists the right getter under a category named after the owning Blueprint, not "Default".
- **Expected:** A variable getter resolves to the graph's own class first.

**GC-48: Preview accepts an enum default that the edit refuses (medium)**
- **Type:** Defect (preview and edit disagree).
- **Observed:** On an enum equality node, a default on the second input passed the preview. The edit rolled back with "Unsupported type Wildcard". The pin is typed only after the first input is wired. Workaround: append without the default, then set it with the pin default tool.

**GC-42: No way to bind a function or a typed custom event to a delegate (medium)**
- **Type:** Limitation.
- **Observed:** Append cannot write an event form. A Create Event node cannot get its function name set (the property is refused). A custom event made by the engine create tool has no parameters, so it cannot connect to a typed delegate. The working path was the engine create tool on the "Assign" delegate node, which spawns a matching event.
- **Proposed behavior:** An append form for "bind delegate to new event" or "bind delegate to function", with the signature taken from the delegate.

**CB-05: No read-only way to find a comment box by its text (high)**
- **Type:** Limitation, with side effects.
- **Observed:** The node find lists 14 boxes all titled "Comment". The layout report gives box indexes, but the comment tool needs IDs, and the box capture needs indexes. The fit action changed two wrong boxes (one shrank, one moved into another box). To find the right box, the agent set the text on 8 boxes and set it back, one by one. The asset undo offered to revert a different, later step.
- **Expected:** The node find prints each box's text, and the layout report prints box IDs.
- **Proposed behavior:** Print box text and ID in both tools. Make fit with a node list grow the box, not move it.

**CP-16: Neighbours capture refused on a large graph (medium)**
- **Type:** Limitation.
- **Observed:** A neighbours capture of one node, 2 hops, in an event graph of about 400 nodes was refused with "graph exceeds the minimum canvas zoom". A region capture of the same area worked. The neighbour set is small, so the size of the whole graph does not seem relevant. The cause is not confirmed.
- **Expected:** The neighbours mode frames only the selected nodes.

**ED-20: Asset undo preview offers a different step than the one the agent wanted (medium)**
- **Type:** Usability.
- **Observed:** After two wrong comment box fits, the undo preview offered to revert the latest batch of node positions instead. Comment box changes did not appear as the latest undo step. A direct undo call without the preview confirmation was refused. The agent restored the boxes by setting their sizes by hand.
- **Expected:** Each comment box edit is its own undo step, and the preview names it.

**X-96: Parameter names differ between similar tools (low)**
- **Type:** Usability.
- **Observed:** Similar tools use different names for the same input: "asset" in some tools, "assets" in compile, save and the whole-Blueprint read; "toolsetName" and "toolName" in the help tool but "toolset" and "tool" in the schema tool; "nodeIds" in node resolve. Graph read requires "format" and "detail" even when empty. Each wrong guess cost one refused call, about 10 times in this session.
- **Proposed behavior:** Use one name per concept across all tools, or accept aliases.

## 3. Recurrence

- X-91 is like FT-88: the agent still has no text view of where things are and what surrounds them.
- X-92 relates to BL items about the relation layer.
- GC-47 repeats GC-31 (getter of the wrong class, with a read-back that says it matched).
- GC-48 and GC-38 repeat the preview and edit disagreement in GC-32.
- TD-39 and TD-40 repeat TD-29 (tidy rejects longer layouts).
- CB-05 relates to X-89 (no box-level view in the layout report).

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-95 | Node context as text: entry event, exec path to the node, its inputs and the next nodes, for N hops | 2 captures read as images, and a user correction |
| FT-96 | Callers of a function or event, each with its trigger event and graph | A filtered read with a wrong "nothing calls" list |

## 5. Suggested priority

1. X-91 and FT-95, because "what triggers this" is a basic question.
2. GC-47, because it gives a false-success read-back and compile errors.
3. CB-05 and ED-20, because they changed unrelated content.
4. X-92, FT-96, GC-42, GC-39.
5. GL-19, CP-16, GC-48, GC-38, AS-04, DT-11, X-96.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-91, X-92, GL-19, GL-20, GL-21, AS-04, DT-11, X-93, GC-38, SV-13, TD-39, TD-40, GC-39, GC-47, GC-48, GC-42, CB-05, CP-16, ED-20, X-96, FT-95, FT-96. GC-47 and GC-48 were first written as GC-40 and GC-41; they were renumbered because another report of the same day uses those numbers. X-93, TD-39 and SV-13 are also used in 2026-10-22_S2, as the glossary notes. The user correction is in X-91. The report was read back. The local lint was run. Review state stays Pending human review.
