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

## Widget sample conversion contract

Implemented e3fb8c2: refuse null, nested objects and arrays before asset resolution rather than converting them to empty text and attempting property import. Strings, numbers and booleans retain the pinned engine conversion; complex property values require engine text-import strings. Removed obsolete Look-tool harmlessness and immediate-destruction claims.

Build/rendered run f1c6b7e6ec0a4ba8a713dee7b1fb7649 passed, including numeric RenderOpacity and boolean bIsEnabled imports reported as 2 set / 0 refused and image success. This is import/pipeline evidence, not visual-value read-back proof. Run 4f6a745f859c4234bfdc29d769a9027e passed focused pre-load invalid sample refusals and all-tool schema/help inventory. Both restored 16 fixture files. Successful callback execution, render-error injection and broader property types remain unverified.

## Reference, override, comment and DataTable contracts

Implemented 0f115e5 / 0d77e81: nine input descriptions with source-checked filters, empty behavior and examples. DataTable help now states the 80-field/60-row display limits and registry/loaded-reader limitations instead of claiming completeness. Columns is required by the live schema even though its empty value omits row values.

Run 50f15d9a0bc6429a85ee503da6d47fe8 passed build, inventory, refs, overrides and comments. DataTable regression failed because its calls omitted required Columns and expected obsolete out-of-scope text. Corrected those calls and the obsolete guidance assertion; run f31ddabb658d499584e3954baf525608 passed DataTable and all-tool inventory, restoring 16 files. Missing parameter descriptions now 242. This batch changes help/tests, not reader behavior. Native nested/overlapping comment boxes and registry-incomplete DataTable reader discovery remain coverage gaps.

## DataTable reader readiness

Implemented f6e3356: wait for registry completion before reader lookup, then refuse to manufacture counts if registry gathering continues. Unavailable/incomplete lookup returns partial with readers unknown and a retry action; any schema already read remains visible. Successful lookup states saved-registry/loaded-Blueprint coverage and possible missing unsaved/dynamic references.

Run 92faded4b0f34fc6b90a96dc3de07d23 passed build and DataTable regression, including the new coverage text assertions; 16 fixture files restored. Missing-registry/incomplete-scan branches are source-reviewed but not fault-injected. This does not certify dynamic reference discovery.

## Material read contracts and budget truth

Implemented 93fdfe6: ten parameter descriptions across five material readers, accepted asset/instance distinctions and actual advisory budget behavior. BudgetNote no longer falsely claims budget-driven subtree reduction; it explicitly reports overrun and offers narrower readers. Hard budget enforcement remains OPEN, not fixed by this text correction.

Run 95e0f3af4afb42a2b688be785c03a0a6 passed build/all-tool help but material tests failed: new call used wrong boolean key and old fixture assumed Emissive remained unconnected. Test correction e7d8b12 uses the exact bIncludeInstanceOverrides field and selects an unconnected output from the current graph report. Run ea243845c48941d5a2cac41d05915130 passed material regression including tiny-budget honest-overrun assertion. Other material reader branches and hard-cap implementation remain incomplete.

## Empty material trace recovery (2026-10-24)

Implemented 01691e3: empty/whitespace OutputProperty refusals now list the actual connected outputs instead of falsely saying none have been listed. Materials without connections explicitly report no connected output to trace. Run eaeeb1cccf494b80a5b698b32e7fa2fc passed build and material regression, including both empty forms checked against current fixture output names; 16 fixture files restored. No-connected-output branch remains source-reviewed rather than independently fixture-tested.

## Material output English (2026-10-24)

Implemented 0de7207: replaced obsolete shorthand and roadmap guidance in material-reader output with plain English and callable ReadTurboMCPMaterialLoad guidance. Unboxed expressions are described as outside comment boxes; empty reader content no longer implies an empty material.

Interrupted observation was recovered: supervisor run f90a6dd24c194dc59159f36c81dd30af completed with a test failure because one assertion expected the old wording. Test correction b3cccb7 checks the numeric expression count with the current wording. Run a59643b22b524e898953813e5fe3572c passed build and test-material at b3cccb7d9acdf0d8dc2ae2e3d0391cb06c815038; all 16 fixture files were restored. This verifies the exercised material paths, not every output branch or all tools. Hard material budget enforcement and the remaining parameter/output review are still open.

## Material findings and recovery wording (2026-10-24)

Implemented 72a485f: removed unsupported claims that missing labels imply disconnected islands, and stopped inferring that empty comment boxes are leftovers. Output now describes observed contents and uses callable Blueprint, widget and material preview names. Simplified token-estimate wording and obsolete reader comments. Supervised run 92e32a97c2424cdc9db06cab8e9e59c3 passed build and test-material; this exercises the fixture's material findings, not every empty-result/wrong-type branch. Those branches remain source-reviewed only.

## Call-log and cost output contracts (2026-10-24)

Implemented a316551 / 0535c38: six parameter descriptions, accurate filter differences and timestamp text-comparison limits, plain-English empty-result guidance with callable tool names. An empty retained log no longer claims no tools have run. CallLog limits now reject fractions/signs and saturate large positive digit strings at 500 without integer overflow. Cost headings no longer use emphatic claims about calls being cheap.

Run 372ef3a59a244b3299f9c8404782bc28 passed build/help but exposed a missing import in the new test and stale text assertions. Corrections a8b9467 / ba53674 preserve refusal checks. Run e79777dcdc2042b6a6bd9c48b1337c92 passed build, call-log regression (including fractional/huge limits) and all 125 help responses; cost failed only its two stale cap wording assertions. Latest live schema count: 226 parameters still lack descriptions. Timestamp validation/normalization is not implemented; documented as text comparison. Broader behavior coverage remains incomplete.

Final run 5abb594a3d784632bbd8337dec435f76 passed build and call-cost regression at ba53674. Together these runs verify the exercised call-log/cost paths; they do not certify every tool's behavior.

## Recording controls and truncation guidance (2026-10-24)

Implemented a5712e5: recording/clear parameters now documented; enabling recording accurately preserves earlier retained records without recovering calls made while disabled. Truncation guidance distinguishes newer Since results from omitted older records. Settings changes are checked in memory, and disk persistence is described as requested rather than verified. Removed obsolete token-estimate document reference and clarified confirmation output.

Next source-review findings: TraceReadTools still recommends obsolete E-TRACE/R-TRACE names and incorrectly describes toolset class membership as a mutation guard. PlayCheckTools reports a clean run based on zero captured warnings/errors; qualify this as captured-log evidence rather than full behavioral correctness. These findings are not fixed by a5712e5.

Run 4079d0fcafdd49029c855a2a44fd83b8 passed build, call-log regression and all-tool schema/help inventory. Restart persistence and settings failure injection remain unverified; the output now states that limit.

## Trace and play-check text (2026-10-24)

