# Documented choices

Every choice made while setting up the engine and building the plugin, with the
reason. `Spec/05_DECISIONS.md` holds the owner's decisions; this file holds the
builder's.

Two rules while reading this:

- **A choice that failed is recorded too**, with what went wrong. Those entries
  are worth more than the ones that worked, because they stop the same mistake
  being made twice.
- **Nothing here is a claim about behaviour.** What is verified lives in
  `TestRecord.md` with measured numbers.

---

## Part 1: engine and environment

### E-1 Build from source, not the Launcher

**Chosen:** UE 5.8.3, branch `release`, cloned from `EpicGames/UnrealEngine`.

**Rejected:** the Launcher's prebuilt 5.8.

**Why:** the plugin registers toolsets with the engine's `ToolsetRegistry` and
`ModelContextProtocol` plugins. Those need engine source to develop against. A
Launcher install also cannot be the target of a plugin that uses engine internal
API, which this one does.

### E-2 Branch `release`, not the `ue5-main` URL given

**Chosen:** branch `release` = 5.8.3.

**Rejected:** `ue5-main`, which the request named.

**Why:** `ue5-main` is **5.9.0**, while the specification targets 5.8.1. Building
it would have drifted from every note in `07_ENGINE_NOTES.md`. Raised as Q-1.

### E-3 Clone with `--filter=blob:none --depth 1`

**Chosen:** a partial clone of the single branch.

**Why:** the full repository is about 39 GB of history. Only the current tree is
needed, and the filter fetches file contents on demand, so the clone is a
fraction of the size and still complete for building.

### E-4 Mount the plugin with a directory junction

**Chosen:** a junction at `Engine/Plugins/Marketplace/TurboMCP` pointing at the
repository.

**Rejected:** copying the plugin in; `AdditionalPluginDirectories` in the
`.uproject`.

**Why:** UBT refused the plugin from outside `Engine/Plugins` with "Unable to
find plugin 'TurboMCP'". `RulesAssembly::EnumeratePlugins` chains to the engine
assembly, and the project's own assembly did not expose it. The project's
`AdditionalPluginDirectories` field was tried and did not resolve it either.

The junction is the documented location for a third-party source plugin, needs no
second copy, and keeps the repository the single source of truth.

**Cost when forgotten:** one failed build attempt and a confusing error.

### E-5 Build `TurboMCPTestEditor`, not `UnrealEditor`

**Chosen:** the editor target of the test project.

**Why:** a plugin has no target of its own. It is compiled as part of a project,
so the project editor target is the only thing that builds it. It also pulls in
exactly the modules the plugin depends on.

### E-6 Cap parallelism with `-MaxParallelActions=8`

**Chosen:** 8 compiler processes.

**Rejected:** the default, which used 12.

**Why:** the machine has 31.7 GB of RAM and a fixed 14.9 GB pagefile, so the
commit ceiling is about 42 GB. At default parallelism Unreal Build Accelerator
killed compilers:

```
UbaSessionServer - Killed process SharedPCH.Engine.Cpp20.cpp -
Low on memory (44.5gb/46.8gb). Kill threshold is 44.4gb
```

The build stalled at action 8 of 9005. Capping the action count keeps it under
the ceiling. It then runs close to serially and is slow, but it finishes.

**Measured consequence:** 18% CPU utilisation, one compiler at a time, and a full
editor build of 2h13m.

### E-7 Build `ShaderCompileWorker`

**Chosen:** build it as part of engine setup.

**Why:** a rendering editor launches it to compile shaders and **exits** if it is
missing:

```
LogShaderCompilers: Error: Couldn't launch ShaderCompileWorker.exe!
FPlatformMisc::RequestExitWithStatus(1, 1)
```

Every earlier run used `-nullrhi`, which never compiles shaders, so the missing
binary was invisible for hours. The first *visible* editor start died on it.

**This is the reason `Tools/start-editor.ps1` refuses to start without it.**

### E-8 Use the 24 second rebuild loop, not Live Coding

**Chosen:** stop the editor, rebuild the one module, start again, probe.

**Rejected:** Live Coding.

