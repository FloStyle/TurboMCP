# TurboMCP split and branch layout session report

**Report ID:** 2026-10-21_S15
**Date:** 2026-10-21
**Review state:** Pending human review
**Plugin version:** Version with the fixes for S10 to S12. The exact commit is Unknown.
**Engine version:** Unreal Engine 5.8, source build
**Client:** Claude Code desktop app
**Test case:** One widget Blueprint. Two small function graphs: one with a branch and two lanes (13 nodes), and one with a condition cluster and a math chain (17 nodes).
**Context:** Not in play. Source control in use. No submit.
**Tools used:** PreviewTurboMCPGraphTidy, EditTurboMCPGraphTidy, ReadTurboMCPGraphLoad, ReadTurboMCPNodeFind, ReadTurboMCPNodeResolve, EditTurboMCPGraphSplitValue, LookTurboMCPLayoutReport, EditTurboMCPGraphPositions, LookTurboMCPCapture, EditTurboMCPCompile.
**Outcome:** Both graphs done in memory: tidy, then split, then hand positions. Compile clean. Not saved; the user reviews first. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-86: Tidy is a start, not a finish, on small graphs (medium)**
- **Type:** Limitation.
- **Observed:** Both tidies passed the gate and matched the preview. Both results still needed a hand pass. About 22 TurboMCP calls for two small graphs.
- **Expected:** Small graphs come out of one tidy call in the user's style.

## 2. Tool observations

### Tidy

**TD-34: Pure math nodes sit on the exec row (medium)**
- **Type:** Limitation.
- **Observed:** In both results, math nodes (modulo, add, greater, and) sat on the exec row. The exec wire ran through them. In the second graph the whole math chain formed one row with the exec line.
- **Expected:** Pure nodes sit below the exec row, in columns left of their consumer.
- **Proposed behavior:** Reserve the exec row for exec nodes. See TD-30 for the recipe.

**TD-35: Tidy does not split shared values (medium)**
- **Type:** Limitation.
- **Observed:** The previews reported 0 getters would be split. One self reference fed 3 consumers. One count getter fed 3 consumers. The tidy kept each one shared, with long wires.
- **Proposed behavior:** Split shared self and getter nodes as part of the tidy, or offer it in the preview (TD-32).

### Graph edits

**ED-15: Split value tool works (positive)**
- **Observed:** The split made 3 copies, one for each consumer, in one undo step. It removed the original node. The result said that a layout is required.

**ED-16: Split result does not name the new nodes (low)**
- **Type:** Usability.
- **Observed:** The split result gave the copy count but no node IDs, and no consumer for each copy. The agent needed a node find, a node resolve and a layout report to learn which copy feeds which pin. The node resolve gave pins but no link targets.
- **Proposed behavior:** List each new node ID with its consumer node and pin.

### Node resolve

**NR-07: Pin links are not listed (low)**
- **Type:** Limitation.
- **Observed:** The resolve output gives a link count for each pin, but not the linked node and pin.
- **Proposed behavior:** Add the linked node ID and pin name.

### User review

**TD-36: Use input getters instead of long wires from the entry (medium)**
- **Type:** Proposal from the user review.
- **Observed:** The user rated the hand layout of the math graph almost perfect. The user wants the tool to make this kind of result. The one change the user made: the two function inputs were wired from the entry node across the whole graph. The user replaced each wire with a getter node for that function input, placed next to its consumer. The diagonal wire across the condition nodes went away.
- **Expected:** When a function input wire is long or crosses other nodes, the layout uses an input getter beside the consumer instead.
- **Proposed behavior:** Treat function inputs like shared values in TD-35. Split them into one getter per consumer and place each getter in the input column of its consumer.

## 3. Recurrence

- TD-35 repeats TD-32.
- TD-34 matches the TD-30 recipe gap.
- FT-88 still applies: hand positions used capture estimates.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-90 | Replace a function input wire with an input getter beside its consumer | The user did this by hand for 2 wires. |

FT-88 also applies.

## 5. Suggested priority

1. TD-34, TD-35 and TD-36, because they cause most of the hand pass.
2. ED-16 and NR-07.

## 6. Completeness and privacy review

All tools used are listed. Item IDs: X-86, TD-34, TD-35, ED-15, ED-16, NR-07, TD-36, FT-90. TD-36 and FT-90 come from the user review. The report was read back. The local lint was run. Review state stays Pending human review.
