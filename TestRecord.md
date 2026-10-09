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

## 2h. Phase 2: R-BP-LOAD

Built: `FTurboMCPBlueprintRead` and `ReadTurboMCPBlueprintLoad`, the main Read
tool. 17 tools. The four detail levels are strictly nested and the graph bodies
come from the same `FTurboMCPGraphText` that `R-GRAPH-LOAD` uses, so the two
cannot print a graph differently.

Tested on `BP_NodeTest`, whose EventGraph the engine's own writer populated with an
event, a variable write, a print, a branch reading that variable, and a print on
each arm.

| Test | Result | Evidence |
|---|---|---|
| The manifest names everything | **PASS** | `counts: 3 graph(s), 2 variable(s), 1 component(s)`, then each name |
| The status line is first, with real counts | **PASS** | `read: 1 of 1 asset(s), detail manifest, 402 character(s)` |
| Variables print type, category, default and flags | **PASS** | `Health : Float (double-precision)`, `category: Default`, `flags: BlueprintReadWrite, NotInstanceEditable` |
| Events carry real signatures | **PASS** | `ReceiveActorBeginOverlap(AActor* OtherActor)`, `ReceiveTick(float DeltaSeconds)` |
| Components carry their class | **PASS** | `DefaultSceneRoot : SceneComponent` |
| The call graph names what each graph calls | **PASS** | `EventGraph -> Greater_DoubleDouble, MakeLiteralDouble, PrintString` |
| The variable map names readers and writers | **PASS** | `Health`: `read by: EventGraph`, `written by: EventGraph` |
| A variable nothing touches is called out | **PASS** | `TargetName`: `never read and never written in any graph` |
| The graph filter narrows the read | **PASS** | `graphFilter: EventGraph` gives `bodies: 1 graph(s)` |
| A filter is declared in the relations | **PASS** | `NOTE: a graph filter is active, so these relations cover the 1 matching graph(s) out of 3` |
| A filter matching nothing is refused | **PASS** | names the real graphs rather than returning an empty read |
| `bodies` prints the graph | **PASS** | 2986 characters, with `:then` and `:else` arms |
| A bad detail level is refused with the list | **PASS** | `Accepted: manifest, signatures, bodies, full.` |
| `format json` is refused, not faked | **PASS** | says it is not implemented rather than returning text a caller would parse as JSON |
| A non-numeric budget is refused | **PASS** | says what to pass instead |

### Four defects found by reading the real output

**1. The conclusion was printed before the evidence.** The first version built the
opening line with the counts already in it, before the loop had run, so it read
`read: 1 asset(s): 1 read, 0 refused` followed by the body. The counts are now
computed first and the status line is **prepended** to the body, not assigned over
it — an intermediate attempt assigned over it and threw the whole body away.

**2. Contradictory variable flags.** `Health` printed
`flags: BlueprintReadWrite, InstanceEditable, NotInstanceEditable`, which is
self-refuting. `CPF_Edit` means "user-settable in the editor" and **every**
Blueprint variable carries it, so it is not evidence of anything. The per-instance
question is answered by `CPF_DisableEditOnInstance` alone.

**3. Every event was listed as a function nobody calls.** A `UK2Node_FunctionEntry`
exists in the **event graph** too, for the ubergraph function the engine generates,
so testing for that node alone made `ReceiveBeginPlay` appear as an event, as a
function, and then as a function nothing calls. A real function graph is one the
Blueprint holds in `FunctionGraphs`, and that is now the test.

**4. A filtered read silently dropped relations.** Filtering to `EventGraph` made
`Health` vanish from the variable map, which reads as "this Blueprint never uses
it" rather than "it is used in a graph you filtered out". The relations section now
states which graphs it covers when a filter is active.

The dead-code list also now says that it lists functions no **node** calls, and
that an engine-invoked function such as `UserConstructionScript` appears there and
is not dead — otherwise a caller could delete something that runs.

---

## 2i. Phase 2: R-REFS

Built: `ReadTurboMCPRefs`. 18 tools. Run its checks with
`python Tools/test-refs.py`.

| Test | Result | Evidence |
|---|---|---|
| Outgoing references are found | **PASS** | `/AudioWidgets/AudioFader/AudioFader` reports **7** |
| Incoming references are found | **PASS** | `/Engine/EditorBlueprintResources/StandardMacros` reports **81** |
| Every reference prints a full object path | **PASS** | `/AudioWidgets/AudioFader/AudioFader_Inner.AudioFader_Inner` |
| Every reference prints its class | **PASS** | `[CurveFloat]`, `[Texture2D]`, `[WidgetBlueprint]` |
| Engine and script packages are counted, not listed | **PASS** | `(4 engine or script package(s) were not listed: they are not content)` |
| A zero explains itself | **PASS** | `none. try detail signatures in ReadTurboMCPBlueprintLoad ...` |
| A bad direction is refused by name | **PASS** | `Accepted: out, in, both.` |
| Every project path form agrees | **PASS** | short, package and object forms give one identical body |
| A short name outside `/Game` explains itself | **PASS** | says the search covers `/Game` only and to pass a full path |
| One direction at a time works | **PASS** | `direction: out` prints no `used by (in)` section |

### The most important entry in this record

About an hour went into nearly "fixing" a bug that did not exist, and the lesson is
worth more than the tool.

`R-REFS` reported **zero references in both directions** for every asset in the
project, which looks exactly like a broken dependency walk. Two investigations both
pointed at our code: reading the registry from a commandlet showed
`get_dependencies -> None`, and the engine's **own**
`editor_toolset.toolsets.asset.AssetTools.get_dependencies` returned `[]` for
`/Engine/EditorBlueprintResources/StandardMacros`, an engine asset that must depend
on something.

The tool was right the whole time:

- **The project assets genuinely have no content dependencies.** Everything
  `/Game/Tests/BP_NodeTest` references is `/Script`, which this tool counts and does
  not list. Zero was the honest answer.
- **The engine's own tool also returns an empty list for that engine asset**, so an
  empty answer there is Epic's behaviour and not ours.
- **The data is populated for other assets.** `/AudioFader` reports 7 outgoing
  references and `StandardMacros` reports 81 incoming. The machinery works.

The mistake was choosing test assets that could not tell "broken" apart from
"empty". A test that cannot fail the way reality fails proves nothing — the same
lesson as the two editors, arriving from a different direction. `Tools/test-refs.py`
now tests **both** an asset with references and one with none, so a zero and a
failure can be distinguished.

### Three real defects found along the way

**1. Paths ending in `.None`.** The registry gives the object name `None` for a
dependency recorded at package level, and that was treated as a real object name,
producing `/AudioWidgets/AudioFader/AudioFader_Inner.None`. It looked like an object
path and was not one. `None` is now treated as absent, so the package's own asset
path is printed instead.

**2. A short name outside `/Game` failed without explaining why.** The short-name
search deliberately covers `/Game` only, so `AudioFader`, which lives in a mounted
plugin, cannot be found by its own name. The refusal now says so and says to pass a
full path. This limitation stays, stated rather than hidden.

**3. A leftover `LiveCodingConsole.exe` blocked a build an hour later.** It is a
separate process the editor starts, it can outlive the editor, and while it runs
UnrealBuildTool refuses every build with `Unable to build while Live Coding is
active`. `Tools/start-editor.ps1 -Stop` now stops it too, so the trap cannot recur.

Building that error message also cost two compile failures: UE's compile-time format
checker refuses a `TCHAR*` passed for `%s` unless it can see the argument is a
literal, and a member variable is not one. The message is built by concatenation
instead.

---

## 2j. Phase 2: R-OVERRIDES

Built: `ReadTurboMCPOverrides`. 19 tools. Run its checks with
`python Tools/test-overrides.py`.

| Test | Result | Evidence |
|---|---|---|
| The parent chain is reported | **PASS** | `parent: /Script/Engine.Actor` |
| The overridable set is found | **PASS** | 23 overridable, 4 already overridden, 19 available |
| Signatures have inputs marked | **PASS** | `ReceiveAnyDamage : (float Damage, UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)` |
| Signatures have outputs marked | **PASS** | `ReceiveHit : (...) -> (FHitResult Hit)` |
| Events are labelled | **PASS** | `[event]` |
| The declaring class is named | **PASS** | `from: /Script/Engine.Actor` |
| It never names the Blueprint's own class | **PASS** | `BP_NodeTest_C` appears nowhere |
| The `overridden` filter works | **PASS** | `shown: 4 of 4`, and no `state: available` line |
| The `available` filter works | **PASS** | `shown: 19 of 19`, and no `state: overridden here` line |
| The two filters add up | **PASS** | 4 + 19 = 23 |
| Counts are taken before the filter | **PASS** | the `overridden` result still says `23 overridable` |
| A bad filter is refused by name | **PASS** | `Accepted: all, overridden, available.` |
| A non-Blueprint is refused | **PASS** | `is not a Blueprint, so it has no parent chain to override` |
| Every path form agrees | **PASS** | short, package and object forms give one identical body |

### The engine already had the answer, and finding it changed the design

The first version of this tool walked the parent chain itself with
`TFieldIterator<UFunction>` and filtered on `FUNC_Event`. It reported **3**
overridden functions. Then `UBlueprintEditorLibrary` turned out to expose
`ListEvents` and `ListFunctions`, documented as listing "locally defined function
graphs, overridable parent-class functions, and functions declared on interfaces
(both explicitly implemented and **inherited through a natively-implemented
interface**)", each with an `is_implemented` flag.

That is the required set, computed by the editor. The tool is now built on it and is
**shorter** than the hand-rolled version, which was correct for a plain Actor
Blueprint and would never have seen a natively-implemented interface member at all,
because those are not in `UBlueprint::ImplementedInterfaces`.

Two things the hand-rolled version got wrong, both caught by comparing against the
engine:

- **It said 3 overridden; the engine says 4.** The fourth is
  `UserConstructionScript`, whose graph is a real override. The hand-written walk
  missed it, and the test now pins the engine's count so a regression to a
  hand-rolled filter changes it.
- **It needed a special case to drop `UObject::ExecuteUbergraph`.** That function
  carries `FUNC_Event`, so a `FUNC_Event` filter keeps it, and it is not an override
  target. The engine's set does not contain it, so the special case is gone.

This is the third time in this phase that reading the engine's own implementation
beat writing a rule: the node type ID, the `FText` source string, and now the
overridable set. A rule that mimics the editor is a second source of truth for
something the editor already enforces.

### Two compile failures on the way

- **`FindFunction` as a free function shadowed `UObject::FindFunction`.** Inside a
  member of a toolset, an unqualified call to a helper of that name resolves through
  the class's base, and the compiler reported `FindFunction takes no 2 arguments`
  for a call that was not the one being made. Renamed to `ResolveFunction`.
- **`FindFunctionByName` takes the include-super flag second**,
  `(FName, EIncludeSuperFlag::Type)`, not first.

`Tools/start-editor.ps1 -Stop` also proved itself: a build failed with
`Unable to build while Live Coding is active`, the stop command cleared it, and the
next build succeeded. That trap cost an hour before the fix and now costs one
command.

