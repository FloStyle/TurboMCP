# Test content

This folder holds the test asset kit that `06_ACCEPTANCE.md` section 1 defines.
The kit is what the plugin's tools are measured against.

## How these assets must be made

**By hand, in the editor, not with the plugin's own tools.** `06_ACCEPTANCE.md`
says this directly, and the reason is circular otherwise: a tool tested against
assets a tool built proves nothing.

## Status

**Empty.** None of TA-1 to TA-13 exist yet. The plugin is being compiled for the
first time, so no test run has happened. `TestRecord.md` in the plugin root is
where results and baselines are written.

## What has to be built

| ID | Asset | Key requirement |
|---|---|---|
| TA-1 | Actor Blueprint, hand-laid function | 60 to 90 nodes, 6 to 8 comment boxes with one nested, a sequence, three branches, shared getters, four-node pure chains, two local variables, a split struct pin, a promotable operator, an enum literal, a call with several exec outputs, zero backward wires |
| TA-2 | Same Blueprint, event graph | 3 events, 1 custom event, boxes inside boxes, a timer or delay |
| TA-3 | Same Blueprint, "messy" function | Built with the **engine's own** MCP write tools: one long row of about 13 nodes, no boxes, plus an unwired copy of 9 of them that reaches no entry point |
| TA-4 | Same Blueprint, "stacked" function | Two exec nodes in sequence with the second under the first, a getter under the setter it feeds, a second copy under a branch, one node at the origin linked far away |
| TA-5 | Material, about 60 expressions | At least 8 named reroutes, 3 comment boxes as blocks, a value crossing two blocks with no label, a label read inside its own block, two usage nodes of one label in one box, lerps with constants on unlinked inputs, a value used in three places, a node that is both a label source and used directly, one parameter reaching no output, plain reroute dots on a long wire |
| TA-6 | Material instance of TA-5 | Two overridden parameters |
| TA-7 | Widget Blueprint, "card" | A text, an image, a button with a bound click event, a hidden text with non-zero slot padding, a widget without the variable flag that still has a bound event, an instance editable text variable |
| TA-8 | Widget Blueprint, "page" | Two copies of TA-7, one with the class default and one with its instance editable variable overridden; a `ReturnButton` child of class TA-7 with a bound event and a call into it |
| TA-9 | Widget Blueprint, "card alt" | Same click delegate as TA-7, different set of functions |
| TA-10 | Data table with a user struct | 3 rows, 4 fields, read by TA-1 with a row lookup |
| TA-11 | User enum asset | 4 values |
| TA-12 | Name collision pair | Two Blueprints whose initials match, each with a function `Setup` |
| TA-13 | Blueprint whose name is also a folder name | Tests path resolution |

TA-3 is deliberately built with the engine's tools. The others are hand-made.

## Baselines to record once built

`06_ACCEPTANCE.md` requires the node count, comment box count and total wire
length of each asset to be recorded in `TestRecord.md`, and checked before every
test run. A baseline that does not match means the asset was changed and must be
restored before testing, otherwise a passing test proves nothing.

## Where the tests run

The plugin must be testable in any project, so the kit lives here in the plugin
rather than in `TestProject`. The plugin's `.uplugin` sets
`CanContainContent: true`, which is what lets this folder ship.
