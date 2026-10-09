# TurboMCP plan of action

Deadline: **Monday 12 October 2026**. Written Friday 09 October, ~2.5 days.

## Verified starting point

Everything below was proven this session, not assumed.

| Check | Evidence |
|---|---|
| Engine builds from source | UE 5.8.3, commit `396c9f059`, `release`, 481 editor DLLs |
| Editor runs **autonomously** | launched headless, no human interaction |
| MCP server starts automatically | `LogModelContextProtocol: Starting MCP server on port 8000` |
| Plugin loads | `InternalLoadLibrary: 'TurboMCP'` |
| Four toolsets register | `Registering Toolset TurboMCP.TurboMCP*` x4, no failures |
| Tools execute over the wire | all four called through `call_tool`, real answers returned |
| Config-file fix works | auto-start came from `DefaultEditorPerProjectUserSettings.ini` |

### The autonomous loop

```
build:   Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development ^
             -Project="<repo>\TestProject\TurboMCPTest.uproject" -WaitMutex -MaxParallelActions=8
run:     UnrealEditor.exe TestProject\TurboMCPTest.uproject -nullrhi -unattended -nosplash -nosound
probe:   python Tools\check-mcp-server.py
```

Rebuild of just the module and restart takes about 30 seconds plus editor boot, so
the edit-build-test cycle is fast now that the engine is built.

### The wire contract, discovered from the live server

This corrected two wrong assumptions:

- Toolset name is `<Module>.<ClassName>` with the leading **`U` stripped**:
  `TurboMCP.TurboMCPReadToolset`. This comes from the `ScriptName` metadata UHT
  writes, not from `FFunctionLibraryToolset::GetToolsetClassName`, which keeps
  the `U`. Our own helper was wrong and has been fixed.
- Tool name is `<Toolset>.<Function>`: `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus`.
- Calling any TurboMCP tool goes through `call_tool` with `toolset_name`,
  `tool_name` (bare) and `arguments` (a JSON **string**).
- A tool that returns `FString` arrives as `{"returnValue": "..."}`.
- Toolset descriptions come from the **class doc comment**; tool descriptions
  from the **function doc comment**.

Helper scripts in `Tools/`: `check-plugin.py` (pre-build gate),
`check-mcp-server.py` (probe), `discover-mcp.py`, `describe-toolsets.py`,
`call-tools.py`, `verify-names.py`.

## Scope reality

`03_TOOLS.md` defines **31 capabilities** across 10 roadmap phases. 2.5 days
cannot deliver all 31 to the spec's "done" bar (each needs tests on hand-built
assets, per `06_ACCEPTANCE.md`). The plan is therefore dependency-ordered, and
whatever is not reached stays honestly unmarked in `TestRecord.md`.

Also note `08_ROADMAP.md` lists the **test asset kit** in phase 0. TA-1 to TA-13
must be built by hand in the editor — that work is the owner's, not the agent's,
because tools tested against tool-made assets prove nothing.

## Order of work

### Step 1 — retarget the test project (done)
The test project already exists and the engine runs it headlessly. Nothing to do
beyond keeping it working.

### Step 2 — Phase 1, agent help
Highest value per unit of risk, and no graph-writing machinery needed.

| Order | Capability | State |
|---|---|---|
| 1 | Traps data file (D-07) | **DONE** - verified, project override works with no rebuild |
| 2 | `R-TOOL-INDEX` | **DONE** - 7 checks pass |
| 3 | `R-TOOL-SCHEMA` | **DONE** - 6 checks pass, 297 chars against a 2000 budget |
| 4 | `R-ASSET-SEARCH` | **DONE** - 5 checks pass |
| 5 | `R-ASSET-DESCRIBE` | **DONE** - 3 checks pass, all path forms agree |
| 6 | `R-TYPE-FIND` | **DONE** - 5 checks pass |
| 7 | `R-DIRTY-LIST` | **DONE** - verified against a really modified asset |
| 8 | `E-COMPILE` | **DONE** - refuses kinds it cannot compile, reports from the compiler log |
| 9 | `E-SAVE` | **DONE** - three modify-then-save cycles pass |

**Phase 1 is complete: 13 tools, both suites pass.** 9 of the spec's 31
capabilities are done.

Two phase 1 parts still need the kit, and are recorded as gaps rather than
claims: `E-COMPILE` on a broken Blueprint (the node ID tie-in), and `E-SAVE`'s
refusal during play.

### Step 3 - Phase 2, Blueprint reading (next)

Eight capabilities, and the base for phases 6, 7 and 8:
`R-BP-LOAD`, `R-GRAPH-LOAD` (both formats), `R-REFS`, `R-OVERRIDES`,
`R-NODE-RESOLVE`, `R-NODE-FIND`, `R-DATATABLE`, `R-COMMENT-LIST`.

Build order inside the phase, because later readers reuse the earlier ones:

1. Node IDs (`04_FORMATS.md` section 3) with `R-NODE-RESOLVE` and
   `R-NODE-FIND`. Proving the ID scheme before any body reader uses it means a
   broken ID cannot look like a broken reader.
2. `R-GRAPH-LOAD` at detail `manifest` and `signatures`, which needs no body
   walking.