---

## 2k. Phase 2: R-COMMENT-LIST

Built: `ReadTurboMCPCommentList`. 20 tools. Run its checks with
`python Tools/test-comments.py`.

Verified against `/Engine/EditorBlueprintResources/StandardMacros`, which is
**Epic's own content** and carries 21 comment boxes, 17 of them covering nodes.

| Test | Result | Evidence |
|---|---|---|
| Boxes are found | **PASS** | `counts: 21 box(es), 0 nested, 17 with nodes` |
| Every box has a title | **PASS** | `title: Execute Loop Body`, and a long warning title |
| Every box has a colour | **PASS** | `colour: #FFFFFF` |
| Every box has a rectangle | **PASS** | `rect: x -240, y -320, 1152 wide, 256 tall` |
| Every box says whether it is nested | **PASS** | `parent box: none, this is a top level box` |
| Node membership is computed | **PASS** | 5 node IDs listed under the `DoOnce` box |
| A box covering nothing says so | **PASS** | `(this box covers no node)` |
| A graph with no boxes says so | **PASS** | `graph CompareFloat: no comment boxes` |
| One graph at a time works | **PASS** | `graphName: DoOnce` gives `1 comment box(es)` and no `ForEachLoop` |
| A missing graph lists the real names | **PASS** | names the Blueprint's graphs |
| A non-Blueprint is refused | **PASS** | `is not a Blueprint, so it has no graphs to list comment boxes from` |
| A Blueprint with no boxes is an honest zero | **PASS** | `counts: 0 box(es)` plus a sentence saying the graphs were looked at |

### What is NOT verified, and is therefore not claimed

**Nesting and overlap are unverified against real content.** Every engine asset tried
reports `0 nested` and `0 node(s) inside more than one box`, so no nested or
overlapping box was available to render. Both code paths are written and **neither
has been seen to produce output**.

The arithmetic behind them is checked against a **model** in
`Tools/test-comments.py`: containment, edge and corner coverage, innermost-parent
selection, identical rectangles not parenting each other, and a node counted once per
covering box. That proves the arithmetic is right. It does **not** prove the C++
renders a nested box correctly, because there was no nested box to render.

This is the R-REFS lesson applied deliberately: a check that cannot fail the way
reality fails proves nothing, so the gap is named rather than papered over with a
test that passes for the wrong reason. `QUESTIONS.md` records it.

### Why membership is computed rather than read

A comment box stores its title, colour and rectangle. It does **not** store which
nodes it covers or which box contains it: the editor computes both from the
rectangles while drawing. So this tool computes them the same way rather than
inventing a rule.

Two decisions came out of that:

- **A node is covered by its position, not its rectangle.** An unlaid-out node
  reports a zero-size rectangle, so testing the rectangle would silently miss nodes
  the editor counts as inside. Position is also what a reader means by "inside".
- **A box is not a node inside a box.** Nesting is reported on the `parent box` line
  instead, so a nested box is described once rather than also appearing in its
  parent's node list.

A node inside two boxes is reported with a warning and counted, rather than being
assigned to one, because moving it changes the membership of every box that covers
it.

---

## 2l. Phase 2: R-DATATABLE

Built: `ReadTurboMCPDataTable`. 21 tools. Run its checks with
`python Tools/test-datatable.py`.

Verified against `/DatasmithContent/Datasmith/AreaLightsTable`, real content with a
known struct, known fields and 5 known rows.

| Test | Result | Evidence |
|---|---|---|
| The row struct is reported | **PASS** | `/DatasmithContent/Datasmith/AreaLightsStruct.AreaLightsStruct` |
| Fields are reported with their types | **PASS** | `fields: 1`, `Mesh_...  : UStaticMesh*` |
| The row count is exact | **PASS** | `rows: 5` |
| Row names are listed | **PASS** | 5 names, sorted |
| Row values are not printed | **PASS** | says so, and names the engine's own tools |
| Readers are found from the registry | **PASS** | `readers: 2 asset(s)` |
| An unloaded reader says its graph was not searched | **PASS** | `graph: not searched, this asset is not loaded` |
| A loaded reader's graph is named | **PASS** | `graph: UserConstructionScript`, and `2 of 2 reader(s) had their graphs searched` |
| The `schema` and `readers` filters each omit the other | **PASS** | asserted both ways |
| A bad filter is refused by name | **PASS** | `Accepted: all, readers, schema.` |
| A non-DataTable is refused with the class it found | **PASS** | names `Blueprint` |
| Every path form agrees | **PASS** | one identical body |

### A defect in the test, not the tool

The first version asserted that the readers start **unloaded** and that their graphs
are therefore not searched. It passed once and failed on every run after that,
because the Blueprints stay loaded for the rest of the editor session.

That is a test that cannot be re-run, which is worse than no test: it trains the
reader to ignore a red result. It now reads the actual starting state and asserts the
matching branch, so it passes on a fresh editor **and** on a second run in the same
session. Both were checked.

### A project-owned DataTable was attempted four times and abandoned

The engine's table proves the schema, the field types and the row names. It does
**not** prove the reader against a `UserDefinedStruct`, which is what a project-owned
table would have. Four attempts, all recorded because each is an engine rule worth
knowing:

| Attempt | Result |
|---|---|
| a `UserDefinedEnum` as the row struct | refused: a `UEnum` is not a `UScriptStruct` |
| `BlueprintEditorLibrary.add_member_variable` on a `UserDefinedStruct` | refused: that function only takes a **Blueprint**, so the struct stayed empty |
| rows written into that empty struct | only the last row survived, as row `Gamma`, against one auto-named field `MemberVar_0_...` |
| CSV import through `AssetImportTask` | refused: `A Data table row type must be specified in the import settings json file for automated import` |

Setting the row struct afterwards is not an option either: `RowStruct` is
**read-only** once the asset exists.

So a project-owned table needs either an import-settings JSON or a C++ commandlet.
Neither is worth building only to restate what the engine's own table already proves,
so the attempt is stopped and the gap **named** rather than hidden. Raised as Q-6.

One useful thing came out of it: the abandoned `ST_ShapeRow` and `DT_ShapeTest`
assets were deleted rather than left in the project, because a half-built asset that
`R-DATATABLE` can still open is worse than no asset — it looks like evidence.

---

## 3a. Phase 3: L-LAYOUT-REPORT

Built: `FTurboMCPLayoutReport` and `LookTurboMCPLayoutReport`, the first Look tool.
22 tools. Run its checks with `python Tools/test-layout.py`.

A Look tool, so it measures and never moves anything (`P3`).

### The check that matters: a clean graph and a messy one must differ

A layout checker that finds problems in everything is useless, and one that finds
nothing is worse. So the same tool was run over two of Epic's own macros: one laid out
cleanly, one with real wiring problems.

| Graph | Result |
|---|---|
| `StandardMacros / DoOnce` | 13 nodes, 18 wires, **`findings: none.`** |
| `StandardMacros / ForEachLoop` | 14 nodes, 24 wires, total wire length 18550, **1 backward data wire and 1 backward exec wire** |

That pair is the evidence. A tool reporting findings in both would be crying wolf; one
reporting none in either would be measuring nothing.

| Test | Result | Evidence |
|---|---|---|
| Node count, wire count, extent | **PASS** | `13 node(s), 18 wire(s)`, `extent: x ...` |
| Total wire length is the main measure | **PASS** | `total wire length: 6550  (longest single wire: 792)` |
| It says pin positions are computed, not read | **PASS** | in the result, not only in the source |
| A clean graph reports no findings | **PASS** | `findings: none. No backward wire, no overlap...` |
| Backward wires are found | **PASS** | `backward-data:` and `backward-exec:` separately |
| A backward wire is not condemned | **PASS** | says a loop and a cast-failed path are legitimately backward |
| Findings name node IDs | **PASS** | `...:5ee026 -> ...:abf042 : the target starts at x 1120 and the source ends at x 1400` |
| A graph with no wires says so in words | **PASS** | `this graph has no wires at all, so there is no flow to measure` |
| A bad graph name lists the real ones | **PASS** | names the Blueprint's graphs |
| A missing graph name is refused | **PASS** | `no graph given. Pass a graph name; the tool refuses rather than guessing` |
| A non-Blueprint is refused | **PASS** | `is not a Blueprint, so it has no graphs to measure` |

### Pin positions are computed, and the result says so

`03_TOOLS.md` calls total wire length "the main layout measure", and a wire's length is
the distance between its two pins. The engine has **no pin-position API** on
`UEdGraphNode`: the only one is `SGraphPanel::GetBoundsForNode`, which needs a live
graph widget and so cannot be reached from a tool call.

So pin positions are computed from each node's rectangle and its pin order. The measure
is therefore **consistent and comparable rather than pixel exact**, which is what a
layout measure is for: a tidy graph of a given size scores lower than a tangled one.
The tool prints that caveat in its own result, because a number whose meaning lives
only in the source is a number a caller will misread.

### A backward wire is reported, not condemned

`ForEachLoop` genuinely contains a loop that returns upward and a completed path that
rejoins behind it. Both are correct code and both are backward wires. Reporting them is
what the spec asks; calling them defects would be wrong, and "tidying" them would break
the macro.

The tool says so in words rather than only in this record, so a caller does not repair
something that was never broken.

### How the findings were checked before being believed

The first result flagged a `Make Literal Float` feeding a `float > float` comparison
whose target started to the left of where the source ended. That looked like it could
be a false positive, so the three flagged node IDs were resolved through
`R-NODE-RESOLVE` to confirm they were the nodes the finding named. They were.

The raw `NodePosX` could not be read back through the engine's `get_properties`, because
`NodePosX` is not a `UPROPERTY` and so is invisible to reflection. That is why the check
is a **pair of graphs that must differ** rather than a spot check of coordinates: two
graphs that must differ is evidence, one coordinate that happens to match is not.

---

## 3c. Phase 3: L-CAPTURE

Built: `LookTurboMCPCapture`. 24 tools. Run its checks with
`python Tools/test-capture.py`.

A Look tool, so it changes nothing but the image it writes (`P3`). Three framing modes:
the whole graph, one comment box by index, one node by ID.

### WHAT IS VERIFIED

| Test | Result | Evidence |
|---|---|---|
| The whole graph is framed | **PASS** | `framed: the whole graph, 11 node(s)`, `frame bounds: x 0 to 1960, y 0 to 416` |
| The view is really moved | **PASS** | `zoom: asked 0.729, applied 0.750 at x 980 y 208` — read back, not assumed |
| A PNG of exactly the size asked for | **PASS** | IHDR read back: **1600x900**, 48388 bytes |
| A second size is honoured | **PASS** | `800x400` renders a file whose IHDR is **800x400** |
| The name says asset, graph and mode | **PASS** | `BP_NodeTest_EventGraph_graph_1600x900.png` |
| A node ID from `R-NODE-FIND` is framed | **PASS** | `framed: node`, with the ID `R-NODE-FIND` supplied, so the two tools agree on the form |
| Asking for box 9 of a graph with none | **PASS** | `asked for box 9, this graph has 0 comment box(es)`, and **no fallback to the whole graph** |
| Mode `box` without a target | **PASS** | `needs a target` |
| An unknown node key | **PASS** | refused, and suggests `R-NODE-FIND` |
| A bad mode | **PASS** | `Accepted: graph, box, node.` |
| A bad size | **PASS** | refused by name |
| A graph that does not exist | **PASS** | lists the real graphs |
| An asset that is not a Blueprint | **PASS** | `is not a Blueprint` |

