# TurboMCP widget hover state graph edits session report

**Report ID:** 2026-10-22_S3
**Date:** 2026-10-22
**Review state:** Pending human review
**Plugin version:** e9135a9 (local install, as last recorded; not read back from a tool in this session)
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One shared button card base widget Blueprint (2 new functions, 2 new variables, 2 event bodies changed). Two menu page widget Blueprints (1 new object variable each, 1 new function in one page, 3 to 4 existing functions extended). Three more widget Blueprints read only.
**Context:** A play session was running at the start, then stopped by the user. Source control: Perforce, available. One read-only asset was exclusively checked out by another user.
**Tools used:** ReadTurboMCPBlueprintLoad, ReadTurboMCPGraphLoad, ReadTurboMCPRefs, ReadTurboMCPAssetSearch, ReadTurboMCPToolSchema, ReadTurboMCPToolHelp, ReadTurboMCPNodeFind, ReadTurboMCPNodeResolve, ReadTurboMCPEditQueue, ReadTurboMCPDirtyList, PreviewTurboMCPVarsAdd, PreviewTurboMCPGraphAppend, PreviewTurboMCPGraphReplace, PreviewTurboMCPGraphRepoint, PreviewTurboMCPNodeType, PreviewTurboMCPNodePins, EditTurboMCPVcsCheckout, EditTurboMCPVarsAdd, EditTurboMCPFunctionAdd, EditTurboMCPGraphAppend, EditTurboMCPGraphReplace, EditTurboMCPGraphCopy, EditTurboMCPCompile, EditTurboMCPSave. Engine tools for pin break, pin connect, node delete, node create, node type search and object variable add.
**Outcome:** Task done over four user iterations. The user tested the result in a play session and accepted it. Final compiles clean for all three edited assets. All three saved. No crash.
**Session limits:** Call counts and durations were not recorded. The call log was not read.
**Correction of:** None

## 1. Cross-cutting issues

**X-94: Preview passes a text that the edit then refuses (medium)**
- **Type:** Defect.
- **Scenario:** Graph append into a function and into another function, on two widget Blueprints.
- **Reproduction status:** Observed twice in this session, with two different causes.
- **Steps to reproduce:**
  1. Write an append text where a pinless getter carries a dummy self pin line, or where a link goes into the self pin of a call from a getter whose name is shared by several classes.
  2. Run the append preview, then the append edit with the same text.
- **Observed:** Both previews passed. The first edit refused with an invalid pin default for the self pin. The second edit refused because the engine did not keep the link. The second case was caused by GC-43: the getter was for another class, so the preview checked a link the edit could not make.
- **Expected:** The preview runs the same checks as the edit. The tool contract says a text the preview accepts is one the edit accepts.
- **Impact:** 2 failed edits and extra diagnosis calls. Nothing was written, because the edit rolled back each time.
- **State after:** Unchanged both times. Rollback reported complete.
- **Workaround:** Put a label after the pinless form instead of a dummy pin. For the second case, see GC-43. Both worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Run pin default validation and link creation on a throwaway graph during the preview.

## 2. Tool observations

### 2.1 Graph append and replace

**GC-43: Append silently picked another class's variable getter (high)**
- **Type:** Defect.
- **Scenario:** Append a getter for a widget variable of the edited Blueprint. Six other classes in the project have a variable with the same name.
- **Reproduction status:** Observed once in this session. Recurs, see section 3.
- **Steps to reproduce:**
  1. In a function of a widget Blueprint, append a form for the getter type ID of a widget variable whose name also exists on other classes.
  2. Link its output to the self pin of a call on that widget class.
