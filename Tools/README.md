# Tools

Scripts used to build, start, drive and verify TurboMCP. None of them ship with
the plugin; they exist so the same checks can be run again by anyone, including
a future session that remembers nothing.

Every script here is non-interactive and returns a non-zero exit code when it
fails, so it can be used as a gate instead of something a person reads.

## Why each one exists

| Script | Runs against | Job |
|---|---|---|
| `setup-engine.ps1` | nothing | Build UE 5.8.3 from source and mount the plugin where UBT can find it |
| `check-plugin.py` | the source tree | Pre-build gate: engine version, plugin mount, includes, engine APIs, UHT freshness, toolset shape |
| `check-includes.py` | the source tree | Every `#include` in the plugin resolves to a real file |
| `start-editor.ps1` | the machine | Start the editor and **prove** it reached a working state |
| `tmcp.py` | a running editor | Call one TurboMCP tool over MCP and print its answer |
| `check-mcp-server.py` | a running editor | Prove the server answers and all four toolsets are registered |
| `test-agent-help.py` | a running editor | The agent help acceptance checks |
| `test-asset-tools.py` | a running editor | The asset and type tool acceptance checks |
| `test-refs.py` | a running editor | The reference tool acceptance checks, in both directions |
| `test-overrides.py` | a running editor | The overridable-function acceptance checks |
| `test-comments.py` | a running editor | The comment box acceptance checks, plus a model of the rectangle rules |
| `test-datatable.py` | a running editor | The DataTable acceptance checks |
| `build-test-graph.py` | a running editor | Rebuilds the test graph through the engine's writer, then saves and verifies it |
| `make-test-assets.py` | a headless editor | Create test-kit **scaffolding** (see the warning below) |

## The order they are used in

```powershell
# once, to set up the engine
Tools\setup-engine.ps1

# before any build
python Tools\check-plugin.py

# start the editor and verify it works
Tools\start-editor.ps1 -StopFirst

# check the tools on the wire
python Tools\check-mcp-server.py
python Tools\test-agent-help.py
python Tools\test-asset-tools.py
python Tools\test-refs.py
python Tools\test-overrides.py
python Tools\test-comments.py
python Tools\test-datatable.py
```

## A leftover LiveCodingConsole blocks every build

`LiveCodingConsole.exe` is a separate process that the editor starts, and it can
outlive the editor. While it runs, UnrealBuildTool refuses **every** build with:

```
Unable to build while Live Coding is active. Exit the editor and game, or press
Ctrl+Alt+F11 if iterating on code in the editor or game
```

Nothing about that message points at the real cause, and a leftover console from a
Live Coding experiment once blocked a build an hour later.
`Tools\start-editor.ps1 -Stop` now stops it as well as the editor, so the trap
cannot recur. If a build fails this way, that command clears it.

## The iteration loop

Measured at **24 seconds** end to end, and it is what every change is developed
against:

| Step | Time |
|---|---|
| `start-editor.ps1 -Stop` | 5s |
| rebuild the plugin module | 9s |
| `start-editor.ps1` until the MCP server binds | 10s |

```powershell
Tools\start-editor.ps1 -Stop
& cmd.exe /c "cd /d C:\UnrealEngine && Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development -Project=`"<REPO>\TestProject\TurboMCPTest.uproject`" -WaitMutex -MaxParallelActions=8"
Tools\start-editor.ps1
```

**Live Coding does not work here and is not used.** `UBT -LiveCoding` compiles a
patch but cannot deliver it to a headless editor, and `07_ENGINE_NOTES.md`
section 1.1 rules it out anyway because it cannot patch reflection data, which is
what a tool is. See `../PLAN.md` for the evidence.

## start-editor.ps1

The only script that can start something, so it is careful about it.

```powershell
Tools\start-editor.ps1                # start visibly and verify
Tools\start-editor.ps1 -StopFirst     # replace any running editor
Tools\start-editor.ps1 -Stop          # stop it and nothing else
Tools\start-editor.ps1 -NullRHI       # faster, no window, no shader worker needed
```

It checks six things, because they fail independently:

1. the editor binary, the project, and the **plugin binary** exist
2. `ShaderCompileWorker.exe` exists — a rendering editor exits without it, and a
   `-nullrhi` run hides that completely because it never compiles shaders
3. no editor is already running, and the MCP port is free
4. the process stays alive and reports a window
5. the MCP server binds its port
6. **a real tool call answers and all four TurboMCP toolsets are registered**

Step 6 is the one that matters. Steps 1 to 5 can all pass on an editor with a
broken plugin.

**It refuses to start a second editor** rather than warning. It did warn once,
and started one anyway, which left two servers racing for port 8000. Exit code 6
means "something is already running".

**Stop the editor before a merge, not only before a build.** The editor holds an open
handle on every `.uasset` it has loaded, and on Windows that blocks the unlink git
needs, so a checkout fails with `unable to unlink old '...uasset': Invalid argument`
and the merge never runs. Worse, the following `git push origin main` then reports
`Everything up-to-date`, which is true and misleading because `main` was never
updated. Run `start-editor.ps1 -Stop` first, and read the merge's own exit code rather
than the push's output.

## tmcp.py

The quickest way to ask the plugin something.

```powershell
python Tools\tmcp.py                                     # status and tool index
python Tools\tmcp.py --tool ReadTurboMCPStatus
python Tools\tmcp.py --tool ReadTurboMCPAssetSearch --args '{\"query\":\"type:Material\"}'
python Tools\tmcp.py --tool ReadTurboMCPToolHelp --args '{\"toolsetName\":\"TurboMCP.TurboMCPReadToolset\",\"toolName\":\"ReadTurboMCPStatus\"}'
```

Escaping JSON inside PowerShell is painful; prefer a small script that imports
`tmcp.Session` and calls `session.call(toolset, tool, args)` with a real dict.

## Two rules learned the hard way

**`call_tool`'s `arguments` is a JSON object, not a string.** A string makes the
engine answer "input params Json is empty". `07_ENGINE_NOTES.md` section 3.1
records the opposite, and it is wrong.

**`list_toolsets` is an MCP-level meta-tool.** Call it with `tools/call` and the
bare name. Going through `call_tool` needs a toolset name for the meta-tool
itself, and guessing it fails. `check-mcp-server.py` got both of these wrong once
and only a hard gate caught it.

## make-test-assets.py: scaffolding only

`06_ACCEPTANCE.md` requires the test kit TA-1 to TA-13 to be built **by hand**,
because a tool tested against an asset a tool built proves nothing.

```powershell
UnrealEditor.exe <project> -run=PythonScriptCommandlet -Script=Tools/make-test-assets.py -unattended -nullrhi
```

It may create an empty asset of the right class and save it, so the asset
registry and the path forms have something real to resolve against. It must
**not** build the structure the acceptance tests measure — TA-1's 60 to 90
hand-laid nodes and its nested comment boxes — and no test result may be recorded
until that structure exists.

A better route than this commandlet is
`editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script`,
which runs Python against an **already running** editor over MCP.