**Why:** `UBT -LiveCoding` compiles a patch against a running editor, but the
patch never arrives. Measured: plugin DLL hash unchanged, a probe marker absent
from the tool's live output, no patch line in the editor log, and the editor
listening on 8000 and 1985 only. Delivering a patch needs the Live Coding Console
wired to the editor's Live Coding server, which a headless editor does not
provide.

Independently, `07_ENGINE_NOTES.md` section 1.1 rules it out: Live Coding patches
function bodies only and cannot touch reflection data, and **every tool here is a
`UFUNCTION`**, so almost every change needs a full rebuild anyway.

The loop is 24 seconds: stop 5s, rebuild 9s, start 10s.

---

## Part 2: plugin structure

### P-1 One editor module, four toolsets split by kind

**Chosen:** a single `Editor` module, and one `UToolsetDefinition` subclass per
tool kind: `UTurboMCPReadToolset`, `...LookToolset`, `...PreviewToolset`,
`...EditToolset`.

**Why:** `03_TOOLS.md` section 1.1 requires a toolset that holds only Read and
Look tools to be unable to change an asset, "by structure, not by care". Making
the kind the **class** means a write tool can only live in the Edit class, and
its absence from the others is visible in review rather than enforced by memory.

A separate module per kind was considered and rejected: it would need a third
"contracts" module for the shared formats, which is three modules doing what one
does.

### P-2 `LoadingPhase: PostEngineInit`

**Chosen:** register toolsets from
`FCoreDelegates::OnAllModuleLoadingPhasesComplete`.

**Rejected:** registering in `StartupModule`.

**Why:** the toolset registry subsystem may not exist during `StartupModule`, and
a toolset registered too early is **silently** never served.

`07_ENGINE_NOTES.md` section 2.1 says to use the "all module loading phases
complete" callback. That delegate does **not** exist in `ModuleManager.h`. The
real one is `FCoreDelegates::OnAllModuleLoadingPhasesComplete` in
`CoreDelegates.h` line 244. Found by searching the engine source, not by trusting
the note.

### P-3 One tool per kind even while a kind is empty

**Chosen:** each of the four toolsets has at least one real tool from the start.

**Why:** `UToolsetRegistry::RegisterToolsetClass` refuses a toolset whose
`HasValidTools()` is false, and the editor still looks healthy. An empty toolset
would have vanished silently.

The first attempt at this was wrong in a different way: the four placeholder
tools printed status while being named `Look...` and `Preview...`, which broke
`P10` (a description starts with its kind) and misled an agent. They now do
something real and appropriate to their kind:

| Tool | What it actually does |
|---|---|
| `ReadTurboMCPStatus` | the server state and every toolset's registration |
| `LookTurboMCPToolset` | lists the live Slate windows a capture could target |
| `PreviewTurboMCPToolset` | reads every toolset schema and flags any over the help budget |
| `ReadTurboMCPEditQueue` | reports whether play or simulate is running, to gate writes |

### P-4 `Public/` for headers, module root for sources

**Chosen:** headers under `Source/TurboMCP/Public/`, `.cpp` files at the module
root, subdirectories preserved inside `Public/`.

**Why:** UnrealBuildTool adds `Public/`, `Classes/` and `Internal/` to the
include path, **not the module root**. Headers first went into `Panel/` and
`Tools/` next to their sources, and the build failed with:

```
fatal error C1083: cannot open include file: 'Panel/STurboMCPPanel.h'
```

This was invisible to three static checks, because they searched the engine for a
file with that name and found one. Only the compiler could see it.

### P-5 A trap file that a project can override

**Chosen:** `Content/Traps/EngineToolTraps.json` shipped with the plugin, and
`<Project>/Config/TurboMCP/Traps.json` which wins over it.

**Why:** `D-07` requires traps to be data so they can be corrected without a
rebuild. The project file exists so a project can record a trap this plugin does
not know yet. Verified: a project trap appeared with **no rebuild at all**, and
the result named the file it read.

The traps are deliberately narrow: one plain sentence each, naming the failure
that was actually seen. `07_ENGINE_NOTES.md` section 3 is the source.

