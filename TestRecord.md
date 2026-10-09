# TurboMCP test record

Required by `00_START_HERE.md` section 5 and `06_ACCEPTANCE.md`: every capability
is done only when its tests have been run on the test assets and the numbers are
recorded here.

Status: **phase 0 builds, loads, and registers.** Confirmed twice from the
editor's own log against the built DLL: the module loads and all four toolsets
register with no failure line. The editor window was not opened by a person, so
its layout is unverified. The test asset kit does not exist yet, so no tool
behaviour has been tested. Nothing below claims otherwise.

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
| Plugin builds | **PASS** | `Result: Succeeded`; `UnrealEditor-TurboMCP.dll` 237 KB |
| Module loads | **PASS** | `InternalLoadLibrary: 'TurboMCP'` then `LogTurboMCP: TurboMCP module started` |
| Four toolsets register | **PASS** | four `Registering Toolset` lines, no failures |
| Tools appear in the MCP tool list | **PASS** (via tool search) | `Tool search enabled: ... (4 toolsets discoverable via list_toolsets)` |
| Tools execute over the wire | **PASS** | all four called through `call_tool`; real answers returned |
| Editor starts with no human | **PASS** | launched headless, MCP server bound to port 8000 on its own |
| Panel opens and shows the server row | not yet run | needs an interactive editor |
| Panel output matches the MCP output (`T-PANEL-5`) | not yet run | |
| Test asset kit exists | no | TA-1 to TA-13 must be built by hand |

---

## 2b. Phase 1 checks: agent help

Run with `python Tools/test-agent-help.py` against a running editor. The script
asserts the spec's own rules and exits non-zero on failure.

| Test | Result | Evidence |
|---|---|---|
| `T-HELP-1` tool index | **PASS** | 7 checks: server state, tool search mode, 5 toolsets, every tool, source labels, counts, and the kinds Read/Look/Preview/Edit in order |
| `T-HELP-2` one tool's help | **PASS** | 6 checks: opens with its kind, states kind and source, parameters and traps, and stays under 2000 characters (297 measured) |
| `T-HELP-3` wrong toolset name | **PASS** | refuses and lists the real toolset names |
| `T-HELP-5` wrong tool name | **PASS** | refuses and names the closest toolset |
| `D-07` traps are data | **PASS** | a trap added to `TestProject/Config/TurboMCP/Traps.json` appeared with **no rebuild**, and the result named the file it read |
| Joined `Toolset.Tool` name accepted | **PASS** | `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus` resolves |
| Short toolset name accepted | **PASS** | `TurboMCPReadToolset` resolves |
| Summary mode with an empty tool name | **PASS** | one line per tool in the toolset |

Measured character counts for one tool's help: 297 for a TurboMCP tool, against a
2000 character budget.

### Wire-format facts established by testing, not by reading

These correct `07_ENGINE_NOTES.md` and were found by experiment:

- `call_tool`'s `arguments` must be a **JSON object**, not a JSON string. A string
  makes the engine answer "input params Json is empty". `07_ENGINE_NOTES.md`
  section 3.1 records it as a string.
- The published tool `name` is the **full `Toolset.Function`**, while `call_tool`
  wants the bare function name.
- The toolset name has the leading `U` **stripped**: `TurboMCP.TurboMCPReadToolset`.
- A `UFUNCTION` parameter name is exposed **camelCased**: `toolsetName`, not
  `ToolsetName`.
- An `FString` parameter is **required** in the schema whatever its default, so an
  optional string is expressed by passing `""`, not by omitting the field. This is
  the trap `07_ENGINE_NOTES.md` section 2.1 warns about, now confirmed.

### Not yet verified in phase 1

`R-ASSET-SEARCH`, `R-ASSET-DESCRIBE`, `R-TYPE-FIND`, `R-DIRTY-LIST`, `E-COMPILE`
and `E-SAVE` are not written yet.

### How registration was verified without a human

The editor was run headless as a commandlet, which loads every module and
broadcasts the loading-phases-complete delegate exactly as an interactive start
does:

```
UnrealEditor.exe TurboMCPTest.uproject -run=DerivedDataCache -unattended -nullrhi -log
```

The commandlet finished with `result 0`. From
`TestProject/Saved/Logs/TurboMCPTest.log`:

| Log line | What it proves |
|---|---|
| `LogPluginManager: Mounting Engine plugin TurboMCP` | the plugin was discovered |
| `InternalLoadLibrary: 'TurboMCP' (...UnrealEditor-TurboMCP.dll)` | the built DLL is the one loaded |
| `LogTurboMCP: TurboMCP module started` | `StartupModule` ran |
| `Registering Toolset TurboMCP.TurboMCPReadToolset` | Read registered |
| `Registering Toolset TurboMCP.TurboMCPLookToolset` | Look registered |
| `Registering Toolset TurboMCP.TurboMCPPreviewToolset` | Preview registered |
| `Registering Toolset TurboMCP.TurboMCPEditToolset` | Edit registered |
| `LogTurboMCP: TurboMCP toolsets registered` | the plugin's own confirmation |
| `(4 toolsets discoverable via list_toolsets)` | the MCP server serves them |

No `did not register`, `invalid tool list` or `Unable to register Toolset` line
appears anywhere in the log.

The four toolset names came out **exactly** as predicted from reading
`FFunctionLibraryToolset::GetToolsetClassName`: `<Module>.<ClassName>` with the
leading `U` kept.

Two engine behaviours worth recording, both confirmed here:

- Auto-start really is refused in a commandlet, with the engine saying so:
  `Auto-start skipped when running as a commandlet. Pass
  -ModelContextProtocolStartServer to override`.
- The version-dependent counter in the engine's own log line went 1, 2, 3, 4, 5
  and then back to 4 as the engine unregistered its own `AgentSkillToolset`
  during shutdown. Ours stayed registered to the end, so the final count of 4 is
  the engine toolset plus our three non-Edit toolsets still live at that moment.
  Do not read that counter as a TurboMCP tool count.

### Build failures fixed along the way

Both were real and are now fixed and committed (`5b73e01`).

1. Four private `.cpp` files included their own headers as
   `Panel/STurboMCPPanel.h` and `Tools/TurboMCPToolsets.h`. Unreal searches the
   module directory and `Public/`, not the subdirectory the including file is
   in, so those did not resolve:
   `fatal error C1083: 'Panel/STurboMCPPanel.h': No such file or directory`.
   Fixed by adopting the conventional layout, headers under `Public/`.
2. `STurboMCPPanel.cpp` held the MCP module as
   `const IModelContextProtocolModule*`, but `GetServer()` is a non-const
   virtual: `error C2662`. The pointer is no longer const.

### Static evidence collected before the first run

Not test results, but build-time facts worth keeping, because they caught
mistakes that would otherwise only appear hours into a build.

| Fact | Where it came from |
|---|---|
| UnrealHeaderTool accepted all four toolset classes | `UHT processed TurboMCPTestEditor in 7.9275933 seconds (4 generated files written)` |
| The `AICallable` metadata survived reflection, four times | `{ "AICallable", "" }` in `TurboMCPToolsets.gen.cpp` |
| All four toolset class constructors were generated | `Z_Construct_UClass_UTurboMCPReadToolset` and its three siblings |
| The module exports its symbols | `TURBOMCP_API` on the generated constructors |
| Every engine API the plugin calls exists | `Tools/check-plugin.py`, 23 of 23 |
| Every include resolves | `Tools/check-plugin.py`, 54 of 54 |

Why this matters: a toolset whose functions are not `AICallable` is refused by
`UToolsetRegistry::RegisterToolsetClass` with only an engine log line, and the
editor still looks healthy. The runtime log above is what actually rules that
out.

---

## 4. Log

| Date | What | Result |
|---|---|---|
| 2026-10-07 | Engine 5.8.3 cloned, dependencies and project files generated | success |
| 2026-10-07 | Plugin scaffold written | not yet compiled |
| 2026-10-07 | UHT pass over the plugin | success, 4 generated files |
| 2026-10-08 | First full build | **FAILED**, two C1083 include errors |
| 2026-10-08 | Layout fixed, second build | **FAILED**, one C2662 const error |
| 2026-10-08 | Const fixed, third build | **SUCCEEDED**, 6.04 s |
| 2026-10-08 | Headless commandlet run | **PASS**, all four toolsets registered |