3. The graph text writer shared by `R-GRAPH-LOAD` and `R-BP-LOAD`. This is the
   hardest single piece.
4. `R-BP-LOAD`'s relation layer, then its body levels.
5. `R-REFS`, `R-OVERRIDES`, `R-DATATABLE`, `R-COMMENT-LIST`.

A real Blueprint is needed to test any of it, so kit scaffolding comes first.

### The engine's toolsets are OFF by default

This changes how the plugin must be tested. `07_ENGINE_NOTES.md` describes the
Blueprint and widget tools as if they are always present. In UE 5.8.3 they live in
plugin toolsets under `Engine/Plugins/Experimental/Toolsets/`, and every one of
them ships disabled. With only `ModelContextProtocol` on, the server served a
single engine toolset.

`TestProject/TurboMCPTest.uproject` now enables:

| Plugin | What it brings |
|---|---|
| `EditorToolset` | 17 toolsets: `BlueprintTools`, `MaterialTools`, `AssetTools`, `DataTableTools`, `ObjectTools`, `SceneTools`, and more |
| `UMGToolSet` | the widget tools |
| `SlateInspectorToolset` | Slate widget inspection, useful for phase 3 captures |
| `AutomationTestToolset` | test running |
| `SemanticSearchToolset` | semantic asset search, a comparison point for `R-ASSET-SEARCH` |
| `PythonScriptPlugin` | scripted editor work, which is how the test kit is scaffolded |

Compiling these took 18 seconds because their binaries already existed.

### Building the test kit

The editor can run editor Python headlessly:

```
UnrealEditor.exe <project> -run=PythonScriptCommandlet -Script=Tools/make-test-assets.py -unattended -nullrhi
```

Verified: it created `Content/Tests/TA11_TestEnum.uasset`, exit code 0.

Only **scaffolding** is scripted - an empty asset of the right class, saved, so
the registry and the path forms have something real to resolve. The **structure**
the acceptance tests measure (TA-1's 60 to 90 hand-laid nodes, its nested comment
boxes) must stay hand-built, and no test result is recorded until it is.

Verified by `python Tools/test-agent-help.py`, which exits non-zero on failure.
Run it after every change to this phase so a regression is caught at once.

#### The fast loop, in full

```powershell
# 1. stop the editor, or the plugin DLL is locked
Get-Process UnrealEditor -EA SilentlyContinue | Stop-Process -Force
# 2. build just the module (about 30 seconds once the engine exists)
& cmd.exe /c "cd /d C:\UnrealEngine && Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development -Project=`"<REPO>\TestProject\TurboMCPTest.uproject`" -WaitMutex -MaxParallelActions=8"
# 3. start it headless, which also starts the MCP server on 8000
Start-Process 'C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @('<REPO>\TestProject\TurboMCPTest.uproject','-nullrhi','-unattended','-nosplash','-nosound') -WindowStyle Hidden
# 4. verify on the wire
python Tools\test-agent-help.py
```

#### Findings that correct the spec or the engine notes

Recorded because each one cost a build cycle:

- `call_tool`'s `arguments` is a **JSON object**, not a JSON string as
  `07_ENGINE_NOTES.md` section 3.1 says. A string yields "input params Json is
  empty".
- The published tool `name` is the full `Toolset.Function`; `call_tool` wants the
  bare function name.
- `UFUNCTION` parameter names are exposed **camelCased** (`toolsetName`).
- An `FString` parameter is **required** in the schema whatever its default, so
  there is no way to express an optional string. Pass `""` and treat it as "not
  given". This is exactly the open question `07_ENGINE_NOTES.md` section 2.1
  raises, now answered.
- The toolset name is the class name with the leading `U` **stripped**
  (`TurboMCP.TurboMCPReadToolset`). `FFunctionLibraryToolset::GetToolsetClassName`
  keeps the `U` and is therefore the wrong function to copy.
- The engine's Blueprint and widget toolsets are **not registered** in this
  project; only `AgentSkillToolset` is. The shipped traps target tools that are
  absent here, so the trap lookup is proven through the project override file
  instead.

### Step 3 — Phase 2, Blueprint reading
`R-BP-LOAD`, `R-GRAPH-LOAD` (both formats), `R-REFS`, `R-OVERRIDES`,
`R-NODE-RESOLVE`, `R-NODE-FIND`, `R-DATATABLE`, `R-COMMENT-LIST`.
This is the largest reader phase and the base for everything after it.

### Step 4 — remaining phases, in roadmap order, as time allows
3 Looking, 4 Structure, 5 Call log, 6 Layout, 7 Graph writing, 8 Materials,
9 Runtime.

## Rules held throughout

- `P1` never return nothing: a zero is printed with its reason.
- `P2` refuse rather than guess, and list what would work.
- `P3` Read, Look and Preview never change anything.
- `P4` every Edit is one transaction, compiles, and rolls back on a new error.
- `P6` every list is capped and says how many were left out.
- `P9` the panel and the MCP run one code path.
- `P10` a tool description starts with its kind (Read, Look, Preview, Edit).
- Verify on the running server, not by reading code.
- Record real numbers in `TestRecord.md`; never mark untested work as done.