### P-6 The panel and the MCP call the same code

**Chosen:** `STurboMCPPanel` calls `GetTurboMCPStatusText()`, which calls
`UTurboMCPReadToolset::ReadTurboMCPStatus()`.

**Why:** `P9` requires one code path. The cheapest way to keep that promise is
for the window to call the tool's own code rather than a copy of it, so the two
cannot disagree.

---

## Part 3: mistakes made and corrected

Recorded because each one cost a build cycle or worse.

### M-1 A toolset cannot have zero tools

Assumed a toolset could be registered empty and filled later. It cannot:
`HasValidTools()` returns false and the registry refuses the whole toolset with
one engine log line. Fixed by giving each kind a real tool (see P-3).

### M-2 Headers edited after UHT ran

Renamed three tool functions in the header **after** UnrealHeaderTool had
generated reflection. The generated code still named the old functions, so
compiling would have failed on mismatched reflection, and a header change makes
UBT re-plan and discard completed actions. The build was stopped, the names
restored, and UHT re-ran.

**This is why `Tools/check-plugin.py` has a "UHT freshness" check**: it fails
when any plugin header is newer than the generated code.

### M-3 `%hs` is not `%s`

Used `%hs` with a `TCHAR*` in `FString::Printf`. In printf `%hs` means a narrow
string, so it would have printed one byte per character. Fixed to `%s`.

### M-4 The toolset name keeps no `U`

Wrote a helper that copied `FFunctionLibraryToolset::GetToolsetClassName`, which
builds `<Module>.<ClassName>` from the raw C++ name and **keeps** the leading
`U`. The live server reports `TurboMCP.TurboMCPReadToolset`, no `U`. The real
name comes from the `ScriptName` metadata UnrealHeaderTool writes.

Fixed, and verified against `list_toolsets`.

### M-5 A tool that reported no change as a change

`E-SAVE` opened with `changed: saving 1 asset(s)` and then said `0 saved, 1 had no
changes`. `E-COMPILE` said `changed:` when it had refused everything. `P4`
requires a result to open with either `unchanged` and the reason, or a summary of
what changed. Both now say `unchanged` when nothing happened.

### M-6 `FEditorFileUtils::PromptForCheckoutAndSave` does not work headless

The first `E-SAVE` used it. It **failed on every attempt** while the engine's own
save succeeded on the same asset, because it goes through source control
checkout, which cannot complete with no provider. The asset was never written.

Fixed by using the path the engine's own editor toolset uses,
`UEditorAssetLibrary::SaveLoadedAsset`, which needed `EditorScriptingUtilities`
in `Build.cs`. Three consecutive cycles then passed.

This is the `D-33` lesson from the other side: `D-33` warns that a save call's
own answer is not the truth because it once reported failure while succeeding.
A save call reporting **failure** proves nothing either, so the unsaved flag is
the only thing either result may be built on.

### M-7 The package path form resolved the package, not the asset

The three path forms of one asset gave **three different answers**, which
`T-ASSET-2` forbids. The worst: `/Game/Tests/TA11_TestEnum` resolved to the
`UPackage` and reported `class: Package`.

Two faults. The resolver dropped the object name and then looked the result up as
an object, finding the container. And `ClassPath` was invented as `<Asset>_C` on
one route and left empty on another.

Fixed: the resolver tells an object path from a package path by whether the
object name is really part of the input, and a new `AssetClass` field is set by
all three routes. The forms are now byte-identical.

### M-8 Two editors started at once

`Tools/start-editor.ps1` found an editor already running, **warned**, and started
another one anyway. Two MCP servers then raced for port 8000.

Three fixes: it now refuses with exit code 6, it watches the PID it started
rather than any process named `UnrealEditor`, and `-Stop` waits for the plugin
DLL lock to clear.

**Found by the owner watching the screen.** Every check I had written said PASS
while two editors were running, because they asked "does an editor answer?"
rather than "how many editors are there?". That is the lesson: a check that
cannot fail in the way reality failed is not a check.

### M-9 A UHT check that cried wolf