Implemented 34ac513: trace refusals use callable tool names; comments attribute the mutation boundary to explicit action checks, not Read class membership. Play-check zero-warning output describes captured evidence without certifying gameplay. Four parameter descriptions cover start/poll, duration, map and trace status inputs. Compact help must retain execution, non-sandboxed effects and no-explicit-save information.

Remaining source finding: an invalid ReadTrace action still reaches shared Plan guidance listing arm/clear/status, although arm/clear belong to the Edit tool. The specific mutation refusal is fixed; general invalid-action guidance needs a follow-up. Existing trace status only reports recorded events; zero hits are not proof of non-execution.

## Trace evidence wording follow-up (2026-10-24)

Implemented d31fc8d: zero recorded hits no longer claim the nodes never executed; output directs users to check IDs and event delivery. ReadTrace invalid actions now recommend status instead of shared edit-only actions, and whitespace means status. Corrected test-trace GraphLoad inputs against the complete live schema (Revision and EntryFilter required; obsolete TokenBudget removed). Compact play help uses grammatical execution/effect wording in d3dfc00.

Run 0866169ac48a4f9293c1f29552c87066 passed play-check (28 checks), but trace omitted Revision and compact help dropped effects after the first sentence. Run 7c84cc5124bd43bdab5acfdcf8c94421 passed all-tool help after the summary correction, but trace still omitted EntryFilter. These are recorded failures, not runtime trace acceptance. Latest trace/help verification remains pending.

Run b6d424f9f8994201b696e831bb055d3d passed build, trace behavior checks and all 125 schema/help checks at d31fc8d. This includes actual fixture event delivery and read-action refusals. Wider trace behavior and other tool coverage remain separate.

## Blueprint lint output and budgets (2026-10-24)

Implemented def08ab: lint input descriptions distinguish estimated-token finding budget from total output; strict integer parsing rejects fractions/signs/overflow. Character-budget arithmetic uses int64. Truncated results retain every check summary, and the header counts checks that actually ran. Mixed asset refusals are marked partial. Findings use callable PreviewTurboMCPGraphDead guidance and avoid certifying assets as defect-free. Removed Read-class harmlessness claims.

Regression now covers invalid and maximum budgets, retained summaries with a tiny budget, and current GraphLoad required fields. Hard total-output caps are not implemented: headings/counts and whole-line overshoot remain explicitly outside the finding budget. Verification pending.

Run 94f6d880550f4e8caee2b1c35b2f2b48 passed build/help and all new budget/summary checks; the old unknown-tool assertion rejected the actual Unknown tool reply. Correction d2c5c16 checks that exact reply and removes blanket exception-as-pass behavior, without claiming a class-level safety boundary. Final run 720fcf76304a47888a886fe8cc9f06a6 passed build and lint regression. This covers fixture lint and disk fingerprints, not every analysis algorithm or arbitrary asset side effect.

## Asset-cost provenance and counts (2026-10-24)

Implemented cc1c570: dependencies with unknown disk sizes remain in their hard/soft/editor-only category counts and are also listed as unknown. Reports describe direct registry dependencies, category overlap and recorded package disk sizes rather than promising runtime load/cook behavior. Empty query results no longer claim to prove the absence of references. Two parameter descriptions specify Cap defaults and its limited display scope.

Regression checks category-count consistency and explicit provenance. Remaining gap: dependency-query success/readiness is not yet represented separately from an empty query result; the output now avoids certifying completeness, but explicit partial/unknown query status still needs implementation and failure-path coverage. Verification pending.

Run c272268814ad496ba5de3152ff3ba0da passed build/help; the asset-cost test misclassified explanatory text as an unknown-size measurement. Corrected in 75bba93 to inspect measurement rows and avoid claiming unresolved-asset coverage proves missing-size behavior. Run fd3340b48ce048ab99080d3f78e30771 passed build and asset-cost suite. The fixture returned zero dependencies, so nonzero/unknown dependency counts remain source-reviewed and need a targeted fixture; the count-consistency assertion alone does not prove that branch.

## Asset dependency query status (2026-10-24)

Implemented fefc6fe: wait for registry completion, return partial/unknown if gathering persists, and check GetDependencies success for each category. Failed queries no longer produce zero category counts; the report preserves available own-size information and names unavailable queries with retry guidance. Removed the public header's class-based harmlessness assertion.

Run 46ceba9ba1fd4995917e4e1c5e285114 passed build and asset-cost regression; 16 fixture files restored. The normal fixture still returned zero dependencies. Query failure, ongoing gathering and nonzero/unknown-size dependency paths remain unexercised and require targeted coverage; passing this suite does not close those coverage gaps.

## Widget-tree reader coverage (2026-10-24)

Implemented 369f5fe: removed the unsupported lossless read/write identity promise and Read-class mutation guarantee. Live output and help describe selected hierarchy/visibility/padding coverage and omitted properties, bindings and animations. Slot index order is no longer equated with complete visual draw order. The no-root result no longer points at an obsolete tool name.

Source finding retained: SlotAssignments rounds nonzero padding to whole units and omits nearly-zero values. The response now warns about it, but precision preservation is still an actionable defect, not fixed by wording. Full widget backup/export and arbitrary write-back preservation are not certified. Verification pending.

Run e83564cffc0848bf984d8ecce1697cfd passed build/help but the widget suite omitted MCP session initialization and failed its first request with HTTP 400; the supervisor restored 16 fixtures. Test setup corrected in f229f45. Run a5f92310d40444978c80c6eeb723458e passed build and widget-tree-sync checks. This verifies the limited reader and coverage message, not full widget write-back identity.

## Widget padding precision (2026-10-24)

Implemented 441b1cb: both widget readers preserve fractional float padding with nine significant digits and omit only exact zero, rather than rounding to whole units or dropping tiny nonzero values. Removed the obsolete rounding warning; incomplete-export and omitted-state warnings remain.

Run 40b3974c53604410b186cef29c1f8361 built successfully. The new native checks for fractional, negative, tiny nonzero and zero padding produced no failures; they execute before the existing graph append test. Widget-tree-sync passed 15 checks; live help inventory passed for all 125 tools. The overall run FAILED: Widget.RoundtripAcceptance later failed “Plan is acceptable” for its graph append. The reason is not yet captured by that assertion and remains an actionable investigation; this batch does not certify full roundtrip behavior. Supervisor restored 16 fixture files. Next: expose the rejected plan diagnostics and investigate without weakening the acceptance check.

## Widget acceptance rendering prerequisite (2026-10-24)

53f70b1 exposes the full rejected append preview in the native acceptance report. Reproduction ddfd086f2cee4be6a658dc002eb1c8c1 identified the concrete refusal: append footprint render target unavailable. The runner had launched NullRHI; measured append placement requires rendering. This is a harness prerequisite failure, not evidence of an append behavior defect.

