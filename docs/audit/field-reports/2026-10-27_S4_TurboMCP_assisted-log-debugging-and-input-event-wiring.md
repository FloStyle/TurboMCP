# TurboMCP assisted log debugging and input event wiring session report

**Report ID:** 2026-10-27_S4
**Date:** 2026-10-27
**Review state:** Pending human review
**Plugin version:** Local install; exact commit not recorded in this session
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** A menu focus-loss bug was debugged in play mode, with the user doing the input. Debug log nodes were added to one menu widget. One small function graph in a news widget was edited. A second news widget got a new function and two input action events.
**Context:** Play mode was running during the debugging and stopped for most edits. Perforce source control in use.
**Tools used:** ReadTurboMCPPlayInputState, ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPGraphLoad, ReadTurboMCPBlueprintLoad, ReadTurboMCPWidgetTree, ReadTurboMCPVcsState, ReadTurboMCPDirtyList, ReadTurboMCPToolHelp, PreviewTurboMCPGraphAppend, EditTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphRemove, EditTurboMCPGraphTidy, EditTurboMCPGraphPositions, EditTurboMCPPinDefault, EditTurboMCPFunctionAdd, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPVcsCheckout. Other tools: the engine play-state query, the engine console variable search, the engine node-type search, and the engine node creation tool.
**Outcome:** The focus bug cause was found from logs. All edited assets compiled clean and were saved. Runtime behavior of the last edit was not verified in this session. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-110: No tool runs a console command (medium)**
- **Type:** Limitation.
- **Scenario:** A play-mode debugging session needed log verbosity changes, a debugger start command and a console variable test.
- **Reproduction status:** Observed four times.
- **Steps to reproduce:**
  1. Start play mode.
  2. Search the tool help for a console or command execution tool.
- **Observed:** No TurboMCP or engine tool executes a console command. The user typed every command. One typed command had a typo and the debug capture was lost for one repro.
- **Expected:** A tool that runs a console command in the editor or play world.
- **Impact:** Each step waited on the user. One repro was repeated.
- **State after:** No change.
- **Workaround:** The user typed the commands. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-123.

**X-111: No tool reads or filters the output log (medium)**
- **Type:** Limitation.
- **Scenario:** Several repros needed log lines filtered by category and by time.
- **Reproduction status:** Observed in every repro of the session.
- **Observed:** No tool reads the output log. The agent read the log file from disk with shell commands after each repro.
- **Expected:** A log read filtered by category, text and start time.
- **Impact:** Extra shell calls per repro. Output size had to be controlled by hand.
- **Workaround:** Read the log file from disk. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-124.

**X-112: Live input state read helped locate the focus owner (positive)**
- **Scenario:** A menu lost gamepad focus in play mode.
- **Observed:** ReadTurboMCPPlayInputState listed the focused widget type, the active mapping contexts, the enabled buttons and the active menu widgets. It showed that the menu router still believed a menu had focus. Combined with the engine log, this confirmed the cause.

## 2. Tool observations

### Graph append

**GC-68: Append refuses execution nodes linked from an input action event (medium)**
- **Type:** Defect.
- **Scenario:** Two new input action events in a widget event graph. A call node was to run on the triggered output.
- **Reproduction status:** Observed three times.
- **Steps to reproduce:**
  1. Append an input action event.
  2. Append a call node whose execute input links to that event's triggered output.
- **Observed:** Preflight refused with "newly created execution nodes have no path from recognized entries". The same happened when the event already existed in the graph. Compile also lists nodes after input action events as not reached from recognized entries.
- **Expected:** Input action events count as entries.
- **Impact:** The edit needed a second toolset.
- **State after:** Unchanged by the refused calls.
- **Workaround:** Created the call nodes with the engine node creation tool, then linked them with EditTurboMCPGraphConnect. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Treat input action events as entry nodes in preflight and in the reachability report.

**GC-69: Append has no custom event form (low)**
- **Type:** Limitation.
- **Scenario:** A custom event was tried as an entry for the GC-68 workaround.
- **Reproduction status:** Observed once.
- **Observed:** A guessed custom event form was refused as an unknown type. The tool help gives no custom event syntax. The engine node-type search returned a custom event entry that append did not offer.
- **Expected:** A documented form for a named custom event.
- **Impact:** A function was created instead.
- **Workaround:** EditTurboMCPFunctionAdd plus append into the function graph. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept a named custom event form and list it in the help.

