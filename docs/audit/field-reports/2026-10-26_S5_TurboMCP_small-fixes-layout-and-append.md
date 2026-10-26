# TurboMCP small graph fixes session report

**Report ID:** 2026-10-26_S5
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** builds of 2026-10-26 (a5775da, then a build compiled 14:50); source revision of the later build Unknown
**Engine version:** 5.8.3, project build
**Client:** Claude Code desktop app, version Unknown
**Test case:** Four small Blueprint fixes: one value inversion in a widget function, one boolean inversion in a widget function, one event-graph rewire with a new override and log lines, and one data table default.
**Context:** No play session. Source control was in use.
**Tools used:** ReadTurboMCPGraphLoad, ReadTurboMCPNodeFind, ReadTurboMCPNodeContext, ReadTurboMCPRefs, ReadTurboMCPDataTable, ReadTurboMCPMaterialLoad, PreviewTurboMCPNodeType, PreviewTurboMCPGraphAppend, PreviewTurboMCPGraphReplace, EditTurboMCPGraphAppend, EditTurboMCPGraphConnect, EditTurboMCPGraphRemove, EditTurboMCPGraphPositions, EditTurboMCPOverrideAdd, EditTurboMCPComment, EditTurboMCPDataTableRows, EditTurboMCPCompile, EditTurboMCPSave, LookTurboMCPGraphPositions, LookTurboMCPLayoutReport, LookTurboMCPLayoutDoneCheck.
**Outcome:** All four fixes complete. Compile clean. Saved. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

None observed.

## 2. Tool observations

### Comment boxes

**CB-06: Comment fit grows the box from its old position instead of moving it to the selected nodes (medium)**
- **Type:** Defect.
- **Scenario:** The nodes of a comment box were moved to a new area of the graph. Fit was then called with that box and those nodes.
- **Reproduction status:** Observed once.
- **Observed:** The box kept its old top-left corner and grew to about 3300 by 2400 units to reach the nodes. It covered most of the graph.
- **Expected:** Fit with explicit nodes encloses those nodes, including a move of the box.
- **Impact:** Two extra calls: a position edit on the comment, then a size set.
- **Workaround:** Move the comment with EditTurboMCPGraphPositions, then set its size. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Let fit move the box, or add a position field to set.

**CB-07: EditTurboMCPComment requires the text field for fit (low)**
- **Type:** Usability.
- **Observed:** Two fit calls without text were refused by the schema. The help says to pass empty for fit.
- **Expected:** Fields that the action does not use are optional.
- **Workaround:** Pass an empty text. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Make unused fields optional.

### Graph append

**GC-55: An append label cannot be used as a link source (low)**
- **Type:** Usability.
- **Scenario:** A new getter form with a label, then a second new form that links to the getter by that label.
- **Reproduction status:** Observed once.
- **Observed:** The link by label was refused. A link by the full type ID of the earlier form worked. A label on the same line as the opening of a form also hid the closing bracket in a comment, which was refused with a clear message.
- **Expected:** A label given to a new form can be used as a link end.
- **Impact:** Long type IDs in link text.
- **Workaround:** Use the full type ID. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Accept labels as link ends.

**GC-56: GraphReplace does not accept the reader text format (low)**
- **Type:** Usability.
- **Observed:** Text written in the GraphLoad reader form was refused with 5 parse errors. The help says to start from a complete reader result.
- **Expected:** The reader form, or a documented conversion, is accepted.
- **Workaround:** Append plus connect. It worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Align the replace grammar with the reader output, or document the differences.

**GC-57: Append with auto continuation spliced a new call into the execution chain as intended (positive)**
- **Observed:** A new call placed after an existing node was inserted before the old next node, and the result said so.

### Tool help

**TH-14: Help for the new merge tools now documents every parameter (positive)**
- **Observed:** On the later build, the help for GraphTransfer, WidgetSubtreeCopy and MergeMembers lists each parameter with constraints. This resolves TH-13 for those tools.

### Untested

- EditTurboMCPMenuActivate and EditTurboMCPPlayInput were not retested on the later build. PI-01 status is not known.

## 3. Recurrence

- TH-14 resolves TH-13 (report 2026-10-26_S3) for three tools.
- CB-06 is similar to CB-05 (an earlier report): a fit changed a box in an unwanted way. Similar symptoms do not establish a common cause.

## 4. Features that would have helped

None observed beyond the items above.

## 5. Suggested priority

1. CB-06: it changes the graph in an unwanted way.
2. GC-55, GC-56, CB-07: friction.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: CB-06, CB-07, GC-55, GC-56, GC-57, TH-14. The report was read back. Local lint passed. The glossary is in the archive folder and was not updated. Automated checking does not prove confidentiality.
