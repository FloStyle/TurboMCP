# Tool quality audit and repair ledger

## Scope and evidence

Goal: reliable behavior and accurate descriptions, parameters and responses across all 125 registered TurboMCP tools. DSH paused; no subagents; stock engine.

Live inventory run `91838362a8be4b14bd2fc5817a6fd36b` at `11636b1`: 31 Read, 6 Look, 39 Preview and 49 Edit-toolset entries (including the read-only edit-readiness query). All 125 descriptions and compact help responses were collected. None exceeded the 2000-character compact help budget. Of 367 input properties, 366 lacked descriptions. This is a content defect despite successful transport and budget checks.

Source inventory extracted 4419 text fragments from 106 non-test C++ files. Extraction and pattern screening are not a line-by-line semantic review or proof of every response branch. Runtime mutation, rollback, compile and persistence behavior remain separate verification work.

Local raw evidence: `.local/executions/91838362a8be4b14bd2fc5817a6fd36b/02-suites/test-tool-text-inventory.log`. Workspace audit copies are in `outputs/tool-quality-audit/` outside the repository; the repeatable collector is `Tools/test-tool-text-inventory.py`.

## Findings and remaining work

| Finding | Priority | State | Next action / acceptance |
| --- | --- | --- | --- |
| Compact widget preview help omits execution warning; Look class claims no effects | P1 | Implemented in 1204b10; build and live help checks passed | Live help must retain consent and non-reversible project-code effects. |
| Revert preview and edit descriptions blur which call discards changes | P1 | Implemented in 1204b10; build and live help checks passed | Live preview says no discard; edit says all unsaved package edits and current confirmation. |
| 366 parameter descriptions absent | P1 | Open; existing issue 0e4658ad | Add source-backed @param documentation, formats, defaults and empty semantics; verify live schemas, not source text alone. |
| Compact help retains only the first description sentence | P1 | Partial; special execution/revert warnings repaired | Review each tool's essential conditions; preserve them within budget with a clear full-help path. Do not silently lose safety or input grammar. |
| Toolset schema-size report compares whole toolsets with a single-tool 2000-character limit | P2 | Open | Correct the metric and wording in TurboMCPToolsets.cpp; do not report a false budget violation. |
| Graph-load write-format refusal cites a future phase and old internal documents | P2 | Open | State supported format and usable next action without a delivery promise. |
| Window-list response says image writing is a later phase | P2 | Implemented in 1204b10; build and live help checks passed | Point to current LookTurboMCPCapture. |
| Header explains wrong U-prefixed registry names and impossible class-level safety guarantee | P2 | Implemented in 1204b10; source reviewed | Continue removing obsolete historical commentary; keep current constraints and reasons. |
| GraphLoad/LayoutReport summaries lead with an option, not their purpose | P2 | Open | Action first, scope second, options in parameter documentation. |
| Some long comments narrate old repairs instead of explaining present invariants | P3 | Open | Simplify in touched code; preserve evidence in audit records, not repeated prose. |

## Tracking rules

Parameter and help reconciliation stays under existing issue `0e4658ad`; execution-side-effect documentation links to `3be553c8`. Do not create duplicate bug counts for each missing parameter. Implementation state and validation state remain separate. The overall goal remains open until source and runtime evidence cover the requested behavior and text scope.

## Safety-help validation

Run `955012b68a36431fb09359efceb76196`, candidate `1204b10`: build passed, all 125 tool help responses inventoried with execution/revert assertions, existing test-agent-help passed. Supervisor restored 16 fixture files and stopped the editor. This verifies published help, not destructive-operation behavior.

## Discovery and graph-read parameter batch

Implemented in 5761cbd with empty-type-search correction 1cbd470: 15 parameter descriptions across ToolHelp, AssetSearch, AssetDescribe, TypeFind, DirtyList and GraphLoad. GraphLoad adjacent documentation blocks merged so live discovery includes the actual purpose and paging contract. Corrected whole-toolset schema-size wording and removed a future-phase promise from unsupported graph-write-format refusal.

Run dc45a50acd664f1d8a039ddab4e4965d passed build, all-tool live inventory and existing help checks on 5761cbd. Live inventory has 351 missing parameter descriptions remaining (down from 366). Final corrected revision 1cbd470 passed build and all-tool live inventory in run d22eb9ca7dc84827bbea375f15b93729; editor cleanup and fixture restoration completed. No claim that all parameter semantics or behavior paths have been runtime-tested.

## Graph mutation parameter batch

Implemented 46a56a2, formatting follow-up 6e876ed: 40 additional descriptions across append, patch, swap-call, connect and position tools. Documents exact identifiers, pin directions, keep-old options and continuation policy. Restores append purpose hidden by adjacent documentation. Removes unconditional preview-to-edit acceptance and class-based safety claims from the touched public contracts.

Run 8c48bedeaaf4497589c5808bc237e2e9 passed build, all-tool inventory assertions and existing help checks. Live missing-description count is 311 of 367, down from 351. Final formatting revision 6e876ed passed build and all-tool live inventory in run aae37409f1ce4753941e49740a52ced3; owned editor cleanup completed. These checks validate discovery/help, not the mutation semantics themselves; focused disposable-fixture behavior checks remain open.