### WHAT IS NOT VERIFIED, AND IS THEREFORE NOT CLAIMED

**The captured canvas is empty.** The image contains the real graph editor chrome — the
toolbar, the back and forward arrows, the breadcrumb `BP_NodeTest > EventGraph`, and the
`BLUEPRINT` watermark — so the widget being rendered really is the graph editor. But **no
node is drawn**, and the image is not a usable picture of the graph.

I am recording this rather than presenting the file as a working capture. A picture of a
graph with no nodes in it is not a picture of the graph.

What was established before stopping:

- `SetViewLocation` **is** applied. Reading `GetViewLocation` back gives `applied 0.750 at
  x 980 y 208` against `asked 0.729`, so the engine accepted the move and clamped the
  zoom to its own limit. The frame is not simply being ignored.
- The render pipeline works: the chrome comes out, the size is exact, the file is a valid
  PNG, and the name is right.
- So the gap is specifically that the node widgets are not drawn into the render even
  with the view moved. `FSlateApplication::ForceRedrawWindow` was added to force the panel
  to recompute what is on screen, and it did not change the outcome.

### Follow-up session: the cause is narrowed, and the tool now fails honestly

The first attempt rendered the graph editor widget alone with
`FWidgetRenderer::DrawWidget`. That was replaced with a different route, and one real
**root cause was found and fixed**, but the canvas is still empty. Both facts matter.

**Found and fixed: a new tab has no geometry until Slate has ticked more than once.**

`OpenGraphAndBringToFront` spawns the tab, but the panel reports a size of `0x0` until the
layout and activation passes have run. One `FSlateApplication::Tick` was not enough. The
tool now pumps up to eight ticks, stopping as soon as the panel reports a size, and the
panel goes from `0x0` to **`988x742`**, with the frame landing fully inside the window
instead of being clipped. The tab was drawn greyed out in the window before this and is
active after it.

**Also fixed:** Slate geometry is in slate units and the picture is in pixels, so the
panel origin is divided by the widget scale. Leaving that out is a real bug that puts the
crop on the wrong panel on a scaled display. On this machine the scale is 1.00, so it
changes nothing here — it is recorded because it would matter elsewhere.

**Still not fixed: the framed crop does not land on the graph canvas.** With the geometry
correct, the computed frame is `x 17 to 1488, y 344 to 657` in a 1670x911 window, and the
image that comes out shows the **left sidebar** — the `My Blueprint` panel with its GRAPHS,
FUNCTIONS, MACROS and VARIABLES lists — not the graph canvas. So the picture is of the
editor, in the right place in the editor, but not of the graph.

**The engine's own screenshot was tried as the fallback and does work**: it returns a real
1670x911 PNG of the whole editor window, and `USlateInspectorToolset::Screenshot` was
called rather than reimplemented, as its own doc comment asks. Because the spec forbids a
silent fallback, the result labels it in words — `framed: the whole editor window, NOT the
graph, because the graph panel reported no geometry` — so the two cases can never be
confused.

**A test that could not fail the way reality fails.** Every check in this suite passed for
the whole session while the canvas was empty. Existence, size, header and file name are all
true of a picture of nothing. The suite now counts distinct colours in the image and
requires at least eight, which a flat fill cannot reach. That is the check that was
missing, and its absence is the more useful lesson than the bug.

Measurements taken while narrowing this, so they are not retaken:

| Fact | Value |
|---|---|
| Window size | 1670x911 |
| Graph panel inside it | x 259 y 130, **988x742** |
| Panel screen position | x 704 y 370, window at x 445 y 241 |
| Widget scale | 1.00 |
| Frame asked for | zoom 0.729, applied **0.750** at x 980 y 208 |
| Frame in window pixels | x 17 to 1488, y 344 to 657 (**1470x312**) |

`FWidgetRenderer::DrawWindow` paints through the engine's own `SWindow::Paint`, and the
render thread update it schedules uses `FApp::GetDeltaTime()`, not the `DeltaTime` argument
the caller passes. That is the next thing to look at, and it is written into Q-7.

Raised as Q-7, still open.

### A wrong assumption this tool corrected

The first version of the suite asserted that a **Widget Blueprint** would be refused, and
the tool's own doc comment said so. It is not refused: `UWidgetBlueprint` derives from
`UBlueprint`, so a Widget Blueprint's graph is captured like any other. The test now
asserts the real behaviour, and the doc comment was corrected with it.

This is worth recording because the assumption was mine and the doc comment repeated it:
writing a claim into a doc comment does not make it true.

### Three compile failures, all avoidable

- `UTextureRenderTarget2D` used without its header.
- A missing semicolon after an `Out += TEXT(...)` line — **twice**, in two places.
- `*WantedMode.IsEmpty() ? a : b` — the `*` binds to the bool, so the compiler reported
  *dereferencing a bool* for what was a precedence mistake. The label is computed on its
  own line now.

The semicolon one is worth noting: `Out += TEXT("a")` followed by `Out += TEXT("b")` with
no separator is a mistake that looks like valid continuation, because every other `Out +=`
in the file wraps across lines. It is the same mistake an earlier phase made once already.

`Tools/start-editor.ps1 -Stop` cleared a live `LiveCodingConsole` twice during this work,
each time unblocking a build that had already failed with `Unable to build while Live
Coding is active`.

---

## 3b. Phase 3: L-WIDGET-PREVIEW

Built: `LookTurboMCPWidgetPreview`. 23 tools. Run its checks with
`python Tools/test-widget-preview.py`.

A Look tool, so it changes nothing that outlives the call (`P3`). The instance it makes
is transient, is not saved, and is destroyed before the tool returns. The one thing it
writes is the PNG, which is the point of the tool.

| Test | Result | Evidence |
|---|---|---|
| It renders and writes a PNG | **PASS** | `image: .../Saved/TurboMCP/Captures/WBP_NodeTest_1280x720.png` |
| The file is a real PNG at the size asked for | **PASS** | IHDR read back: **1280x720**, 17709 bytes |
| A requested size is honoured | **PASS** | `640x360` renders a second file whose IHDR is **640x360** |
| It says how many values it set | **PASS** | `sample values: 0 set, 0 refused` |
| It says the function was not asked for | **PASS** | `function: none asked for` |
| It explains the forced alpha | **PASS** | says why a transparent PNG would look empty |
| It reports script messages | **PASS** | `script messages: 0 warning(s), 0 error(s)` |
| An unknown variable is refused with suggestions | **PASS** | `'NotAVariable' is not a property of this widget. Some it does have: ColorAndOpacity, ForegroundColor, Padding, ...` |
| Sample values must be a JSON object | **PASS** | an array is refused, saying `a variable has to be named` |
| A bad size is refused by name | **PASS** | `'banana' is not a size. Pass WxH, for example 1280x720.` |
| A size above the limit is refused, not clamped | **PASS** | `9000x9000 is larger than the 4096 limit` |
| A function taking parameters is refused | **PASS** | `'SetPositionInViewport' takes 2 parameter(s)` |
| A non-widget is refused with its class | **PASS** | `is a Blueprint, and only a Widget Blueprint can be previewed` |

### The image is entirely black, and that is the correct result

The test asset is a bare Canvas Panel with two empty Text Blocks and no background, so
the PNG is black. That is what those widgets look like. The suite asserts the pipeline
rather than the picture: an instance is made, the render target is written, the pixels
come back at **exactly** the requested size, and a valid PNG lands on disk with a
readable IHDR.

It is recorded this way deliberately. Presenting a black rectangle as "a rendered
widget" would be exactly the kind of claim this record exists to avoid. A caller wanting
a meaningful image sets values or adds content, which is their job and not the tool's.

### Three defects found building it

**1. `FImage` does not link.** Its constructor is not exported from the module, so
`FImage Image(...)` produced an unresolved external. `FImageUtils::PNGCompressImageArray`
takes the `FColor` array directly, which is exactly what the render target read back, so
`FImage` is not needed at all.

**2. `EGammaSpace::SRGB` does not exist.** The value is `sRGB`, lower case s.

**3. A short name did not find an asset that was loaded but not yet saved.** This is a
real hole in the resolver, not a mistake in the test.

An asset created in this editor session is loaded and addressable by its **full path**,
but the short-name search goes through the asset registry, and a package that has never
been saved may not be in it. So `WBP_NodeTest` was reported as missing while
`/Game/Tests/WBP_NodeTest.WBP_NodeTest` worked, and the scaffolding script tried to
create the asset a second time and was told `The path already exists`.

Fixed by checking the object graph with `FindObject` before the registry search, for a
short name only. That is the one case the registry cannot see, and it is exactly the
case a caller hits right after creating something.

### The lesson, for the third time

The same Widget Blueprint was created, previewed, and then **gone**: it had never been
saved, so stopping the editor destroyed it. `Tools/make-test-widget.py` now creates it
**and saves it**, and proves the save from the asset's own unsaved flag rather than from
a tool's return value.

This is the third occurrence in this project — after 13 lost nodes and the `R-DATATABLE`
readers. The pattern is always the same: a tool call that returns success is not evidence
that anything persisted. Only the asset's own flag is.

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

---

## 3c-bis. The UHT freshness check gave a false positive

`Tools/check-plugin.py` reported, all session:

```
FAIL  these reflection headers are newer than the generated code: TurboMCPToolsets.h
```

It was wrong, and this is the proof rather than an argument:

```
generated file: Intermediate\...\UHT\TurboMCPToolsets.generated.h
  contains LookTurboMCPCapture        ? True
  contains LookTurboMCPWidgetPreview  ? True
```

Both declarations are in the generated header, and `LookTurboMCPCapture` was loading and
answering MCP calls throughout. The build succeeded, exit 0, several times.

**Why it happens.** Editing a reflection header changes its mtime. UHT re-runs. When the
reflected declarations have not changed, UHT's output is **byte-identical**, so the
generated file keeps its old mtime while the edited header's is new. A timestamp
comparison cannot tell "UHT has not run" from "UHT ran and produced the same bytes".

**What was fixed.** The check compared *every* reflection header against one hard-coded
generated header, which is a category error: `TurboMCPToolsets.generated.h` says nothing
about whether another header is fresh. It now compares a header only against **its own**
generated file, and reports a header with no generated file of its own as a note rather
than as staleness. The failure message now says the result can be a false positive and
tells the reader to confirm before acting.

