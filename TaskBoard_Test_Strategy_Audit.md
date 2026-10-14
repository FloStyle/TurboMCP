# Task board audit and test strategy

## Plain-English test policy

- **Fast** means: run the smallest relevant static check or targeted suite against an already-running editor. This is the default fixing loop.
- **Full** means: run the complete default sweep, whose per-suite reload is the fixture-isolation proof.
- `run-suites.ps1 -Fast` defers restore to the end. Use it for one focused suite; use the default path when later suites must be isolated from earlier ones.
- A **Done** card does not need to be rerun now. Its recommendation says what to use if that area changes or the card is reopened.

## Where the current P0 work stands

1. **T-LOG03 — Done.** The TurboMCP panel already presents human-readable tool-call rows, distinguishes engine-only calls, shows running/error state, and passed the focused panel checks.
2. **T-FAST / T-FAST-1 — implemented and measured.** Baseline records were about **35 s per editor suite** and **about one hour per full sweep** because restore forced stop/start. The kept-alive runner restores changed bytes and dirty in-memory packages through an editor-only fixture tool. Rebuild scripts do not stop the runner-owned editor.
3. **Warm audience:** `test-graph-tidy -Fast` passed in **4.332 s** against PID 73040, and PID 73040 remained alive. Split: **0.525 s** reuse gate, **2.560 s** suite, **0.193 s** health, **0.909 s** fixture restore, **0.000 s** stop.
4. **Full audience:** 64-suite default sweep completed in **413.945 s (6m 53.945s)**. Split: **0.979 s** startup/reuse, **365.286 s** suites, **10.652 s** health, **32.537 s** restore of 32 packages/files, and **4.319 s** final stop. Verdict: **61 passed, 3 failed, 0 blocked**. The real failures were `test-lookup-guards`, `test-play-check`, and `test-vcs`; the runner completed every suite.
5. **Isolation proof:** an unsaved marker changed editor memory while the disk hash stayed unchanged; restore found one dirty package, reloaded it, the next read could not see the marker, and the editor PID did not change. A later save-heavy run restored one changed file, then editor shutdown preserved the SHA-256 and left `git status` clean.
6. **Repository fixtures are clean.** The implementation files remain uncommitted task output; no tracked fixture is modified.

## Full non-archived board check

### Backlog — 30 cards