0329b3f enables rendering for test-widget-roundtrip and selects render requirements from resolved suite names, fixing comma-separated batches and all-suite selection for the listed render-dependent suites. Removed the incorrect comment that all tools work without rendering. Other suites' rendering requirements still need inventory; this is not a complete runner classification audit.

Run ed50a8e6f98c47eea9556d5bbead101d passed build, Widget.RoundtripAcceptance (Success, zero errors/warnings/skips), and widget-tree-sync. All 16 fixture files restored. This resolves the preceding padding/append validation failure and verifies the native fixture's targeted append path; it does not certify arbitrary widget export or every tool operation.

## Append preview wording and effects (2026-10-24)

Implemented dd4224d: replaced blanket “changes NOTHING” with target-graph append status, temporary-node/asset-loading effects, compile/save status and exact PreviewTurboMCPGraphAppend/EditTurboMCPGraphAppend next actions. Explicit-continuation text now distinguishes the plan summary from the public wrapper's scratch validation, rather than claiming validation is absent. Render-target failure gives a concrete rendering prerequisite. Removed the wrapper's false claim that class membership prevents side effects and its claim that all execution failures are unchanged.

Run bcc1e9cb90b440cd8e3d35491cc79010 passed build, widget-roundtrip and widget-append-acceptance, covering accepted and refused plan text alongside graph-preservation assertions. Supervisor restored 16 fixture files. The revised render-target failure sentence and explicit-continuation wording are source-reviewed but not separately forced in this run. Other append rollback text still needs a full evidence audit; this does not certify every failure branch.

Inventory recount from live capture 40b3974c53604410b186cef29c1f8361: 214 parameter descriptions remain missing across 69 tools. All-tool help enumeration passing is not completion of parameter documentation or behavior review. Next documentation candidates include VcsState, ToolSchema, PureChecks and node reader/context tools.

## Node reader descriptions and refusal guidance (2026-10-24)

Implemented 8e870d9: documented eight parameters across NodeResolve, NodeFind and NodeContext, including caps, separators, matching semantics, exact-ID requirements, graph scoping and hop defaults/range. Removed unsupported writer-compatibility and complete-runtime-context promises. Context labels entry nodes rather than claiming to enumerate paths; output states static connectivity limits and requested side effects. Missing graph/node and invalid-hop refusals provide concrete recovery guidance.

Registered focused suite in c74295b after the supervisor correctly refused the initially uncatalogued suite. Run 6aea08c4665f43c6a8146a0f341e84f0 passed build, node-reader-output and all-125-tool help inventory; 16 fixture files restored. Checks cover finding/resolving one fixture node, zero-hop context, stale ID, missing graph, out-of-range hops, empty IDs and no-match search guidance. Ambiguous identities, capped output, pure-node consumer traversal and broader graph topology are not certified by this test. Live missing-description count is now 206 (was 214).

## Diagnostic reader text and query effects (2026-10-24)

Implemented 6f59f84: PureChecks output now uses concise English, removes obsolete R-CALL-COST wording and blanket race/write guarantees, and distinguishes literal string results from verdicts or asset existence. Normal invocation recording is acknowledged. Removed historical narrative from its wrapper. ToolSchema help accurately describes a compact rendering rather than an exact/full JSON schema; its unknown-toolset response names ReadTurboMCPToolIndex. VcsState help describes provider refresh/network effects and unknown state. Five parameters now document formats, empty behavior and limits.

Run c91bf381760d4b0ca96d7520c4f28b64 passed build, 22 PureChecks checks and all-125-tool schema/help inventory. Supervisor restored 16 fixture files. Live missing parameter descriptions: 201, down from 206. This run does not exercise a live source-control provider or the ToolSchema unknown-toolset refusal. ToolSchema's first-match short/suffix resolution remains documented behavior and a candidate ambiguity improvement, not a uniqueness guarantee.

## Blueprint reader coverage and budgets (2026-10-24)

Implemented 0a42fb7: documented all five BlueprintLoad parameters, removed obsolete JSON-unsupported and complete-export promises, and described the existing per-asset JSON sections inside a text envelope. Character budgets are explicitly not token counts or hard whole-response caps. Budget notes no longer claim an oversized minimum manifest fits. Nested detail reduction now propagates the actual returned detail rather than relabeling it signatures. Status distinguishes requested detail and body character count.

Run c9d9a77a0af74d478b39fc3db25bbb6a passed build, focused budget checks and all-tool help inventory; 16 fixture files restored. The fixture's 451-character minimum manifest exceeded a budget of 1 and returned the explicit limit warning without recursion. Zero, negative, fractional and overflowing budgets were refused. JSON behavior and the nested fallback labeling branch were not separately exercised. The existing JSON over-budget error also labels every non-manifest request as signatures; that remains actionable for the next batch. The new help documents the current envelope rather than claiming standalone JSON. Five further parameter descriptions are covered; expected missing count 196, pending recount.

## Blueprint JSON budget refusals (2026-10-24)

Implemented 8012c58: oversized JSON errors identify manifest, signatures, bodies or full correctly, state that no truncated document was returned, and name TokenBudget/GraphFilter recovery options. Removed the false claim that text output always fits its budget. Simplified the related implementation comment.

Run ac9f620e8d0e44cc906e2085c729b71e passed build and the extended budget suite. All four JSON detail levels returned parseable successful documents with a generous budget and parseable error sections with budget 1; errors named the requested level and counted zero successful reads. Existing text-budget and invalid-input checks passed. Supervisor restored 16 fixture files. This validates the tested JSON sections within the existing text envelope, not a standalone JSON response contract or complete Blueprint serialization.

## Layout reader effects and review limits (2026-10-24)

Implemented 9dba3ef: documented eight parameters across LayoutReport, LayoutDoneCheck and GraphPositions, including rendering prerequisites and tolerance limits. DoneCheck help discloses editor UI/image-file effects and does not promise an automatic approval. Wire-length text distinguishes estimated geometry from rendered curves and visual-quality scores. No-findings text now limits its claim to enabled checks. Removed a class-based harmlessness comment. Registered test-layout-output in 25563e5 and included it/test-layout in render-dependent suite selection.

Run afc6da5366464f55ac1a153c3b402112 passed build, rendered layout-output checks and all-tool help inventory; 16 fixture files restored. Verified report/position output and invalid tolerance refusals, including DoneCheck returning before capture on invalid input. Successful DoneCheck image creation, zero-findings wording and complete layout heuristics were not exercised by this focused suite. Live missing parameter descriptions: 188, down from 196 after the preceding Blueprint documentation batch.

## Capture numeric validation

Implemented in 0911913; test invocation corrected in 7998f14. Capture dimensions now require decimal digits in 1..4096; selector hop counts require digits in 0..4096. Overflow, fractions and signs are refused with limits, examples and empty-value defaults. Removed obsolete numeric-parser commentary.

