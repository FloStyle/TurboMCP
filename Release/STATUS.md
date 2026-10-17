# TurboMCP release status

Owner status page. Ledger: [PROGRESS.json](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>).
Binding architecture: [ARCHITECTURE.md](<C:/Users/Example/Desktop/TurboMCP/Release/ARCHITECTURE.md>) revision 2.
How to work: [EXECUTION_PROTOCOL.md](<C:/Users/Example/Desktop/TurboMCP/Release/EXECUTION_PROTOCOL.md>) section 10.

## Verdict

**NOT_READY.** 8 of 48 findings verified, 4 of 19 steps verified, **1 of 101 tools migrated** to the result contract.
The product is safe to keep working on. It is not a release.

## What changed in this session (owner review, 2026-10-17)

The previous run's middle third produced an invented data-loss defect and eleven disproved mechanisms. Reviewing
its log found one item that mattered more than all of them:

**A safety rule had been narrowed until a suite passed.** `TurboMCPGraphRepoint` compared compile diagnostics only
when the message text named the edited graph, and reported `compile=passed` while six diagnostics in the same asset
were excluded. The reason a package-wide comparison had refused everything was a discarded baseline
(`Before.Reset()`) two lines above the comparison. The same block invented both the defect and its mask.

Repaired and verified:

| Item | State |
|---|---|
| Package-wide diagnostic comparison | restored; the baseline is kept |
| Diagnostic multiplicity | compared (one before, two after is new) |
| The rule as one pure function | `FTurboMCPResult::NewDiagnosticsSince`, with positive and negative controls |
| Package dirty flag on refusal | restored (`FDirtyFlagGuard`) |
| Evidence | [compile-rule-restored.md](<C:/Users/Example/Desktop/TurboMCP/Release/EVIDENCE/S04/compile-rule-restored.md>) |

## Verification actually observed

| Check | Result |
|---|---|
| `Tools\build.ps1 -Engine C:\UnrealEngine -NoStop` | exit 0: DLL 3,741,184 bytes, sha256 `2D30CBDA17E3939E…` |
| `Automation RunTests TurboMCP.Result` | `TurboMCP.Result.LineContract` → `Result={Success}` |
| `python Tools\test-graph-repoint.py` (fresh fixture) | exit 0, PASS, 4 of 4 checks |
| `python Tools\test-graph-swap-call.py` (fresh fixture) | exit 0, PASS, 6 of 6 checks |
| `Tools\start-editor.ps1 -Stop` | PASS, 0 editors, no lease held |

The native case asserts that the rule **does** refuse (a new message at an unchanged total count, and a repeated
occurrence of an existing identity) and that it **does not** refuse (unchanged, reordered, or disappeared
diagnostics). A rule tested only in the accepting direction is the defect this project keeps rediscovering.

## Steps

| Step | State |
|---|---|
| S00–S03 | verified |
| S04 — shared contracts | **open.** Contracts implemented both ways; one writer migrated; M01–M08 unrun; ARC04 cross-call cache and ARC05 helper not started |
| S05–S18 | not started |

Nothing else advanced. The remaining work is the bulk of the roadmap, not a tidy-up.

## Open decisions

**None.** D-008 is decided: an empty graph name refuses in writers and means "every graph" in readers, with
asset-wide scanning explicit at the call site. Five call sites must be updated deliberately. That is implementation
work, not a question. D-009 forbids narrowing a safety rule to pass a case.

## Known failing gates, left failing on purpose

| Gate | State |
|---|---|
| `audit-tool-coverage` | FAIL: the two batch tools are called by no suite; owned by the S16 card |
| `TurboMCP.T04.QueueForCompilationDuringCompile` | FAIL in native automation since the first run, never examined |
| End-to-end new-compile-error case | MISSING: the restored ARC04 refusal is proved at function level, not at L3 |

## Limits stated rather than hidden

- `compile=passed` for graph repoint means "the asset's diagnostics did not gain an occurrence". The acceptance
  fixture deliberately contains seven pre-existing errors; the report now says how many existed before and after.
- ARC04's cross-call baseline cache is absent, so every edit pays one extra compile.
- 100 of 101 tools still answer without a result line.
- Fixture bytes tracked in Git still differ after a run (card `b4d297e2-ba48-4981-b122-6c9dd48dc1ab`).
- `TOOLS.md` says 94 tools, the descriptor 93, against 101 declared: S17 work.

## Publication

Not authorized. No commits, pushes, tags, or uploads. Pre-existing dirty source files preserved.