| Card | Plain-English scope | Preferred test |
|---|---|---|
| T-C3-01 | Finish the remaining comment-versus-code audit and compile the structural header changes. | **Full** — whole target build is explicitly required. |
| T-METHOD-01 | Make every reported count say what was counted. | **Fast** — document/process checks. |
| T-AUDIT-TYPES | Fix the last misleading type-result count. | **Fast** — targeted type-reader suite. |
| T-AUDIT-06 | Fix node IDs with colons and reject too-short or malformed keys consistently. | **Full** — node IDs are shared infrastructure. |
| T-AUDIT-09 | Finish the material-tidy rollback/count issues across related writers. | **Full** — mutation and rollback affect several tools. |
| T-AUDIT-10 | Correct one misleading parser comment; behavior already round-trips. | **Fast** — static/parser check only. |
| T-AUDIT-11 | Report malformed node keys correctly and stop scanning after the near-miss cap. | **Fast** — focused node-resolve cases. |
| T-AUDIT-12 | Make override removal report the count that proves it did the right work. | **Fast** — targeted override test. |
| T-AUDIT-14 | Report measured enum rename/display results instead of repeating the request. | **Fast** — focused enum suite. |
| T-AUDIT-16 | Correct a false universal claim in token-estimate documentation. | **Fast** — static check. |
| T-AUDIT-17 | Pure checks were audited clean; keep the structural explanation. | **Fast** — no live editor needed. |
| T-AUDIT-18-19 | Clean up analytics/module comments and an unread member. | **Fast** — compile plus focused static checks. |
| T-AUDIT-20 | Trap handling was audited clean; preserve the distinct failure states. | **Fast** — targeted trap tests. |
| T-AUDIT-21-24 | Prevent duplicate `error:` prefixes across tool wrappers. | **Full** — shared wrapper behavior spans many tools. |
| T-AUDIT-22 | Fix a substituted call-cost cap parameter and preserve clear reporting. | **Fast** — focused call-cost suite. |
| T-AUDIT-27-29 | Play-check code was clean; keep it as the good wrapper example. | **Fast** — focused play-check regression. |
| T-SUITE-02 | Improve suite-quality rules beyond the `FAILS ON` wording convention. | **Fast** — static suite-audit tests. |
| T-AUDIT-30 | Fix the same duplicate-prefix problem in WidgetsAsVars. | **Full** — close together with T-AUDIT-21-24. |
| T-AUDIT-31 | Overrides reader was audited clean. | **Fast** — focused read-only regression. |
| T-AUDIT-32 | Decide whether the unread node-type contract field should be removed or reported. | **Fast** — focused node-type tests. |
| T-X4-01 | Record that Tools has no orphaned symbols and Source has five. | **Fast** — static scan. |
| T-AUDIT-33 | Verify the final widget-retarget writer and its measured counts. | **Fast** — targeted mutation suite with isolation. |
| T-AUDIT-UNCOVERED | Strengthen one weak check in the uncovered-tools suite. | **Fast** — run that suite only. |
| T-AUDIT-35 | Layout audit found no defect; preserve the explicit zero/count scope. | **Fast** — focused layout check. |
| T-AUDIT-36 | Align the GraphLoad plan count with the supported `write` format. | **Fast** — static/spec check. |
| T-SUITE-01 | Extend rollback assertions to the material paths that currently lack proof. | **Full** — rollback spans multiple material writers. |
| T-C3-02 | Separate comment-audit ownership from rollback-pattern ownership. | **Fast** — board/static review. |
| T-SUITE-03 | Confirm obsolete suites stay removed and finish the stated final sweep. | **Full** — the card is about the full sweep. |
| T-FIXTURE-01 | Make the acceptance fixture reproducible instead of changing bytes on every rebuild. | **Full** — the fixture underpins the whole suite set. |
| T-OOS-01 | Levels and actors remain out of scope; no implementation is planned. | **Fast** — decision check only; archive is reasonable. |

### Todo — 43 cards

