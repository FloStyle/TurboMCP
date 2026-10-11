# Row 1.3 measured: what a source control provider does headless

**One row per provider. AVAILABLE / NOT AVAILABLE / NEEDS A SETTING, with the
file and line that decides it.**

Measured on `main` = `origin/main` = `07cf757` (the row was launched at
`7f8b7e4`; the only commits between them are the orchestrator's own
`TestRecord.md` record entries - `git diff --stat 7f8b7e4 HEAD` is
`TestRecord.md | 68 +++++`, source untouched), against the engine install at
`C:\UnrealEngine` (`Engine/Build/Build.version`: `MajorVersion 5`,
`MinorVersion 8`, `PatchVersion 3`, `CompatibleChangelist 55116800`).

*** THIS IS A MEASUREMENT FILE, NOT AN IMPLEMENTATION. NOTHING UNDER
`Source/TurboMCP/` WAS TOUCHED, `TurboMCPToolsets.h` WAS NOT TOUCHED, NO
`Build.cs` AND NO COMMANDLET WAS TOUCHED, AND NO SUITE WAS ADDED. THE
DELIVERABLE IS THE TABLE AND THE LINES. ***

---

## 1. THE TABLE

The runtime columns are **measured, not read**: `Tools`-free, from this
engine's own `PythonScriptCommandlet` (`-run=PythonScriptCommandlet -Script=...`),
which wraps the whole script in `FScopedSourceControl`
(`PythonScriptCommandlet.cpp:57`) - the engine's own "commandlets aren't
guaranteed to have source control" scope. The probe is
`.agent/sc-probe.py`; the raw outputs are `.agent/logs/sc-probe-*.txt` and the
run logs `.agent/logs/sc-run-*.log`.

| Provider | Selected by `-SCCProvider=` | `current_provider()` | `is_enabled()` | `is_available()` | Verdict | The file and line that decides it |
|---|---|---|---|---|---|---|
| **None** (default) | unset, `""`, or `None` | `'None'` | `False` | `False` | **NOT AVAILABLE** - and it is the provider you get when nothing is configured | `SourceControlModule.cpp:255-258` falls back to `&DefaultSourceControlProvider` when the configured name matches nothing; `DefaultSourceControlProvider.cpp:41-44` `IsEnabled()`/`IsAvailable()` both `return false`; `:52-55` `GetState` and `:77-80` `Execute` both `return ECommandResult::Failed` |
| **Git** | `Git` | `'Git'` | `True` | `True` | **AVAILABLE** in a commandlet, once selected | `GitSourceControlProvider.cpp:141-150` both return `bGitRepositoryFound`, set by `CheckRepositoryStatus` (`:60-85`) from `CheckGitAvailability` (`:32-58`) called by `Init` (`:21-30`) **regardless of `bForceConnection`** - so the repository is found even though `SourceControlModule.cpp:263` calls `Init(false)` |
| **Perforce** | `Perforce` | `'Perforce'` | `True` | `False` | **NEEDS A SETTING** - a reachable server, a workspace and a login; and note `IsEnabled()` is `true` even when nothing is available | `PerforceSourceControlProvider.cpp:232-235` `IsEnabled()` `return true`; `:237-240` `IsAvailable()` is `bServerAvailable && !IsLoginError() && bIsWorkspaceValidForProject`; `:99-104` `Init(false)` sets `EInitFlags::None` so **no** connection is attempted, and the connection lives at `:310-330` under `EInitFlags::AttemptConnection` |
| **Plastic SCM** | `Plastic SCM` | `'Plastic SCM'` | `True` | `False` | **NOT AVAILABLE here** - the `cm` client is not installed on this machine, and a connection is still needed | `PlasticSourceControlProvider.cpp:283-286` `IsEnabled()` `return true`; `:289-292` `IsAvailable()` is `bServerAvailable`; `:89-109` `CheckPlasticAvailability()` sets `bPlasticAvailable` from `FindPlasticBinaryPath()` / `PlasticSourceControlShell::Launch()`; `:72-86` the connection needs `bForceConnection` |
| **Subversion** | `Subversion` | `'Subversion'` | `True` | `False` | **NOT AVAILABLE here** - the `svn` client is not installed on this machine | `SubversionSourceControlProvider.cpp:79-82` `IsEnabled()` `return true`; `:84-88` `IsAvailable()` is `!bWorkingOffline`; `:26-29` `Init` -> `ParseCommandLineSettings(bForceConnection)`, and `bWorkingOffline` is set at `:646` / cleared at `:650` by a command result |
| *(FileSandbox proxy)* | would be `Sandbox` | - | - | - | **NOT A PROVIDER HERE** - the plugin is **not** enabled by default, so it never registers | `Engine/Plugins/Developer/Sandbox/FileSandbox/FileSandbox.uplugin:13` `"EnabledByDefault": false`; it would register at `SandboxSourceControlProviderProxy.cpp:101` |