**What was NOT fixed, and is recorded instead.** A content-based replacement was written:
require every reflected name a header declares to appear in its `.generated.h`. It behaves
correctly on the real tree and the name extraction was verified to find all 27 declarations
with 0 missing. It was **withdrawn** because breaking the generated header on purpose did
not make it fail, three times, including after the first attempt was found to have written
through the `Marketplace` junction rather than the real path.

A check whose failure cannot be demonstrated is worth less than one whose false positive is
written down. Raised as Q-8.

The check is still red on this tree, and that is stated rather than worked around.

---

## 3c-ter. The reference picture that located Q-7, and three checks that can now fail

`L-CAPTURE` gained a `debug-window` mode: it renders the **whole editor window** at the
requested size with **no cropping**. It is what turned Q-7 from a guess into a
measurement, and it is kept because re-deriving it costs more than the mode does.

The uncropped 1600x900 picture shows the real graph canvas with the real nodes in it —
`Print String`, `In String`, `Development Only` — clustered in the upper right of the
canvas, which is where a view centred on x 980 y 208 at zoom 0.75 puts them.

**So the render pipeline works.** The defect is not that nodes fail to draw. The nodes
draw, and the crop cuts out the wrong rectangle: it lands on the left sidebar. Q-7b has
the arithmetic, including the back-solved view centre that disagrees with the one read
back, and the vertical origin mismatch that explains it.

### The framed image was 195 colours and the window picture 1659

That pair is the evidence, and it is now a check. Before this, every assertion in the
suite was true of a picture of nothing: existence, exact size, a valid PNG header and the
right file name.

### Three checks were shown to fail, and one method was wrong twice

The colour count is only worth having if it can go red, so it was broken on purpose:

| What was done | Result |
|---|---|
| A real framed image | **195** distinct colours, `>= 8` passes |
| The same file overwritten with a flat black fill | **1** colour, `>= 8` fails |
| A missing file | `None`, and reported as *could not be read*, not as a flat image |
| The uncropped window picture | **1659** colours, `>= 500` passes |

**The first two attempts to break it proved nothing**, and both are worth recording
because each one looked like a passing test:

1. The image was flattened **while the editor was running**. The suite calls the tool,
   which writes a fresh image, and then checks that image — so it checked a good picture
   and passed. `exit code 0`, and it meant nothing.
2. The same thing was then done **with the editor stopped**, so the tool could not
   overwrite it. The suite does not reach its check at all in that state, because the
   first RPC fails with `WinError 10061` and it exits on the connection. `exit code 1`,
   from the wrong cause.

The fix was to stop testing the suite and test **the check**: `distinct_colours` was
called directly on a real image, on a flattened one, and on a missing path. That is the
version in the table above and the only one that shows anything.

The general lesson, and it is the same one as section 3c-bis: a test that exercises the
whole suite to prove one check can fail is testing the harness, not the check. Break the
thing the check reads, then call the check.

---

## 3d. Phase 2 closed: R-BP-LOAD's widget tree section

The last substantive item of phase 2 is done. `R-BP-LOAD` at `signatures` and above now
carries a compact widget tree for a Widget Blueprint: one line per widget with its name,
class, whether the designer marked it a variable, its visibility when that is not the
default, its slot padding when that is not zero, any bound events, and the properties whose
value on a placed copy differs from the class default. Run its checks with
`python Tools/test-widget-tree.py`.

The JSON output format is still not done, and is still refused by name rather than faked.

### The bindings come from the Blueprint's own list, not from reflection

The first version walked the generated class's multicast delegate properties and matched
them to a widget by name prefix. That is a **reconstruction** of a fact the asset already
stores: `UWidgetBlueprint::Bindings` is an array of `FDelegateEditorBinding`, each naming
`ObjectName` (the widget), `PropertyName` (the delegate) and `FunctionName` (the function
graph bound to it). It was replaced with the direct read.

That is the fourth time in this project that reading the engine's own data beat deriving it,
after the node type ID, the `FText` source string and the overridable set.

### Three defects found while building it

**1. Object properties always differ.** The comparison reported `Slot` on every widget,
because `Slot` is a reference to that placed copy's own slot and so can never equal the
class default. Object properties are now skipped, which is what the spec means by the
instance-editable *values* a placed copy stores.

**2. `FText` cannot be compared as exported text.** Editing both sides with `ExportText` and
comparing the strings reported a difference on every `TextBlock` in the tree. `ExportText`
writes a text literal as `NSLOCTEXT(namespace, key, value)`, and the two sides can carry
different keys for the same visible string. `FText::EqualTo` compares what the user sees.

**3. One side of a difference is not a difference.** After fix 2, a real difference printed
as `Text="Text Block"`, which reads like no difference at all. Both sides are printed now,
so it reads `Text="Text Block" (class default "")`.

### The asset was lying, not the reader

The first run reported one widget where the script had added three, and the reader looked
wrong. It was not: the engine's own `GetWidgets` reported one widget too, so the **asset**
had lost its children somewhere between being scaffolded and being saved, and the loss was
silent. The tree now holds three because they were added and saved again, and the
scaffolding script asserts the count after every mutating step so the loss cannot happen
unnoticed a second time.

### A check that could never pass, found by reading rather than by running

`make-test-widget.py` verified the tree with `'"widgetCount":3' in tree`. `Session.call`
**decodes** the result, so `tree` is a dict and that substring can never be in it. The check
would have been red on every run, for a reason with nothing to do with widgets, and reading
the line is what found it.

### Both new checks were shown to fail

`test-widget-tree.py` passes on the real asset, and it was then run with
`EXPECTED_WIDGETS = 99` to confirm it goes red:

```
exit code avec EXPECTED_WIDGETS=99 : 1
FAIL it reports the widget count  expected 99
RESULT: FAIL (1) -> it reports the widget count
```

### A real defect in the plugin manifest, reported by the build

UHT warned that `TurboMCP` does not list `SlateInspectorToolset` as a dependency while its
module depends on that module. `L-CAPTURE` added that module dependency for its fallback and
the manifest was not updated with it. The plugin now declares it, and the warning is gone.

### What is still not done in phase 2

The **JSON output format** for `R-BP-LOAD`. It is refused by name, which is honest, and it
stays on the list rather than being quietly dropped. It is the last item of the phase.

---

## 3e. Phase 2 closed: R-BP-LOAD's JSON output

The last item of phase 2 is done, for the `manifest` level. 11 acceptance suites now. Run
its checks with `python Tools/test-json.py`.

### The decision that shapes it

`04_FORMATS.md` warns that most bugs in this project came from two tools disagreeing about
one format, and its rule is **one collection pass with two renderings**, never two
collections. So the JSON is a second *writer* over the same accessors the text writer uses,
and `Tools/test-json.py` reads the same asset both ways and requires the facts to agree.
The check is what keeps them from drifting.

Three further decisions, each recorded where it is implemented:

- **Graph bodies stay strings.** `04_FORMATS.md` section 2 defines the graph text format as
  S-expressions. Reshaping that into JSON objects would create a second node model for the
  same nodes, which is exactly the disagreement the format rules exist to prevent.
- **Indented by default, not compact.** The audience is an agent reading the document into a
  context window, where a compact one-line document costs the same tokens and is far harder
  to read when part of it is quoted back.
- **`signatures`, `bodies` and `full` are REFUSED by name, not answered partially.** A
  caller that asked for signatures and got a manifest-shaped document would read fields that
  are not there and conclude the Blueprint has no functions.

### A refusal is a parseable document

A machine-readable error is `{"ok": false, "error": "..."}`. A caller tests `ok` and never
has to parse the message, and a JSON parser handed the refusal does not fail with a syntax
error that says nothing about what went wrong. Verified:

```
read: 0 of 1 asset(s), detail signatures, 264 character(s)
{
	"ok": false,
	"error": "format 'json' is implemented for detail 'manifest' only, and 'signatures' was asked for..."
}
; 1 asset(s) were refused
```

### The equivalence checks, and why they cannot pass vacuously

Checking that the JSON parses proves almost nothing, because `{}` parses. The suite instead
compares the two renderings of the same asset:

| Check | Result |
|---|---|
| The asset has graphs, variables and components | **PASS** | 3, 2, 1 — asserted first, so the comparisons below cannot pass on an empty asset |
| Each count matches the list it summarises | **PASS** | 3 vs 3, 2 vs 2, 1 vs 1 |
| Nothing in the text is missing from the JSON | **PASS** | `missing []` for graphs, variables and components |
| Nothing in the JSON is absent from the text | **PASS** | `extra []` for all three |
| An empty list is an empty array, not a missing key | **PASS** | `interfaces: []` while the text says `interfaces: none` |
| A refusal is parseable JSON reporting `ok: false` | **PASS** | and it carries no partial document |
| The text rendering still works | **PASS** | JSON did not replace it |

The comparison was then shown to detect a divergence rather than merely passing: given a
text naming a graph the JSON does not have, it reports exactly that graph as missing.

### A defect in my own test, found by the refusal path

The first run failed `the refusal still parses`. The refusal was **valid JSON the whole
time**. The test used `json.loads` on everything from the first brace to the end of the
output, and the tool appends its own footer after the document — a count of refused assets
and the next step — which is not JSON. A strict parse of the whole tail therefore failed on
a well-formed document.

`json.JSONDecoder().raw_decode` reads one document and reports where it ended, which is the
right tool when the surrounding text belongs to the wrapper. Worth recording because the
failure looked like a defect in the tool and was a defect in the check, and the check was
the only reason it surfaced at all.

### What phase 2 now stands at

Every capability of phase 2 is implemented. The JSON format covers `manifest`; the three
larger levels are refused by name and the refusal is honest about what to use instead. That
is the state: **not complete, and not claimed to be.**

---

## 3f. R-BP-LOAD's JSON reaches signatures, and the budget becomes a refusal

`signatures` is now written in JSON as well as text. `bodies` and `full` remain, refused by
name with a parseable document that carries no partial content.

### The budget decision, which the level forced

The text writer steps a graph's detail down when the budget is passed, because a text
document that stops early is still readable. **A JSON document that stops early is not**: it
does not parse at all, so a truncated one is worse than useless, because the caller cannot
even read why it failed.

So the JSON is built in full, measured, and **refused** with a parseable error that names the
real size, the budget, and what to ask for instead. Measured on `BP_NodeTest`:

```
{"ok": false,
 "error": "the JSON document for detail 'signatures' is 1136 characters and the budget is
           500, so it was refused rather than cut. A cut JSON document does not parse,
           which is worse than no document. Ask for a lower detail level, or use the text
           format, whose writer steps a graph's detail down to fit."}
```

The same call with a 100000 budget succeeds, which is what makes the refusal about the budget
rather than about the level.

### One collection, two renderings, enforced by refactoring

A variable's flag list and a function's parameter list lived in an anonymous namespace inside
the text writer, which was fine while the text writer was the only reader. Two spellings of
one fact is the drift `04_FORMATS.md` warns about **even when both are right**, so they moved
to a shared `TurboMCPDescribe` module and both writers call it. The event and function
collection moved onto `FTurboMCPBlueprintRead` as a public method for the same reason.

