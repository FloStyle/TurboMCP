# TurboMCP widget data lookup session report

**Report ID:** 2026-10-22_S2
**Date:** 2026-10-22
**Review state:** Pending human review
**Plugin version:** e9135a9 (local install, as last recorded; not read back from a tool in this session)
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One widget Blueprint with one function of 7 nodes. One new user struct with 2 fields. One new data table with 1 row. One function library and one existing data table read only.
**Context:** Editor open, no play session. Source control: Perforce, available.
**Tools used:** ReadTurboMCPAssetSearch, ReadTurboMCPWidgetTree, ReadTurboMCPNodeFind, ReadTurboMCPGraphLoad, ReadTurboMCPBlueprintLoad, ReadTurboMCPNodeResolve, ReadTurboMCPDataTable, ReadTurboMCPDirtyList, ReadTurboMCPToolHelp, ReadTurboMCPToolSchema, ReadTurboMCPVcsState, PreviewTurboMCPDataTableRows, PreviewTurboMCPGraphAppend, EditTurboMCPStructNew, EditTurboMCPDataTableRows, EditTurboMCPWidgetsAsVars, EditTurboMCPGraphAppend, EditTurboMCPCompile, EditTurboMCPGraphTidy, EditTurboMCPGraphPositions, EditTurboMCPSave, LookTurboMCPGraphPositions, LookTurboMCPCapture. Engine tools for data table create and row read, object property read, node type search, node info, node create, node delete and pin connect.
**Outcome:** Task done. Final compile clean. All three assets saved. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-93: Two unrelated assets became unsaved during the session (medium)**
- **Type:** Defect.
- **Scenario:** The session edited one widget, one struct and one data table. At the start the unsaved list was empty.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Start with no unsaved assets.
  2. Edit, compile and save one widget Blueprint with the tools listed above.
  3. Read the unsaved list.
- **Observed:** Two assets that no call edited were listed as unsaved: one widget Blueprint and one data table.
- **Expected:** Only edited assets become unsaved (user expectation).
- **Impact:** The user must check and discard two packages by hand.
- **State after:** The two assets stay unsaved. They were not saved.
- **Workaround:** None observed. The agent left them unsaved and told the user.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** The save or compile result names any other package that became unsaved during the call.

## 2. Tool observations

### Asset search

**AS-05: A type filter with a search word fails in both orders (low)**
- **Type:** Usability.
- **Scenario:** Search for widget Blueprints whose name holds one word.
- **Reproduction status:** Repeated, 3 times.
- **Steps to reproduce:**
  1. Search with the word first and a type filter second.
  2. Search with the type filter first and the word second.
- **Observed:** Word first was refused as an unsupported filter. Filter first gave 0 matches, and the summary showed the word as part of the type. A search with only the word gave the expected results.
- **Expected:** The tool help says filters can appear before or after words.
- **Impact:** Two extra calls. The type filter could not be used.
- **State after:** No change. Read only.
- **Workaround:** Search with the word only. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** A filter value ends at the first space.

### Graph append

**GC-33: Append made a getter for another class's variable (high)**
- **Type:** Defect.
- **Scenario:** A widget child was just marked as a variable. The Blueprint was not compiled yet. Another widget class has a variable with the same name.
- **Reproduction status:** Observed once in this session.
- **Steps to reproduce:**
  1. Mark a widget child as a variable.
  2. Without a compile, append 3 getters of that variable in the short variable form, each feeding a call.
- **Observed:** The preview and the edit both passed. All 3 getters targeted the other widget class. The next compile failed with 6 errors.
- **Expected:** A false pass is not acceptable. The getter targets this Blueprint, or the preview refuses the ambiguous name.
- **Impact:** 3 nodes deleted and created again with engine tools, plus 3 connects and 2 compiles.
- **State after:** Fixed in memory, then compiled clean and saved.
- **Workaround:** Engine node create with the type ID that names this Blueprint. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** The preview prints the class each variable node targets and refuses a target outside the Blueprint and its parents.

**GC-34: A getter with no input pins needs a dummy pin line (low)**
- **Type:** Limitation.
- **Scenario:** Append a variable getter with a local label.
- **Reproduction status:** Repeated, 3 times.
- **Steps to reproduce:**
  1. Write a getter form with a label and a closing bracket on its own line.
  2. Write it again with the closing bracket after the self pin name.
- **Observed:** Both were refused. A closing bracket alone was read as a pin name, and so was the pin name with the bracket glued to it.
- **Expected:** A form with no pin lines can be closed.
- **Impact:** 3 extra preview calls.
- **State after:** No change. Preview only.
- **Workaround:** Give the self pin an empty quoted value with the bracket after it. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept a closing bracket alone on a line.

**GC-35: A comment inside a nested value hides its closing bracket (low)**
- **Type:** Usability.
- **Scenario:** Append with a nested inline value that carries a label comment.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Write a pin value as a nested node form, with a comment before its closing bracket.
- **Observed:** The error said there was an unclosed form. It did not name the line or the comment.
- **Expected:** The error names the line where the comment took the bracket.
- **Impact:** One extra call.
- **State after:** No change. Preview only.
- **Workaround:** Put each node in its own form with a label. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Name the line and say that a comment ends at the end of the line.

