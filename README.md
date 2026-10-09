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

## Working on a feature

Work happens on a branch, is validated, and is merged only when every check
passes. `origin` is `https://github.com/FloStyle/TurboMCP`, so a branch is also
the backup.

```powershell
git checkout -b feature/<capability>          # for example feature/r-graph-load

# ... build, and validate before merging: all five must pass ...
python Tools\check-plugin.py                  # 10 pre-build checks
python Tools\check-includes.py                # every include resolves
python Tools\check-mcp-server.py              # the server and all four toolsets
python Tools\test-agent-help.py
python Tools\test-asset-tools.py
python Tools\build-test-graph.py              # rebuilds the test graph and saves it

git add -A && git commit -m "<what changed and why>"
git push -u origin feature/<capability>

git checkout main
git merge --no-ff feature/<capability>
git push origin main
```

**A `.uasset` cannot be merged.** Test content changed on two branches once and
stopped a merge completely. `.gitattributes` now routes content files through the
`ours` driver so git does not attempt it, which needs this once per clone:

```powershell
git config --global merge.ours.driver true
```

Every dependency must be in both files: `TurboMCP.Build.cs` **and**
`TurboMCP.uplugin`. A module dependency present in only one of them fails at
link time or, worse, at load time with a toolset silently missing.

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

## License

**Mozilla Public License 2.0** (`MPL-2.0`). The full text is in [`LICENSE`](LICENSE).

**You can use this plugin in a commercial game and ship that game.** MPL-2.0 is a file-level copyleft
license, not a viral one: it does not reach into your game's own code, your Blueprints, or your assets. Your
game stays yours, under whatever license you choose.

**If you modify the plugin's own files, those files stay under MPL-2.0.** That is the one condition, and it
is deliberately narrow:

| You do this | What MPL-2.0 asks of you |
|---|---|
| Ship a commercial game that uses the plugin | Nothing. No fee, no notice in your game. |
| Modify a plugin `.cpp` or `.h` and distribute it | Publish **that file's** source under MPL-2.0. |
| Add a **new** file that includes plugin code | That new file is also covered, because it contains Covered Software. |
| Add a **new**, separate file that only calls the plugin | Yours. Keep it closed if you want. |
| Combine the plugin with your own code | Allowed, under terms of your choice, as long as the plugin files keep their license. |

Two things MPL-2.0 asks for in practice:

- **Keep the notices.** The MPL-2.0 header at the top of each source file is a license notice. Section 3.4
  says you may not remove or alter it.
- **Tell recipients where to get the license.** If you distribute modified plugin source, point them at
  `https://mozilla.org/MPL/2.0/`, or include the `LICENSE` file. Section 3.1.

Distributing the plugin as a compiled `.dll` inside a shipped game is **Executable Form**, and MPL-2.0
allows it under any terms as long as the source stays available as above (Section 3.2). In practice the
plugin is editor-only and does not ship in a game build at all.

Every source file carries this header, and `Tools/apply-license-header.ps1` reapplies it:

```cpp
// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
```

The `SPDX-License-Identifier` line is the machine-readable form, so tooling can identify the license
without parsing prose. Run `pwsh -File Tools\apply-license-header.ps1 -Check` in a build to fail when a new
file is missing the notice; the script is idempotent, so running it twice changes nothing.

This is a plain description of what the license says, not legal advice. For a commercial release, read
[`LICENSE`](LICENSE) itself.