- **Observed:** The created getter read an array variable on an engine widget class, not the Blueprint's own widget. The node-type lookup listed the same type ID for 7 classes. Only node resolve showed the wrong owning class and the array container flag.
- **Expected:** The getter targets the edited Blueprint, or the tool refuses the ambiguous ID and names the candidates.
- **Impact:** 1 failed edit, about 5 extra calls to diagnose, and a manual fix with engine tools.
- **State after:** The wrong getter was deleted. The correct getter was created with an engine tool and connected. Compile clean. Saved.
- **Workaround:** The engine node type search listed a distinct ID under the widget variable category. Creating the node with that ID and connecting it with the engine pin tool worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Prefer the edited Blueprint's own member when a variable type ID is ambiguous. Print the owning class of every created getter in the edit result.

**GC-44: Linking from a connected exec output moved the old wire onto the wrong output (medium)**
- **Type:** Defect.
- **Scenario:** Insert a branch between an event and its only call node, so the call runs only on the false output.
- **Reproduction status:** Observed twice (two events in one append). Recurs, see section 3.
- **Steps to reproduce:**
  1. Append a branch whose exec input links from an event output that already feeds a call node.
  2. In the same text, link the existing call node's exec input from the branch false output.
- **Observed:** The edit reported that it moved the existing wire to the branch true output. The false output link was also made. The call node then ran on both outputs, so the gate did nothing. The compile was clean. Only node resolve link counts showed the problem.
- **Expected:** When the text links the old target from a named output, the moved wire is not added again on another output. The preview states which output gets the moved wire.
- **Impact:** A clean compile with wrong behavior. 2 extra engine calls to break the extra links.
- **State after:** Fixed by breaking the two extra links. Compile clean. Saved.
- **Workaround:** Break the true-output link with the engine pin break tool. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Let the text choose the continuation output for an insert. Never move an old wire to an output when the text already links that target.

**GC-45: A label attaches to the form above it (low)**
- **Type:** Usability.
- **Scenario:** Append pinless getters with local labels.
- **Reproduction status:** Observed in 4 attempts. Recurs, see section 3.
- **Steps to reproduce:**
  1. Write a label comment on the same line as a form head, or on the line before a form.
  2. Preview the append.
- **Observed:** A label on the head line hid the closing bracket. A label on the line before a form named the previous form. A label on the line after a closed form worked, also for a form with no pins.
- **Expected:** The help documents where a label goes.
- **Impact:** 3 refused previews before the working form was found.
- **State after:** Unchanged by the refusals.
- **Workaround:** Close the form, then put the label on the next line. This worked in every later append.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Document the rule with one example in the tool help.

**GC-46: Replace listed every removal, and link forms joined existing nodes (positive)**
- **Scenario:** Replace a 7-node function body. Append forms that name an existing node by ID to add a second exec source.
- **Observed:** Replace listed all 6 removed nodes by key, class and title, and reported one undo transaction. Link forms by node ID added exec merges to an existing node without a copy. Pins with spaces in the name worked as link targets.

### 2.2 Graph edits outside append

**ED-18: No TurboMCP edit breaks one link or deletes one node (medium)**
- **Type:** Limitation.
- **Scenario:** Remove two wrong links and replace two small gates in an event graph.
- **Reproduction status:** Observed in 3 separate needs in this session.
- **Steps to reproduce:**
  1. Find a wrong exec link between two nodes.
  2. Look for a TurboMCP tool that removes only that link or only a few nodes in an event graph.
- **Observed:** Repoint refused a source and target on the same node. Replace works on one function or event body only. The agent used engine tools for 2 link breaks and 9 node deletes.
- **Expected:** A previewed, undoable remove for one link or a list of nodes.
- **Impact:** 11 engine calls with no preview and no read back.
- **State after:** Correct after manual checks with node resolve and the graph reader.
- **Workaround:** Engine pin break and node delete tools. These worked. The node references came from node resolve.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-98.

**ED-19: Graph copy refused a plain variable getter (low)**
- **Type:** Limitation.
- **Scenario:** Copy one member variable getter from one function to another function in the same Blueprint.
- **Reproduction status:** Observed once. Related to ED-06, see section 3.
- **Steps to reproduce:**
  1. Run graph copy with one getter node ID, same asset, other function, no anchor.
