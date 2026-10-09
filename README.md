# TurboMCP

An editor-only Unreal Engine plugin that registers its own toolsets with the
MCP server built into the engine, plus one editor window.

The engine's own MCP tools work. This plugin fills the gaps around them:

- reading a whole Blueprint in one call, with nothing missing,
- one graph text format for reading and writing,
- node IDs that stay valid across edits,
- safe Edits: one undo, a compile check, and a roll back on a new error,
- compact help for the engine's own tools, with the traps they have,
- layout reports, captures and widget previews,
- graph tidying that respects the author's comment boxes.

Nothing here ships in a game build.

- What to build and why: [`Spec/00_START_HERE.md`](Spec/00_START_HERE.md)
- The rules every tool follows: [`Spec/02_PRINCIPLES.md`](Spec/02_PRINCIPLES.md)
- Decisions already made: [`Spec/05_DECISIONS.md`](Spec/05_DECISIONS.md)
- Build order: [`Spec/08_ROADMAP.md`](Spec/08_ROADMAP.md)

## Layout

```
TurboMCP.uplugin        plugin descriptor
Source/TurboMCP/        the plugin's single editor module
  Public/               headers, including the tool declarations
  (root)                the .cpp files, one job each
Content/Traps/          engine tool traps, as data (see D-07)
Spec/                   the specification, read in the order above
TestProject/            the project used to build and test the plugin
Tools/                  helper scripts, not part of the plugin
```

## Documentation

| File | What it is |
|---|---|
| `Spec/` | The specification, written by the owner. The authority on what to build |
| `PLAN.md` | What is done, what is next, and the build-order reasoning |
| `DOCS.md` | Every choice made while building, and why |
| `TestRecord.md` | What has actually been tested, with the measured numbers |
| `QUESTIONS.md` | Open questions for the owner |
| `Tools/README.md` | What each helper script does and how to run it |
| `.agent/ENVIRONMENT.md` | The machine: engine, toolchain, and every trap hit while setting it up |

## Requirements

- Unreal Engine 5.8.3 built from source. A Launcher install cannot run this
  plugin, because the plugin registers toolsets with the `ToolsetRegistry` and
  `ModelContextProtocol` plugins, which need engine source.
- Visual Studio 2022 with the C++ game development workload.
- Windows.

`Tools/setup-engine.ps1` does the whole engine setup, including the two steps
that are easy to miss and cost a build attempt each: the plugin must be mounted
where UnrealBuildTool looks, and `ShaderCompileWorker` must be built or a
rendering editor exits during shader compilation.

## Building and running

```powershell
Tools\setup-engine.ps1          # once
python Tools\check-plugin.py    # before any build; a hard gate
Tools\start-editor.ps1 -StopFirst
```

`Tools/start-editor.ps1` builds nothing; it starts the editor and then **proves**
it reached a working state, including that all four toolsets answer over MCP.
It refuses to start a second editor.

To rebuild after a code change:

```powershell
Tools\start-editor.ps1 -Stop    # releases the plugin DLL lock
& cmd.exe /c "cd /d C:\UnrealEngine && Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development -Project=`"$PWD\TestProject\TurboMCPTest.uproject`" -WaitMutex -MaxParallelActions=8"
Tools\start-editor.ps1
```

That whole loop takes about 24 seconds. The plugin's own panel is under
**Window > TurboMCP**, and it calls the same code path the MCP does (`P9`).

`Tools/check-mcp-server.py` speaks JSON-RPC to the running server, lists what it
serves and reports whether TurboMCP's toolsets are visible. It exits non-zero
when the server is unreachable or TurboMCP is missing, so it can be used as a
build check rather than read by eye. Python is used rather than `curl` because
Windows Schannel fails inside the DSH sandbox and Python carries its own TLS
stack.

## How the plugin registers tools

The engine builds a tool's schema from reflection and uses the doc comment on
the function as the tool description an agent reads. So a tool is:

```cpp
UCLASS()
class UTurboMCPReadToolset : public UToolsetDefinition
{
    GENERATED_BODY()
public:
    /**
     * Read the MCP server state and this plugin's registration state.
     *
     * The first line of this comment is what the agent sees.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPStatus();
};
```

The four toolset classes are one per tool kind, so a toolset that holds only
Read and Look tools cannot change an asset. That is enforced by which class the
tool is written in, not by care. See
[`Source/TurboMCP/Tools/TurboMCPToolsets.h`](Source/TurboMCP/Tools/TurboMCPToolsets.h).

Registration happens after every module loading phase completes, not in
`StartupModule`, because the registry does not exist earlier.
