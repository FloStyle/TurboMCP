# TurboMCP widget merge session report

**Report ID:** 2026-10-26_S2
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** 2f7264b
**Engine version:** Unknown
**Client:** Claude Code desktop app, version Unknown
**Test case:** Merge of two widget blueprints (a menu page that holds a second menu page as a child widget) into one new widget blueprint. A third widget blueprint (a card entry with one child class) was read for impact.
**Context:** No play session. Source control was in use. Original assets were not edited. Only a new duplicate asset was changed.
**Tools used:** ReadTurboMCPBlueprintLoad, ReadTurboMCPGraphLoad, ReadTurboMCPToolHelp, ReadTurboMCPToolIndex, ReadTurboMCPTypeFind, LookTurboMCPGraphPositions, EditTurboMCPVarsAdd, EditTurboMCPFunctionAdd, EditTurboMCPCompile, PreviewTurboMCPGraphCopy. Other tools by role: engine asset reference reader, engine asset duplicate and delete, engine widget tree add and property set, engine event dispatcher add, engine graph DSL reader, engine data table schema reader.
**Outcome:** The merge was not completed. The new asset was reset two times. No save. No crash.
**Session limits:** The user stopped the task to record the missing tools.
**Correction of:** None

## 1. Cross-cutting issues

**X-99: A whole-asset merge needs several capabilities that no TurboMCP tool has (high)**
- **Type:** Limitation.
- **Scenario:** Two widget blueprints. One holds an interface, tab functions and navigation logic. The other holds the widget layout and about 15 graphs.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Duplicate the layout blueprint as the merge target.
  2. Try to add the interface of the other blueprint to the target.
  3. Duplicate the logic blueprint instead, and try to bring the layout widgets and graphs in.
- **Observed:** Each direction was blocked. Direction one needs an interface add (FT-102). Direction two needs a widget subtree copy (FT-103) and a whole-graph copy that GraphCopy refuses (GC-49). The agent rebuilt 11 widgets by hand with engine tools, then stopped at the graph copy.
- **Expected:** A merge or move of members between two Blueprints is possible with TurboMCP tools alone. This is a user expectation.
- **Impact:** The workflow was blocked. About 40 calls were spent on a path that was later discarded.
- **State after:** The new asset is a plain duplicate again. Originals were not changed.
- **Workaround:** Ask the user to add the interface in the editor. Not yet tried.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See section 4.

## 2. Tool observations

### Graph copy

**GC-49: GraphCopy refuses a function body that uses local variables, a macro loop or external library calls (high)**
- **Type:** Defect and limitation.
- **Scenario:** A function graph of about 30 nodes: a data-table loop macro, struct breaks, two function-local variables, calls to engine and plugin library functions, a cast, and two delegate binds. The target function was created first with the same name.
- **Reproduction status:** Observed once, in preview.
- **Steps to reproduce:**
  1. Create an empty function of the same name in the target with EditTurboMCPFunctionAdd. Compile the target.
  2. Call PreviewTurboMCPGraphCopy with all nodes except the function entry.
- **Observed:** The preview refused. It listed 13 missing members. Some were real: two function-local variables and one widget variable. Others were library functions on other classes (a widget container reset and create, a subsystem query, a function on the entry widget class). These are not members of the target. The preview also refused the data-table loop macro as structural creation.
- **Expected:** Calls to functions of other classes do not count as missing target members. Function-local variables are created in the target function, or the tool says how to create them. Standard engine macros copy.
- **Impact:** The only TurboMCP path to move a graph between Blueprints was blocked.
- **State after:** Preview only. Nothing changed.
- **Workaround:** The engine graph DSL reader was tried. Its text dropped one boolean term, the struct field names and the bound function names, so it was not used.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Resolve external calls against their owning class. Create needed function locals. Support standard macros. Allow selection of the whole function including its entry, mapped to the target entry.

### Variables and functions

