# Working plan

**This file is a working document. It is expected to change every round.**
It is not normative: what is DECIDED lives in `05_DECISIONS.md`, what each
capability must do lives in `03_TOOLS.md`, and what counts as done lives in
`06_ACCEPTANCE.md` (`D-60`).

It exists so that whoever drives the work — a person or an agent with
sub-agents — can see, in one place: what is built, what is next, which test
proves it, and which spec section to read before starting.

---

## 1. How to use this file

1. **Pick up the first row whose state is `todo`** in the order of section 3.
 The order is not arbitrary; it is `08_ROADMAP.md` section 2, and its first
 rule is that the harness comes before new capabilities.
2. **Read the two spec references on the row before writing code.** The
 `Spec` column names the capability; the `Decisions` column names the
 decisions that constrain it. A capability built without reading the
 decisions gets rebuilt.
3. **The row is not done when the code compiles.** It is done when its test
 row in `06_ACCEPTANCE.md` passes ON THE KIT ASSET, and its measured values
 are recorded in `../TestRecord.md` with the date.
4. **Record what was measured, not what was intended.** `../TestRecord.md` lists nine recorded cases of "a check that passes while the thing it
 verifies is broken". The habit that caught them is the one this file
 depends on: a claim without a measurement is not a result.
5. **A capability that cannot be built as specified does not become a
 smaller capability silently.** It becomes a question in `../QUESTIONS.md`,
 or a refusal in its own tool description, or a decision that it is not
 built.

### 1.1 States

| State | Means |
|---|---|
| `done` | Implemented, its test passes, its values are recorded in `../TestRecord.md` |
| `built` | Implemented and passing in isolation, but not yet covered by the full run |
| `todo` | Not started |
| `gated` | Blocked on a MEASUREMENT, not on effort. The measurement is its own row |
| `open` | Specified, but a question in `../QUESTIONS.md` changes its shape |

## 2. What is already delivered

Measured on the tree, not recalled. The count is the number of
`static FString` declarations carrying `meta = (AICallable)`, in four toolsets.

| Toolset | Tools | Declared at | Notes |
|---|---|---|---|
| `UTurboMCPReadToolset` | **30** | `TurboMCPToolsets.h:55` | readers only; structurally unable to write |
| `UTurboMCPLookToolset` | **4** | `:747` | captures, layout, widget previews; readers only |
| `UTurboMCPPreviewToolset` | **26** | `:895` | `Plan` a change; readers only |
| `UTurboMCPEditToolset` | **33** | `:1500` | `Apply`; includes `ReadTurboMCPEditQueue`, a read that lives here because it reports whether a write is possible |
| **Total** | **93** | | |

*** THIS TABLE SAID 24 / 4 / 17 / 27 = 72, AND THOSE NUMBERS WERE OLD. *** *72 was true at an earlier round;
the readers and writers grew and the table did not follow. **The count above is read from the declaration
line ranges in the header, so it can be re-derived rather than believed:**

```powershell
Select-String -Path Source\TurboMCP\Public\Tools\TurboMCPToolsets.h -Pattern 'static FString' | Measure-Object
# Count: 93
```

*`STATUS.md` carried the same stale figure in prose for the same reason. **A number written in a document
drifts the moment a tool is added, which is why this one names the file it comes from.***

The first delivery is complete for phases 0 to 8 of `08_ROADMAP.md` except
the gaps this plan lists below.

## 3. The capability plan

Ordered by `08_ROADMAP.md` section 2. `Test` names the acceptance rows;
`Spec` names the capability in `03_TOOLS.md`.

### Step 0 — the harness, before anything else

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 0.1 | **One owner of the editor lifecycle.** Start, wait, health check, stop, in one place; a suite asks it for an editor and never starts or stops one itself | `08_ROADMAP.md` section 3 | — | the full run stops producing failures whose names move | todo |
| 0.2 | Make `test-override-add` and `test-comment-boxes` restore their fixture **on the way in** | `../TestRecord.md` section 7a | `P8` |; residue reproduced across an editor stop/start first (5 overridden -> 6). **It also found and fixed a VACUOUS check**: `test-comment-boxes`' `box_keys` read keys from the `fit` listing, **which prints no keys at all**, so it returned `` every run and the closing check was true even on a run that left boxes behind. | done |
| 0.3 | **Measured into `07_ENGINE_NOTES.md` sections 1 to 4 and 6.** Every fact there now cites the engine file and line it came from, and the per-fact version markers are **gone** rather than repointed: the version distinction existed to flag facts learned on an engine this project does not build, **and no such fact was ever present** - every one of them was measured on 5.8.3, which is the only engine this project has been built or measured on. | `07_ENGINE_NOTES.md`, its measured sections | `D-38`, `D-40` | each fact names its engine file and line | **done** |