The refactor was verified **behaviour-neutral before any JSON was added**: the existing suites
passed and the text output was read back unchanged. That order matters — a refactor and a
feature landing together cannot be told apart when something breaks.

### Four defects in my own test, all found by the refusal and comparison paths

1. The first run reported that the refusal did not parse. **The refusal was valid JSON the
   whole time.** `json.loads` was applied from the first brace to the end of the output, and
   the tool appends its own footer after the document. `raw_decode` reads one document and
   reports where it ended, which is right when the surrounding text belongs to the wrapper.
2. The text extractor treated the header's trailing count as a name, so `events: 3`
   contributed the name `"3"`.
3. It then ended a section only at a line ending in a colon, so it ran through
   `functions: 1` and the entire relation layer. **Indentation** is what delimits these
   sections, and the extractor uses it now.
4. A stale section of the suite still asserted that `signatures` was **refused**, which
   stopped being true the moment it was implemented.

Every one of those looked like the tool disagreeing with itself and was **the reader being
wrong**. That is the same shape as 3c-ter: a test exercising the harness rather than the
check.

### The equivalence check was still shown to fail

Fed a text naming a graph the JSON does not have, it reports exactly that graph as missing,
and the section boundaries were checked directly on a synthetic document rather than through
the whole suite.

### Where the JSON stands

| Level | State |
|---|---|
| `manifest` | **done** |
| `signatures` | **done** |
| `bodies` | refused by name, no partial document |
| `full` | refused by name, no partial document |

---

## 3g. R-BP-LOAD's JSON reaches bodies, checked by exact equality

Each graph now carries its body as the S-expression text, produced by the same
`FTurboMCPGraphText` the text writer and `R-GRAPH-LOAD` use, so all three cannot disagree.
`full` remains, refused by name.

### The comparison at this level is exact, and that is the point

For `manifest` and `signatures` the check is that a fact in one rendering appears in the
other, which is necessarily lenient because the text is prose. **A body is not prose**: both
renderings call one writer, so the two bodies must be identical character for character. The
suite requires that, and it was verified that a one-character difference is detected.

A lenient comparison would have passed while all three defects below were present.

### Three defects in my test, all in the text extractor

1. The document's own trailer — `; N item(s) could not be printed in full:` — has no graph
   marker, so a first version attached it to the **last** graph and reported that graph as
   diverging, `318 characters` against `145`, when the bodies themselves were identical.
2. Adding trailer detection then **discarded the last graph**, because the trailer header
   both ends the final body and starts the trailer.
3. The trailer's header line was collected as a note, so the text reported 2 notes where the
   JSON reported 1. A header is a header, not a note.

Each looked like the tool disagreeing with itself. The exact comparison is what found them.

### The filter and the notes are shared behaviour, not two implementations

The graph filter is applied the way the text writer applies it, so both cover the same
graphs, and a filter matching nothing is a **refusal** rather than an empty document. The
reason is the one the text writer already gives: an empty answer looks like "this Blueprint
has nothing" when the truth is "your filter is wrong".

Anything the graph writer could not print is carried as `body_notes`, a list, rather than
dropped. The text inlines those as `; error:` comments; a JSON caller gets values it can
test, and their **presence** is the signal that a body is incomplete. Both renderings are
required to agree on whether anything was unprintable, and the suite checks it: 1 note in
each, for the same node.

### Where the JSON stands

| Level | State |
|---|---|
| `manifest` | **done** |
| `signatures` | **done** |
| `bodies` | **done** |
| `full` | refused by name, no partial document |

### Two decision records the round confirmed rather than assumed

The budget decision from 3f holds at this level, which is where it matters most, because
bodies is where the document gets large: the JSON is built in full, measured, and refused
with its real size rather than cut.

The "bodies stay strings" decision also holds: the JSON body of `EventGraph` is the same
S-expression text the text rendering carries, verified by equality rather than by eye.

---

## 3h. R-BP-LOAD's JSON is complete: all four levels

`full` was the last one. Every detail level now has a JSON rendering, and every level's bodies
are required to be **identical character for character** between the two renderings.

### Full is forwarded, not reimplemented

The graph text writer already knows what `full` adds: each node's C++ class, its linked
outputs, the long-form node IDs, and a knot named on a link instead of being followed through
it. `TurboMCPBlueprintJson` picks the detail level and hands it over. Reimplementing any of that
would be the second node model the "bodies stay strings" decision exists to prevent.

Measured for the three graphs of `BP_NodeTest`: `full` is **2872** characters against `bodies`'
**1993**.

### Three of the four details the spec names are verified

| Detail | Result | Evidence |
|---|---|---|
| node type IDs, long form | **PASS** | `full` carries the full path; `bodies` does not |
| each node's C++ class | **PASS** | 8 × `; class ` at full, 0 at bodies |
| linked outputs | **PASS** | 7 × `; out ` at full, 0 at bodies |
| a knot named on a link | **NOT VERIFIED** | `BP_NodeTest` has no knot, so the branch never runs |

### The fourth is not verified, and is not claimed

`TurboMCPGraphText.cpp` has the branch:

```cpp
if (IsKnot(Target) && bFull)
{
    return FString::Printf(TEXT("(-> %s ; via knot)"), *TurboMCPNodeId::MakeForNode(Target));
}
```

**No test in this repository reaches it.** The JSON forwards the level to that same writer, so a
knot would flow through correctly — but *would* is not evidence, and this project has already
shipped four checks that passed while the thing they claimed to verify was broken.

The suite says so in its own output rather than leaving it implicit:

```
note: the knot branch of 'full' is NOT exercised: this asset has no knot. See QUESTIONS Q-9.
```

The **pin indexes** the spec also names are not visible in the output either: `full` prints
`; out NAME -> N link(s)` but no numeric index. That may be a deliberate difference from the
spec's wording or a gap, and it is not claimed either way. Both are recorded as Q-9.

### Two defects found

1. The document's `detail` field announced **`bodies`** for a `full` request, because the
   ternary fell through to bodies for everything above `signatures`.
2. A **stale section of the suite** still asserted that `full` was refused, which stopped being
   true the moment it was implemented. It also replaced a real check with a printed promise to
   check, which is worse than the stale assertion. A misspelled detail **name** is checked
   instead, because that is what a caller can actually reach through the tool.

### Where R-BP-LOAD now stands

| Level | Text | JSON |
|---|---|---|
| `manifest` | done | **done** |
| `signatures` | done | **done** |
| `bodies` | done | **done** |
| `full` | done | **done** |

One item remains in this area: the **widget tree in JSON** at `signatures` and above, which the
text rendering already carries.

---

## 3i. R-BP-LOAD is complete: the widget tree in JSON

The last leftover. The text rendering has carried the widget tree since 3d; the JSON now carries
the same facts, and `R-BP-LOAD` is complete in both renderings at all four levels.

### One collection, two renderings

The JSON calls `FTurboMCPWidgetTree::Collect`, the same call the text rendering makes, rather
than walking the widget tree a second time. A second walk would be a second opinion about the
same tree, which is the drift `04_FORMATS.md` warns about.

### The visibility difference is deliberate, and the suite checks it as such

The text omits `visibility=Visible` because it is the enum default, and printing it on every
line buries the widgets whose visibility is **not** the default. JSON has no such noise: a
caller reads a field and compares it, so the default is exactly what it wants written down.

JSON therefore carries the visibility of **every** widget, and that makes the two renderings
**deliberately different on one point**. A comparison that treated every difference as drift
would fail; one that ignored differences would miss real ones. The suite is written to expect
this specific difference:

| Check | Result |
|---|---|
| Every widget in the JSON is named in the text | **PASS** | `RootCanvas`, `TitleText`, `TitleText2` in both |
| Every non-default visibility in the JSON appears in the text | **PASS** | `visibility=SelfHitTestInvisible` is in both |
| The text does **not** print the default visibility | **PASS** | `visibility=Visible` appears nowhere in the text |
| The class of each widget appears in the text | **PASS** | `[CanvasPanel]`, `[TextBlock]` |
| The count agrees | **PASS** | `widget tree: 3 widget(s)` |
| The root is at depth zero and a child is deeper | **PASS** | 0 and 1 |

Padding is carried as an **object** of four numbers rather than as the text's four-number
string, for the same reason: a caller parsing JSON should not have to parse a string inside it.

The comparison was shown to fail: fed a JSON naming a widget the text does not have, it reports
exactly that widget as missing.

### Three defects in my test, all the same shape as the previous rounds

1. It loaded the Widget Blueprint at the default `manifest` level, where **neither** rendering
   carries a tree, and reported the tree missing from **both**.
2. The name extractor returned whole decoration strings for widget lines —
   `RootCanvas [CanvasPanel] visibility=...` — so the two renderings were compared on strings
   the JSON does not repeat.
3. It then returned the facts that follow a name — `bound: ...` and
   `differs from the class default: ...` — as if they were names of widgets.

Each looked like the tool disagreeing with itself. That is now the fifth round in a row where
the reader was the defect, and it is recorded as the standing risk it is.

### A cast worth explaining rather than hiding

`Cast` cannot drop const, and the engine's widget-tree accessors are non-const, so the JSON
writer takes a `const_cast`. **Nothing writes through it**: this is a Read tool and `P3` says a
Read tool cannot change anything. The cast is for the accessor signatures, not for a write, and
it says so in the code.

### R-BP-LOAD, complete

| Level | Text | JSON |
|---|---|---|
| `manifest` | done | **done** |
| `signatures` | done | **done** |
| `bodies` | done | **done** |
| `full` | done | **done** |
| widget tree | done | **done** |

---

## 3j. Q-9's knot half: answered, and the answer is that no available tool can make a knot

The `full` level's knot branch cannot be exercised, and the reason is now a **measured fact**
rather than an omission. Three attempts, each with its exact refusal:

### 1. The engine's own DSL cannot create a reroute node

`blueprint_dsl.py` **recognises** one. It defines `_REROUTE_NODE = '|RerouteNode'`, has a
`_follow_reroutes` walker that traverses knots while reading a graph, and even raises
`Cycle detected in reroute (knot) nodes` on a malformed chain. So the type is known to the
engine's writer.

Writing one through `write_graph_dsl` is refused anyway:

```
AssertionError: The node could not be created / |RerouteNode does not exist
in: (|RerouteNode)
in: (event EventBeginPlay)
```

**The engine's writer can traverse a knot and cannot make one.** That is the answer.

### 2. A knot is not a menu node type

A sweep of `find_node_types` filtered to `Utilities|FlowControl` returns **914** types and none
is a knot. Consistent with the refusal above rather than a second problem.

### 3. `UK2Node_Knot` is not reachable from Python

It is declared `UCLASS(MinimalAPI)`, and the programmatic toolset permits only `copy`, `re`,
`math`, `json`, `datetime` and `time` as imports, so `unreal` is not available to a script at all.

### The suite now asserts the refusal

That turns a documented gap into a **live check**: if a future engine version starts accepting a
reroute node, `test-json.py` goes **red** and the knot branch can finally be exercised. The suite
also checks that the refused write left the graph **unchanged** — 3 graphs before, 3 after —
because a tool that refuses *after* writing something would be worse than one that accepts.

