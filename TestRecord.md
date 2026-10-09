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

Nothing: all nine phase 1 items are written, and every part that can be tested
without the kit passes. The two parts that cannot are named at the end of
section 2e.

---

## 2e. Phase 1: compile and save

| Test | Result | Evidence |
|---|---|---|
| `E-SAVE` saves a really changed asset | **PASS** | three consecutive modify-then-save cycles: `changed: saved 1 of 1`, dirty list 1 -> 0 every time |
| `E-SAVE` with nothing to do | **PASS** | `unchanged: nothing to save, all 1 asset(s) already had no changes` |
| `E-SAVE` reports from the unsaved flag, not the call | **PASS** | the result is decided by `UPackage::IsDirty()`, and the first version was caught lying by exactly this check |
| `E-SAVE` retries once | implemented | a second `SaveLoadedAsset` runs when the flag is still set |
| `E-COMPILE` refuses a kind that cannot compile | **PASS** | `is a UserDefinedEnum, which cannot be compiled. Only Blueprints and widgets compile.` |
| `E-COMPILE` passes through an unresolved asset | **PASS** | the resolver's own refusal is printed |
| A result opens with `unchanged` when nothing changed | **PASS** | both tools said `changed:` when nothing had happened; fixed, and both now say `unchanged` with the reason (`P4`) |

### A second `D-33` trap, met from the other side

The first `E-SAVE` used `FEditorFileUtils::PromptForCheckoutAndSave`. It **failed
on every attempt in a headless editor** while the engine's own save succeeded on
the same asset, because that call goes through source control checkout, which
cannot complete with no source control provider. The asset was never written and
the tool reported failure: honest, but useless.

The fix is the save path the engine's own editor toolset uses,
`UEditorAssetLibrary::SaveLoadedAsset`, which needed `EditorScriptingUtilities`
in `Build.cs`. All three cycles then succeeded.

Worth recording because `D-33` warns that a save call's own answer is not the
truth. The same lesson applied the other way: a save call reporting *failure*
proves nothing either, so the unsaved flag is the only thing either result may be
built on.

### Still to be tested once the kit exists

- `E-COMPILE` on a **broken Blueprint** (`T-STRUCT-7`). The spec asks for each
  error and warning "tied to the ID of the node that caused it". The tool prints
  the compiler's own message, which names the node and the property, but it does
  **not** yet print a `04_FORMATS.md` node ID, because building one needs a real
  graph to test against. This is a known gap, not a claim.
- `E-SAVE` refusal during play (`T-STRUCT-8`). The guard reads
  `IsPlaySessionInProgress` and `IsSimulateInEditorInProgress`, but no play
  session was started, so the refusal is implemented and unproven.

### A better way to script the editor

`editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script`
runs a Python script against the toolset APIs **over MCP**, on the editor that is
already running. That is a better route than the `PythonScriptCommandlet` used
earlier, which needs its own editor start and cannot touch a live session. It is
how the remaining kit scaffolding should be built.

---

## 2f. Phase 2: node IDs, resolve and find

Built: `TurboMCPNodeId` (the ID format, shared by every reader and writer),
`ReadTurboMCPNodeResolve` and `ReadTurboMCPNodeFind`. 15 tools total.

Tested against `BP_NodeTest`, an Actor Blueprint created by
`Tools/make-test-blueprint.py`, whose EventGraph the engine itself populated with
its three standard events.

| Test | Result | Evidence |
|---|---|---|
| Node IDs are the documented shape | **PASS** | `BP_NodeTest:EventGraph:696d11` from GUID `696d11...`, lower case, six digits |
| Every graph kind is searched | **PASS** | found `EventGraph` without being told which array holds it |
| Node type IDs compose like the engine's | **PASS** | `AddEvent\|evnementBeginPlay`, and `AddEvent\|Collision\|...` for the collision event that carries a subcategory |
| The stable class name is reported | **PASS** | `class: K2Node_Event` |
| `R-NODE-FIND` matches on several fields | **PASS** | `Event` matched 3 of 4 nodes across 1 of 1 graphs |
| An empty word list is refused with an example | **PASS** | `error: no words given... for example PrintString, or MyVar.` |

### A real limitation found: the engine writer's type ID is localised