**VA-06: No TurboMCP tool adds function-local variables (medium)**
- **Type:** Limitation.
- **Scenario:** A new function needs two local variables before a graph copy.
- **Reproduction status:** Observed once.
- **Observed:** EditTurboMCPVarsAdd adds member variables only. EditTurboMCPLocalVars promotes arithmetic values only.
- **Expected:** A tool that declares locals on a named function with a type and default.
- **Impact:** Blocks GraphCopy for most real functions.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add a local-variable mode to EditTurboMCPVarsAdd, with a function name field.

**FA-09: No TurboMCP tool adds an event dispatcher (medium)**
- **Type:** Limitation.
- **Scenario:** The merge target needs a dispatcher with a bool and an object parameter.
- **Reproduction status:** Observed once.
- **Observed:** The engine dispatcher add and engine parameter add tools were used. They worked.
- **Expected:** A TurboMCP tool with the same signature syntax as EditTurboMCPFunctionAdd.
- **Impact:** Low. The engine fallback worked.
- **Workaround:** Engine dispatcher tools. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add a dispatcher kind to EditTurboMCPFunctionAdd.

**FA-10: EditTurboMCPFunctionAdd worked well with struct and object parameters (positive)**
- **Scenario:** Six new functions with a gameplay tag struct, a project struct and an object parameter.
- **Observed:** All six were created in one call each, with correct pins and a clear report.

**VA-07: EditTurboMCPVarsAdd set an object default by asset path (positive)**
- **Scenario:** One object-reference variable with a data asset default, and one bool.
- **Observed:** Both were created in one call. The default syntax was accepted.

### Widget tree

**WT-10: No TurboMCP tool writes the widget tree (medium)**
- **Type:** Limitation.
- **Scenario:** Rebuild of an 11-widget subtree with slot padding, alignment, text style and entry class.
- **Reproduction status:** Observed once.
- **Observed:** The engine widget add and property set tools were used for every widget and every slot. This took about 25 calls.
- **Expected:** A TurboMCP widget writer, or a subtree copy from another widget blueprint (FT-103).
- **Impact:** Many calls. The text block got a new localization key.
- **Workaround:** Engine widget tools. It worked, but the work was later discarded.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** See FT-103.

### Reference reads

**RF-09: No TurboMCP reference reader was found by name search (low)**
- **Type:** Usability.
- **Scenario:** The agent needed the referencers of three assets before a merge.
- **Reproduction status:** Observed once.
- **Observed:** A guessed tool name was refused. The index lists ReadTurboMCPRefs, but the agent used the engine referencer tool instead. Its suitability was not tested.
- **Expected:** The refusal message suggests the closest real tool name, not only the toolset list.
- **Impact:** Low.
- **Workaround:** Engine referencer tool. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Suggest close tool names on an unknown tool name.

## 3. Recurrence

None observed.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-102 | Add or remove an implemented interface on a Blueprint, and create its function stubs | The blocked direction of the merge; a manual editor step |
| FT-103 | Copy a widget subtree, with slots and properties, from one widget blueprint to another | About 25 engine calls to rebuild 11 widgets |
| FT-104 | Copy a whole function or event graph between Blueprints, creating locals and missing members after a preview | The refused GraphCopy and the lossy DSL attempt |
| FT-105 | Merge helper: list members of a source Blueprint that a target lacks, and create them in one call | Manual variable, function and dispatcher creation |
| FT-106 | Add an event dispatcher with a typed signature | Three engine calls |

## 5. Suggested priority

1. GC-49 and FT-104: graph moves between Blueprints are blocked.
2. FT-102: one missing write blocks a whole refactor direction.
3. FT-103 and WT-10: widget work falls back to many engine calls.
4. VA-06, FT-105, FT-106, FA-09: smaller gaps.
5. RF-09: wording.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-99, GC-49, VA-06, VA-07, FA-09, FA-10, WT-10, RF-09, FT-102 to FT-106. The engine version and client version were not observed. The report was read back. Local lint passed. The glossary is in the archive folder and was not updated, by user choice. Automated checking does not prove confidentiality.