### The branch is not wrong, and it is not verified

The JSON forwards the level to the same writer, so a knot would flow through. That is
**reasoning, not evidence**, and section 3h records three of the four details as verified. This
is the fourth, and it stays unverified.

### How it could still be closed

Recorded in Q-9: an engine tool that spawns nodes directly rather than through the DSL, or a small
commandlet in this plugin using `FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Knot>` the way
`EdGraphSchema_K2.cpp` does. The second is also the route to the hand-made acceptance kit that
`06_ACCEPTANCE.md` requires, so it would pay for itself twice.

### Still open, and deliberately not closed on a partial answer

Whether the **pin index** the spec names should be printed. `full` prints `; out NAME -> N
link(s)` and no numeric index. Deliberate difference or gap is not decided, and it is not claimed
either way. Q-9 keeps that half open.

---

## 4a. Phase 4 begins: the P/E-ENUM-ADD pair

Phase 4 is the first **writing** phase. 26 tools now. `T-STRUCT-2` is exercised by
`python Tools/test-enum-add.py`.

### One code path, two outcomes, enforced by construction

`FTurboMCPEnumAdd::Plan` validates the request, `RenderPreview` prints it, and `Apply` carries out
exactly that plan. Both tools call `Plan` and differ only in what they do with the result, because
`D-02` requires a Preview and its Edit twin to share everything up to the write step.

### The engine's own creator, not a hand-built asset

The enum is made through `FEnumEditorUtils::CreateUserDefinedEnum`, and its values through
`FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum`, so it stays **editable in the enum editor**
rather than being an asset the editor cannot open.

### A mistake the tool caught on itself

The first version tried to set the enumerator names by writing `UEnum`'s private `Names` array
through reflection. It could not, and it **said so** rather than writing wrong names:

```
error: TA18_ColourSuite was created with 3 value(s) but the engine's enumeration
names could not be reached, so they carry generated names.
```

`UEnum::Names` is a private **union** — a `const FName*` used by compiled-in enums and a `TArray`
used by user-defined ones — and it carries no `UPROPERTY`, so `FindFProperty` cannot see it. The
tool detecting its own limitation is what stopped a broken enum from shipping.

### The right API, found by reading the engine

`FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum` itself calls **`UEnum::SetEnums`** to
rebuild the name list, and `SetEnums` is public. The tool now uses it. It rebuilds in one step,
which is also safer than poking entries: there is no half-renamed state to leave behind.

Two naming traps, both hit and both recorded: **`EEnumFlags` is a GLOBAL enum** in
`ObjectMacros.h`, while **`EAddMaxKeyIfMissing` and `EUnderlyingType` are members of `UEnum`**.

### The check that matters: the names are in the SAVED file

The tool reports how many names it set, but that number comes from **the same code that set them**,
so on its own it proves nothing. The suite therefore:

1. creates the enum through the tool,
2. **saves** it, because an unsaved asset dies with the editor and this project has lost test
   assets that way three times,
3. confirms the asset's **own unsaved flag**, and
4. **opens the `.uasset` and looks for the value names in it.**

A name that was never written is not in a serialised archive. Measured on the **1933-byte** file:
`Red`, `Green`, `Blue`, `Dark Green` and the engine's own `_MAX` entry are all present. Stripping
them makes the check go **red**, which was verified rather than assumed.

### Seven refusals, each naming its reason

| Refused | Message |
|---|---|
| an empty path | `no path given` |
| a relative path | `does not start with '/'` |
| no values | `no values given` |
| a name with a space | `' ' is not a letter, a digit or an underscore` |
| a name starting with a digit | `does not start with a letter or underscore` |
| `_MAX` | `reserved: every user-defined enum carries one` |
| a duplicate | `'Red' is given twice` |

A refusal opens with **`error:`** and an existing asset opens with **`unchanged:`**. They are
deliberately different: one means the request was wrong, the other means nothing needed doing and
a caller should not retry it.

### Still to come in phase 4

`P/E-VARS-ADD`, `E-WIDGETS-AS-VARS`, `P/E-OVERRIDE-ADD`, `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`,
and `P/E-WIDGET-RETARGET`.

---

## 4b. P/E-VARS-ADD, and a Preview that lied

28 tools. 13 acceptance suites. `T-STRUCT-1` is exercised by `python Tools/test-vars-add.py`.

### A preview that lied, found by its own suite

The first version resolved the variable's **type** in `Apply` only. So `P-VARS-ADD` happily printed
a plan for `X: banana` while `E-VARS-ADD` refused it — **exactly the disagreement `D-02` exists to
prevent**.

It was found because the suite asks the **Preview** to refuse the bad inputs rather than the edit.
The type is now resolved while planning, so both tools refuse the same list. This is the second
time in phase 4 that the shared-code-path rule has caught a real divergence rather than being
decorative.

### The splitter counts depth, because a map has a comma in it

`Map<String, int>` carries a comma inside it, so a plain comma split turns one variable into two
broken ones. Depth is counted rather than pattern-matched. A type whose brackets do not balance is
refused **by name** rather than silently truncated.

### Five API names were guessed and all five were wrong

Each was corrected by reading the engine, and each is worth recording because guessing is what
this project keeps having to unlearn:

| Guessed | Reality |
|---|---|
| `GetEdGraphPinTypeFromSimpleType` | does not exist; the wrappers are `GetBasicTypeByName`, `GetStructType`, `GetArrayType`, `GetSetType`, `GetMapType` |
| `UBlueprintEditorLibrary::GetEnumType` | does not exist; an enum's pin type is `PC_Byte` with the `UEnum` as the sub-category object |
| `CPF_Private` | **does not exist in UE 5.8**; a Blueprint variable's "private" is `CPF_EditConst` |
| `CPF_BlueprintReadWrite` | does not exist either; read-write is the **absence** of `CPF_BlueprintReadOnly` |
| `SetBlueprintVariableMetaData(4 args)` | takes a local-variable-scope argument, `NULL` for a Blueprint-level variable, as the engine's own `BlueprintEditorLibrary.cpp` passes |

### The engine's type resolver cannot report failure

`GetBasicTypeByName` says so in its own comment: it *"returns 'int' type if invalid type is
provided"*. A caller trusting it would **silently add an int where a `banana` was asked for**. So
the name is checked against the accepted list first, and the resolver only ever sees a name known
to be valid.

### The check that matters reads back through a different tool, after a save

`E-VARS-ADD` reports what it added and read back, but **both numbers come from the same code**. The
suite then calls `R-BP-LOAD`, which reads the Blueprint's own variable list:

| Fact | Evidence after the save |
|---|---|
| the variables exist | `Speed : Float (double-precision)`, `bIsArmed : Boolean`, `Label : String` |
| the category survived | `category: Movement` |
| the read-only flag survived | `BlueprintReadOnly` |

**Every name is checked before anything is added**, so a list of five that would clash on the
fourth adds none and says `unchanged:`, rather than leaving the Blueprint half-changed.

### Nine refusals, each naming its reason

An unknown type, an unknown field, a duplicate name, a name with a space, a missing colon, an
unbalanced type, an empty list, no asset, and a container of an unknown type.

### Still to come in phase 4

`E-WIDGETS-AS-VARS`, `P/E-OVERRIDE-ADD`, `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`, and
`P/E-WIDGET-RETARGET`.

---

## 4c. E-WIDGETS-AS-VARS: an Edit with no Preview twin

29 tools. 14 acceptance suites. `T-STRUCT-3` is exercised by
`python Tools/test-widgets-as-vars.py`.

### Edit only, on purpose, and the reporting earns it

`03_TOOLS.md` says no Preview twin is needed *"because it is a flag change and the Edit reports old
and new state"*. A preview exists to show the shape of a change a caller cannot see without making
it, and **a boolean flag has no shape**. What replaces the preview is the reporting:

```
changed: 2 of 2 widget(s) in /Game/Tests/WBP_NodeTest.WBP_NodeTest
set to: variable
  TitleText: was not a variable, now variable
  TitleText2: was not a variable, now variable
; there is no Preview twin for this tool: it is a flag change, and this line reports the
  old and the new state of every widget, which is what a preview would have shown.
```

The suite checks that the **old** value is really there, because that is the claim a preview-less
tool has to earn.

### The editor's own call

The flag is set through `FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable`, the same call the
details panel's "Is Variable" checkbox makes, so a widget marked here is indistinguishable from one
marked by hand. The class is public and its methods are per-method exported, so it links without
reaching into the engine.

### Atomic, and proved rather than asserted

Every name is resolved **before** anything is touched. A call naming one real widget and one
misspelled one changes **nothing** and lists the real names:

```
unchanged: 1 widget name(s) do not exist in /Game/Tests/WBP_NodeTest.WBP_NodeTest, so nothing
was changed: NoSuchWidget. The widgets are: RootCanvas, TitleText, TitleText2
```

The suite proves the atomicity rather than trusting it: it marks two widgets, then calls with one
good and one bad name to **unmark**, then reads the flag back — and the good name is **still
marked**, so the refused call did not half-apply. That is `P8`, and it is the check most likely to
be skipped.

### Idempotent, and honest about it

A second call in the same direction opens with `unchanged:` and says `already in that state: 2`
rather than counting them as changes. A caller that asked for ten and got three needs to know
**which** three.

### The read-back is through a different tool

The tool reports what it changed; `R-BP-LOAD` prints a widget as `variable` from the asset's own
flag. After a save:

```
widget tree: 3 widget(s), depth-first from the root
  RootCanvas  [CanvasPanel]  visibility=SelfHitTestInvisible
  TitleText  [TextBlock]  variable
  TitleText2  [TextBlock]  variable
```

### Still to come in phase 4

`P/E-OVERRIDE-ADD`, `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`, and `P/E-WIDGET-RETARGET`.

---

## 4d. P/E-OVERRIDE-ADD, E-OVERRIDE-REMOVE, and a data-loss bug I caused

32 tools. 15 acceptance suites, all green. `T-STRUCT-4` is exercised by
`python Tools/test-override-add.py`.

### Why this is a wrapper and not a new mechanism

The engine **already has the operation**. `UBlueprintEditorLibrary` carries `AddFunctionOverride`,
`AddEventOverride`, `RemoveFunctionGraph`, `ListFunctions` and `ListEvents`. What it does not have
is the three things `03_TOOLS.md` asks for, and those are the whole reason this capability is in
the roadmap:

| Requirement | What the engine does instead |
|---|---|
| the Preview prints the exact **signature** | resolved here from the real reflected `UFunction` |
| it **refuses** when already overridden, and says **where** | the engine would quietly add a **second graph** |
| the Edit reports what it made, and reads back | not done |

**The "already overridden" check lives in `Plan`, not `Apply`,** so the **Preview** refuses too. A
preview that shows a plan the edit will not carry out is the disagreement `D-02` exists to prevent.
This is the **second time** in phase 4 that putting a check in the wrong half was caught by the
suite rather than by reasoning.

### I destroyed the test Blueprint's event graph