**Verbatim probe output, all six runs** (`.agent/logs/sc-probe-*.txt`):

```
--- none ---            current_provider='None'       is_enabled=False  is_available=False
--- git ---             current_provider='Git'        is_enabled=True   is_available=True
--- perforce ---        current_provider='Perforce'   is_enabled=True   is_available=False
--- plastic ---         current_provider='Plastic SCM' is_enabled=True  is_available=False
--- subversion ---      current_provider='Subversion' is_enabled=True   is_available=False
--- perforce-co ---     current_provider='Perforce'   is_enabled=True   is_available=False
--- plastic-co ---      current_provider='Plastic SCM' is_enabled=True  is_available=False
```

`is_available()` is `IsEnabled() && GetProvider().IsAvailable()`
(`SourceControlHelpers.cpp:324-329`), so the Perforce/Plastic/Subversion rows
say exactly what they look like: **selected and enabled, with no server.**

`-SCCProvider=` is the setting that decides it: `SourceControlSettings.cpp:58-61`
reads the `SCCProvider` switch pair off the command line before anything else,
and `:64-65` falls back to `Provider=` in the ini whose section is
`SourceControl.SourceControlSettings` (`SourceControlSettings.cpp:12`). **Nothing
in this repository sets either one**: no `SourceControlSettings.ini` exists
anywhere under the project, `Engine/Config`, `Engine/Saved/Config` or the user's
`Unreal AppData`, and no file under `Tools/` mentions `SCCProvider`.

### What is registered and enabled, and the count that proves it

Five things register a `"SourceControl"` modular feature in this install, and
every registration is unconditional in its `StartupModule`:

| # | Registrant | Line |
|---|---|---|
| 1 | `FDefaultSourceControlProvider` (always, provider name `None`) | `SourceControlModule.cpp:78` |
| 2 | `FGitSourceControlProvider` | `GitSourceControlModule.cpp:35` |
| 3 | `FPerforceSourceControlProvider` | `PerforceSourceControlModule.cpp:19` |
| 4 | `FPlasticSourceControlProvider` | `PlasticSourceControlModule.cpp:20` |
| 5 | `FSubversionSourceControlProvider` | `SubversionSourceControlModule.cpp:35` |
| - | `FSandboxSourceControlProviderProxy` (plugin disabled by default) | `SandboxSourceControlProviderProxy.cpp:101` |

All four plugins are `"EnabledByDefault": true` and the project's
`TestProject/TurboMCPTest.uproject` enables none of them and disables none of
them: `GitSourceControl.uplugin:13`, `PerforceSourceControl.uplugin:13`,
`PlasticSourceControl.uplugin:13`, `SubversionSourceControl.uplugin:13`.

**AND THAT IS CONFIRMED AT RUNTIME, IN A COMMANDLET, FROM THE PROJECT'S OWN
LOG.** `TestProject/Saved/Logs/TurboMCPTest_2.log` is a
`-run=TurboMCP.TurboMCPPanelProbeCommandlet -unattended -nullrhi` run
(`:463` is the command line):

```
:300  LogPluginManager: Mounting Engine plugin GitSourceControl
:304  LogPluginManager: Mounting Engine plugin PerforceSourceControl
:306  LogPluginManager: Mounting Engine plugin PlasticSourceControl
:313  LogPluginManager: Mounting Engine plugin SubversionSourceControl
:354  LogModuleManager: InternalLoadLibrary: 'PerforceSourceControl' (...)
:358  LogModuleManager: InternalLoadLibrary: 'PlasticSourceControl' (...)
:1440 LogModuleManager: InternalLoadLibrary: 'GitSourceControl' (...)
:1450 LogModuleManager: InternalLoadLibrary: 'SubversionSourceControl' (...)
:356,357,359,1441,1451  SourceControl: Revision control is disabled
```

*** "Revision control is disabled" IS
`FDefaultSourceControlProvider::Init` (`DefaultSourceControlProvider.cpp:14`),
AND IT IS THE PROOF THAT NO SETTING SELECTED ANY OF THE FOUR THAT HAD JUST
LOADED. *** The same five lines appear in the interactive editor log
`TurboMCPTest.log` (`:206,210,213,217` mounts, `:359,363,1434,1444` loads,
`:361,362,364,1435,1445,1851` six "disabled" messages), so **the commandlet and
the full editor agree**: the modules load, the providers register, and the
active provider is `None` until a setting names one.

---

## 2. DOES ANY PROVIDER PATH READ `GEditor`, SLATE, OR AN OPEN ASSET EDITOR?

**NO - not one of the four, on any path a tool would call: registration,
selection, `Init`, `GetState`, `Execute`/`CheckOut`, `UsesCheckout`.**

The evidence is the negative plus the guard lines, and both are specific:

- The four `StartupModule`s register unconditionally, with no editor guard:
  `GitSourceControlModule.cpp:35`, `PerforceSourceControlModule.cpp:19`,
  `PlasticSourceControlModule.cpp:20`, `SubversionSourceControlModule.cpp:35`.
- Where the plugins *do* mention the commandlet, they **suppress UI** rather
  than require an editor:
  `GitSourceControlModule.cpp:49` `if (FApp::IsUnattended() || IsRunningCommandlet()) return;`
  (inside `SaveSettings`),
  `SubversionSourceControlModule.cpp:54` (same, in `SaveSettings`),
  `PlasticSourceControlProvider.cpp:584` `if (!IsRunningCommandlet())` (before
  `FMessageDialog::Open` for version/connection errors),
  `PerforceSourceControlProvider.cpp:763` `if (PersistentConnection && !IsRunningCommandlet())`
  (keeps the connection alive in a long-running commandlet).
- The only `GEditor` read in the four plugins is **guarded and not on the
  provider path**: `PlasticSourceControl/PackageUtils.cpp:19` `if (GEditor)`,
  inside `GetCurrentWorld()` (`:17-27`), reached only from
  `ListPackagesToReload` (`:140`), whose callers are the branch/changeset
  **widgets** and `PlasticSourceControlMenu.cpp`. A null `GEditor` degrades to
  `nullptr`, it does not assert.
- The `FSlateApplication` references are all widget code:
  `SPerforceSourceControlSettings.cpp:233`,
  `SPlasticSourceControlBranchesWidget.cpp:740,1545`,
  `SPlasticSourceControlChangesetsWidget.cpp:1035-1698`
  (the last also dereferences `GEditor` eleven times at `:1037,1650,1653,1656,1663,1668,1674,1692,1698`).
  These are `MakeSettingsWidget()`/dialog paths, not `ISourceControlProvider`.

*** SO THE ANSWER TO ROW 1.3'S CENTRAL QUESTION IS THE **NARROWING** ONE - THE
SAME SHAPE AS SECTION 123's `FStructureEditorUtils` RESULT: A SOURCE CONTROL
PROVIDER NEEDS AN **EDITOR PROCESS**, NOT AN OPEN EDITOR, AND NOT SLATE. ***
`LaunchEngineLoop.cpp:3343`'s `!IsRunningCommandlet()` guard on
`FSlateApplication::Get().InitializeRenderer(...)` therefore **does not bind
here**, and this was measured rather than argued: the Git case returned
`is_available=True` from a `-nullrhi -unattended` commandlet with no Slate at
all (the same commandlet run logs
`LogTurboMCP: Focus guard NOT installed: Slate is not initialised`).

**THE CONTRAST IS THE EDITOR-SIDE SAVE PATH, AND IT IS THE OTHER SHAPE.** The
interactive `FEditorFileUtils` checkout path does need Slate, at lines that are
*not* guardable by a provider:
`FileHelpers.cpp:2150` loads `FPackagesDialogModule` unconditionally, `:2161`
`CreatePackagesDialog`, and `:2218` `ShowPackagesDialog(...)`, which reaches
*** `PackagesDialog.cpp:118  GEditor->EditorAddModalWindow( EditorPackagesDialogWindowRef );` ***
- an unguarded `GEditor->` dereference, the same shape as
`BlueprintEditorModule.cpp:214`. That is the line that decides "not headless"
for `FEditorFileUtils`, and it is a *dialog* dependency, not a *provider* one.

---

## 3. WHAT THE PROJECT'S EXISTING SOURCE CONTROL ROWS ASSUME

There is exactly **one** source control acceptance row and **one** capability:

| Where | What it says |
|---|---|
| `Spec/03_TOOLS.md:1343-1367` section 17 | `E-VCS-CHECKOUT`. Input: assets or nothing to read state. Before an Edit writes, the file must be writable in the project's SCM - "Perforce will refuse a save that is not checked out; git will not, and will happily leave a modified file for someone else to find" (`:1348-1351`). It reports per asset: on/off and which provider, under source control, checked out here, out of date (`:1352-1355`). It checks out **through `ISourceControlModule`'s own provider rather than by shelling out to `p4` or `git`** (`:1356-1358`). "**The engine's own checkout path fails headless**" (`:1359-1361`). No automatic checkout until this tool's own tests pass (`:1365-1366`). Test: `T-VCS-1` (`:1367`). |
| `Spec/06_ACCEPTANCE.md:265` | `T-VCS-1 Checkout`: "With source control off: the answer says so per asset. With it on: the state is reported per asset, a checkout is attempted through the provider, and the provider's own answer is repeated. A headless run of the same call records what the provider did, because that is the case that already failed once". |
| `Spec/05_DECISIONS.md:383` | `D-57`: `E-VCS-CHECKOUT` is built and tested before any write tool calls it. |
| `Spec/07_ENGINE_NOTES.md:292-298` section 5.3 | The open question, as the row inherited it. |
| `Spec/WORKING_PLAN.md:88`, `:173`, `:174`, `:204` | Row 1.3; row 10.1 (`E-VCS-CHECKOUT`, **gated on 1.3**); row 10.2 (wire automatic checkout, gated on 10.1); `Q-20` "which source control provider comes first" - open. |
| `Spec/08_ROADMAP.md:41`, `:50`, `:90` | T-VCS-1's headless half is one of the three measurements that gate whole groups; source control is roadmap row 10; `Q-20` again. |
| `Source/TurboMCP/TurboMCPCompile.cpp:373-383` | Why `E-SAVE` does not use `FEditorFileUtils`. |