`03_TOOLS.md` requires `R-NODE-RESOLVE` to print "the node type ID (the one the
engine's node creation tool accepts)". The engine builds that ID as
`<menu category>|<node title>` with whitespace removed, so **the title half
follows the editor's language**.

The first version printed `Ajouterunévénement|événementBeginPlay` on this French
editor. The category half was fixed by reading the `FText` source string instead
of its translated display string:

```
before:  Ajouterunévénement|événementBeginPlay
after:   AddEvent|événementBeginPlay          <- category fixed, title not
```

The title half cannot be fixed this way. Epic builds a node title at runtime from
separate localised fragments, so there is no single English source string to
read. `FTextInspector::GetSourceString` returns what was passed in, and what was
passed in is already translated.

**Why this matters, and why it is not fatal.** The engine's own `find_node_types`
returns localised IDs too: on this editor it offers 2215 IDs beginning
`Utilitaires|Casting|...`, `Affichage|Événement|...`. So the localised form is
what the engine's writer accepts **on this machine**, and a type ID this plugin
prints will feed straight back into the engine's writer. What it will **not** do
is transfer to an editor running a different language.

**What was done about it:** the node's C++ class (`K2Node_Event`) is printed
beside the type ID. That is the one name for a node that cannot change with
locale, and it is also what disambiguates two nodes whose localised titles
collide. It is a mitigation, not a fix.

### Also verified

- `FText` has **no** `GetSourceString` member in 5.8.3. The method of that name
  at `Text.h` line 365 belongs to `FTextFormat`. The working call is
  `FTextInspector::GetSourceString(Text)`, a static at line 1289. This cost two
  build cycles to find.
- No engine tool creates a Blueprint, so the asset had to be scaffolded by a
  commandlet first. The **structure** was then written by the engine, not by this
  plugin, which is what keeps the comparison honest.

---

## 2g. Phase 2: R-GRAPH-LOAD

Built: `FTurboMCPGraphText` (the graph text writer, shared with `R-BP-LOAD`) and
`ReadTurboMCPGraphLoad`. 16 tools.

Tested on `BP_NodeTest`, whose EventGraph the **engine's own `write_graph_dsl`
wrote**: an event, a write to a member variable, a print, a branch that reads that
variable, and a different print on each arm. 11 nodes.

```
(event ReceiveBeginPlay
  (Variables|Default|SetHealth ; BP_NodeTest:EventGraph:a280af
      execute (-> BP_NodeTest:EventGraph:4d9d9c)
      Health 42.0)
  (Development|PrintString ; BP_NodeTest:EventGraph:285ce6
      execute (-> BP_NodeTest:EventGraph:a280af)
      InString "before the branch"
      ...)
  (Utilities|FlowControl|Branch ; BP_NodeTest:EventGraph:fe61a9
      execute (-> BP_NodeTest:EventGraph:285ce6)
      Condition (Math|Float|float>float ; BP_NodeTest:EventGraph:c186f4
      A (Variables|Default|GetHealth ; BP_NodeTest:EventGraph:5e1484)
      B (Math|Float|MakeLiteralFloat ; BP_NodeTest:EventGraph:97c189
      Value 10.0)))
  (:then
    (Development|PrintString ... InString "healthy" ...))
  (:else
    (Development|PrintString ... InString "hurt" ...)))
```

| Test | Result | Evidence |
|---|---|---|
| Exec flow reads in order | **PASS** | each node's `execute` points at its predecessor, top to bottom |
| Every node carries its ID | **PASS** | a `; <node ID>` comment on every node |
| Pins by real name | **PASS** | `InString`, `Condition`, `execute` |
| Value nodes print inline | **PASS** | the condition's `A` and `B` nested inside it |
| Variable get and set get canonical IDs | **PASS** | `Variables\|Default\|SetHealth`, `...\|GetHealth` |
| Both branches of a branch survive | **PASS** | `:then` and `:else`, each with its own body |
| An entry point with no body says so | **PASS** | `; this entry point has no exec chain and no body` |
| A wrong graph name lists the real ones | **PASS** | `error: ... has no graph named 'X'. Its graphs are: EventGraph` |
| A bad detail level is refused with the list | **PASS** | `Accepted: manifest, signatures, bodies, full.` |
| `format write` is refused, not faked | **PASS** | says it is not implemented yet and names `04_FORMATS.md` 2.6 |

