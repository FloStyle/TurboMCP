# TurboMCP test record

Required by `00_START_HERE.md` section 5 and `06_ACCEPTANCE.md`: every capability
is done only when its tests have been run on the test assets and the numbers are
recorded here.

Status: **no tests run yet.** The plugin has not been loaded in an editor. This
file exists so the first test run has somewhere to write, and so nothing is
mistaken for tested when it is not.

Engine under test: **5.8.3**, source build at `C:\UnrealEngine`, commit
`396c9f059903aed5fec78ecd3d437a40c6415368`.

Note on version: `06_ACCEPTANCE.md` and `07_ENGINE_NOTES.md` were written against
5.8.1. See `QUESTIONS.md` Q-1. Every result recorded below must state the engine
version it was measured on.

---

## 1. Test asset kit

`06_ACCEPTANCE.md` section 1 lists TA-1 to TA-13. These must be built **by hand in
the editor**, not with the plugin's own tools, so the tools are tested against
human-made assets.

Where they will live: `TestProject/Content/Tests/`.

| ID | Asset | Built | Baseline recorded |
|---|---|---|---|
| TA-1 | Actor Blueprint, hand-laid function graph | no | no |
| TA-2 | Same Blueprint, event graph | no | no |
| TA-3 | Same Blueprint, "messy" function | no | no |
| TA-4 | Same Blueprint, "stacked" function | no | no |
| TA-5 | Material, about 60 expressions | no | no |
| TA-6 | Material instance of TA-5 | no | no |
| TA-7 | Widget Blueprint, card | no | no |
| TA-8 | Widget Blueprint, page | no | no |
| TA-9 | Widget Blueprint, card alt | no | no |
| TA-10 | Data table with a user struct | no | no |
| TA-11 | User enum asset | no | no |
| TA-12 | Name collision pair | no | no |
| TA-13 | Blueprint whose name is also a folder name | no | no |

### Baselines

`06_ACCEPTANCE.md` requires each asset's node count, comment box count and total
wire length to be recorded once built, and checked before every test run. A
baseline that does not match means the asset was changed and must be restored
before testing.

| Asset | Nodes | Comment boxes | Total wire length |
|---|---|---|---|
| TA-1 | | | |
| TA-2 | | | |
| TA-3 | | | |
| TA-4 | | | |
| TA-5 | | | |

---

## 2. Phase 0 checks

The phase 0 deliverables from `08_ROADMAP.md` are the plugin, the module, toolset
registration, the shared formats, the editor window and the test asset kit.

| Check | Result | Evidence |
|---|---|---|
| Plugin builds | not yet run | build in progress, see below |
| Module loads | not yet run | |
| Four toolsets register | not yet run | |
| Tools appear in the MCP tool list after an editor restart | not yet run | |
| Panel opens and shows the server row | not yet run | |
| Panel output matches the MCP output for one tool (`T-PANEL-5`) | not yet run | |
| Test asset kit exists | no | |

### Static evidence collected before the first run

These are not test results. They are build-time facts worth keeping, because
they caught mistakes that would otherwise only appear hours into a build.

| Fact | Where it came from |
|---|---|
| UnrealHeaderTool accepted all four toolset classes | `UHT processed TurboMCPTestEditor in 4.2956506 seconds (4 generated files written)` |
| The `AICallable` metadata survived reflection, four times | `{ "AICallable", "" }` in `TurboMCPToolsets.gen.cpp` |
| All four toolset class constructors were generated | `Z_Construct_UClass_UTurboMCPReadToolset` and its three siblings |
| The module exports its symbols | `TURBOMCP_API` on the generated constructors |

Why this matters: a toolset whose functions are not `AICallable` is refused by
`UToolsetRegistry::RegisterToolsetClass` with only an engine log line, and the
editor still looks healthy. The generated code above is what rules that out.

---

## 3. Log

| Date | What | Result |
|---|---|---|
| 2026-10-07 | Engine 5.8.3 cloned, dependencies and project files generated | success |
| 2026-10-07 | Plugin scaffold written | not yet compiled |
| 2026-10-07 | UHT pass over the plugin | success, 4 generated files |
| 2026-10-07 | `TurboMCPTestEditor` build | in progress |