`Tools/check-plugin.py` failed on **every** header newer than the generated code,
including `TurboMCPGraphText.h` and `TurboMCPNodeId.h`, which carry no reflection
at all. UnrealHeaderTool reads a header for `UCLASS`, `USTRUCT`, `UENUM`,
`UFUNCTION` and `UPROPERTY`; a header with none of those cannot make the generated
code stale.

It was wrong rather than merely cautious, and wrong in the harmful direction: a
check that fails on a correct change teaches the reader to ignore it. It now
examines only reflection headers and reports how many it examined, so a zero is
visible instead of silent.

### M-10 Thirteen nodes were written, read back, and lost

The graph text reader was developed against a graph the engine's own
`write_graph_dsl` wrote. It produced a correct 13-node read. Then the editor was
stopped for a rebuild and **all 13 nodes were gone**: `write_graph_dsl` compiles
the Blueprint, which marks it dirty, but nothing saves it.

The verification was perfect and the work did not exist. This is the same shape as
M-6 and M-8: a result that looked right while the thing itself was absent.

`Tools/build-test-graph.py` now ends by saving through the plugin's own `E-SAVE`
and then **proving the save from the asset's unsaved flag**, so the graph cannot
be trusted without being on disk.

### M-11 A binary asset stopped a merge

Merging `feature/r-graph-load` into `main` failed with
`CONFLICT (content): Merge conflict in TestProject/Content/Tests/BP_NodeTest.uasset`.
The same test asset had been changed on both branches, and a binary file has no
merge.

`.gitattributes` now sends `*.uasset` and `*.umap` through the `ours` driver, so
git does not attempt a merge it cannot do. That driver is not built in and needs
`git config --global merge.ours.driver true` once per clone.

The deeper point: **a test asset that both branches changed has no automatic right
answer.** It has to be rebuilt from whichever branch holds the state the tests were
written against. For this kit that is `Tools/build-test-graph.py`, which is why the
graph is written by a script rather than by hand-editing an asset.

---

## Part 4: wire format, established by testing

These were found by experiment against the running server, not by reading docs.
Two of them contradict `07_ENGINE_NOTES.md`.

| Fact | Note |
|---|---|
| `call_tool`'s `arguments` is a **JSON object**, not a string | `07_ENGINE_NOTES.md` section 3.1 says string. A string gives "input params Json is empty" |
| The published tool `name` is the full `Toolset.Function`, but `call_tool` wants the bare function name | |
| `UFUNCTION` parameter names are exposed **camelCased** (`toolsetName`) | |
| An `FString` parameter is **required** in the schema whatever its default, so an optional string is `""` and never an omitted field | This is the open question `07_ENGINE_NOTES.md` section 2.1 raises, now answered |
| The toolset name has the leading `U` **stripped** | |
| `list_toolsets` is an MCP-level meta-tool: call it with `tools/call` and the bare name | Going through `call_tool` needs a toolset name for the meta-tool itself |
| The engine's Blueprint, material and widget toolsets are **off by default** | They live in plugin toolsets under `Engine/Plugins/Experimental/Toolsets/`. With only `ModelContextProtocol` on, the server served **one** toolset |
| `bIsSimulatingInEditor` is not public | The real checks are `UEditorEngine::IsPlaySessionInProgress()` and `IsSimulateInEditorInProgress()` |
| `UBT -LiveCoding` against a headless editor compiles but delivers nothing | |
| A rendering editor needs `ShaderCompileWorker.exe`; a `-nullrhi` one does not | |

---

## Part 5: making a push unattended

### W-1 One named credential, so nothing is ever chosen

Windows holds several `github.com` credentials and Git Credential Manager opens a
**"Select an account"** dialog whenever more than one matches, which made every
push interactive. The credentials were never the problem: all three `git:` entries
were checked against the GitHub API and all three authenticate as `FloStyle`. The
*choosing* was.

`Tools/credential/` reads one named entry straight out of the Windows Credential
Manager and hands it to git, so there is nothing to choose. It adds no new secret
and leaves the machine-wide setup untouched, so every other repository keeps using
GCM. See `Tools/credential/README.md`.