**GC-36: An inserted branch rejoins the old next node on one lane only (medium)**
- **Type:** Limitation.
- **Scenario:** Insert a branch between two existing exec nodes. Both lanes must continue to the old next node.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Append a branch whose input links from the old exec output, with one call on each lane.
- **Observed:** The tool moved the old link to the end of the first lane, and said so clearly. The second lane ended with no link.
- **Expected:** A way to say that every lane rejoins the old next node.
- **Impact:** One engine connect call.
- **State after:** Fixed and compiled clean.
- **Workaround:** Engine pin connect. This worked.
- **Cause:** Not applicable. The tool reported what it did.
- **Proposed behavior:** See FT-97.

**GC-37: Append wrote 8 nodes and 10 wires in one call with a clear read back (positive)**
- **Scenario:** Data table lookup, struct break, two calls and three getters in one function.
- **Observed:** The preview matched the edit. The result listed every wire and every moved link.

### Layout

**TD-39: Tidy put getters so their wires cross other data wires (low)**
- **Type:** Limitation.
- **Scenario:** One function with 15 nodes, one branch and 3 getters.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Tidy the function after the append.
  2. Capture the graph.
- **Observed:** Wire length went down. One getter fed a call from far below and crossed another wire. A pure node sat on the exec row.
- **Expected:** The user's layout rules: pure nodes below the exec row, next to their consumer.
- **Impact:** 2 hand position calls and 2 captures.
- **State after:** Positions fixed and saved.
- **Workaround:** Hand positions. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** None.

**ED-17: The positions format is found only by failing (low)**
- **Type:** Usability.
- **Scenario:** Set 8 node positions in one call.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Read the tool schema. It shows only a string parameter.
  2. Call with lines of text, then with a JSON array that uses a wrong key.
- **Observed:** Two refusals. The second one named the needed keys.
- **Expected:** The tool help gives the format.
- **Impact:** 2 extra calls.
- **State after:** No change until the third call.
- **Workaround:** A JSON array with the keys from the error. This worked.
- **Cause:** Not applicable.
- **Proposed behavior:** Add the format and one example to the tool help.

### Save

**SV-13: A widget save returned only script errors, but the package was written (high)**
- **Type:** Defect.
- **Scenario:** Save one widget Blueprint after a clean compile.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Save the widget with the normal save tool.
- **Observed:** The result was an error made of two script access-none lines. There was no save summary. The unsaved list then showed the widget as saved.
- **Expected:** The true result, plus the script errors as warnings.
- **Impact:** One extra check call.
- **State after:** Saved.
- **Workaround:** Read the unsaved list. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Catch script errors from design-time widget code and report them next to the save result.

### Positive observations

**SN-03: Struct new created both fields in one call (positive)**
- **Scenario:** A new struct with a tag field and a text field.
- **Observed:** The fields were read back. The first type error named the bad type and how to fix it.

**DT-12: Data table rows names the real field names (positive)**
- **Scenario:** Add one row to a table with a new user struct.
- **Observed:** A short field name was refused with the full internal names listed. The next call worked and was read back.

**WT-09: Widget tree reader showed which children are variables (positive)**
- **Scenario:** Find which text child is not set by script.
- **Observed:** The variable marker found the cause with no graph read.

**NR-08: Node resolve gave engine references and pin indexes (positive)**
- **Scenario:** Hand off to engine connect and delete calls.
- **Observed:** Every engine call worked on the first try with the given references.

**CP-15: Capture showed layout faults the text did not show (positive)**
- **Scenario:** Check the tidy result.
- **Observed:** Two captures showed the crossing wires.

**VC-09: Source control state for new and edited assets (positive)**
- **Scenario:** Check three assets after save.
- **Observed:** Opened for add and checked out were reported correctly.

## 3. Recurrence

| New ID | Earlier ID | Matching observation | Changed conditions |
| --- | --- | --- | --- |
| X-93 | X-18 | Assets became unsaved with no call editing them | This time during edit and save work, not only reads |
| AS-05 | AS-04, AS-01 | Type filter with a word fails | Both word orders tried |
| GC-33 | GC-31 | Append getter for the wrong class | The variable was flagged but not compiled |
| GC-34 | GC-32 | Getter with no input pins is hard to write | A workaround was found |
| TD-39 | X-86, TD-34 | Tidy is a start, not a finish; pure nodes on the exec row | None |
| ED-17 | X-71 | Hand layout cost | Batch positions now exist; only the format was unclear |
| SV-13 | SV-02, SV-07 | Save error, but the package was written | Normal save on a widget |

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-97 | An append option that makes every new lane end in the old next exec node | 1 resolve and 1 engine connect call |

## 5. Suggested priority

1. GC-33: a false pass that broke the compile.
2. SV-13: the save looked failed, but it worked.
3. X-93: unrelated assets became unsaved.
4. GC-36 and FT-97: common case when inserting a branch.
5. AS-05, GC-34, GC-35, ED-17, TD-39: friction and help text.

## 6. Completeness and privacy review

All tools used are listed. All item IDs from the session checklist are present: X-93, AS-05, GC-33 to GC-37, TD-39, ED-17, SV-13, SN-03, DT-12, WT-09, NR-08, CP-15, VC-09, FT-97. No call counts beyond those observed. The report was read back and the local lint was run. A lint pass does not prove confidentiality. Review state stays Pending human review.
