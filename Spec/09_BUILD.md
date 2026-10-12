# Building this plugin

How to build TurboMCP, and what to change when the engine is not vanilla
5.8.3.

This file exists because two of the engine plugins this plugin depends on are
**experimental** and ship with the engine, so an engine that renamed, moved or
removed one of them fails this plugin's build in a way that reads like a bug
in this plugin (`D-39`). Every step below was checked against the engine in
this workspace; where a step could differ on another machine, it says what to
change.

---

## 1. What must be true before you build

| Thing | Value | Where it is stated |
|---|---|---|
| Engine | 5.8.3, Windows editor | `01_BRIEF.md`, `D-38` |
| Target kind | **Editor only.** Nothing here ships in a game build | `TurboMCP.Build.cs` header |
| Engine plugins | all three enabled: `SlateInspectorToolset`, `ToolsetRegistry`, `ModelContextProtocol` | `TurboMCP.uplugin` |
| Python | needed for two reasons, both named below | `TurboMCP.Build.cs` |

**Three** engine plugins are declared in `TurboMCP.uplugin`, and **two of them
carry the MCP server and the tool registry**: `ModelContextProtocol` and
`ToolsetRegistry`. The third, `SlateInspectorToolset`, is a dependency like the
other two and is listed beside them in `Build.cs` (`:85-87`, `:116`), because a
module dependency needs an entry in both lists (section 1.1). They are
experimental: their names, modules and APIs are the parts most likely to move
between engine versions.

### 1.1 The two dependency lists must agree

A module dependency is **two edits, not one** (`07_ENGINE_NOTES.md`
section 1.2):

1. the module in `PublicDependencyModuleNames` or
   `PrivateDependencyModuleNames` in `Source/TurboMCP/TurboMCP.Build.cs`;
2. the plugin that OWNS that module in the `Plugins` list of
   `TurboMCP.uplugin`.

Do only the first and the build tool prints a warning about a missing plugin
dependency. On another engine, this is the first thing to check when a module
that "obviously exists" cannot be found: the module can be present and still
be unreachable because its plugin is not listed as a dependency here.

### 1.2 Why the dependency list looks the way it does

`TurboMCP.Build.cs` carries a comment on most entries saying what uses it and
what failed without it. Two of those comments are load-bearing knowledge
rather than documentation, and are repeated here because they are the failures
a fresh machine hits first:

- **`InputCore` is a LINK error, not a compile error.** `SListView` and
  `SComboBox` read `EKeys::Enter` and eleven other keys, `EKeys` lives in
  `InputCore/Public/InputCoreTypes.h`, and `Slate` plus `SlateCore` are not
  enough. The failure is twelve `LNK2019: unresolved external symbol` errors
  naming keys, which reads like a Slate problem and is not one.
- **`MaterialEditor`'s header is included as `MaterialEditingLibrary.h`** and
  not as `MaterialEditor/MaterialEditingLibrary.h`, because it sits at that
  module's `Public` root. The slash form fails the build with "No such file
  or directory".

### 1.3 Why `PythonScriptPlugin` is a dependency

One commandlet needs it: the PIE test map's Blueprint actor is built with the
engine's own Python graph writer (`BlueprintTools.write_graph_dsl`) rather
than with a second graph writer written here, because a second writer could
disagree with the readers. The plugin is enabled in `TurboMCP.uplugin` and in
the test project's `.uproject` for the same two-edit reason as section 1.1.

## 2. The build, step by step

1. **Close the editor.** While it runs, the plugin's DLL is locked and a full
   rebuild cannot replace it.
2. Generate project files for the engine you are building against.
3. Build the `TurboMCP` module for the Editor target.
4. Start the editor and check that the four toolsets appear: the plugin's
   status tool lists them, and each missing one prints a line saying why
   (`ReadTurboMCPStatus`).
5. Confirm the tools are served: the tool index lists every tool with its
   kind, and it says whether the server is in tool search mode, because in
   that mode a client reaches every tool through `call_tool` and sees three
   tools only (`07_ENGINE_NOTES.md` section 2.2).

### 2.1 Live Coding is not a rebuild

Live Coding patches function bodies only. It cannot apply changes to
reflection data, and when it cannot it **does not fail cleanly**: the toolset
stays listed with no tools and an unknown version, and only a full rebuild plus
an editor restart brings it back (`07_ENGINE_NOTES.md` section 1.1).

