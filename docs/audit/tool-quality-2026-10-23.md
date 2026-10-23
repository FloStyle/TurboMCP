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
| Toolset schema-size report compares whole toolsets with a single-tool 2000-character limit | P2 | Implemented in 5761cbd; live checks passed | Whole-toolset schema size no longer compared with the compact per-tool help limit. |
| Graph-load write-format refusal cites a future phase and old internal documents | P2 | Implemented in 5761cbd; live checks passed | Graph-load write-format refusal names current supported behavior without a future-phase promise. |
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

## Layout and refactor quality batch

Implemented d8a4945: parameter documentation for both preview/edit Tidy, FixBackward, SplitValue, Remove and Sequence (46 properties). Includes numeric layout ranges, JSON-array requirements, reader identities, empty selection behavior and grouping limits.

Source finding: GraphRefactorTools::Run treated an owned UndoTransaction success as proof of complete restoration and manually restored the prior dirty flag. The failure path now reports partial with rollback applied/failed and restoration unverified, does not manually clear dirty state, and asks for inspection before retry/save. This is an honest-reporting repair, not completed restoration verification. Full-property/callback restoration comparison and fault-injected runtime evidence remain open. Affects connect/remove/split/sequence/delegate refactor operations using Run; not the separate batch-edit implementation.

Run 81cf6ab4ac064c32a056dd1090d1eec0 passed build, all-tool inventory and existing help regression. Live missing parameter descriptions: 265. Editor stopped and 16 fixture files restored. The all-tool inventory does not execute the rollback branch; failure-path runtime verification remains pending.

## Persistence help quality batch

Implemented a282c1c: seven compile/save/undo/revert tool contracts rewritten around current behavior, with nine parameter descriptions. Removes obsolete claims that Widget saves draw thumbnails and direct saves only accept /Game mounts. Documents explicit saves of clean-flag packages, per-asset partial persistence, checkout limitations, exact undo GUIDs and 60-second single-use revert confirmations. No implementation behavior changed.

Supervised run 7375735b6928474f97a8c9c3aa739a1a passed build, all 125 live tool inventory checks and existing help tests; 16 fixture files restored. Missing parameter descriptions: 256 of 367. Destructive reload, undo and save failure-path behavioral checks remain pending; this batch does not certify them.

New live finding: compact help truncates parameter descriptions at the first period, including the abbreviation e.g.; save help omits later thumbnail-execution and partial-persistence details. Full schema text is corrected, but compact-help fidelity remains open and needs formatter repair plus assertions for those details.

## Compact help fidelity repair

Implemented 6389df5: parameter descriptions retain their full text instead of ending at the first period; tool summaries avoid dots in paths, decimals and common abbreviations. Compile/save/undo effects precede optional detail. Over-budget help ends at a complete line with an explicit route to describe_toolset for full schema descriptions, rather than silently losing detail or exceeding 2000 characters. This does not yet prove every tool has adequate effect information.

Run 3d3a3ae687c440c988682c5aac2ab5b4 passed build, all 125 tool help checks (including save example and persistence effects) and existing help regression; supervisor restored 16 files. Inspected live save output retains its example, 25-entry limit, thumbnail execution warning and partial-save warning. Runtime mutation coverage remains separate and incomplete.

## Widget preview dimension validation

Implemented e06e5aa: dimensions require decimal digits and bounds 1..4096, checked before multiplication to prevent overflow. Fractions/signs no longer reach Atoi and silently truncate. Parsing and consent refusal precede asset resolution. Five preview parameters now documented. Replaced the false claim that every input is validated before construction: property/function checks occur later.

Run 3e8df026748649778aab9c4beed465d0 passed build and inventory but failed a case-sensitive consent assertion. Test-only correction 6bbf9bd then passed build, focused preview-input checks and all-tool inventory in fa5ac4ce3b9e4cddbf8603e5cf644677. Covers invalid dimensions, consent refusal and valid dimensions reaching JSON validation; does not cover rendering/callback behavior. Existing rendered-suite bound assertion updated in 358d3f7, not rerun.

Workflow exception: reacquiring the original scope for the test capitalization correction refused because its new-file entry already existed; the shell continued and the integrator correction was committed. No worker claim was acquired for 6bbf9bd. Future continuation must classify that file as existing and stop shell execution on claim failure.

## Widget preview result quality and rendered validation

Implemented 4bafe5c: rejected sample values/callbacks, logged errors and render failure produce an explicit partial heading and inspection guidance. Captured error details are now emitted, not just counted. Success reports image output without certifying widget behavior. Removed comments claiming garbage collection reverses external effects or immediately destroys the instance. Supervisor enables rendering for the standalone widget-preview suite.

Run 58f060350d5e4219818acbecb3fbf7ae passed build and rendered test-widget-preview, including actual PNG dimensions at default/requested sizes, invalid-input refusals, and partial results for rejected sample property and parameterized callback. Supervisor restored 16 fixture files and stopped its editor. Logged-error and render-failure injection remain unverified; successful nonempty sample imports and successful explicit callback behavior need separate coverage. The fixture image is simple and does not certify real-project visual correctness.