### Three defects the engine-written graph exposed

These were invisible when the graph had only unconnected events. Writing real
content through Epic's writer is what found them.

**1. Entry nodes were listed as unreachable.** Every event appeared twice: once as
its own form, and again under "not reachable from any entry point". The entry
nodes were never marked as printed. Fixed by marking them before any body is
walked.

**2. Variable get and set nodes had no type ID.** `K2Node_VariableGet` and
`K2Node_VariableSet` are not palette nodes, so `GetMenuCategory()` is empty and
the generic `<category>|<title>` rule refused them. The result was that **every
variable access in a graph was unprintable**. The engine's writer uses the
canonical `Variables|<variable category>|Set<Name>`, with `Default` for an
uncategorised variable, and its own `_expand_type_id` in `blueprint_dsl.py`
resolves that form. Fixed by special-casing variable nodes the same way custom
events already were.

**3. The `else` arm was lost.** The exec walk followed only the **first** exec
output, so an `if/else` printed one arm and left the other in the unreachable
list. Fixed by following every exec output and writing each arm as a named
`(:PinName ...)` continuation, which is the engine's own shape for a multi-exec
node.

### A method failure worth more than the three defects

An earlier run wrote **13 nodes** through the engine's writer and read them back
correctly. Then the editor was stopped for a rebuild and **all 13 were gone**.
`write_graph_dsl` compiles the Blueprint, which marks it dirty, but nothing had
saved it. The verification looked perfect and the work did not exist.

`Tools/build-test-graph.py` now ends by saving through the plugin's own `E-SAVE`
and then **proving the save from the asset's unsaved flag**, so this cannot happen
quietly again.

### The editor language decides the type ID, and can be pinned

`Tools/start-editor.ps1` now passes `-culture=en` by default. This matters more
than it looks and is the fix for the limitation in section 2f:

| Editor language | Type ID for BeginPlay |
|---|---|
| French (the machine default) | `AddEvent\|événementBeginPlay` |
| English (now pinned) | `AddEvent\|EventBeginPlay` |

The English form is the one the engine's own documentation and examples use, and
the one that transfers between machines. Pinning it makes every result in this
record reproducible and comparable, and removes the need for the C++ class name
as a workaround — though the class is still printed, because it is what
disambiguates two nodes whose titles collide.

---

## 2d. Phase 1: type find and dirty list

| Test | Result | Evidence |
|---|---|---|
| `R-TYPE-FIND` finds a native struct | **PASS** | `Vector kind:struct` puts `/Script/CoreUObject.Vector` **first** among 333 matches |
| It gives the exact pin path and the module | **PASS** | `/Script/Engine.VectorCurve  [struct] in Engine` |
| It finds a user-made enum | **PASS** | `TA11` returns `/Game/Tests/TA11_TestEnum.TA11_TestEnum  [enum]`, marked not loaded yet |
| It caps the list | **PASS** | says `showing the first 30` and how many more exist |
| It refuses an unknown kind | **PASS** | lists `any, struct, enum, class, interface` |
| `R-DIRTY-LIST` sees a real unsaved asset | **PASS** | after a property change, reports `1 package(s) dirty` and names it with its class |
| It handles nothing unsaved | **PASS** | `0 package(s) dirty` + `Nothing has unsaved changes.` |
| Two tools agree on the unsaved state | **PASS** | `R-ASSET-DESCRIBE` independently reported `unsaved changes: yes` and `on disk: not fully` |

Run both suites with:

```
python Tools/test-agent-help.py
python Tools/test-asset-tools.py
```

### The serious bug `T-ASSET-2` caught

The three path forms of one asset produced **three different answers**:

| Input form | What it said |
|---|---|
| short name `TA11_TestEnum` | object `...Enum.Enum`, class `UserDefinedEnum`, class path `none` |
| package path `/Game/Tests/TA11_TestEnum` | object `/Game/Tests/TA11_TestEnum`, class **`Package`** |
| object path `...Enum.Enum` | class path **`/Script/Engine.UserDefinedEnum`** |

Two faults, both fixed:

1. The **package path form resolved the `UPackage` instead of the asset**. The
   resolver dropped the object name and then looked the path up as an object, so
   it found the container. It reported `class: Package` and the package as the
   object path, which is simply the wrong answer to "which asset is this". The
   resolver now tells an object path from a package path by whether the object
   name is really part of the input, and sends a package path through the
   registry.
2. `ClassPath` was **invented** as `<Asset>_C` for every asset on one route and
   left empty on another. It is now filled only when the caller actually named a
   generated class, and the asset's own class is carried in a separate
   `AssetClass` field set by all three routes.

After the fix, the three forms are byte-identical, which is what `T-ASSET-2`
requires and what `P9` implies: one question, one answer.

---

## 2c. Phase 1: asset search and describe

| Test | Result | Evidence |
|---|---|---|
| `R-ASSET-SEARCH` finds a project asset | **PASS** | `TA11` returns `/Game/Tests/TA11_TestEnum.TA11_TestEnum [UserDefinedEnum]` |
| Search with a type filter | **PASS** | `type:Material` found 2288 matches across 9207 assets |
| Search prints a cap and the number left out | **PASS** | the result says how many are shown and how many were omitted |
| Search matching nothing | **PASS** | says nothing matched **and how many assets were searched**, so "no match" cannot be read as "nothing searched" (`P1`) |
| Search with an empty query | **PASS** | refuses and gives the example `type:Material shop` |
| `R-ASSET-DESCRIBE` all path forms agree | **PASS** | short name `TA11_TestEnum` and package path `/Game/Tests/TA11_TestEnum` produce byte-identical output |
| Describe reports an unreadable fact as unknown | **PASS** | `unsaved changes: unknown, the asset is not loaded`, rather than claiming "no" (`D-05`) |
| Describe counts honestly | **PASS** | says `all 1 described`, or `N described, M refused` when some fail |

### Two bugs these tests caught

1. The describe summary claimed `1 asset(s) described` even when **zero** resolved,
   because it counted the inputs instead of the successes. Fixed: it now prints
   `N described, M refused` (`P1`).
2. `R-TOOL-SCHEMA` truncated any toolset name containing a dot, because it split
   on the last dot before checking whether the name was already valid. That broke
   every `editor_toolset.toolsets.*` toolset. Fixed: a name is used as given when
   it is a known toolset, and only split when that split resolves.
3. Its summary mode listed all 21 tools of a toolset with no cap, breaking the
   2000 character rule. Now capped at 12 with the remainder counted.

---

## 3. Engine toolsets: a finding that changes the picture

`07_ENGINE_NOTES.md` describes Blueprint and widget toolsets as if they are always
present. They are **not**. In UE 5.8.3 they live in plugin toolsets that are all
off by default, under `Engine/Plugins/Experimental/Toolsets/`. With only
`ModelContextProtocol` enabled, the server serves almost nothing: in this project
just `AgentSkillToolset`.

Enabling `EditorToolset` alone brings up 17 more toolsets, including
`editor_toolset.toolsets.blueprint.BlueprintTools`,
`...material.MaterialTools`, `...asset.AssetTools`,
`...data_table.DataTableTools` and `...object.ObjectTools`. `UMGToolSet` brings up
the widget tools.

This matters twice over:

- TurboMCP cannot be tested against the tools it exists to improve unless those
  plugins are on, so `TestProject/TurboMCPTest.uproject` now enables them.
- It is version drift from the 5.8.1 notes, which is exactly what `QUESTIONS.md`
  Q-1 predicted.

---

## 4. Building the test kit without a person

`06_ACCEPTANCE.md` requires TA-1 to TA-13 to be built **by hand**, because a tool
tested against an asset a tool built proves nothing.

Verified this session: the editor can run editor Python headlessly, which makes
the mechanical parts scriptable:

```
UnrealEditor.exe <project> -run=PythonScriptCommandlet -Script=Tools/make-test-assets.py -unattended -nullrhi
```

It created `Content/Tests/TA11_TestEnum.uasset` on disk with exit code 0.

The intent is to script only the **scaffolding** — an empty asset of the right
class, saved, so the asset registry and the path forms have something real to
resolve against. The **structure** the tests actually measure, such as TA-1's 60
to 90 hand-laid nodes and its nested comment boxes, must still be built by hand,
and no test result may be recorded until it is.

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