`E-OVERRIDE-REMOVE`'s first version called `RemoveGraph` on the graph holding the event node. **For
an event override that graph IS the event graph**, so removing one override took `ReceiveBeginPlay`,
`ReceiveActorBeginOverlap` and `ReceiveTick` with it and left the Blueprint with **one graph instead
of three**. The overridden count went from **4 to 1**.

It was caught **only** because the count *after* the removal was read back and compared with the
count *before*. That is the read-back rule this project keeps relearning, and this time it caught a
real loss rather than a cosmetic one.

**The asset was restored from git**, which is why committing the test asset matters: the broken
state had never been committed, so `git checkout -- BP_NodeTest.uasset` brought back the graph and
the four overrides. Verified afterwards: **3 graphs, 4 overridden**.

The fix removes the **node** rather than the graph, through `Modify`, `DestroyNode` and
`MarkPackageDirty`, which is what the editor's own node deletion does.

### Two other suites were broken by my own work, and both had pinned counts

- `test-overrides` asserted `4 overridden, 23 overridable`.
- `test-capture` asserted `11 nodes`.

The override capability added overrides and the node count grew, so **three checks in one suite and
one in the other** failed for a reason that had nothing to do with the tools they test. Both are
fixed by making the checks **relative** rather than absolute:

- the overrides suite **parses the summary line** and checks internal consistency — the two filters
  add up to the total, the count matches the list;
- the capture suite takes the node count from the tool's report and **cross-checks it against
  `L-LAYOUT-REPORT`** counting the same graph.

Those hold however much the shared asset changes, and they are what the tools actually promise.

### The suite now owns its event and gives it back

An earlier version overrode any event that happened to be free, which **consumed the asset**: each
run consumed one event, and after four runs there was nothing left. It now overrides one event it
owns, checks the behaviour, **removes its own override**, and asserts the count is back where it
started. A test that consumes a finite shared resource is not re-runnable, and this one now is.

### Still to come in phase 4

`E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`, and `P/E-WIDGET-RETARGET`.

---

## 4e. E-FUNCTION-ADD and E-FUNCTION-REMOVE: a silent rename caught in the act

34 tools. 16 acceptance suites, all green. `T-STRUCT-5` is exercised by
`python Tools/test-function.py`.

### The engine does not use the name it is given

`UBlueprintEditorLibrary::AddFunctionGraph` **uniques the name against the objects still in the
package**. Asking for `ComputeScore` when something still held that name created a graph called
`ComputeScore_0` — and the call **returned a valid graph**, so the first version of this tool
reported

```
changed: added function 'ComputeScore' to /Game/.../BP_NodeTest
```

and was **WRONG**. The caller would then call `ComputeScore`, find nothing, and have no way to know
why.

**Nothing in the tool's own report showed it.** It was found by removing a function, re-adding the
same name, and **reading the functions back**: the list said `ComputeScore_0`. That is the **third
time** in phase 4 that reading the result back through a different view caught a defect the
writer's own report hid.

### The fix, and the proof that it fires

After creating, the graph's name is compared with the name asked for. When they differ the graph is
**removed again** and the call is **refused** — removed rather than reported, because leaving a
`ComputeScore_0` behind is worse than not creating it.

**The guard was shown to fail before it was believed.** The pre-check that should have caught the
clash was temporarily disabled, the code rebuilt, and the same name added twice:

```
unchanged: the engine would not create a function named 'ProveGuard' and made 'ProveGuard_0'
instead, because something in the package still holds that name. The graph was removed again,
so nothing was added.
```

and reading the graphs back showed **only `ProveGuard`** — no `ProveGuard_0` left behind. The check
was then restored and the tree verified free of the temporary change. **A guard that has never been
seen to fire is not a guard.**

### The three refusals are told apart, as the spec requires

| Refused | Message ends with |
|---|---|
| `ComputeScore`, already a function here | `'ComputeScore' is already a function on this Blueprint` |
| `ReceiveBeginPlay`, already an event here | `'ReceiveBeginPlay' is already an event in 'EventGraph'` |
| `ReceiveDestroyed`, declared by the parent | `declared by the parent class Actor … **Use P/E-OVERRIDE-ADD instead.**` |
| `Has Space` | `' ' is not a letter, a digit or an underscore` |
| `1Bad` | `it does not start with a letter or underscore` |

The parent case is refused **differently** from a plain clash, because a caller who wants to
implement a parent function wants an **override**, not a new function that would leave the parent's
version untouched and never called.

### Remove now claims only what it can check

It said `the name is free again: yes` on a check that only looks at the Blueprint's graphs, and the
**package** can still hold the name. It now says:

```
no function graph holds that name now: yes
; this does NOT guarantee the package has released the name. The engine uniques a new function's
  name against the objects still in the package, so adding the same name back immediately may
  still be renamed; E-FUNCTION-ADD detects that and refuses rather than creating a numbered graph.
```

**Add and Remove are one capability** because REMOVE is what frees a name an ADD took by mistake.
Without it, a mistyped function name could only be cleared in the editor by hand.

### Still to come in phase 4

`P/E-WIDGET-RETARGET`. `E-COMPILE` is already done.

---

## 4f. P/E-WIDGET-RETARGET closes phase 4

37 tools. 17 acceptance suites, all green. **Phase 4 is complete: all six capabilities.**

### The engine's own check is too narrow for real widget events

`FDelegateEditorBinding::IsBindingValid` looks for a **`FDelegateProperty`** named
`<PropertyName>Delegate` or `<PropertyName>`. Measured against the engine's own headers:

```
Button.h:18   DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnButtonClickedEvent);
Button.h:75   UPROPERTY(BlueprintAssignable, Category="Button|Event")
Button.h:76   FOnButtonClickedEvent OnClicked;
```

`UButton::OnClicked` is a **multicast** delegate, so that check **never finds it**. A rule that
rejected every normal widget event would be useless and its **"ok" case would be unreachable** —
which means a test of the broken case alone would have passed while proving **nothing** about the
good one. Both property kinds now count, and the result says which was found:

```
ok      TitleText.TextDelegate  on TextBlock  (a single delegate)  -> GetTitleText
```

### Both paths are checked, and that is the whole point

A tool that removes broken bindings can be tested with a broken binding alone — and such a test
**passes while the tool deletes EVERY binding it sees**. The suite makes one of each:

```
preview: retarget /Game/Tests/WBP_NodeTest.WBP_NodeTest
1 binding(s), 1 the current class cannot satisfy
  ok      TitleText.TextDelegate  on TextBlock  (a single delegate)  -> GetTitleText
  BROKEN  TitleText2.NoSuchDelegate  TextBlock has no delegate 'NoSuchDelegateDelegate'
```

and checks that **exactly one survives**, reading the result back through the **preview** rather
than trusting the edit's own report.

### `E-WIDGET-BIND-ADD` exists because the retarget tool has nothing to repair without it

A binding is a `FDelegateEditorBinding` inside the Widget Blueprint, and **the editor only writes
one when a user picks an event in the details panel**, which does not exist headless. Without this
tool the retarget capability could only ever be tested against an asset with **no bindings** — a
check that passes while proving nothing.

Binding a delegate the widget's class does **not** carry reproduces exactly the state a class swap
leaves behind, and the result **says** which kind it made.

### A wrong guess, corrected by reading the engine

The first attempt bound `TextBlock.OnTextChanged` on the assumption that `TextBlock` carries it.
**It does not:**

```
TextBlock.h:35   FGetText TextDelegate;
TextBlock.h:44   FGetSlateColor ColorAndOpacityDelegate;
TextBlock.h:73   FGetLinearColor ShadowColorAndOpacityDelegate;
```

The "ok" path stayed unreachable until `TextBlock.h` was read. That is the **fourth** guessed API
name this phase has had to correct, and the reason this project reads the engine before writing
against it.

### The preview says how validity is decided

Its own output carries the sentence a reader needs to trust the `BROKEN` marks: *validity is decided
the way the engine's own compiler decides it: the binding's delegate is looked for ON THE WIDGET'S
CURRENT CLASS.*

The removal is by the pair the engine itself uses as a binding's identity — `ObjectName` and
`PropertyName` — rather than by list index, which would shift under the loop. The read-back
**re-counts** the bindings and reports how many are still broken, rather than writing `N of N`,
which would be tautological and prove nothing.

### Phase 4 complete

| Capability | Commit |
|---|---|
| `P/E-ENUM-ADD` | `d08f4bb` |
| `P/E-VARS-ADD` | `eb5ad04` |
| `E-WIDGETS-AS-VARS` | `8a0f994` |
| `P/E-OVERRIDE-ADD` + `E-OVERRIDE-REMOVE` | `a488aad` |
| `E-FUNCTION-ADD` + `E-FUNCTION-REMOVE` | `b107b17` |
| `P/E-WIDGET-RETARGET` + `E-WIDGET-BIND-ADD` | this commit |

Next is **phase 5: call log and analytics**.

---

## 5a. The call log, over an engine hook that carries less than the spec expected

40 tools. 18 acceptance suites, all green. `E-CALL-LOG`, `R-CALL-LOG` and `E-CALL-LOG-CLEAR`.
`T-LOG-1` to `T-LOG-3` are exercised by `python Tools/test-call-log.py`.

### The engine's hook does not carry what the spec assumed

Read from the engine's own `ModelContextProtocolAnalytics.cpp`, a tool-call event carries **exactly
five attributes**:

| Attribute | Value |
|---|---|
| `SessionId` | the MCP session id |
| `ToolsetNameHash` | `LexToString(FBlake3::HashBuffer(utf8(name)))` |
| `ToolNameHash` | the same, over the tool identifier |
| `Duration` | seconds |
| `ResultStatus` | `Success` or `Error` |

**The names are BLAKE3 hashes and there are no arguments at all.** `07_ENGINE_NOTES.md` section 2.4
anticipated this and gave the shape of the answer the plugin took: *"record the fields it has for
engine tools, and record full detail for the plugin's own tools from inside those tools."*

### Never replace a hook without passing its events on

`FTurboMCPAnalyticsProvider` holds whatever provider was installed before it and forwards **every**
call to it. That was the exact risk the phase 0 stub was guarding against — *"so that a wrong guess
cannot corrupt engine telemetry"* — and it is handled in the provider rather than by not installing
one.

### The hash is reproduced, not guessed

The engine hashes the **UTF-8 bytes** of the name and prints the hash with `LexToString`. Hashing
the `TCHAR` buffer instead would differ for any non-ASCII name, so the UTF-8 step is reproduced
exactly. This plugin's own toolset names then resolve:

```
2026-10-09T17:07:20.229Z  end  engine  TurboMCP.TurboMCPEditToolset...
```

### The harder half does not resolve, and is labelled rather than invented

`UToolsetRegistry` exposes **no list** of served toolsets or tools — only `IsToolsetRegistered(name)`
— so the engine's tools cannot be enumerated from here and their hashes cannot be turned back into
names. Those lines carry the first twelve characters of the hash with the word `engine`:

```
(engine tool ending 43b186416e61)
```

