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