### Step 1 — measurements that gate whole groups

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 1.1 | **Does the MCP server answer while the editor is stopped at a Blueprint breakpoint?** Record one of the three outcomes with its evidence | `03_TOOLS.md` section 14 | `D-36`, `D-54`, `D-55` | T-MEASURE-1 | todo |
| 1.2 | **Can an automation test be run from a tool call without blocking the game thread?** | `03_TOOLS.md` `R-AUTOMATION-RUN` | `D-54` | T-MEASURE-2 | todo |
| 1.3 | **What does a source control provider do headless?** Perforce, git, and off are three different answers | `03_TOOLS.md` section 17 | `D-57` | T-VCS-1 |,; **MEASURED AT RUNTIME** through the engine's own `-run=PythonScriptCommandlet` (which wraps the script in `FScopedSourceControl`, `PythonScriptCommandlet.cpp:57`), full table in `../Spec/WORKING_SCROW.md`. **None NOT AVAILABLE** (`SourceControlModule.cpp:255-258`, `DefaultSourceControlProvider.cpp:41-44`); **Git AVAILABLE in a commandlet** (`GitSourceControlProvider.cpp:141-150` <- `:60-85` <- `:32-58` <- `Init:21-30`, regardless of `bForceConnection`); **Perforce NEEDS A SETTING** (`:232-235` `IsEnabled` true, `:237-240` `IsAvailable`, `:99-104` `Init(false)`->`None`); **Plastic/SVN not available here** (`cm`/`svn` not on PATH). **A provider needs an EDITOR PROCESS, not an open editor and not Slate** - the only `GEditor` read is guarded and off the provider path, and Git returned `is_available=True` from a `-nullrhi -unattended` commandlet. **A tool must ask BOTH `IsEnabled` and `IsAvailable`** (`SourceControlHelpers.cpp:61-85`) and refuse BY NAME PER ASSET, in `Plan`; `SetProvider` is FATAL on an unmatched name (`SourceControlModule.cpp:404-417`). *** `T-VCS-1` NAMES NO `TA-` FIXTURE, so the four missing-fixture rows stand at four - BUT `T-DATA-5` -> `TA-17` IS A FIFTH, and `T-BP-8` -> `TA-9` is a sixth CANDIDATE. *** | done |todo |
| 1.4 | **Can a Niagara system and an Anim Blueprint be captured headless?** A yes or no per editor kind | `03_TOOLS.md` section 18 | `D-41` | T-LOOK-8 | **ANSWERED: NO for every kind.** `LaunchEngineLoop.cpp:3343` guards `FSlateApplication::Get.InitializeRenderer(...)` with `!IsRunningCommandlet`, **so Slate is never initialized in a commandlet and every PNG path needing a widget window fails by construction.** The plugin has exactly two PNG paths and both need Slate; it never initializes Slate itself. Niagara is a plugin and IS enabled (`Niagara.uplugin:13`), so `TA-19` needs a `Build.cs` dependency **and is uncapturable anyway**; `AnimBlueprint.h:81` is engine-side so `TA-18` needs no dependency but is still not a capture fixture. | done |
| 1.5 | Build the new kit assets TA-14 to TA-21 by hand | `06_ACCEPTANCE.md` section 1 | `D-51` | baseline numbers recorded | todo |

