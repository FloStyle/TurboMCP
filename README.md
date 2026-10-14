# TurboMCP

**Version 1.0.0** — An editor-only Unreal Engine 5.8.3 plugin that registers **93 tools** with the MCP
server built into the engine, plus one editor window.

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-blue.svg)](https://www.mozilla.org/MPL/2.0/)

> **Status: 1.0.0, measured.** `passed 58, failed 0, blocked 0, of 58` across 58 suites, `exit code 0`.
> 93 tools, every one reached by at least one suite, and a quality audit that **blocks the run** when a
> check cannot fail or cannot pass. The numbers in this file are read from the tree, not recalled.
> What is built, what is left, and **what I do not know**: [`HANDOVER.md`](HANDOVER.md).

The engine's own MCP tools work. This plugin fills the gaps around them:

- reading a whole Blueprint in one call, with nothing missing,
- one graph text format for reading and writing,
- node IDs that stay valid across edits,
- safe Edits: one undo, a compile check, and a roll back on a new error,
- compact help for the engine's own tools, with the traps they have,
- layout reports, captures and widget previews,
- graph tidying that respects the author's comment boxes.

**Nothing here ships in a game build.** The module is `"Type": "Editor"`, so it is not compiled into a
packaged game, and every tool is a `UFUNCTION` on a toolset the engine's MCP server exposes.

## The four toolsets

| Toolset | Tools | What it does | Writes? |
|---|---|---|---|
| `UTurboMCPReadToolset` | **30** | Blueprints, graphs, materials, widgets, data tables, references, costs | **No** |
| `UTurboMCPLookToolset` | **4** | Captures, layout, widget previews | **No** |
| `UTurboMCPPreviewToolset` | **26** | `Plan` a change and see what it would touch | **No** |
| `UTurboMCPEditToolset` | **33** | `Apply`, with one undo and a rollback on a new compile error | **Yes** |

**The readers in `Read` and `Look` are structurally unable to write.** A tool that only reads is
declared on a toolset whose class holds no editing code, so "this reader cannot modify your project" is
a property of the type rather than a promise in a comment.

- What to build and why: [`Spec/00_START_HERE.md`](Spec/00_START_HERE.md)
- The rules every tool follows: [`Spec/02_PRINCIPLES.md`](Spec/02_PRINCIPLES.md)
- Decisions already made: [`Spec/05_DECISIONS.md`](Spec/05_DECISIONS.md)
- Build order, and where the project actually is: [`Spec/08_ROADMAP.md`](Spec/08_ROADMAP.md)
- **The live plan: [`Spec/WORKING_PLAN.md`](Spec/WORKING_PLAN.md)** — what is built,
  what is next, which test proves it, and which spec section to read first. It is a
  working document and changes every round.
- Building against an engine that is not vanilla 5.8.3: [`Spec/09_BUILD.md`](Spec/09_BUILD.md)
- **Handover: [`HANDOVER.md`](HANDOVER.md)** — the four commands that establish the state, what is
  finished, what is not, and the state of each of my own priorities.

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
| **[`TOOLS.md`](TOOLS.md)** | **The 93 tools**, generated from the header: name, toolset, the description an agent receives, and how to call one |
| **[`HANDOVER.md`](HANDOVER.md)** | **Start here for the state of the project.** The four commands that establish it, what is finished, what is not, what I do not know, and what I would do next |
| `Spec/` | The specification, at version 1.1. The authority on what to build |
| `Spec/WORKING_PLAN.md` | The live plan: state of each capability and what to pick up next. Changes every round |
| `Spec/09_BUILD.md` | How to build, and what to change on an engine that is not vanilla 5.8.3 |
| [`REPORT.md`](REPORT.md) | The measured state, the plan, the environment's constraints, what is missing, and what I would spend time on |
| [`STATUS.md`](STATUS.md) | The status headline at the revision it was written |
| [`PLAN.md`](PLAN.md) | What is done, what is next, and the build-order reasoning |
| [`DOCS.md`](DOCS.md) | Every choice made while building, and why |
| [`TestRecord.md`](TestRecord.md) | The running record: every measurement, every defect, and the reasoning behind every correction |
| [`QUESTIONS.md`](QUESTIONS.md) | Questions put to the owner, with their answers |
| [`MPL-2.0-SETUP.md`](MPL-2.0-SETUP.md) | Where the license lives: the `LICENSE` text, the per-file notice, and the script that reapplies it |
| [`Tools/README.md`](Tools/README.md) | What each helper script does and how to run it |
| [`Tools/INVENTORY.md`](Tools/INVENTORY.md) | **Every script in `Tools/`, generated from the directory** so no tool can be undocumented |
| [`LICENSE`](LICENSE) | The Mozilla Public License 2.0, verbatim |

The machine facts a contributor needs — the engine version, the toolchain, and every trap hit while
setting it up — are in [`Spec/09_BUILD.md`](Spec/09_BUILD.md). Working logs are not published; `.agent/` is
in `.gitignore`.

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
Tools\build.ps1
```

`Tools/build.ps1` **stops a running editor first** (it holds the plugin DLL open, and `LNK1104` reads like a build error when it is a file lock), **finds the engine itself**, and prints the DLL's timestamp beside its verdict — because `Result: Succeeded` alone is not evidence the binary changed, which this project measured the hard way.

That whole loop takes about 24 seconds. The plugin's own panel is under
**Window > TurboMCP**, and it calls the same code path the MCP does (`P9`).

## Where the engine is found

**Nothing in this repository names a path of anyone's machine.** Every script derives the repository root from its own location and resolves the engine the same way, in this order:

1. an explicit `-Engine <path>` argument, which is never overridden;
2. the `TURBOMCP_ENGINE` environment variable;
3. the `UE_ROOT` environment variable;
4. `EngineAssociation` in the `.uproject`;
5. the Epic launcher registry;
6. `C:\UnrealEngine` and `D:\UnrealEngine`.

**And the version decides, not the order of discovery.** This machine has UnrealEngine **5.8.3** at `C:\UnrealEngine` *and* an Epic launcher install of **5.6**; resolving by discovery alone returned **5.6**, which would have failed inside the engine's own headers and read as a defect in this plugin. A candidate whose `Engine/Build/Build.version` does not match **5.8.3** is reported as a near miss rather than silently used, and when nothing matches the resolver **says what it tried and stops** instead of guessing.

`Tools/common.ps1` (PowerShell) and `Tools/tmcp_paths.py` (Python) are the two implementations, and they resolve the same way.

## Running the tests

`Tools/run-suites.ps1` owns the editor's life cycle, runs every suite, and prints one verdict per suite
with a total. **It is the signal that matters — not a commit, and not a build.** Every iteration of this
work is preserved, so an individual commit is sometimes red on purpose:

```powershell
Tools\run-suites.ps1                                    # all 65 suites, exit 0 when green
Tools\run-suites.ps1 -Suites test-graph-tidy            # one suite, reusing a healthy editor
Tools\run-suites.ps1 -Suites test-graph-tidy -Fast      # focused loop; restore once after the pass
Tools\run-suites.ps1 -SkipSlow                          # skip the ones that take minutes
```

Use `-Fast` for one focused suite while fixing: it reuses one healthy editor and restores the shared
fixture once at the end. The default path restores exact run-start bytes and reloads changed packages inside
the kept-alive editor after every suite. That default is the isolation proof; a multi-suite `-Fast` pass can
still let a later suite observe an earlier suite's state, so failures are re-run alone and reported both ways.

**Eight of the 65 need no interactive editor** — static readers and true commandlet suites run first, before
the runner starts the one editor used by the other **57**. **`check-mcp-server` is deliberately on the editor
side**, because it asserts that the MCP server answers; classifying it as static would test nothing.

Two gates are worth running on their own, because they are fast and they fail loudly:

```powershell
python Tools\check-plugin.py     # 11 checks: declarations against what UHT generated
python Tools\check-includes.py   # 1459 includes resolved against the engine, in seconds
```

## Calling a tool

The server runs **in the editor**, on `http://localhost:8000/mcp`, and `Tools\start-editor.ps1` proves it
answers before it reports success. With tool search enabled — the engine default, kept here — a client sees
three meta-tools and reaches everything through the third:

- **`list_toolsets`** — what is registered, including TurboMCP's four;
- **`describe_toolset`** — one toolset's tools and their input schemas;
- **`call_tool`** — `toolset_name`, `tool_name`, and `arguments`.

Two details a caller gets wrong first, and both are in [`Spec/07_ENGINE_NOTES.md`](Spec/07_ENGINE_NOTES.md):

- `tool_name` is the **bare** function name. A listing prints the full `Toolset.Function`; `call_tool` wants
  the part after the dot.
- `arguments` is a **JSON object, not a string**. Sending it as a string makes the engine answer
  "input params Json is empty".

**[`TOOLS.md`](TOOLS.md) lists all 93 tools**, generated from the declarations in the header, with the
description an agent receives for each.

## Working on a feature

Work happens on a branch, is validated, and is merged only when the whole run passes. **This repository has
two remotes, and which one you push to matters:**

| Remote | What it holds |
|---|---|
| `private` | the full history. **This is where work goes** — it is the default push target |
| `origin` | `https://github.com/FloStyle/TurboMCP`, the published tree, which carries **a single commit with no history** |

**A plain `git push` goes to `private`**, because `remote.pushDefault` is set to it. Publishing is a separate,
deliberate act: the public tree is rebuilt from the current tree as one parentless commit, so pushing the
history to `origin` would undo that.

```powershell
git checkout -b feature/<capability>          # for example feature/r-graph-load

# ... build, and validate before merging: all six must pass ...
python Tools\check-plugin.py                  # 11 pre-build checks
python Tools\check-includes.py                # every include resolves
python Tools\check-mcp-server.py              # the server and all four toolsets
python Tools\test-agent-help.py
python Tools\test-asset-tools.py
python Tools\build-test-graph.py              # rebuilds the test graph and saves it

git add -A && git commit -m "<what changed and why>"
git push -u private feature/<capability>

git checkout main
git merge --no-ff feature/<capability>
git push private main
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
[`Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`](Source/TurboMCP/Public/Tools/TurboMCPToolsets.h).

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
// Copyright (c) 2026 FloStyle
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