**GC-70: Append type IDs differ from the engine node-type search (low)**
- **Type:** Usability.
- **Scenario:** Type IDs for an input action event, a web browser call and math nodes were looked up.
- **Reproduction status:** Observed four times.
- **Observed:** The engine search returned short IDs. Append created nodes whose reported IDs used a different form. A guessed long form for an input event was refused. A float multiply operator was not found in either form. Append link ends need the long form of the same type.
- **Expected:** One ID form, or a tool that maps one form to the other.
- **Impact:** Several refused previews. A math step was moved into a string literal instead.
- **Workaround:** Trial and error with both forms. Partly worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept engine search IDs in append, and list promotable operators in the type lookup.

**GC-71: The append link grammar is not in the tool help (low)**
- **Type:** Documentation.
- **Scenario:** Spliced log nodes and new links in existing graphs.
- **Reproduction status:** Observed twice.
- **Observed:** The help shows a node example only. Two link syntaxes were refused before the correct form was found from an earlier session.
- **Expected:** The help shows how to link a new node to an existing pin.
- **Impact:** Two refused previews.
- **Workaround:** Reused the syntax from an earlier session. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add a link example to the append help.

### Graph connect and remove

**GC-72: GraphConnect refuses short node IDs (low)**
- **Type:** Usability.
- **Scenario:** Connecting a new pure node output to an existing input in a small function graph.
- **Reproduction status:** Observed once.
- **Observed:** Short IDs were refused with "pins must resolve uniquely". Full IDs worked. Other tools in the session accepted short IDs.
- **Expected:** The same short ID rules as the readers and context tools.
- **Impact:** One retry.
- **Workaround:** Full IDs. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept unique short IDs.

**GC-73: GraphConnect named the displaced node for cleanup (positive)**
- **Scenario:** A variable getter was replaced by a function call on the same input.
- **Observed:** The result named the old getter as a pure node with no consumers and pointed to the remove tool. The remove then worked with one call.

### Layout

**TD-47: Whole-graph tidy rolled back while wire length went down (low)**
- **Type:** Defect.
- **Scenario:** A seven-node function graph after one node swap.
- **Reproduction status:** Observed once.
- **Observed:** The result said no acceptable layout and rolled back. The reported total wire length fell by about a fifth.
- **Expected:** The reason for rejection is stated, or the shorter layout is accepted.
- **Impact:** The layout was left as placed. Minor.
- **State after:** Original positions restored. Compiled clean.
- **Workaround:** None used.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Name the quality criterion that failed.

**TD-48: Function graph tidy worked well and split a shared getter (positive)**
- **Scenario:** A new four-node function graph.
- **Observed:** The tidy removed drift, kept zero overlaps and split one getter per consumer.

### Members

**CB-09: FunctionAdd created a function with a typed input (positive)**
- **Scenario:** A helper function with one string input.
- **Observed:** The function was created and reported as implemented. Append into its graph then linked the entry pins. The result compiled clean.

## 3. Recurrence

- X-110 relates to PI-03 and FT-119 (2026-10-27_S2): play-mode work again depended on the user for input and commands.
- TD-47 relates to TD-43 (2026-10-27_S2) and TD-44 (2026-10-27_S3): tidy results that disagree with the wire length measure.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-123 | Run a console command in the editor or play world | Four commands typed by the user, one repeated repro |
| FT-124 | Read the output log filtered by category, text and start time | Shell reads of the log file after every repro |
| FT-125 | Treat input action events as entries in append | A second toolset to create and link two call nodes |

## 5. Suggested priority

1. X-110 and FT-123, X-111 and FT-124: assisted play-mode debugging is blocked on the user without them.
2. GC-68 and FT-125: a common widget input edit cannot be done with append alone.
3. GC-70, GC-69, GC-71, GC-72: friction in finding the right syntax.
4. TD-47: minor layout friction.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs: X-110, X-111, X-112, GC-68, GC-69, GC-70, GC-71, GC-72, GC-73, TD-47, TD-48, CB-09, FT-123, FT-124, FT-125. Missing evidence: plugin commit, engine revision and client version. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