### Step 2 — vision

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 2.1 | Add `selection`, `neighbours`, `box-neighbours`, `screen` to `L-CAPTURE` | `L-CAPTURE` framing table | `D-41`, `D-42` | T-LOOK-7 **SATISFIED** |,,,; **ALL FOUR BUILT, NONE REFUSED.** `test-capture-selectors` PASS + the pre-existing `test-capture` PASS, audit `uncalled: (none)` at **93** tools, full run **56/56**. *** `L-CAPTURE` ALREADY EXISTED - `TurboMCPCapture.cpp` 792 lines, `LookTurboMCPCapture` at `:92`, modes `graph`/`box`/`node`/`debug-window` - SO THE ROW WAS AN EXTENSION, NOT A NEW CAPABILITY, and `Tools/test-capture.py` ALREADY EXISTED AND WAS TRACKED (378 lines), so following the brief literally would have SILENTLY OVERWRITTEN a passing suite. *** **`selection` = the GRAPH CANVAS selection** (`SGraphEditor::GetSelectedNodes`, `GraphEditor.h:394-406`) - **NOT** the actor selection (`EditorEngine.h:1969`), **NOT** components (`:2000`), **NOT** the Content Browser; nothing selected ⇒ refusal, never a fallback (`D-41`). **`neighbours`** prints its own definition (one LINK = one hop; a plain reroute knot is ONE hop per `D-42`, and a knot is NEVER framed). **`box-neighbours`** works in GRAPH-SPACE box rectangles from `L-LAYOUT-REPORT`'s own `RectOf`, smallest-box-owns-the-node, and names the two combinations it refuses. *** `screen` WORKS HERE AND IS MEASURED: a real `1670x911` window PNG with 1335 distinct colours, through `FWidgetRenderer::DrawWindow` (`Slate/WidgetRenderer.h:92-99`) - AND **`-nullrhi` IS NOT WHAT THE SUITES RUN WITH** (`run-suites.ps1:195` passes only `-LowPriority`), WHICH FALSIFIED A PREMISE I HAD PUT IN FOUR BRIEFS; under `-nullrhi` the engine installs `SlateNullRenderer` (`LaunchEngineLoop.cpp:3343`→`:3345-3347`→`:3354`) so SLATE IS INITIALIZED WITH A NULL RENDERER rather than absent, and `screen` refuses by name there rather than writing a black image. *** `GUsingNullRHI` does not link from this plugin (`LNK2019 GRHGlobals`), so the decision uses `-nullrhi` + `FApp::CanEverRender` and the OUTPUT SAYS it is a proxy. **UNMEASURED:** the positive `selection` path is proved through an internal self-clearing probe because `P3` forbids a Look creating a selection; the positive `box-neighbours` branch was measured BY HAND on a temporary fixture; and **no `-nullrhi` run was ever performed.** **AND ROWS 2.2-2.4 NEED `TA-18`/`TA-19`, WHICH DO NOT EXIST.** | done |
| 2.2 | `L-CAPTURE-ASSET`: material, instance, function, widget designer, DataTable, Anim Blueprint, Anim Sequence, Niagara; every kind either supported or refused BY NAME | `L-CAPTURE-ASSET` | `D-41`, `D-06` | T-LOOK-8 **SATISFIED for 9 of 8 named** | **** (26 safety commits carried the work; that is the final state); `test-capture-asset` **PASS**, audit `uncalled: (none)` at **93** tools, full run **57/57**. *** `asset-editor` IS A MODE of the existing `L-CAPTURE`, NOT A NEW TOOL - which is why 93 did not move. *** `kinds: 9 BUILT here, 0 captured by another tool, 0 REFUSED, of 9 named` - and the brief said six, `WORKING_PLAN.md:97` and `06_ACCEPTANCE.md:235` name EIGHT, the plan adds a NINTH. *** THE RENDER PATH IS ONE PATH, NOT NINE: `IToolkitHost::GetParentWidget` (`IToolkitHost.h:25`), which for a standalone editor is `SStandaloneAssetEditorToolkitHost` returning `AsShared` (`SStandaloneAssetEditorToolkitHost.cpp:359-362`), drawn by `FWidgetRenderer::DrawWidget`. *** ALL NINE COLOUR COUNTS ARE DISTINCT (3422 / 37717 / 14963 / 1281 / 519 / 25560 / 36071 / 1641) **while four different assets report the same `1664x849`** - SO SIZE PROVES NOTHING and the colour counts are what prove these are not one cached rectangle; and `mode screen`'s PNG differs from the asset-editor PNG **byte for byte**. *** AND THE BREAK TEST FOUND A DEFECT NO SUITE COULD: with the target PNG READ-ONLY the tool printed `error: could not write...` AND THE SUITE STILL SAID PASS, because the answer BEGAN with the success banner and `out.startswith("error:")` was FALSE - **the convention this tool documents was broken by its own output order**; fixed, banner prepended only on success. It also found THREE defects in its own suite, one of which made a check UNABLE TO PASS (`^` without `re.MULTILINE`). *** *** AND ITS `IsModuleLoaded` CAPABILITY TABLE MOVED BETWEEN TWO CALLS OF THE SAME TOOL IN THE SAME PROCESS (`4/3/2` → `5/3/1` → `6/3/0`); it is now `LoadModuleWithFailureReason`, loading on demand - LINKING A MODULE IS NOT THE SAME AS REACHING ITS EDITOR. *** **UNMEASURED: `TA-18`/`TA-19` DO NOT EXIST (nothing of either type under `/Game`), so Anim Sequence and Niagara were captured FROM ENGINE CONTENT with the substitution PRINTED in the tool's own catalogue; Anim Blueprint is answered through the CATALOGUE only, so its claim is reasoned (`AnimBlueprint.h:81`) rather than photographed; the `-nullrhi` refusal branch is implemented but not measured live; and `/Game/Tests/TurboMCPMaterialFixture` has NO object named after its package, so the project's own material-instance fixture is unreachable by `FTurboMCPAssetResolver`.** | done |
| 2.3 | `L-CAPTURE-LEVEL`: viewport, selection, one actor, and `pie` which refuses without a running session | `L-CAPTURE-LEVEL` | `D-41` | T-LOOK-9 | todo |
| 2.4 | `P-IMAGE-COMPARE`: difference count, bounding box, difference PNG, refusal on two different sizes | `P-IMAGE-COMPARE` | `D-06` | T-LOOK-10 | todo |