Supervised execution 7d1d3451dba6478abf017efc6684df8a passed the stock-engine build and test-layout-output: 12 invalid-input refusals, 7 accepted-input boundary/default controls, and existing rendered layout-output checks. All 16 fixture files restored. Earlier execution 2965274cbcc047a280089d5711eea7f6 built successfully but the test omitted the schema-required target argument; corrected before the passing rerun. Accepted-input controls intentionally stop at empty-asset resolution: they prove numeric parsing, not successful image capture at each boundary. Full capture-mode documentation and rendering coverage remain pending; overall tool-quality review remains incomplete.

## Capture help and parameter coverage

Implemented in 2260ec9, 047f71b and 852fa6b. Replaced obsolete capture narrative with five parameter descriptions covering mode-dependent meanings, required empty strings, limits and examples. Corrected the false fixed-resolution promise for empty Size. Compact help now retains asset/UI/callback effects, compile/save limits and lack of automatic undo in its first sentence.

Supervised run 5efe58f066964de3a470051e663f17d8 passed stock build and live schema/help inventory for all 125 tools; 16 fixture files restored. Earlier checks caught warnings lost by first-line/first-sentence summarization and were corrected. Parameter-description gaps now total 183. This verifies help presence, not all capture modes or all tool behaviors. Capture support-table prose and selector refusal messages still need review; permissive comment-index parsing is a remaining implementation finding.

## Capture comment-index refusal

Implemented in d084197: both public box capture modes validate Target as a nonnegative int32 written in digits before asset resolution. Fractions, signs and overflow cannot silently select a different comment. Refusals name LookTurboMCPLayoutReport and ReadTurboMCPNodeFind instead of obsolete aliases.

Supervised run 593b3f668e9a4882b3e1b7f50fbf776c passed the stock build and test-layout-output, including 10 malformed-index refusals, zero/max-int32 parser controls for both modes, prior size/hop checks and rendered layout checks. All 16 fixture files restored. Parser controls stop at asset resolution; they do not certify successful box captures or comment ordering. The internal selector still uses its older conversion after the public entry-point guard; support-table and remaining selector prose review are pending.

## Node discovery text and limit validation

Implemented a223890; regression ID extraction corrected in 09dd3cc. Added seven parameter descriptions for node-type discovery and pin inspection, including empty event-graph selection, exact IDs and bounded browsing. Invalid limits now refuse instead of silently defaulting, truncating or overflowing. Pin output no longer claims all asset state unchanged merely because a temporary graph was used; wrappers no longer define Preview as inherently harmless. Replaced obsolete discovery tool aliases.

Run 7812114be7ad4a0386cbcbc42720404b passed the stock build and all-125 live help inventory, but exposed legacy test parsing that truncated complete IDs and scraped guidance prose. After correcting extraction, supervised run dfd8e224f5e4407baf3c4e01525e8b29 passed the build and probe suite: seven invalid-limit refusals, missing-graph refusal, 17 answered types and three real pin lists. All 16 fixtures restored. Missing parameter descriptions now 176. This does not certify arbitrary probe side effects, every node type, or all discovery ranking behavior.

## Trace control text

Implemented d8a77ad. Documented both EditTurboMCPTrace parameters and the clear-all empty-ID behavior. Compact help retains non-undoable history removal. Arming output now reports registry changes, unvalidated IDs and instrumentation dependence instead of claiming every compiled node emits events. Clear output states that removed history cannot be recovered with Undo. Removed obsolete wrapper commentary equating Read tools with absence of state changes.

Supervised run e30d36767c1d42e1b6d0c959ea798d76 passed stock build, all-125 help inventory and 28 trace checks with zero skips, including one actual recorded Blueprint execution hit. All 16 fixture files restored. Parameter-description gaps now 174. This does not certify every instrumentation configuration or prove absent execution from zero hits.

## Checkout help

Implemented 755b7bc. Added both checkout Assets parameter descriptions. Preview help now names forced provider-state refresh and possible server contact, without promising later checkout success. Edit help names source-control/file-state changes and lack of rollback across partial successes. Removed wrapper claims that separate Preview/Edit calls cannot differ.

Supervised run a1ccec98761748d6a422b0258450c499 passed stock build and all-125 live help inventory, including essential checkout warnings; 16 fixture files restored. Missing parameter descriptions now 172. No live depot checkout was executed. Remaining source-control output debt: renderer still contains obsolete aliases, overly broad no-file-write/no-provider-contact statements, and assumes UsesCheckout=false proves a harmless no-op. Those implementation/output paths remain open for the next batch; this entry closes only help coverage.

## Checkout result wording

Implemented 7817a96. Source-control responses distinguish pre-operation state from checkout results and direct callers to refresh afterward. Removed claims that UsesCheckout=false proves a harmless no-op, that provider checks never contacted the provider, and that checkout cannot change files. Per-file results retain provider messages and explain lack of rollback for earlier successes. Updated legacy VCS assertions to the corrected wording without changing provider-result expectations.

Supervised run da7d92160df3466d990ed03d3026016c passed stock build and six empty-input/missing-asset status/refusal checks across Read, Preview and Edit. All 16 fixtures restored. No valid-asset checkout was attempted. Live provider execution, partial successes and the revised legacy multi-provider suite remain unverified; historical comments and less prominent output prose still need review.

## Widget binding validity claims

Implemented bf730f7; test transport decoding corrected in 2cc72b8/24a5ab8. Added six parameter descriptions for binding creation and retarget Preview/Edit. Help states missing-delegate/function permissiveness, replacement behavior, compile/save limits and lack of a dedicated undo transaction. Binding output reports delegate presence rather than claiming complete validity; retarget names unchecked function/signature/runtime behavior. Removed the nonexistent E-WIDGET-BIND-REMOVE recommendation and obsolete save aliases.

Run 9136a8e81dfe4885b0276d427505f39a passed build and all-125 help inventory; the old behavior suite exposed JSON-wrapped text followed by dirty-package telemetry. After preserving that suffix while decoding the response, run a659085cbc0543a79b673e3295064b67 passed stock build and widget-retarget checks (missing input, delegate presence, selective removal, read-back and idempotence); all 16 fixture files restored. Parameter-description gaps now 166. The mixed JSON/text transport remains a separate output-quality finding; this test adjustment does not fix that production formatting. Function compatibility, reliable undo and runtime binding execution remain unverified.

## Dirty-report JSON envelope repair

Implemented 78c9b30. The recorder now appends dirty-package telemetry inside a JSON returnValue string while preserving other envelope fields, rather than concatenating text after the JSON object. Plain-text and error paths retain their existing handling. Removed the widget test decoder workaround; the normal Session client must decode the result and retain dirty_packages telemetry.

Supervised run eeefed0e23d24043afcadac34005fdde passed stock build and widget-retarget behavior, including normal-client decoding with dirty telemetry; all 16 fixture files restored. Offline test-result-contract also passed. An initial mixed suite request was refused because that contract suite is catalogued offline; it was run in the correct mode afterward. The offline result suite does not exercise the recorder itself. Nested/double-encoded envelopes, encoding-failure fallback and final-metadata ordering with telemetry require separate coverage; no blanket protocol certification is claimed.