- **Observed:** Refused: copied pin identity, type or defaults could not be preserved. Rollback complete.
- **Expected:** A getter for a member of the same Blueprint copies.
- **Impact:** 1 extra call. The engine node create path was used instead.
- **State after:** Unchanged.
- **Workaround:** Engine node create with the distinct category ID. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** None.

### 2.3 Variables and functions

**VA-05: Variables add still refuses object reference types (medium)**
- **Type:** Limitation.
- **Scenario:** Add a variable that holds a reference to a widget Blueprint class.
- **Reproduction status:** Observed once in this session, on 2 needs. Recurs VA-02.
- **Steps to reproduce:**
  1. Run variables add with a widget Blueprint generated class path as the type.
- **Observed:** Refused: a variable can be an enum or a struct, not that.
- **Expected:** Object and class reference types are accepted, as in the details panel.
- **Impact:** 2 engine calls without read back, 1 extra compile.
- **State after:** Unchanged by the refusal.
- **Workaround:** Engine object variable add, then a compile. This worked both times.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept an object path as an object reference type.

**FA-08: Function add with an input worked and read back (positive)**
- **Scenario:** Add 3 functions with zero or one bool input.
- **Observed:** Each call created the graph and input and confirmed the editor lists the function as implemented. The entry node was then found with node find for later links.

### 2.4 Lookups and reads

**NT-09: Node-type lookup cannot tell same-named variable getters apart (medium)**
- **Type:** Usability.
- **Scenario:** Search the getter for a widget variable whose name exists on 7 classes.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Run the node-type lookup with the variable name in a function of the edited Blueprint.
- **Observed:** 7 results with the same type ID, one marked as defined in this Blueprint. No result gave an ID that append would resolve to that one.
- **Expected:** The result for this Blueprint carries an ID that append resolves to this Blueprint only.
- **Impact:** Led to GC-43.
- **State after:** No change.
- **Workaround:** The engine node type search gave a distinct category ID. This worked with the engine node create tool.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-99.

**AS-06: Asset search refused a type filter written with a class keyword (low)**
- **Type:** Usability.
- **Scenario:** Search a name prefix restricted to widget Blueprints.
- **Reproduction status:** Observed once. Recurs AS-04 and AS-05.
- **Steps to reproduce:**
  1. Run asset search with a name word and a type filter for the widget Blueprint class.
- **Observed:** The whole query was refused as an unsupported filter.
- **Expected:** A type filter with a search word works.
- **Impact:** 1 extra call. The references tool found the asset instead.
- **State after:** No change.
- **Workaround:** Read outgoing references of a known parent asset. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** None.

**NR-09: Node resolve link counts caught two wiring defects (positive)**
- **Scenario:** Check new branches and getters after appends.
- **Observed:** Per-pin link counts showed both branch outputs linked (GC-44). The pin type and container flag showed the wrong getter class (GC-43). The engine node reference from the same answer fed the engine pin tools directly.

**X-95: Reader shows exec merges but not which branch output feeds a node (low)**
- **Type:** Usability.
- **Scenario:** Read a function body after a branch insert.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Read a function at bodies detail after inserting a branch.
- **Observed:** The body text showed the call node under the branch without the output name. Node resolve was needed to see which outputs were linked.
- **Expected:** The reader names the source output of each exec link.
- **Impact:** 1 extra resolve call per check.
- **State after:** No change.
- **Workaround:** Node resolve. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Print the source pin name on exec links from multi-output nodes.

### 2.5 Edit queue, source control, compile, save

**EQ-03: Edit queue named the play session as the blocker (positive)**
- **Scenario:** Two edits were refused at the start of the session.
- **Observed:** The edit queue said a play session was running and writes were not allowed. The user stopped it and all later edits ran.