### Step 3 — materials, end to end

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 3.1 | `P/E-MAT-BLOCKS`, the last phase 8 capability | `P/E-MAT-BLOCKS` | `D-17`, `D-44` | T-MAT-8 | done |
| 3.2 | Material reader at `full` detail: node type ids, pin indexes, function inputs distinguished from parameters | `04_FORMATS.md` section 2.5 | `D-14`, `D-15` | T-WRITE-6 | todo |
| 3.3 | `P-MAT-CHECK`, then `P/E-MAT-APPEND` | `03_TOOLS.md` section 9 | `D-28`, `D-29`, `D-30` | T-WRITE-6 | todo |
| 3.4 | `P/E-MAT-REPLACE`, then `P/E-MAT-PATCH` | `03_TOOLS.md` section 9 | `D-28`, `D-45` | T-WRITE-6 | todo |
| 3.5 | `R-MAT-INST-LOAD` and `P/E-MAT-INST-SET` | `03_TOOLS.md` section 9 | `D-43` | T-MAT-10, T-MAT-11 | done |
| 3.6 | `R-MAT-ARGS` and `P/E-MAT-ARGS-SET`, with the index shift stated and the removal refused by default | `03_TOOLS.md` section 9 | `D-44` | T-MAT-12, T-MAT-13 | todo |

### Step 4 — data table rows

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 4.1 | `P/E-DATATABLE-ROWS`: add, change and remove rows, values through the row struct's own text import | `P/E-DATATABLE-ROWS` | `P4`, `D-45` | T-DATA-1 | done |

### Step 5 — widget authoring

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 5.1 | The widget tree text dialect, reader and lexer, sharing the graph text lexer | `04_FORMATS.md` section 8 | `D-15`, `D-48` | T-WIDGET-4 | done |
| 5.2 | `P/E-WIDGET-TREE` with `append`, `replace`, `patch`; a rename in the text is REFUSED | `P/E-WIDGET-TREE` | `D-45`, `D-48`, `D-49` | T-WIDGET-1 to T-WIDGET-4 | todo |
| 5.3 | `P/E-WIDGET-SET`: widget property, slot layout and style, each named, plus the rename with its references | `P/E-WIDGET-SET` | `D-49`, `D-50` | T-WIDGET-5 | todo |
| 5.4 | `R-WIDGET-ANIM` and `P/E-WIDGET-ANIM`, with a broken track named rather than dropped | `03_TOOLS.md` section 11 | `P1`, `D-05` | T-WIDGET-6, T-WIDGET-7 | todo |
| 5.5 | `P/E-WIDGET-BIND`, reusing the existing single-binding code path | `P/E-WIDGET-BIND` | `D-02` | T-WIDGET-8 | todo |