## Dirty telemetry and final metadata

Implemented 06321cb. For successful transport values containing a final tmcp-result-v1 line, dirty telemetry is inserted before that line, preserving the authoritative outcome rather than displacing it. Existing JSON return envelopes remain valid.

Supervised run 715b27b9b7504d87bfb620cb3e9f034f passed stock build and widget-retarget suite, including a real EditTurboMCPGraphRepoint refusal parsed by call_structured with required metadata and retained dirty telemetry. All 16 fixture files restored. This verifies a refusal carried by successful transport plus ordinary widget output. The transport-error branch still appends diagnostics directly and needs equivalent ordering coverage if it contains result metadata; double-encoded envelopes and serializer fallback also remain unverified.

## Widget variable text review

Implemented in b0203e1: documented all three EditTurboMCPWidgetsAsVars parameters; replaced obsolete save aliases and preview-equivalence claims with current compile/save actions and explicit skeleton regeneration, editor callback and undo limitations. Removed historical roadmap commentary. Missing-name refusal now describes unchanged widget flags rather than claiming all state is unchanged.

Supervised run 4281ad84782f41c8b602d3af24588f0d passed the stock-engine build, test-widgets-as-vars and the 125-tool live help inventory; all 16 fixture files restored. The focused suite covers flag transitions, idempotence, missing-name refusal, reader-visible state and revised messages. It does not prove fresh disk reload, undo, or every callback effect. Overall tool-quality review remains incomplete.

## Override creation text review

Implemented in 47472de: documented the four parameters across Preview/EditTurboMCPOverrideAdd; compact help includes loading or edit effects, undo, compile and save limits. Event preview now describes the event node at the fixed origin instead of promising a result node. Replaced obsolete compile/save aliases and made editor-null-return messages explicit that state and rollback are unverified. Simplified obsolete duplicate-creation commentary.

Supervised run bfae987246f049419105ae01c326df68 passed build, test-override-add and the 125-tool help inventory. The focused suite checks event creation, existing-override refusal, reader-visible counts, removal and revised messages. It does not validate function-override creation, undo, fault-path messages or fresh disk reload; these remain coverage gaps. Overall quality work remains incomplete.

## Override removal text review

Implemented in b789b98: documented both parameters and the destructive scope in compact help. Event deletion leaves other nodes, potentially disconnected; function deletion removes the entire graph and does not repair callers. Results explain undo transactions, skeleton/callback effects, compile/save limits and current follow-up tool names. If the editor still reports an implementation, output explicitly states that removal is not fully verified. Obsolete roadmap commentary removed.

Supervised run ca8f4d6a74fb4dc0b40f33eb2994aa1d passed build, event override creation/removal and revised message assertions, plus live help inventory for all 125 tools. Function removal, undo restoration and partial-failure paths were not exercised by this suite; those coverage gaps remain open.

## Function creation quality review

Implemented in d543702: four parameter descriptions, compact execution/compile/save/undo limitations, current tool names and removal of guaranteed reference-resolution claims. Name-mismatch, missing-entry and missing-result cleanup now check graph membership before claiming graph removal; messages distinguish graph cleanup from restoration of other asset effects.

Run 24fb97549c434f89ad0c00fd909997a7 built successfully but exposed an obsolete test parser: manifest graph names now include kind annotations. Corrected in 1971a93 while preserving exact-name comparisons. Rerun 542d31e12ed845e4bff44416d675fd2b passed build, test-function and all 125 live help checks. Function creation/removal, signatures and ordinary refusals were exercised. Cleanup-failure injection, caller reconstruction losses and undo remain unverified; parameter type documentation still needs full container syntax coverage. Overall quality work remains incomplete.

## Function type syntax and removal follow-up

57967b6 adds accepted primitive, struct, container and reference forms to function input help, including nested-container and hashing restrictions. Function removal now documents both parameters, whole-graph deletion, editor/callback effects, unrepaired callers and bindings, and compile/save/undo limitations. Removed the false promise that a deleted name is immediately free.

Run 8327a8b8189849d380fab547a58856b5 passed build, test-function (including explicit deletion-message checks) and all 125 live help checks within the compact-help size limit. Type syntax was checked against the parser; this run does not exercise every type form or prove undo/rollback. Prior fault-path and reconstruction coverage gaps remain.

## Variable creation text review

576fb0b documents both tools and four parameters, removes the false compile-once/no-partial-change promises, explains that preview does not validate the target or existing names, replaces obsolete save guidance, and reports missing read-back names as partial. The empty-input example now uses supported default=value syntax.

809c7a73ed01459a96f2e6906f040d98 passed build and 125-tool help inventory but the variable suite expected obsolete conflict wording. 99d949f checks all three conflicting names and the no-add statement directly. Rerun 29b7f008dfbd4046a1ee2b356b5dea28 passed. Fixture variables already existed: these runs prove preview, conflict/refusal messages and existing-state read-back, NOT fresh creation or partial-failure behavior.

Remaining source findings: variable type resolution has separate legacy container/type handling rather than the shared parser throughout; inspect fallback and hashing behavior. The private option sets EditConst rather than proving Blueprint private access semantics. Multiword tooltip parsing commentary contradicts token handling. Fresh-fixture creation, defaults/flags and partial-add checks remain required; this family is not behavior-certified.

## Variable private access correction

838efdc/cfaf7ac replace the incorrect CPF_EditConst implementation of private with FBlueprintMetadata::MD_Private using the same Set/RemoveBlueprintVariableMetaData APIs as BlueprintDetailsCustomization::OnPrivateChanged. Metadata calls occur after direct description edits because they regenerate the skeleton. This changes private access without incorrectly making the property edit-constant.

Supervised build 321860a132cd4f96811e66ce00379c3d passed. Implementation is source-verified against the engine checkbox, but live private/public access and independent editability checks are still pending. The existing variable suite only encounters pre-existing variables and cannot certify this fix. Keep this correction flagged untested for behavior until a fresh disposable fixture checks metadata and generated-property flags.

## Fresh private-variable verification

3868fe4 adds TurboMCP.Variables.PrivateFlags and its supervised wrapper, registered in 21034c7. Creates a unique transient Blueprint and uses production Plan/Apply to add private/public integer variables with instanceEditable=true. Checks fresh-creation count, Blueprint metadata, skeleton-property private metadata, absence of EditConst and preserved instance editability. Fixture is never saved.

Run fc876fb4528f491cb163418a0cd77060 passed build and native regression with no reported errors or warnings. The private metadata/editability correction now has focused live evidence; it is no longer build-only. Full compilation, external caller access, save/reload, every variable type and partial-failure behavior remain outside this check.