That tells two engine tools apart inside a session and **says what it is**. `QUESTIONS.md` **Q-10**
records the limitation and the one route that would close it.

### A wrong answer worse than a hash, caught by reading the output

The first version tried each tool against **every** toolset prefix and logged:

```
TurboMCP.TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset.ReadTurboMCPBlueprintLoad
```

— the toolset repeated inside the tool name, which **looks right and is not**. It was visible in the
first run's own output. The pairing is now data rather than a loop.

### The observer is part of the log

`R-CALL-LOG` is a tool call like any other, so **every read of the log appends to the log**; and
`E-CALL-LOG-CLEAR` writes its own records, so clearing and then reading found the clear's own lines
and reported ten entries in a memory that had just been emptied. The suite now switches logging off
around the measurement.

### A count is parsed, not substring-matched

A check that looked for the **absence** of `"0 in memory"` was true both when the report was empty
**and** when the memory really held none, so the second run went red on a correct state. That is the
**fourth** order-dependent check this project has had to fix, and the fix is the same each time:
**assert a parsed value, not a string that can appear for two reasons.**

### The switch

The log **starts OFF**, per the spec, and the choice persists in the editor settings via a
`UDeveloperSettings` subclass. The value is parsed **strictly** rather than through
`FString::ToBool`, which answers false for anything it does not recognise — so a caller who typed
`on` would have silently turned the log **OFF**. `E-CALL-LOG-CLEAR` drops only the in-memory entries
and says so: the **file** is the record of what happened.

---

## 5b. Q-10 closed: every name in the call log resolves

18 acceptance suites, all green. `Q-10` is **RESOLVED** rather than worked around.

### The engine already solves this for itself, and the answer was in its own commandlet

The previous round concluded engine tool names could not be resolved because `UToolsetRegistry`
exposes no list of served toolsets or tools. **That was the wrong place to look.** The engine ships
`ModelContextProtocolToolHashMappingCommandlet`, which builds **exactly** the hash-to-name mapping
this provider needs. Reading it gave the whole answer.

### Three facts closed it

| Fact | Consequence |
|---|---|
| `IModelContextProtocolModule::GetTools()` returns every registered tool with its identifier | the hand-written table is **gone**; a tool added anywhere now resolves |
| `HashToolIdentifier` and `ParseToolsetName` are exported `UE_API` | the **reimplementation of the hash is gone** |
| In tool-search mode `GetTools()` returns exactly **three** identifiers, all with an **empty** toolset prefix | the inner event's toolset hash is over the string the caller passed, and its **tool** slot holds the hash of the **whole qualified name** |

**The reimplementation was a liability, not a virtue.** It was correct only while it matched, and
**nothing would have said so when it stopped.** The engine's own function is now called, which makes
the lookup exact by construction.

The third fact is measured, and the engine says so itself in
`ModelContextProtocolToolsetRegistryAdapter.h`: *"In tool-search mode ... toolset tools are
dispatched through `call_tool` without being registered."* `ModelContextProtocolServer.cpp:904`
builds `ResolvedFullName = toolset_name + "." + tool_name` and passes **that**, which is why
registering the bare tool name was not enough.

### The two events per call are the engine's own design, and the earlier note is withdrawn

`ModelContextProtocolServer.cpp:887` emits one event for `call_tool` and `:907` emits a **second**
for the inner tool, with the comment:

> *Tool-search mode routes every actual toolset invocation through `call_tool`; without a second
> emission every routed call collapses onto the empty-prefix toolset hash and per-toolset analytics
> for the real targets stay invisible.*

**Two lines per call is correct.** The note in this file calling it an unexplained duplication is
withdrawn.

### A bug the fix exposed: resolved-and-empty is not unresolved

The empty prefix is a **legitimate key** — the engine's own commandlet registers it — so `call_tool`
resolves to the empty string. The lookup then tested `IsEmpty()` to decide whether resolution had
happened and printed:

```
(engine tool ending af1349b9f5f9)  .  call_tool
```

— **a line saying it could not name a toolset it had just named.** Resolution is now tracked by
whether the key was **found**, and an empty prefix is written as `(top-level)` so a reader sees a
name rather than a blank column.

### Measured, after the fix

```
(top-level)                   .  call_tool
TurboMCP.TurboMCPEditToolset  .  TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog
(top-level)                   .  call_tool
TurboMCP.TurboMCPReadToolset  .  TurboMCP.TurboMCPReadToolset.ReadTurboMCPBlueprintLoad
```

### The check that would have caught the earlier half-state

The suite previously **tolerated** a line with a real name in one slot and a hash in the other, and
that half-resolved state **looks finished**. It now refuses any line containing a hash placeholder.

### The fifth order-dependent check

The log accumulates and is never rotated, so it carried lines from builds whose resolution was
incomplete, and the new strict check went red on work that was **already fixed**. The suite now
starts from an empty log file — **truncated rather than deleted**, because it is the record of what
happened and the suite merely starts its own.

---

## 5c. The panel checkbox, the summary, and rotation

18 acceptance suites, all green. This closes the three items phase 5 had left.

### The checkbox reads and writes the same settings object the tool does

`03_TOOLS.md` asks for the log to be *"turned on by a checkbox in the panel"* with the choice
persisting in editor settings. There is now **one writer** for that fact: the checkbox calls
`FTurboMCPCallLog::SetEnabled`, which is the same call `E-CALL-LOG` makes, and the check state is
read back from the same object. The panel and the tool **cannot disagree**.

### The summary is read from the log, not from a counter beside it

`T-PANEL-6` wants a line count and a token estimate shown. Those figures are **summed from the
entries themselves**, so a number on screen is a number that was written. It also follows `P1` — it
never shows nothing:

| State | Shown |
|---|---|
| off | `off  \|  N entries held, nothing new is being recorded` |
| on, empty | `on  \|  no calls recorded yet this session` |
| on, with records | `on  \|  N records, M calls, C characters, ~T tokens (estimate)` |

The text is green while recording and muted when not, so the state reads at a glance without parsing
it — the same intent as the server dot above it.

### Rotation, because the file grew without bound

**Measured before this was added: 402 KB and 1229 lines from ONE round of testing**, because every
tool call writes a line **and every read of the log is itself a tool call**. `P6` caps the in-memory
list at 1000 entries; **the file had no cap at all.**

### The rotation is tested by reaching it, not by reading the constant

The suite fills the file past 4 MB, makes one call, and checks the outcome. Measured on the way:

```
filled the log to 4281098 bytes
after:  TurboMCPCalls.jsonl     932 octets
after:  TurboMCPCalls.jsonl.1  4281098 octets
```

and the marker line in the new file:

```
{"time":"...","phase":"rotate","source":"plumbing","toolset":"","tool":"",
 "result":"the log reached 4194304 bytes and was rotated to .../TurboMCPCalls.jsonl.1", ...}
```

**A limit nobody has crossed is a limit nobody knows works.**

### One rotation, not a numbered series

A second file is what makes a crash readable: the current file holds what just happened and the
previous one holds what led up to it. A directory of a hundred rotated files is a directory nobody
reads. The older file is **replaced** rather than kept, so the on-disk cost has a hard ceiling of
about twice the limit.

The marker carries **the same field set as every other line**, so a parser needs no special case for
it — and a reader of the older file can see **why** it ends where it does rather than wondering
whether the log lost data.

### What is NOT verified, recorded rather than claimed

**The checkbox's text and layout have not been seen.** This plugin has no Slate inspection tool, so
there is no way to read the panel's widget tree from here, and no automated check covers it.

What IS proven: it compiles, it is constructed in the panel's Slate tree, and the state and the
toggle go through the same code paths the tools do — the tool side is covered by
`test-call-log.py`. **A human looking at the window is still the only proof of the layout.**

---

## 6a. P/E-COMMENT, and two engine caches that are not what they look like

42 tools. 19 acceptance suites, all green. Phase 6 is the local graph repair phase; `P/E-COMMENT` is
its second capability. `T-LAY-12` is exercised by `python Tools/test-comment-boxes.py`.

### Membership in a box is COMPUTED, not read — and that was the whole problem

`UEdGraphNode_Comment::GetNodesUnderComment` returns a **cached** set that
`SGraphNodeComment::HandleSelection` fills from **rendered widgets**: it walks the graph panel's
children and adds each node whose rendered rectangle is contained in the comment's.

**Measured consequence:** an asset read **without its graph editor open** reports **nothing** inside
every box. A box drawn around two nodes reported:

```
changed: created comment box 000000 in ... : EventGraph
size: 360x122 at (240, -82)
holds 0 node(s)          <-- two nodes were boxed
```

and *"list every box with nothing inside"* — which `03_TOOLS.md` asks for **by name** — would have
called **every box in every asset empty**. The engine exposes **no function that rebuilds the cache.**

So the **rule** is reproduced rather than the cache read, taken from
`SGraphNodeComment::IsNodeUnderComment`, which is `FSlateRect::IsRectangleContained` in graph space.
A box's rectangle starts **`TitleBarHeight` below** its position, because the title bar's own strip
sits above it; using the position directly would claim a box contains the row of nodes drawn under
its title.

### The box key did not identify

The first version left every box created in one session with the key **`000000`**, because `AddNode`
is what makes the node's GUID and the key was read back straight afterwards. **Measured: two boxes
were made and BOTH reported `000000`**, so *"refer to it later as 000000"* was useless and `set`
would have hit whichever box came first.

`CreateNewGuid` is now called before the read-back. **A key that does not identify is worse than no
key, because it looks like one.**

### Delete never touches the nodes, and the claim is CHECKED

It is the one thing this tool says **twice**, because getting it wrong destroys a caller's work. The
result says how many nodes the box held and **re-counts the boxes**; the suite counts the nodes **by
their own keys** before and after:

```
=== DELETE removes the box and NEVER the nodes ===
changed: deleted comment box 'Health setup' from EventGraph
boxes in the graph now: 1 (was 2)
its 2 node(s) are untouched: a box is a label, not a container.
```

The node count in the graph drops by one — **the box itself is a node** — so the suite checks the
**member keys**, not the total, which is the check that actually proves the point.

### The suite's own cleanup was the sixth order-dependent mistake

It tried to recognise its work by the **text** it had set, and a `set` that only **resized** a box
left the text alone. **Five boxes were left behind**, and they then made `test-comments` go red —
**correctly**, because that suite reads the boxes on the same asset and expects none.

**Cleaning up by a marker is a guess about what was made; a key is the thing itself.** Every key is
now remembered and removed by key, and the suite asserts **zero** boxes are left.

### Two of the suite's own assertions were too wide

It demanded a `size:` line on a **text-only** set, and it demanded the empty-box sentence on a graph
where **no box was empty**. Both were fixed rather than worked around: **a check must assert what the
call asked for**, not what a different call would produce.

### Still to come in phase 6

`P/E-GRAPH-FIX-BACKWARD`, `P/E-GRAPH-DEAD`, `P/E-GRAPH-TIDY` with nested boxes, `P/E-GRAPH-KNOTS` and
`P/E-LOCAL-VARS`.