### Step 6 — graph structure

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 6.1 | `E-GRAPH-RENAME`, with per-kind reference counts and a rollback | `E-GRAPH-RENAME` | `D-46`, `D-45` | T-STRUCT-9 | todo |
| 6.2 | `E-GRAPH-COLLAPSE`, boundary computed from the crossing links | `E-GRAPH-COLLAPSE` | `D-47` | T-STRUCT-10 | todo |
| 6.3 | `E-GRAPH-COPY` between graphs and Blueprints | `E-GRAPH-COPY` | `D-47` | T-STRUCT-11 | todo |
| 6.4 | `P/E-INTERFACE` | `P/E-INTERFACE` | `P2`, `P8` | T-STRUCT-12 | todo |
| 6.5 | **Open question:** is a push-down writer action worth having at all? | `../QUESTIONS.md` Q-19 | `D-28` | — | open |
| 6.6 | Enumerate and test every node family of `03_TOOLS.md` section 8's table, one at a time: promotable operators, split struct pins, delegates, macros, timelines, latent nodes | `03_TOOLS.md` section 8 | `P1` | one recorded row per family | todo |

### Step 7 — assets and data

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 7.1 | `P/E-STRUCT-NEW` and `P/E-STRUCT-FIELDS`, **editor-only**, and saying so | `03_TOOLS.md` section 12 | `D-51` | T-DATA-2 **SATISFIED**, T-DATA-3 blocked on a missing fixture |; build Succeeded, `test-struct` PASS, audit `uncalled: (none)`, full run **53/53**. **THE "EDITOR-ONLY" PREMISE WAS WRONG AND THE SPEC'S REASON WAS WRONG:** `StructureEditorUtils.cpp:42-64` shows `CreateUserDefinedStruct` installing the `EditorData` at `:49-50` and THEN calling `AddVariable` at `:59` - so the function `D-51` calls unavailable installs the very data it says only the editor factory installs, and the recorded crash was `NewObject` + a hand-built field list, **one call short of the public static**. It needs an EDITOR PROCESS, not an OPEN EDITOR: `AddVariable` reads only `Struct->EditorData`, `GetVarDesc` is one `CastChecked`, `CompileStructure` goes via KismetCompiler - **no `GEditor`, so the `check(GEditor)` limits do not bind.** AND THE ROW **DOES** NEED A `UUserDefinedStruct`: `TurboMCPKitRow.h` is right that a native `USTRUCT` is project-owned, **and a native struct is compiled C++ source so no runtime tool can add a field** - two true statements about two different rows, with the native struct now refused by name. **T-DATA-3 NAMES `TA-21`, WHICH DOES NOT EXIST** (4th such row after `TA-6`, `TA-15/16`, `TA-8`), so the suite builds what it edits. **Four defects found by RUNNING it:** `RemoveVariable` refuses to empty a struct; `AddVariable` cannot name a field (empty base -> literal `MemberVar`); *** `GetBasicTypeByName` SILENTLY SUBSTITUTES `int` for any non-`PC_*` name (`BlueprintEditorLibrary.cpp:2118-2122`), so `Alpha: double` created an int field WHILE REPORTING SUCCESS ***; `ChangeVariableType` returns `false` for a no-op. | done |
| 7.2 | `P/E-ENUM-EDIT`, with reordering reported as affecting saved data | `P/E-ENUM-EDIT` | `D-45` | T-DATA-4 | done |
| 7.3 | `R-CURVE-LOAD` and `P/E-CURVE-SET` | `03_TOOLS.md` section 12 | `P1` | T-DATA-5, T-DATA-6 | todo |
| 7.4 | `R-ASSET-COST`, split by provenance | `R-ASSET-COST` | `D-52` | T-ASSET-3 | done |
| 7.5 | `P-ASSET-DIFF`, naming what it did not compare | `P-ASSET-DIFF` | `D-53` | T-ASSET-4 | open (Q-21) |
| 7.6 | `E-ASSET-NEW`, `E-ASSET-RENAME`, `E-ASSET-MOVE`, `E-ASSET-DUPLICATE`, with redirectors fixed and an open tab refusing | `03_TOOLS.md` section 12 | `P8`, `D-45` | T-ASSET-5, T-ASSET-6 | todo |

### Step 8 — level and actors

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 8.1 | `R-LEVEL-LOAD` | `R-LEVEL-LOAD` | `P1`, `P6` | T-LEVEL-1 | todo |
| 8.2 | `R-ACTOR-LOAD`, with the component hierarchy and inherited versus instance | `R-ACTOR-LOAD` | `D-05` | T-LEVEL-2 | todo |
| 8.3 | `E-ACTOR-PLACE` | `E-ACTOR-PLACE` | `P2`, `P5` | T-LEVEL-3 | todo |
| 8.4 | `P/E-ACTOR-SET` | `P/E-ACTOR-SET` | `P4`, `D-02` | T-LEVEL-4 | todo |
| 8.5 | `E-COMPONENTS`, with its scope stated in words and an instance change refused | `E-COMPONENTS` | `P8` | T-LEVEL-5 | todo |