## Material tidy effect and outcome text

16f8169 documents all four parameters and explicitly discloses that planning/preview recompiles a material for baseline errors. Force is a presence flag, including nonempty false/0 strings. Removed no-effect and exact-rollback claims; failure output now says undo was requested and restoration remains unverified. Success output distinguishes identity/error checks from proof of identical rendered output. Obsolete wrapper documentation removed.

Run cc166c05b88749f19146ff2edc55dac0 passed build, test-material-tidy and all 125 live help checks. This provides focused layout and message evidence, not failure-path rollback verification, every material family or pixel-equivalence certification. Material label/block tools still need the same source-backed text review.

## Material label promotion text

487d4f5 documents both asset parameters, preview compilation/callback effects and edit additions/rewiring. Removed exact-rollback and rendered-equivalence claims from help/results and obsolete wrapper commentary. Failure text reports requested undo with restoration unverified; success text identifies expression/error checks rather than claiming equivalent rendering.

Run c2ef1f85f0294487b38f0cea19887d79 passed build, test-material-label-promote and the 125-tool live help inventory. This is focused fixture evidence, not rollback-failure injection or equivalence across all materials. Material block-tool text remains pending.

## Material block cleanup text

bffe9b5/5c193aa document both asset parameters, preview compilation and destructive edit scope. Rollback success wording now explicitly covers expression identities only, not links, values, layout or other state. Failure messages direct the caller to inspect rollback evidence before retrying or saving. Success reports bounded checks rather than rendered equivalence. Removed obsolete wrapper commentary and user-facing D-17 references.

7798dadc12dc4f3fa9a87e2727253935 passed build, test-material-blocks and all 125 live help checks. Final failure-wording cleanup compiled in be4dac61b6fb44969dbd071248efb74c. Failure injection and full restoration remain unverified; no broad material behavior certification is claimed.

## Pin-default text review — 2026-10-24

Implemented in `05e9f4c`: documented all five EditTurboMCPPinDefault parameters, exact identity matching, literal formats and empty-value behavior. Compact help retains transaction, dirty-state, schema callback, object-load and explicit compile/save information. Failed read-back no longer claims unchanged/complete restoration merely because UndoTransaction returned success; it reports partial, unconfirmed restoration and asks the caller to inspect before saving or retrying.

Validation: supervised execution `4c8daae69c974351a0bce430a028daa7` passed the stock-engine build and live schema/help inventory for all 125 tools; all 16 protected fixtures restored. This is text/schema coverage, not pin-default behavioral certification. Success/refusal and callback-induced read-back failure/undo paths still need focused behavioral coverage; full restoration is deliberately not claimed.

## Local-variable promotion review — 2026-10-24

Source a7480b0 documents both parameters for both tools and removes obsolete aliases and the unsupported promise that input tracing proves equivalent behavior. A critical defect was found: the mutation replaces a calculation with an unassigned local getter. Candidate edits now refuse before mutation as a temporary safeguard. Full repair remains open in todo/67f00521.md; this is not a completed promotion implementation.

Execution d1d3700465734138aec566b36a1d63c3 passed build and the 125-tool live help inventory. test-local-vars failed because its FunctionAdd setup omits required inputs/outputs. Its remaining no-candidate/refusal checks do not cover candidate promotion. Sixteen fixtures restored. Next work: repair promotion semantics and add a real native candidate fixture; fix setup separately without weakening assertions.

## Local-variable test setup — 2026-10-24

f82bd8f repairs the required FunctionAdd arguments, checks the exact existing function before reuse, and removes unrelated suffixed-graph deletion from recovery. Supervised run d97875583b254f95a8b0953920b5acae passed build and test-local-vars. Coverage remains graph selection, no-candidate responses and invalid-input refusals; no candidate transformation is certified. P0 todo/67f00521.md remains open. Source inspection of stock DoPromoteToVariable2f confirms output promotion creates a setter, supporting the missing-assignment diagnosis.

## Enum creation text — 2026-10-24

56f2d7d documents both parameters on Preview/EditTurboMCPEnumAdd: package-path form, separators, identifier rules, sequential values, display-name defaults and empty-input behavior. Compact help states creation/dirty state, callbacks, absent tool-owned undo and explicit compile/save behavior. Preview no longer guarantees creation. Partial creation is labeled partial, unsupported causal claims are removed, and save guidance uses EditTurboMCPSave. Obsolete wrapper references removed.

Supervised run fca73020a7b84f4084f0c52f04d1cf7c passed build, test-enum-add and the 125-tool live help inventory; 16 fixtures restored. The enum fixture already existed, so the suite exercised collision refusals and invalid-input checks plus existing-file inspection, NOT fresh creation or injected partial failures. Fresh creation, display rejection and malformed package-path behavior still require dedicated coverage; Plan currently validates only leading slash and nonempty asset name for paths. These limits remain open under the overall tool-quality review.

## Enum package-path validation — 2026-10-24

Implemented a0f7e93: shared planning validates paths with stock FPackageName::IsValidLongPackageName before package creation; refusal explains the rejected path and accepted package syntax. Added eight live negative checks across preview/edit for object suffixes, spaces, file extensions and an unknown mount. Supervised run 3bba19978187486187aec1e1a31134dc passed build and test-enum-add, including all eight checks. This closes the malformed-path gap identified in the preceding review, not fresh creation or partial-failure coverage. Path syntax/mount validation does not establish filesystem write permission or successful saving.

## Fresh enum creation — 2026-10-24

Native regression 61e3189, catalog 48b3854: uniquely named unsaved enum created through production Plan/Apply; independent UObject inspection checks three names, consecutive numeric values, requested display text, entry count including sentinel and dirty package state. Preview absence and no-object creation are checked; a second request is refused and preserves count, first name and display text. Owned asset registry entry/object/package are cleaned up without saving.

Supervised run 220866a29f8842609306acaca73a7962 passed build and TurboMCP.Enums.FreshCreation (one native test, zero failures). This closes the fresh in-memory happy-path coverage gap from the earlier existing-fixture suite. It does not certify save/reload, all display-name inputs, injected partial creation failures or every collision invariant. Those remain separate coverage gaps.

## Struct creation text — 2026-10-24

76707d3 documents Path/Fields on both creation tools, including options, empty values and examples. Compact help retains type-loading effects for preview and transaction/compilation/dirty/save effects for edit. Preview no longer promises creation. Partial field creation is labeled partial, save guidance names EditTurboMCPSave, and results explicitly limit verification of defaults and metadata. Historical wrapper promises removed.

Supervised run 22c3cc43cec544369119e63dbc3ecfc7: build and 125-tool help inventory passed; test-struct FAILED six assertions. Two field-list assertions parse dirty telemetry `newly_dirty` as a field; loaded-reader checks miss the current wording and incorrectly assume zero dependents despite one listed Blueprint; Map and force checks expect older refusal wording. These require test repair with independent assertions, not relaxed acceptance. This run is not full behavioral certification. Defaults/metadata application return values and partial field-creation cleanup remain source-review findings requiring follow-up. Fixture restoration is recorded in the supervisor report.