| Change | Live Coding | Full rebuild |
|---|---|---|
| Code inside a function body | yes | no |
| Adding a non-member function to a source file | yes | no |
| `UFUNCTION` parameters, defaults, specifiers, category | no | yes |
| **The doc comment above a tool `UFUNCTION`** (the registry uses it as the tool text) | no | yes |
| Adding or renaming a `UCLASS` or `UFUNCTION` | no | yes |
| `Build.cs` or `.uplugin` | no | yes |

For a file Live Coding never patched before, it may only take effect on the
next compile. **When a change appears to have no effect, compile a second time
before treating it as broken.**

### 2.2 `Result: Succeeded` is not evidence the binary changed

Recorded in `../TestRecord.md` section 29: a stale binary masqueraded as a passing fix
three times. The suite failed while the source was correct because the loaded
DLL was the old one. Two habits fix this:

- after a tool description change, reopen the panel tab: Slate builds the
  panel's widgets at tab creation (`07_ENGINE_NOTES.md` section 1.3);
- when a fix appears not to work, check the DLL's timestamp against the source
  before debugging the fix.

## 3. Building against an engine that is not vanilla 5.8.3

The order below is the order that finds the cause fastest. Each step names the
symptom it explains.

1. **Find the three engine plugins in the new engine.** Look for
   `ModelContextProtocol`, `ToolsetRegistry` and `SlateInspectorToolset`. If one
   of the first two is gone or renamed, nothing else matters until the
   dependency lists are updated to match (section 1.1). The modules to check for
   those two are `ModelContextProtocol`, `ModelContextProtocolEngine` and
   `ToolsetRegistry`; `SlateInspectorToolset` contributes the module of the same
   name, listed at `Build.cs:116`.
2. **Check the module names inside each plugin.** A plugin can survive a
   version while its modules are split or renamed; `ModelContextProtocol`
   already provides two modules.
3. **Check the two APIs the plugin calls directly**, because these are the
   engine-version surfaces it depends on beyond the module names:
   - `IModelContextProtocolModule`: `GetServer()`, `SetAnalyticsProvider`,
     `GetAnalyticsProvider`, `RecordAnalyticsEvent`, and the free functions
     `GetServerPortNumber()` and `GetServerUrlPath()`;
   - `UToolsetRegistry::GetAllToolsetJsonSchemas()`.
4. **Check the tool registration timing.** Toolsets are registered after
   `FCoreDelegates::OnAllModuleLoadingPhasesComplete`, not in `StartupModule`,
   because the registry subsystem may not exist earlier. An engine that
   changed that callback's name or ordering shows up as a toolset that is
   missing with no error.
5. **Check the trap notes in the engine's own tools.** `R-TOOL-SCHEMA`'s trap
   data is a file that ships with the plugin precisely so it can change
   without a rebuild (`D-07`). A new engine version with renamed engine tool
   parameters needs that file edited, not the plugin rebuilt.

### 3.1 What to report when it fails

The failures that look like plugin bugs and are not:

| Symptom | Cause | Where to look |
|---|---|---|
| Toolset listed, no tools, unknown version | a reflection change was hot-patched | section 2.1 |
| Twelve `LNK2019` errors naming `EKeys` | `InputCore` missing | section 1.2 |
| "No such file or directory" for a Material Editor header | the slash form of the include | section 1.2 |
| A module that exists cannot be found | its plugin is not in the `Plugins` list | section 1.1 |
| Toolset missing, no error at all | registration ran before the registry existed | section 3 step 4 |
| A tool's description is the old text | the doc comment needs a full rebuild | section 2.1 |

## 4. The test harness

`Tools/run-suites.ps1` owns the editor per suite and waits for both the server
and the process. `../TestRecord.md` sections 6z and 7a record the failure mode that made it
the bottleneck: a hybrid where a suite and the harness both drive the same
editor produces failures whose names move between runs.

Until one owner of the editor lifecycle exists (`08_ROADMAP.md` section 3):

- run a suite **alone** when its result matters;
- treat a full run's failures as unproven rather than as tools;
- record the measured numbers with the date in `../TestRecord.md`, because a test
  whose values are not recorded cannot be compared next time.