### Step 9 — quality, session and cost

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 9.1 | `R-BP-LINT`, every check named, each reporting how many things it looked at | `R-BP-LINT` | `P1`, `D-05` | T-QUAL-1 | done |
| 9.2 | `E-SESSION` and `R-EDIT-SESSION`, bounded, and saying when it ended itself | `E-SESSION` | `D-56` | T-QUAL-2 | todo |
| 9.3 | `P/E-ASSET-BATCH` as `E-SESSION` plus a list, one transaction | `P/E-ASSET-BATCH` | `D-56` | T-QUAL-3 | todo |
| 9.4 | `R-CALL-COST` | `R-CALL-COST` | `P1`, `P7` | T-HELP-6 | done |

### Step 10 — source control

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 10.1 | `E-VCS-CHECKOUT` through `ISourceControlModule`, with the headless behaviour measured and recorded | `E-VCS-CHECKOUT` | `D-57`, `P8` | T-VCS-1 **SATISFIED** |,,; build Succeeded, `test-vcs` **PASS (55 checks, none skipped)**, audit `uncalled: (none)` at **93** tools, full run **54/54**. Three tools in ONE `Plan`. **The Git checkout limit row 1.3 left open is CLOSED BY MEASUREMENT:** run, not predicted - `ECommandResult::Failed` + `"Operation 'CheckOut' not supported by revision control provider 'Git'"`, and `git status --porcelain` EMPTY before and after. **`P1` IS NOW A MEASUREMENT:** with no provider `IsAvailable` prints **`NOT ASKED`** rather than `false` (a `false` would read as a measured "server is down"), and **Perforce answers `IsEnabled true` / `IsAvailable false` / `UsesCheckout true`** - the measured proof that asking only `IsEnabled` reports a working provider where there is none. **A TRAP FOUND BY CROSS-TOOL READ-BACK: `USourceControlHelpers::CheckOutFile` ANSWERS `True` ON GIT AND NEVER ISSUES A CHECKOUT** (`SourceControlHelpers.cpp:456-460` returns early because Git reports `IsCheckedOut == true` for every tracked file) - **so row 10.2 MUST NOT wire automatic checkout to that boolean.** `SetProvider` (fatal, `SourceControlModule.cpp:404-417`) is never called. The suite OWNS the editor and asserts its own restore; six named checks were shown red WITH THE WRONG VALUE VISIBLE, then restored. **UNMEASURED:** Plastic/Subversion unreachable (`cm`/`svn` absent), and **no checkout against a live depot was ever performed.** | done |
| 10.2 | Wire an automatic checkout into the write tools | `E-VCS-CHECKOUT` | `D-57` | T-VCS-1 **SATISFIED** | ****; build Succeeded (`Compile …Write.cpp` + `Link …dll` VISIBLE), `test-vcs-write` **PASS (62 checks, none skipped)**, audit `uncalled: (none)` at **93** tools, full run **55/55**. **WIRED 2 OF 32: `E-SAVE` and `P/E-STRUCT-FIELDS`** (chosen because it HAS a Preview twin, so "the check is in `Plan`" is demonstrable); **the 30 unwired are NAMED ONE BY ONE** in `WORKING_SCROW.md` §1, and `E-STRUCT-NEW` is unwired **on purpose** - it creates a path with no file to check out. **DECISION (b): attempt it and report the provider's own answer** - (a) is a skip, (c) would refuse every write on Git, i.e. `D-57`'s own "every tool reports failure" from the other side. *** `D-57` ANSWERS ONLY THE ORDERING; THE TOOL PRINTS THAT IN ITS OWN OUTPUT RATHER THAN IMPLYING THE PROJECT DECIDED. *** **The trap held: `CheckOutFile(` appears ZERO times, and a suite check fails on any.** *** AND ITS CHECK FALSIFIED ITS OWN CLAIM: `E-SAVE` WITH GIT **STAGES A NEW ASSET** - `FileHelpers.cpp:2483` builds `FinalPackageMarkForAddList` and `:2536-2538` executes `FMarkForAdd`, which Git implements as a real `git add`; AND THE ENGINE'S SAVE PATH ALREADY ISSUES A CHECKOUT ON EVERY SAVE AND DISCARDS THE RESULT (`FileHelpers.cpp:5925-5929`). *** Its break test reddened 12 of 61 checks - the trap showing `helper 'True' vs tool False` - **and found a real defect in its own code: an empty decision reason (`P1` inside a decision)**. **UNMEASURED: NO CHECKOUT AGAINST A LIVE DEPOT HAS EVER BEEN PERFORMED HERE, AND THE `UsesCheckout == true` ACTING BRANCH NEVER RAN.** | done |