**VC-10: Checkout reported another user's exclusive lock clearly (positive)**
- **Scenario:** Check out an asset that another user had open exclusively.
- **Observed:** The tool printed the provider failure verbatim and said no file was written. The agent changed the design to avoid editing that asset.

**CO-10: Compile gave a per-asset verdict with saved state (positive)**
- **Scenario:** 9 compiles over 4 assets.
- **Observed:** Each line gave clean or not, the unsaved state, and a reachability count.

**SV-14: A widget save returned only script errors, but the package was written (high)**
- **Type:** Defect.
- **Scenario:** Save one widget Blueprint after edits.
- **Reproduction status:** Observed once in this session. Recurs, see section 3.
- **Steps to reproduce:**
  1. Save an edited widget Blueprint with the editor save tool.
- **Observed:** The tool returned an error with 8 script access warnings from other widgets. The thumbnail step runs design-time code for those widgets. The dirty list then showed nothing unsaved. The 3 other saves in this session returned a normal answer.
- **Expected:** A save result says whether the package was written, separate from thumbnail script warnings.
- **Impact:** 1 extra call to confirm. A reader could think the save failed.
- **State after:** Saved, confirmed by the dirty list.
- **Workaround:** Read the dirty list after the save. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Catch script warnings from the thumbnail step and report them after the package write status.

## 3. Recurrence

| New ID | Earlier ID | Matching observation | Changed conditions |
| --- | --- | --- | --- |
| X-94 | GC-32, GC-41 | Preview accepts a value or link that the edit refuses | Seen with a self pin default and with a link from a wrong-class getter |
| GC-43 | GC-33, GC-40 | Append made a getter for another class's variable | Engine class here, not a project widget. The preview passed, the edit failed instead of compiling wrong |
| GC-44 | GC-38, GC-36 | Append moved an existing exec wire during an insert | The moved wire went to the branch true output while the text linked the false output |
| GC-45 | GC-12, GC-32, GC-34 | Label and closing bracket rules for pinless forms | A working form was found: the label after the closed form |
| ED-19 | ED-06 | Graph copy refused a simple node | Here a member variable getter in the same Blueprint |
| VA-05 | VA-02 | Variables add refuses object types | Still present on the recorded install |
| AS-06 | AS-04, AS-05 | Type filter with a search word refused | Class keyword form |
| SV-14 | SV-13 (2026-10-22_S2) | Save answered with script errors only, package written | Same pattern on a different widget |

Note for the maintainer: X-93, TD-39 and SV-13 each appear in both 2026-10-22_S1 and 2026-10-22_S2 with different meanings. The glossary counters are behind the reports for CB, GC and TD. This report does not change those reports.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-98 | Previewed, undoable removal of one link or a list of nodes, with read back | 2 engine link breaks and 9 engine node deletes, each checked by hand |
| FT-99 | A declaring class field in append forms, or a self-member ID returned by the node-type lookup | 1 failed edit, 1 wrong node, 1 delete and 1 engine create plus connect |
| FT-100 | An insert mode in append that names the output carrying the old continuation | 2 extra link breaks after a branch insert that compiled clean but did nothing |

## 5. Suggested priority

1. GC-43 and SV-14: a wrong node with no warning, and a save that looks failed.
2. GC-44: a clean compile with wrong behavior after a common insert.
3. X-94: preview and edit disagree, which defeats the preview step.
4. ED-18 and VA-05: common edits that still need engine tools.
5. NT-09, X-95, GC-45, ED-19, AS-06: lookup, reader and documentation friction.

## 6. Completeness and privacy review

All TurboMCP tools used in the session are listed in the header. Engine tools are listed by role. Item IDs X-94, X-95, GC-43 to GC-46, ED-18, ED-19, VA-05, FA-08, NT-09, AS-06, NR-09, EQ-03, VC-10, CO-10, SV-14 and FT-98 to FT-100 are represented. Call counts are partial and durations were not recorded. The report was read back and the local lint was run. Review state stays Pending human review. Automated checks do not prove confidentiality.
