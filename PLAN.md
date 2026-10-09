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

| Order | Capability | Why here |
|---|---|---|
| 1 | Traps data file (D-07) | `R-TOOL-SCHEMA` depends on it; must update without a rebuild |
| 2 | `R-TOOL-INDEX` | Cheapest real tool; makes the whole surface visible |
| 3 | `R-TOOL-SCHEMA` | Needs the traps file; the 2000-char budget is the hard part |
| 4 | `R-ASSET-SEARCH` | Foundation for every asset-taking tool |
| 5 | `R-ASSET-DESCRIBE` | Proves the resolver against real assets |
| 6 | `R-TYPE-FIND` | Struct/enum lookup |
| 7 | `R-DIRTY-LIST` | Small; needed by `E-SAVE` |
| 8 | `E-COMPILE` | First write-class tool; one transaction and a roll back rule |
| 9 | `E-SAVE` | Uses the unsaved flag, per `D-33` |

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