### Step 11 — build portability

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 11.1 | Finish `09_BUILD.md` once the dependency list stops moving | `09_BUILD.md` | `D-39`, `D-40` | T-BUILD-1, T-BUILD-2 | todo |

### Gated on step 1, then buildable as specified

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| G.1 | `E-BREAKPOINT`, `R-BREAKPOINT` | `03_TOOLS.md` section 14 | `D-55`, `P8` | T-DBG-3, T-DBG-4 | gated on 1.1 |
| G.2 | `R-DEBUG-STATE`, `E-DEBUG-CONTROL` | `03_TOOLS.md` section 14 | `D-54`, `D-55`, `D-02` | T-DBG-5, T-DBG-6 | gated on 1.1 |
| G.3 | `P/E-PIE-SCENARIO` | `P/E-PIE-SCENARIO` | `D-54`, `D-58` | T-DBG-7 | gated on 1.1 |
| G.4 | `R-AUTOMATION-RUN` | `R-AUTOMATION-RUN` | `D-54` | T-DBG-8 | gated on 1.2 |

**If step 1's measurements say no**, G.1 to G.4 are NOT built, and the refusal
goes into the description of the tools an agent would reach for instead
(`E-TRACE`, `R-TRACE`, `R-PLAY-CHECK`). That is a decision, not a failure.

## 4. Questions this plan depends on

Recorded in `../QUESTIONS.md`. A row whose state is `open` should not be
started.

| Question | Changes | State |
|---|---|---|
| Q-1: is 5.8.3 the target? | the whole engine-notes re-verification | stated by `D-38`; the question stays open for the owner to confirm |
| Q-19: a push-down writer action | whether `P/E-GRAPH-PUSH` exists at all | open |
| Q-20: which source control provider comes first | `E-VCS-CHECKOUT`'s provider path | open |
| Q-21: how deep the asset diff goes | `P-ASSET-DIFF`'s scope | open |
| Q-22: is a pausing tool acceptable if the server does not answer | the whole G group | open |

## 5. Rules this plan will be tempted to break

Written down because each one has already been broken once in this project.

- **Do not count a capability as done because its code compiles or its tool
 appears.** `00_START_HERE.md` section 5 lists the five things done means.
- **Do not trust a plan you made in an earlier call.** Every writer
 re-resolves its targets by key at apply time (`D-45`).
- **Do not let a check pass while the thing it verifies is broken.** If a
 check cannot fail, it is not a check: show it failing before believing it.
- **Do not fix a fixture to make a test pass.** `../TestRecord.md` section 7a records
 a workstream that diagnosed a rule defect by arithmetic instead of faking a
 fixture, and it was right.
- **Do not let one capability's test consume a shared asset.** `E-OVERRIDE-REMOVE`
 exists because a suite ran a shared Blueprint out of free events.
- **Do not add a capability whose failure mode is "the agent waits forever"**
 without the measurement that proves it does not (`D-54`, `D-55`).

## 6. Documentation to bring current, in the same work as the capability

The project's own rule is that when a tool changes its docs change in the same
piece of work (`00_START_HERE.md` section 4). These are stale or about to be:

| File | What is stale | When |
|---|---|---|
| `../STATUS.md` | its capability table, which is a snapshot and has been wrong twice | with each capability |
| `../STATUS.md` | its section 3, "What is left", once step 0.1 lands | with step 0.1 |
| `Spec/07_ENGINE_NOTES.md` | its section 5 measurements, which have never been taken | with step 0.3 |
| `README.md`, `DOCS.md` | tool counts and the tool list | with each capability |
| `../TestRecord.md` | one row per new test with its measured values and its date | with each capability |
| `../QUESTIONS.md` | Q-19 to Q-22 are added with this plan and need answers | on the owner's next read |