**WHAT THOSE ROWS ASSUME, MEASURED AGAINST THIS ENGINE:**

1. "*through `ISourceControlModule`'s own provider rather than by shelling out*"
   - CORRECT and supported: `CommandletSourceControlUtils.cpp:129,237,395` is the
   engine's **own commandlet-side** source control, and it does exactly that
   (`GetState`, `Execute(FCheckOut)`, `Execute(FRevert)`, `Execute(FDelete)`)
   with **no `GEditor`, no Slate, no asset editor anywhere in its 458 lines.**
   So the engine already has a headless checkout path; row 10.1 can be built on
   it.
2. "*Perforce will refuse a save that is not checked out; git will not*" -
   CORRECT, and now with line numbers: `PerforceSourceControlProvider.cpp:622-625`
   `UsesCheckout()` `return true`; `GitSourceControlProvider.cpp:286-289`
   `UsesCheckout()` `return false`, and `GitSourceControlModule.cpp:21` says so
   in words ("this provider does not uses the `CheckOut` command, which is a
   Perforce `lock`, as Git has no lock command"). Subversion `:239-242` and
   Plastic `:498-501` (a project setting, default on) also `return true`.
3. "*the state is reported per asset*" - CORRECT but conditional: with `None`,
   `GetState` cannot report a state at all (`DefaultSourceControlProvider.cpp:52-60`
   `return ECommandResult::Failed`), so the row's "with source control off the
   answer says so per asset" **must be a refusal per asset, not a state**.
4. "**The engine's own checkout path fails headless**, and the consequence is
   that `FEditorFileUtils` tried to check out during a headless save, failed,
   and made `E-SAVE` report a failure while nothing had been written" - **THE
   NARROWING APPLIES HERE AND THE RECORDED CAUSE IS NOT THE ONE THE CODE
   SHOWS.** With no provider selected, `ISourceControlModule::Get().IsEnabled()`
   is `False` (`DefaultSourceControlProvider.cpp:41-44`), and the editor's save
   path **skips checkout entirely** on that value:
   `FileHelpers.cpp:5925` `if (ISourceControlModule::Get().IsEnabled())` guards
   the only `CheckoutPackages` call in `InternalCheckoutAndSavePackages`
   (`:5912-5951`). `FEditorFileUtils::CheckoutPackages` (`:2380`, `:2610`) is
   never reached. What *is* reached, when a package needs checkout, is the
   **dialog**: `FileHelpers.cpp:2150/2161/2218` -> `PackagesDialog.cpp:118`
   `GEditor->EditorAddModalWindow(...)`. The branch that needs checkout with no
   provider is `FileHelpers.cpp:2061`
   `else if (bPkgReadOnly && bFoundFile && (IsCheckOutSelectedDisabled() || !bCareAboutReadOnly))`,
   and `IsCheckOutSelectedDisabled()` (`:1908-1910`) is `true` with no provider.
   *So the headless failure is a **read-only file plus a Slate dialog**, not a
   provider refusing.* **Not settled by re-running it**: I did not re-run the
   `FEditorFileUtils` save headless, and whether the phase-1 asset file was
   read-only at the time is not recoverable. What I can report is that every
   kit asset on disk today is writable (`IsReadOnly False` for all fifteen
   files under `TestProject/Content/Tests`), so today that branch is not taken
   for them.
5. Row 10.1 is **"gated on 1.3"** and `Q-20` asks which provider comes first.
   **THE MEASUREMENT ANSWERS BOTH: Git is the only provider that is AVAILABLE
   on this machine with nothing but the `Provider` setting, so Git is the one
   that can be built and tested first; Perforce is the one whose refusal
   (`UsesCheckout() == true`) is the reason the capability exists.**

### Do the fixtures T-VCS-1 names exist?

**`T-VCS-1` NAMES NO `TA-` FIXTURE AT ALL.** Its text (`06_ACCEPTANCE.md:265`)
says "per asset" and "with source control off / on" - it needs *an asset* and
*a setting*, not a kit asset. **So `T-VCS-1` IS NOT, AND CANNOT BE, A FIFTH
MISSING-FIXTURE ROW.** The four recorded ones stand at four:
`T-MAT-10`->`TA-6`, `T-LEVEL-1/2`->`TA-15`/`TA-16`, `T-WIDGET-4`->`TA-8`,
`T-DATA-3`->`TA-21` (`06_ACCEPTANCE.md:38-45`, `TestRecord.md:8833-8840`).

### AND THE FIXTURE CHECK FOUND A CANDIDATE FIFTH ANYWAY, OUTSIDE SOURCE CONTROL

Because the brief asks for it, I checked every `TA-` fixture named by an
acceptance row against what exists. Assets under `TestProject/Content`:
`Tests/BP_NodeTest`, `Tests/BP_TurboMCPTestLog`, `Tests/TA11_TestEnum`,
`Tests/TA18_ColourSuite`, `Tests/TurboMCPAcceptance`, `Tests/TurboMCPMaterialFixture`,
`Tests/TurboMCPTestMap`, `Tests/WBP_NodeTest`, `Tests/Kit/BP_TestAlpha`,
`Tests/Kit/BP_TowerArray`, `Tests/Kit/TA01_HandLaid`, `Tests/Kit/TA07_Card`,
`Tests/Kit/TA11_KitEnum`, `Tests/Kit/TA14_KitDataTable`,
`Tests/Kit/TA20_KitParameters`, `Tests/Kit/TA13_KitPath/TA13_KitPath` - sixteen.

- *** `TA-17` ("Curve float and a curve table") IS NAMED BY `T-DATA-5`
  (`06_ACCEPTANCE.md:213`) AND NOTHING CARRIES IT. *** No asset with `TA17` or
  `TA-17` in its path exists, and **no file under `Tools/` mentions `TA-17`,
  `TA17` or a curve at all** (`Tools/*.py` has zero matches for
  `CurveFloat|CurveTable|curve`, case-insensitive). The implementation row that
  would need it is still `todo`: `Spec/WORKING_PLAN.md:145` row 7.3
  (`R-CURVE-LOAD` and `P/E-CURVE-SET`, "Test: `T-DATA-5`, `T-DATA-6`").
  `T-DATA-6` ("Replace the keys of one curve table row") names the same
  fixture by description. **This is the fifth row naming a missing fixture:
  `T-DATA-5` -> `TA-17`** (with `T-DATA-6` behind it).
- `TA-9` ("Widget Blueprint `card alt`") is named by `T-BP-8`
  (`06_ACCEPTANCE.md:95`, "After replacing `ReturnButton` with TA-9 under the
  same name") and nothing carries it either - no `TA09`/`card alt` asset and no
  reference to it anywhere under `Tools/` or `Source/`. **A sixth candidate**,
  reported as a candidate because the row's wording ("replacing ... under the
  same name") may be intended as an in-session class swap rather than a stored
  asset.
- `TA-5` is a **label collision, not a missing asset**: the kit table
  (`06_ACCEPTANCE.md:20`) calls `TA-5` a 60-expression Material, while the kit
  the project actually builds uses that label for the `BackwardProbe` graph -
  `Tools/make-kit-assets.ps1:139` "`TA-5` is the `BackwardProbe` graph", and
  `Tools/test-graph-fix-backward.py:416` "`TA-5` IS THE `BackwardProbe` GRAPH".
  The material fixture exists under its own name (`TurboMCPMaterialFixture`,
  built by `TurboMCPMaterialFixtureCommandlet`, `Tools/make-material-asset.ps1:6-7`),
  and `T-MAT-6`/`T-WRITE-6` are exercised against it.
- The same drift makes the recorded `T-MAT-10 -> TA-6` note **narrower than the
  record says**: `TA-6` is "Material instance of TA-5", and a real material
  instance **does** exist - `MI_TurboMCPFixture`, inside
  `TurboMCPMaterialFixture.uasset`, `TurboMCPMaterialFixtureCommandlet.cpp:609`
  per `Tools/test-mat-inst-load.py:14-15`. The fixture is missing **under the
  name the table gives it**, not absent from the project.

*This is offered as a measurement for the owner to fold in, not as a spec edit:
`Spec/06_ACCEPTANCE.md` is outside this workstream's file boundaries and was
not touched.*

---

## 4. WHAT A TOOL MUST DO WHEN NO PROVIDER IS AVAILABLE

**THE PROJECT'S REQUIREMENT IS REFUSE BY NAME, AND THE ENGINE ALREADY HAS THAT
PATTERN - AND IT IS MEASURED, NOT JUST READ.**

The engine's own gate is `SourceControlHelpers.cpp:61-85`
`VerifySourceControl(bool bSilent)`:

```
:65   if (!SCModule.IsEnabled())          -> :67 "Revision control is not enabled."
:74   if (!Provider->IsAvailable())       -> :76 "Revision control server is currently not available."
:84   return Provider;   (nullptr on either refusal)
```

and every entry point starts with it, e.g. `CheckOutFile`
`SourceControlHelpers.cpp:430-477`: `:441` `VerifySourceControl`,
`:443-446` `if (!Provider) return false;`. **Both refusals were reproduced
verbatim in this tree**, through the Python surface `unreal.SourceControl`
(the class is `UCLASS(..., meta = (ScriptName = "SourceControl"))`,
`SourceControlHelpers.h:244`), calling `check_out_file(..., True)` and reading
`last_error_msg()`:

```
provider None      -> check_out_file = False ; last_error_msg = Text("Revision control is not enabled.")
provider Perforce  -> check_out_file = False ; last_error_msg = Text("Revision control server is currently not available.")
provider Plastic   -> check_out_file = False ; last_error_msg = Text("Revision control server is currently not available.")
```

**So the required shape is achievable with the engine's own API, and the
measured answer to "what must a tool do" is a four-part rule:**

1. **Ask both questions, in this order.** `IsEnabled()` alone is not enough,
   and this is measured: Perforce, Plastic and Subversion all
   `return true` from `IsEnabled()` unconditionally
   (`PerforceSourceControlProvider.cpp:232-235`,
   `PlasticSourceControlProvider.cpp:283-286`,
   `SubversionSourceControlProvider.cpp:79-82`) while `IsAvailable()` is
   `False`. `ISourceControlModule.h:193` says as much in its own words:
   "true if a source control provider is set (regardless of whether the
   provider is available)".
2. **Refuse by name, per asset** - the two literal strings above, which is what
   `T-VCS-1`'s "the answer says so per asset" means once the provider is `None`.
   Not an empty success, not a zero, not a crash.
3. **Put the refusal in `Plan`**, so the Preview and its Edit twin refuse the
   same input - the project's existing shape, demonstrated verbatim by
   `P/E-STRUCT-FIELDS`.
4. **Never reach a provider call to find out.** With `None`, `GetState` and
   `Execute` do not fail loudly - they `return ECommandResult::Failed`
   (`DefaultSourceControlProvider.cpp:52-55`, `:77-80`) and, for
   `GetState(UPackage*)`, hand back a **null state**, which a reader can mistake
   for "not under source control". `CommandletSourceControlUtils.cpp:184-194`
   and `:350-359` show what that costs when a consumer does not check first: it
   silently falls through to deleting files from disk.

**And the project's own pattern for an absent measurement is `P1`, measured in
the reader it names**: `Source/TurboMCP/TurboMCPAssetCost.cpp:22-23` ("A SIZE
THAT CANNOT BE READ IS REPORTED AS UNKNOWN WITH THE REASON, NEVER AS ZERO"),
`:101-105` (`"OWN SIZE: unknown - %s"`), `:129-135` (unknown sizes get their own
section rather than being added as zero), `:223`. A source control tool's
"No provider" line is the same contract: **unknown with a reason** beats an
empty state that reads as "not under source control".

### Two hazards a tool must guard, both read out of the code

- **`SetProvider(FName)` is not safe to call with a name you have not
  validated**: `SourceControlModule.cpp:404-417` runs
  `UE_LOGF(LogSourceControl, Fatal, "Tried to set unknown revision control
  provider: ...")` when no registered provider matches. A tool that wants to
  select a provider must ask `GetProviderNames` first
  (`ISourceControlModule.h:160`, implemented at `SourceControlModule.cpp:266-276`),
  or use `CreateProvider` (`:384-397`), which returns a **null**
  `TUniquePtr` for an unknown name instead of terminating the process.
- **`GetProvider()` dereferences `CurrentSourceControlProvider`**
  (`SourceControlModule.cpp:379-382`) and the only thing that ever assigns it is
  `InitializeSourceControlProviders` (`:237-264`), reached solely from
  `HandleModularFeatureRegistered` (`:488-494`) - so a tool that runs before any
  provider has registered would dereference null through
  `ISourceControlModule::Get()`. **In this tree that cannot be observed**,
  because `StartupModule` adds that callback at `:74` and then registers the
  default provider at `:78`, so the first callback sets it. **It is reported as
  a code fact, not as a measured failure** - no runtime case produced it. A tool
  can guard it with `GetProviderNames(...).Num() > 0`.

---

## 5. WHAT I COULD NOT SETTLE

1. **The `FEditorFileUtils` headless failure was not re-run.** I can show which
   lines a no-provider save skips (`FileHelpers.cpp:5925`) and which line the
   dialog path reaches (`PackagesDialog.cpp:118`), and that today's kit assets
   are all writable - but the phase-1 asset's read-only state at the time is
   gone, so the recorded cause ("cannot complete with no source control
   provider", `TestRecord.md:139-143`) is **narrowed, not falsified.**
2. **No provider was ever given a reachable server**, so "what does Perforce
   *do* when it is available headless" is still unmeasured. `p4 set` on this
   machine (`P4PORT=ssl:example.invalid:1666`, `P4USER=exampleuser`,
   `P4CHARSET=winansi`, `P4EDITOR`, `P4IGNORE`) points at a host that does not
   answer, and the run logged the engine's own
   `SourceControl: Error: Connect to server failed; check $P4PORT.` /
   `OwningSystem=Default, Port=ssl:example.invalid:1666` (and that commandlet run
   exited `1`). A real checkout against a live depot remains untested - which is
   consistent with `03_TOOLS.md:1365-1366` ("no automatic checkout happens
   anywhere until this tool's own tests pass").
3. **`Plastic SCM` and `Subversion` cannot be exercised on this machine at
   all** - `cm` and `svn` are not on `PATH` (measured with `Get-Command`), so
   their AVAILABLE branch is unreachable here, not merely untested.
4. **The `-SCCProvider=Plastic SCM` name contains a space**
   (`PlasticSourceControlProvider.cpp:34`), so the switch needs quoting in any
   harness; that was done here and reported, but it is a trap for row 10.1.
5. **`TA-9` as a fixture** is reported as a candidate only: `T-BP-8`'s wording
   could mean an in-session class swap rather than a stored asset.
6. **`check_out_file` was deliberately not called with Git** (where it would
   pass the gate and run `GetState`/possibly `Execute`), so the *shape of a Git
   checkout answer* - the half of `T-VCS-1`'s "with it on" that lives on the one
   provider this machine does have - is measured only as
   `is_available=True`, not as a checkout result. `GitSourceControlProvider.cpp:286-289`
   (`UsesCheckout() == false`) and `:257-260`
   (`CanExecuteOperation` = "a worker is registered", and no `CheckOut` worker
   is registered - `GitSourceControlModule.cpp:20-29`) predict it is a no-op,
   but prediction is not this file's standard.

---

## APPENDIX: the probe and its verbatim output, so this file stands alone

`.agent/` is `.gitignore`d, so the script and its raw outputs are NOT in the
repository. Both are reproduced here, and the two headings below are the exact
files that were written, character for character.

### `python -run=PythonScriptCommandlet -Script=... ` executed this

```python
"""Measure the ACTIVE source control provider from inside this engine install.

Run by the engine's own PythonScriptCommandlet, which wraps the whole script in
`FScopedSourceControl` (PythonScriptCommandlet.cpp:57) - the engine's own
"commandlets are not guaranteed to have source control" scope.  Every line
written here is a runtime reading, not a header read.

The CHECKOUT probe is opt-in through the environment variable
`TMCP_SC_PROBE_CHECKOUT=1`, so the provider-selected runs cannot touch a real
depot: with no provider at all the call is refused before it reaches a provider
(that refusal IS the measurement), and with a provider selected the call is not
made at all unless the variable is set.
"""

import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
TAG = os.environ.get("TMCP_SC_PROBE_TAG", "result")
RESULT = os.path.join(HERE, "logs", "sc-probe-%s.txt" % TAG)
LINES = []


def emit(text):
    LINES.append(text)
    print(text)
    try:
        unreal.log(text)
    except Exception:  # noqa: BLE001
        pass


def flush():
    with open(RESULT, "w") as handle:
        handle.write("\n".join(LINES) + "\n")


def show(label, fn):
    try:
        emit("PROBE %s = %r" % (label, fn()))
    except Exception as exc:  # noqa: BLE001
        emit("PROBE %s RAISED: %r" % (label, exc))


SC = getattr(unreal, "SourceControl", None)
emit("PROBE tag = %r" % TAG)
emit("PROBE unreal.SourceControl present = %r" % (SC is not None))

if SC is not None:
    show("current_provider", SC.current_provider)
    show("is_enabled", SC.is_enabled)
    show("is_available", SC.is_available)
    show("last_error_msg_before", SC.last_error_msg)

    if os.environ.get("TMCP_SC_PROBE_CHECKOUT") == "1":
        ASSET = ("<REPO>/TestProject/Content/Tests/Kit/"
                 "TA14_KitDataTable.uasset")
        show("check_out_file_returned", lambda: SC.check_out_file(ASSET, True))
        show("last_error_msg_after", SC.last_error_msg)
    else:
        emit("PROBE checkout probe SKIPPED (TMCP_SC_PROBE_CHECKOUT != 1)")
else:
    emit("PROBE FAILED: unreal.SourceControl is not exposed to Python")

emit("PROBE done")
flush()
```

Run as (once per provider, with `TMCP_SC_PROBE_TAG` naming the result file):

```
UnrealEditor-Cmd.exe <project> -run=PythonScriptCommandlet ^
    -Script="<REPO>/.agent/sc-probe.py" ^
    -unattended -nullrhi -culture=en [-SCCProvider=<name>]
```

### the raw result files

```
--- sc-probe-none.txt ---
PROBE tag = 'none'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'None'
PROBE is_enabled = False
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE check_out_file_returned = False
PROBE last_error_msg_after = Text("Revision control is not enabled.")
PROBE done

--- sc-probe-git.txt ---
PROBE tag = 'git'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Git'
PROBE is_enabled = True
PROBE is_available = True
PROBE last_error_msg_before = Text("")
PROBE checkout probe SKIPPED (TMCP_SC_PROBE_CHECKOUT != 1)
PROBE done

--- sc-probe-perforce.txt ---
PROBE tag = 'perforce'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Perforce'
PROBE is_enabled = True
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE checkout probe SKIPPED (TMCP_SC_PROBE_CHECKOUT != 1)
PROBE done

--- sc-probe-plastic.txt ---
PROBE tag = 'plastic'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Plastic SCM'
PROBE is_enabled = True
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE checkout probe SKIPPED (TMCP_SC_PROBE_CHECKOUT != 1)
PROBE done

--- sc-probe-subversion.txt ---
PROBE tag = 'subversion'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Subversion'
PROBE is_enabled = True
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE checkout probe SKIPPED (TMCP_SC_PROBE_CHECKOUT != 1)
PROBE done

--- sc-probe-perforce-co.txt ---
PROBE tag = 'perforce-co'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Perforce'
PROBE is_enabled = True
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE check_out_file_returned = False
PROBE last_error_msg_after = Text("Revision control server is currently not available.")
PROBE done

--- sc-probe-plastic-co.txt ---
PROBE tag = 'plastic-co'
PROBE unreal.SourceControl present = True
PROBE current_provider = 'Plastic SCM'
PROBE is_enabled = True
PROBE is_available = False
PROBE last_error_msg_before = Text("")
PROBE check_out_file_returned = False
PROBE last_error_msg_after = Text("Revision control server is currently not available.")
PROBE done
```

### the per-provider client binaries, measured on this machine

```
git FOUND: C:\Program Files\Git\cmd\git.exe
p4  FOUND: C:\Program Files\Perforce\p4.exe
cm  NOT FOUND on PATH        (Unity Version Control / Plastic)
svn NOT FOUND on PATH
```

`p4 set`, verbatim:

```
P4CHARSET=winansi (set)
P4EDITOR=C:\Windows\system32\Notepad.exe (set)
P4IGNORE=.p4ignore (set)
P4PORT=ssl:example.invalid:1666 (set)
P4USER=exampleuser (set)
```

**A `-SCCProvider=Perforce` run exits 1**, and the reason is in its own log
(`.agent/logs/sc-run-perforce.log`, `:164-176` and the project log):

```
SourceControl: Error: P4ERROR: Failed to connect to revision control server.
SourceControl: Error: Connect to server failed; check $P4PORT.
SourceControl: Error: OwningSystem=Default, Port=ssl:example.invalid:1666
```

That is a *reported* failure with the probe output intact - the measurement
still returned `current_provider='Perforce'`, `is_available=False`.

---

## 6. PREMISES I WAS GIVEN THAT THE TREE CONTRADICTS

1. **"`Tools/list_engine_plugins.py` and `Tools/list_plugin_sources.py` already
   exist in `.agent/` and may do the enumeration for you."** They cannot, and
   that is checkable in one read. `.agent/list_engine_plugins.py:36-41` lists
   exactly four GitHub API paths, all under
   `Engine/Plugins/Experimental/ToolsetRegistry` and
   `Engine/Plugins/Experimental/ModelContextProtocol`, and
   `.agent/list_plugin_sources.py:42-45` walks those same two prefixes. **Neither
   one touches `Engine/Plugins/Developer/` or `Engine/Source/Developer/`, so
   neither can see a source control provider at all** - and both need a
   `GH_TOKEN` and the GitHub `release` ref rather than the local install. The
   enumeration in section 1 was therefore done directly against
   `C:\UnrealEngine`, which is where the providers live.
2. **"BASELINE: `main` = `origin/main` = `7f8b7e4`"** - true when I started, and
   **HEAD moved while I worked, exactly as the brief predicted.** It is now
   `07cf757`, and `git diff --stat 7f8b7e4 HEAD` is
   `TestRecord.md | 68 ++++++++++` - three orchestrator commits
   (`62da539`, `ce907a3`, `07cf757`) that add record entries and touch **no
   source file**. Nothing was reverted, nothing collided, and every measurement
   above is against source that never changed.
3. **The brief's item 1 says the providers "live under
   `Engine/Plugins/Developer/` and under `Engine/Source/Developer/`.** The first
   half is right; the second half holds only the *framework*:
   `Engine/Source/Developer/SourceControl` holds `ISourceControlModule`, the
   default provider and the helpers, and `Engine/Source/Developer/UncontrolledChangelists`
   and `Engine/Source/Developer/SourceControlViewport` are UI/cache consumers,
   **not** providers. The four real providers are all plugins.
