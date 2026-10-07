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
Spec/                   the specification, read in the order above
TestProject/            an empty project used to build and test the plugin
Tools/                  local helper scripts, not part of the plugin
```

## Requirements

- Unreal Engine 5.8.3 built from source. A Launcher install cannot run this
  plugin, because the plugin registers toolsets with the `ToolsetRegistry` and
  `ModelContextProtocol` plugins, which need engine source.
- Visual Studio 2022 with the C++ game development workload.
- Windows.

## Building

The plugin is compiled as part of a project, so build through `TestProject`.

1. Build the editor once:

   ```
   C:\ExampleUnrealengine\Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development ^
       -Project="<repo>\TestProject\TurboMCPTest.uproject" -WaitMutex
   ```

2. Open `TestProject/TurboMCPTest.uproject` in that engine.

3. Check the tools registered. The plugin's own status tool answers, or the
   window shows the same text:

   - Editor window: **Window > TurboMCP**
   - Command line: `python Tools/check-mcp-server.py`
   - MCP server: `http://localhost:8000/mcp`

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