| Card | Plain-English scope | Preferred test |
|---|---|---|
| T05 | Finish the missing graph-node families and shared resolver behavior. | **Full** — broad graph infrastructure. |
| T-FAST | Verify and document fast runner behavior with real verdict output. | **Full** — test infrastructure must pass a full sweep before closure. |
| T06 | Ensure the graph reader prints every node. | **Fast** — focused large-graph reader suite. |
| T07 | Detect unreachable exec nodes and safely remove them. | **Fast** — focused preview/edit test with fixture isolation. |
| T08 | Insert a node into an execution chain. | **Fast** — one focused graph-edit suite. |
| T09 | Swap one call node for another. | **Fast** — focused replacement suite. |
| T10 | Replace a data source without changing unrelated graph behavior. | **Fast** — focused graph suite. |
| T11 | Create a function from text, including its signature. | **Fast** — focused function round-trip. |
| T12 | Apply one batch edit across several assets with one coherent transaction. | **Full** — cross-asset rollback/save behavior. |
| T13 | Dump links at pin level. | **Fast** — focused read-only suite. |
| T14 | Report the current session change list. | **Fast** — focused session-reader test. |
| T15 | Add safe cleanup helpers. | **Fast** — helper-specific tests. |
| T16 | Use real node sizes in every layout tool. | **Full** — shared layout geometry. |
| T17 | Make layout rules and scores match human expectations. | **Full** — broad layout behavior and visual evidence. |
| T18 | Use one getter rule in both the report and the fixer. | **Full** — shared report/edit rule. |
| T19 | Prevent tidy from reporting a cycle that is not real. | **Fast** — focused regression fixture. |
| T20 | Make capture explain exactly what it framed. | **Full** — live visual/editor verification. |
| T21 | Correct layout report counts and denominators. | **Fast** — focused report tests. |
| T22 | Use one field syntax across all field tools. | **Full** — public syntax shared by many tools. |
| T23 | Preview a table row and read it back. | **Fast** — focused table suite. |
| T24 | Include break/make nodes in struct readers. | **Fast** — focused struct suite. |
| T25 | Create data tables with nested rows. | **Fast** — focused create/read-back suite. |
| T26 | Show which assets depend on a struct change. | **Fast** — focused dependency reader. |
| T27 | Standardize one human-readable node-ID format. | **Full** — shared ID format affects many tools. |
| T28 | Improve node-type lookup accuracy. | **Fast** — focused lookup matrix. |
| T29 | Find nodes in every graph, not only the usual graph. | **Fast** — focused multi-graph fixture. |
| T30 | Do not label bound handlers as “never called.” | **Fast** — focused handler fixture. |
| T31 | Produce a short, useful checkout report. | **Fast** — focused source-control report. |
| T32 | Clarify the agent entry point and common traps. | **Fast** — documentation/static checks. |
| T33 | Improve plugin-window presentation. | **Fast** — focused UI test plus one visual check. |
| T34 | Implement widget authoring in bulk. | **Full** — broad writer surface with many mutation paths. |
| T35 | Implement the material reader/writer family. | **Full** — broad material mutation and rollback surface. |
| T36 | Integrate source control beside editor hooks. | **Full** — external-system and editor integration. |
| T37 | Create, rename, move, and duplicate assets safely, including redirectors. | **Full** — destructive asset operations. |
| T38 | Preserve constants on unconnected material inputs. | **Fast** — one focused regression. |
| T39 | Add curve-asset support. | **Fast** — focused create/edit/read-back tests. |
| T40 | Preview the ordered steps for creating and linking a node. | **Fast** — focused preview test. |
| T41 | Build the play-session input test loop. | **Full** — live play, input, gamepad, and menu integration. |
| T05 remainder | Unify the resolver, parse nested bracketed IDs, and support local labels as link targets. | **Full** — shared graph parsing and writing. |
| T-KEEP | Map and run the ten “works today” regression checks as one sequence. | **Full** — it is the broad regression gate. |
| T-RECON | Decide and restore three dropped roadmap clauses. | **Full** — the clauses span graph, asset, and session behavior. |
| T-UPLUGIN-01 | Declare Niagara correctly in the plugin manifest. | **Full** — clean build and packaging/load verification. |
| T-BUILD-01 | Remove two compiler warnings and use the current Slate float API. | **Full** — clean build is the acceptance evidence. |

### Done — 5 cards

| Card | Plain-English outcome | Preferred test if reopened |
|---|---|---|
| T01 | Fixed two crashes and proved both live scenarios. | **Full** — crash and reader/pin infrastructure. |
| T03 | Corrected save behavior and withdrew an option that broke callers. | **Full** — save schema compatibility. |
| T04 | Compile reporting now reads captured engine warnings/errors. | **Full** — engine-log/compile integration. |
| T02 | Tool-call recording, crash persistence, filters, grouping, and panel timeline are implemented. | **Full** — recorder wraps all plugin toolsets. |
| T-LOG03 | Confirmed the TurboMCP window is a readable tool-call log, not the Unreal log. | **Fast** — focused panel checks unless recorder format changes. |

### Failed — 1 card

| Card | Plain-English result | Preferred test |
|---|---|---|
| T-FAST-1 | Warm tests are fast, but disk overwrite does not reset the editor's dirty in-memory package; the attempted isolation design is unsafe. | **Full** — targeted development first, then cross-suite isolation and a complete sweep before closure. |

## Totals

- **79 non-archived cards reviewed:** 30 backlog, 43 todo, 5 done, 1 failed.
- **Fast preferred:** 47 cards.
- **Full required before closure:** 32 cards.
- This keeps **Fast** as the default and reserves **Full** for shared infrastructure, builds, destructive mutations, rollback/isolation, broad integrations, or cards whose explicit acceptance criterion is a full sweep.