## Struct suite parser repair — 2026-10-24

ac1ac78 and 2e1b04b restrict field parsing to the declared field section, parse the current loaded-dependent count, avoid treating an unknown count as zero, and align Map/force negative assertions with the actual shared diagnostics. No behavior assertions were removed. Intermediate run c720bb7d1fc64c739b0b2ec85bc8d6d9 resolved five failures and exposed the remaining stale Map expectation. Final supervised run 20dab17698624afe8ec93adfc5222a00 passed build and test-struct, including the loaded-reader removal guard. This supersedes the six assertion failures recorded above; it does not certify all defaults/metadata, rollback, or save/reload semantics beyond the suite's checks.

## Struct field editing text — 2026-10-24

323ddb5 documents Struct/Changes for preview and edit, including add/change/reorder/remove syntax, empty options, repeated-target restriction and exact loaded-dependent force counts. Compact help states asset loading/status queries versus destructive mutation, undo transaction, checkout, compilation, callbacks and no save. Failed operations no longer invent concurrent modification as their cause; partial output states that changes may remain and no rollback was attempted. Verification limits are explicit and checkout reports use current tool names.

Supervised run 0f02d2f1e1d64a0c9ad169c5994044d9 passed build, test-struct and the 125-tool live schema/help inventory. This covers the suite's normal field operations and refusal cases, not failure injection, all metadata/default setters or provider-specific checkout failures. Overall tool-quality review remains open.

## DataTable row text — 2026-10-24

Implemented dd45e7f: both row tools document Asset/Rows, array grammar, imported literals, new/existing-row defaults, empty behavior and refusals. Compact help retains loading/import effects, destructive removals, transaction, callbacks and no explicit compile/save. Rollback coverage is bounded to verified row state; external callback effects remain unverified. Missing-row-struct diagnostics no longer assume deletion as the cause.

Run 62f93dc2bb3c44a9b8c8a3e25a9c5eee passed build and the 125-tool help inventory. test-datatable-rows FAILED: ReadTurboMCPDataTable setup omits required columns; FIELD remains None and run_rules_checks raises AttributeError. It also uses a Datasmith engine-content table, so follow-up must use an owned disposable fixture rather than extending writes on that target. No row-edit behavioral certification from this run. Prior inventory had 118 missing parameter descriptions; this batch adds four descriptions. Remaining behavior and documentation review stays open.

## Disposable DataTable behavior check — 2026-10-24

fcd4809 adds TurboMCP.DataTable.OwnedRows and its wrapper; d5c9508 registers the suite. It creates a uniquely owned temporary table with the kit's native row struct, runs production Parse/Plan/Apply, and independently checks changed Count, retained Weight/Label, added row, named removal and final count. Planning preserves row values/count. An unknown-field plan is refused by Apply and the existing value remains intact. Nothing is saved; only the owned temporary objects are cleaned up.

Supervised run 6be054e652d54c118bb7956e32b50970 passed build and the native regression. This supplies focused behavior evidence without engine-content writes. It does not repair or certify the legacy test-datatable-rows setup, all value types, injected callback failure/rollback, undo replay or disk persistence; those remain open coverage gaps.

## Material-instance editing quality — 2026-10-24

cbdee23 documents Asset/Overrides and the texture-only contract, loading/render-update effects, absent tool-owned undo and no save. Duplicate requested names and ambiguous declared parameter names now refuse before mutation. Effective texture pointers are read back before success; a mismatch or post-write report failure is partial with no rollback claim. Reader text no longer contradicts override support and references ReadTurboMCPMaterialLoad by its current name.

Supervised run 8388f5113e9e4161aa292df20f883cd1 passed build and the 125-tool live help inventory. The new duplicate/ambiguity/read-back branches are implemented but behaviorally untested; a disposable material/instance fixture is still needed. Sampler validation remains limited to color/linear sRGB checks, and rendered correctness is not certified. Overall review remains open.

## Graph Check wording and verification limits — 2026-10-24

05994d4 documents all three inputs and retains asset-loading effects in compact help. A clean result now says that these checks found no issues; it explicitly does not certify writer acceptance, compilation or behavior. Unjudged values require inspection before writing. Diagnostics use the current reader name and no longer imply that Preview classification guarantees absence of side effects.

Supervised run c2040b0878e84c419548618bee4ead9c passed build and the 125-tool live schema/help inventory. test-graph-check FAILED eight assertions: its closest-ID expectation is stale, GOOD places a closing bracket inside a line comment, missing-graph checks consequently hit syntax refusal first, and GraphLoad requests omit required revision. This is not passing behavioral evidence. The suite also incorrectly claims Preview membership proves no writes, and its after-only memory check does not establish unchanged graph state. Follow-up must repair inputs and compare before/after graph state independently, preserving fault-count/location assertions. The supervisor reports restoration of all 16 fixture files. Overall tool-quality work remains open.

## Graph Check regression repair — 2026-10-24

af7879d updates the controlled PrintString type ID, moves the closing bracket outside its line comment, supplies current reader inputs, and compares graph-reader output before and after the checked inputs. Registration assertions no longer claim that Preview membership proves safety. Existing diagnostic count, kind, location and closest-ID assertions remain intact.

Supervised run d7936c9f5d83485b97dfbb4b82e6c5a3 passed build and test-graph-check. This resolves the preceding eight suite failures. Evidence covers the controlled valid/invalid inputs, unchanged fixture disk fingerprint and unchanged graph-reader output; it does not prove every in-memory property, callback effect, writer acceptance or runtime behavior. Overall quality review remains open.

## Reroute tool wording and dirty-state safeguard — 2026-10-24

066ba06 documents both tools' inputs and graph-name fallback, with loading versus transaction/rewiring effects in compact help. Results no longer promise identical compiled behavior, no remaining crossings, or full restoration from crossing counts alone. Failed routing and rollback are labeled partial. An unverified rollback retains dirty state rather than hiding possible unsaved changes. Preview does not guarantee edit success.

Supervised run 2036b622edff4ead9e19f6f80fca15ac passed build and all 125 live help/schema checks. The dirty-state change and failure branches remain behaviorally untested. Follow-up is required for exact link restoration/read-back, callback pointer lifetime, transaction ownership and failure injection on disposable graphs. Existing heuristic geometry and historical implementation comments also need further review. This is a partial quality improvement, not certification of reroute safety or completion of the overall review.

## Reroute behavioral evidence and fixture limitation — 2026-10-24

7d2001f repairs current reader arguments, rejects error responses as clean-graph success, requires observed node counts, and moves baseline compilation before edits. Both compile results must now be successful; matching failures no longer pass. Comments distinguish crossing counts from restoration and compilation from runtime equivalence. 83fe1a5 fixes a tuple-formatting exception in the new test diagnostics (not rerun yet).

Supervised run fd4b7fd252f54c29b1fa1c84cac451ae built successfully. The live reroute edit added one node (16 to 17) and reduced estimated crossings from 7 to 5, matching the five crossings on skipped wires. Negative-input checks passed. The suite FAILED: the acceptance asset already has seven compilation errors before editing, and result printing then raised TypeError (fixed in 83fe1a5). No passing compilation or rollback evidence is claimed. Next stage needs a dedicated compiling disposable reroute fixture and failure-injection coverage; do not relax the compile assertion to accept this fixture's errors. Remaining live output still overstates geometry precision and needs correction. Overall review remains open.

## Reroute geometry explanation — 2026-10-24

687d7d4 fixes a false diagnosis in preview text: bFootprintUnknown represents missing measured knot dimensions, not proof that all graph nodes have never been drawn. Output now names the fallback reroute size, explains straight-segment/node-bound estimates, and makes no promise of exact rendered outlines or absence of visual crossings. Supervised build d198781d25e94bedb34ba4d919c98b0c passed. This text-only correction was not live-output tested; prior behavior gaps, including the invalid compilation fixture and rollback verification, remain open.

## Unused-node removal text — 2026-10-24

05c6e11 documents both inputs and graph fallback, preserves loading/destructive deletion/no tool-owned undo/no compile/no save in help, and describes static selection without claiming runtime safety. Partial removals are labeled partial, save guidance uses EditTurboMCPSave, and zero-removal output distinguishes missing keys from protected entries. The explicit dirty mark now runs only when a node was removed; loading/callback side effects are not ruled out.

Supervised run 025a7eb1f2fc456ba5f9d88103272168 passed build and all 125 live help/schema checks. No-op dirty-state behavior and removal paths remain behaviorally untested by this run. Full removal identity verification, transaction ownership and static-selection limitations require follow-up; this does not certify destructive-operation safety or finish the overall review.

## Group placement and movement text — 2026-10-24

a90d652 documents 15 parameters across placement preview and movement preview/edit: exact IDs, selection expansion, signed coordinate offsets, empty behavior, overflow and destination-comment constraints. Compact help preserves asset loading/transient widget measurement and edit transaction/dirty/compile/save effects. Placement explicitly describes heuristic search limits and movement preview identifies the result as proposed rather than applied.

Supervised run 7d74d30fdd784e9eace75c15971eb4de passed build and the 125-tool live help/schema inventory. This batch changes documentation/output, not movement algorithms; it supplies no new behavioral proof for comment membership, collisions, callback effects, undo replay or layout quality. Those remain separate review/coverage requirements.

## Delegate-binding tool text — 2026-10-24

ae4582a documents all 24 parameters across BindFunction/BindNewEvent preview and edit. Help explains exact pin selection, paired object context, empty values, naming/signature restrictions, replaced input connections, transient simulation and edit undo/compile/save effects. New-event output states that the body is empty and names the next actions; function binding limits verification to selected function/links rather than runtime correctness.

Supervised run 64366a43ae484470bb9c02f3e979b965 passed build and all 125 live help/schema checks. This text batch supplies no new behavioral proof of delegate compatibility, failed-edit rollback, callback lifetime, runtime execution or generated layout. Overall review and those coverage requirements remain open.

## Graph replacement help and remaining description inventory — 2026-10-24

53a79f1 documents six replacement parameters and explicitly describes whole-named-graph removal of omitted eligible nodes, protected node classes, complete input requirements and edit undo/compile/save effects. Preview no longer claims that nothing can change, and wrapper comments no longer claim toolset membership prevents writes or that Apply performs no checks.

Supervised run ab42a383ffaf4224bda493ccaffc75cd passed build and all 125 live help/schema checks. This text batch does not certify replacement rollback, deletion identity, callback effects or runtime behavior. The preceding live inventory (64366a43ae484470bb9c02f3e979b965) contained 62 missing parameter descriptions across 12 tools; this batch supplies six. Remaining families are Repoint, Copy, Collapse, Comment and BatchEdit (preview/edit). Presence of descriptions is not proof of semantic accuracy or complete behavioral review.

## Graph copy help — 2026-10-24

ad8312c documents 12 parameters across copy preview/edit, including existing destination requirements, selection boundaries, empty-selection behavior and the mandatory empty anchor value while anchor wiring is unsupported. Compact help retains asset loading, edit transaction and no compile/save. External unresolved links and required target members are explicit. Wrapper comments no longer imply matching plans guarantee successful apply or toolset membership prevents side effects.

Supervised run 338a84949d6e4381abdb87c9bcb1af91 passed build and all 125 live help/schema checks. This documentation change adds no new copy behavior evidence. Link/default preservation, failed-copy restoration, reference resolution and resulting runtime behavior remain separate verification requirements; overall review stays open.

## Collapse support-limit text — 2026-10-24

d9e040a documents ten parameters and puts the existing unsupported-collapse limitation first in compact help. Selection analysis no longer starts with 'would change'. The apply refusal states that nothing was extracted or created and points to manual editor extraction followed by fresh graph reads, without historical engineering speculation. This does not implement collapse or close its feature gap.

Supervised run 295bd253016d40d2aa8757785065959a passed build and all 125 live help/schema checks. No new collapse behavioral acceptance is claimed. Existing selection-analysis validity and unsupported-operation refusal coverage remain separate verification work; the overall review remains open.

## Repoint input contract — 2026-10-24

42ae363 documents all 16 parameters across preview/edit. Both selected pins are outputs; the operation redirects consumers rather than connecting an output to an input. Help explains existing-target precedence, probe creation, source retention/deletion, empty boolean defaults and the absence of input/default transfer. Compact descriptions preserve loading/probing versus transaction/deletion/compile/save effects.

Supervised run 634d5f0242654fdd82cb524889f44a80 passed build and all 125 live help/schema checks. No new behavior evidence is claimed. Follow-up source review must address rollback=complete claims based on UndoTransaction's return without independent full-state comparison. BatchEdit's existing example also incorrectly uses an input target pin for repoint and must be corrected in its text review. Overall quality review remains open.

## BatchEdit text — 2026-10-24

6c59f44 documents Operations for both tools, including JSON wrapper/entry rules, required and optional fields, one-graph scope, boolean defaults and refusal cases. The example now selects two output pins, matching repoint semantics. Preview reports no created undo transaction; compile/save statuses say not run/not requested rather than implying automatic deferred execution. Success asks for graph inspection and compilation without claiming runtime verification.

Supervised run 49a3d507b7224e2f9e6f1a50b6206b97 passed build and all 125 live help/schema checks. No new batch behavior evidence is claimed; sequential staging equivalence, failure rollback/read-back and complex callback behavior remain separate coverage requirements. Overall review remains open.
