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
   machine (`P4PORT=ssl:<p4-host>:1666`, `P4USER=<user>`,
   `P4CHARSET=winansi`, `P4EDITOR`, `P4IGNORE`) points at a host that does not
   answer, and the run logged the engine's own
   `SourceControl: Error: Connect to server failed; check $P4PORT.` /
   `OwningSystem=Default, Port=ssl:<p4-host>:1666` (and that commandlet run
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
P4PORT=ssl:<p4-host>:1666 (set)
P4USER=<user> (set)
```

**A `-SCCProvider=Perforce` run exits 1**, and the reason is in its own log
(`.agent/logs/sc-run-perforce.log`, `:164-176` and the project log):

```
SourceControl: Error: P4ERROR: Failed to connect to revision control server.
SourceControl: Error: Connect to server failed; check $P4PORT.
SourceControl: Error: OwningSystem=Default, Port=ssl:<p4-host>:1666
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

---

# ROW 10.1 BUILT AND MEASURED: `E-VCS-CHECKOUT`

**Row 1.3 measured which provider to build and test first. This section is the
capability that measurement authorised, and it re-measures the one thing 1.3
recorded as unsettled.**

*** SECTION 1 OF THIS FILE ANSWERS `Q-20` ("which source control provider comes
first") WITH *Git*, BECAUSE GIT IS THE ONLY PROVIDER AVAILABLE ON THIS MACHINE
WITH NOTHING BUT THE `Provider` SETTING. GIT WAS BUILT AND TESTED FIRST. PERFORCE
IS THE SECOND, AND IT TURNED OUT TO BE THE ONE THAT PROVES THE MOST. ***

## 1. WHAT WAS BUILT, AND WHERE

| File | What it is |
|---|---|
| `Source/TurboMCP/TurboMCPSourceControl.h` | `FTurboMCPSourceControlAsset`, `FTurboMCPSourceControlPlan` and `FTurboMCPSourceControl` (`Plan`, `Render`, `Checkout`). **ALL THREE TOOLS READ ONE `Plan`, SO THE PREVIEW AND ITS EDIT TWIN CANNOT REFUSE DIFFERENT INPUT.** |
| `Source/TurboMCP/TurboMCPSourceControl.cpp` | The whole decision. `Plan` asks the module facts once and then builds one row per asset; `Checkout` is the only function that can reach a provider with a write-intent operation. |
| `Source/TurboMCP/TurboMCPSourceControlTools.cpp` | The three tool bodies, each three lines long, so no tool decides anything. |
| `Source/TurboMCP/Public/Tools/TurboMCPToolsets.h` | Three declarations added, nothing else touched: `ReadTurboMCPVcsState` in the Read toolset, `PreviewTurboMCPVcsCheckout` in Preview, `EditTurboMCPVcsCheckout` in Edit. |
| `Tools/test-vcs.py` | 55 checks, four phases, and it OWNS THE EDITOR to reach every branch. |

**THE MODULE DEPENDENCY NEEDED NO CHANGE AND THAT IS CHECKED RATHER THAN
ASSUMED.** `TurboMCP.Build.cs` was not touched: `UnrealEd` **publicly** depends
on `SourceControl` (`Engine/Source/Editor/UnrealEd/UnrealEd.Build.cs:68`, inside
the `PublicDependencyModuleNames` block that begins at `:52`), so both the
include path and `SOURCE_CONTROL_WITH_SLATE=1` - a **`PublicDefinitions`** entry
at `Engine/Source/Developer/SourceControl/SourceControl.Build.cs:22` - reach this
module transitively, and `ISourceControlProvider.h:25` `#error`s without that
define. The build linked without a single Build.cs edit.

## 2. HOW BOTH QUESTIONS ARE ASKED, AND THE PROVIDER THAT PROVES BOTH ARE NEEDED

`FTurboMCPSourceControl::Plan` follows `SourceControlHelpers.cpp:61-85` in order:

```
ISourceControlModule* Module = FModuleManager::LoadModulePtr<ISourceControlModule>(TEXT("SourceControl"));
...
Plan.bEnabled = Module->IsEnabled();                       // SourceControlModule.cpp:376
Plan.ProviderName = Module->GetProvider().GetName().ToString();
if (!Plan.bEnabled) { RunRefusal = "Revision control is not enabled."; }        // SourceControlHelpers.cpp:67
else {
    Plan.bAvailable = Module->GetProvider().IsAvailable(); // asked ONLY when bEnabled - and SAID to have been
    ...
    if (!Plan.bAvailable) { RunRefusal = "Revision control server is currently not available."; }  // :76
}
```

**AND THE PERFORCE CASE IS THE MEASURED PROOF THAT BOTH ARE REQUIRED**, produced
by relaunching the editor with `-SCCProvider=Perforce`:

```
provider in use      : 'Perforce'
  IsEnabled()        : true
  IsAvailable()      : false
  UsesCheckout()     : true
```

`IsEnabled()` is **true** and `IsAvailable()` is **false**, on the provider whose
`UsesCheckout() == true` (`PerforceSourceControlProvider.cpp:622-625`) is the
reason the capability exists. A tool that asked only the first would report a
working provider here. **THIS IS THE PROVIDER THAT CATCHES IT, AND IT IS
REACHABLE ON THIS MACHINE** - only round-tripping to a depot is not.

## 3. THE TOOL'S OUTPUT, VERBATIM

### 3.1 No provider: the refusal, named per asset

```
--- phase A: state per asset, with no provider ---
=== source control: E-VCS-CHECKOUT ===
WHAT THIS ANSWER IS: the state source control holds for each asset, or, when there is
no usable provider, a REFUSAL NAMED PER ASSET. The refusal strings are the ENGINE'S OWN.

module loaded        : true
provider in use      : 'None'
registered providers : 5 -> Subversion, Git, Plastic SCM, Perforce, None
  IsEnabled()        : false
  IsAvailable()      : NOT ASKED, because IsEnabled() was already false.
  UsesCheckout()     : NOT READ, because no provider was selected.

REFUSAL, THE SAME FOR EVERY ASSET BELOW: Revision control is not enabled.
  the string is the engine's own: SourceControlHelpers.cpp:67 (the engine's own gate)
  NO PROVIDER WAS REACHED, so no state is reported and no zero is printed (P1).

assets: 2

  [1] TA01_HandLaid
      package: /Game/Tests/Kit/TA01_HandLaid
      file: <REPO>/TestProject/Content/Tests/Kit/TA01_HandLaid.uasset
      file found by: FPackageName::DoesPackageExist (PackageName.h:450)
      REFUSED: Revision control is not enabled.
      NOTHING WAS ASKED OF ANY PROVIDER FOR THIS ASSET.

  [2] TA14_KitDataTable
      package: /Game/Tests/Kit/TA14_KitDataTable
      file: <REPO>/TestProject/Content/Tests/Kit/TA14_KitDataTable.uasset
      file found by: FPackageName::DoesPackageExist (PackageName.h:450)
      REFUSED: Revision control is not enabled.
      NOTHING WAS ASKED OF ANY PROVIDER FOR THIS ASSET.
```

*** `IsAvailable()` IS `NOT ASKED` RATHER THAN `false`, AND THAT IS `P1` IN ONE
LINE. *** `false` there would read as a measured "the server is down"; the
measurement did not happen, so the answer says so. `UsesCheckout()` is reported
the same way and for the same reason.

### 3.2 Git: the state per asset, and the checkout answer

```
provider in use      : 'Git'
  IsEnabled()        : true
  IsAvailable()      : true
  UsesCheckout()     : false
  UsesLocalReadOnly(): false
  ^ A CHECKOUT ON THIS PROVIDER IS A NO-OP. It has no lock command, so every
    tracked file in the working copy is already checked out. Say so rather than
    implying a lock was taken.

assets: 2

  [1] TA01_HandLaid
      under source control:  true
      checked out here:      true
      added (open for add):  false
      current (up to date):  true
      out of date:           false
      modified:              false
      ignored:               false
      CanCheckout():         false
      the provider's own state text: Unchanged

  [2] TA14_KitDataTable
      under source control:  false
      checked out here:      false
      added (open for add):  false
      current (up to date):  true
      out of date:           false
      modified:              false
      ignored:               false
      CanCheckout():         false
      the provider's own state text: Not Under Revision Control
```

**THE TWO ROWS ARE ANSWERED INDEPENDENTLY** - one tracked asset and one untracked
one - which is the per-asset claim made observable. The suite asserts only that
the two states DIFFER, never which is which, so it pins no state onto a shared
asset.

### 3.3 THE GIT CHECKOUT **ANSWER**, MEASURED - THE HALF SECTION 5 ITEM 6 LEFT OPEN

```
=== checkout (E-VCS-CHECKOUT) ===
  [1] TA01_HandLaid
      UsesCheckout() == false: A CHECKOUT IS A NO-OP ON THIS PROVIDER. Nothing is
      locked, because this provider has no lock command - so this line is NOT a claim
      that a lock was taken. The operation is issued anyway to record what the
      provider actually answers.
      provider answer, verbatim: ECommandResult::Failed
      ^ AND THAT IS THE MEASURED ANSWER TO A GIT CHECKOUT, not a prediction of one.
      provider error: Operation 'CheckOut' not supported by revision control provider 'Git'
      NO FILE WAS WRITTEN BY THIS TOOL. A checkout is a source control operation;
      the asset's contents are not touched here.
```

*** SO THIS FILE'S SECTION 5 ITEM 6 IS NOW CLOSED: A GIT CHECKOUT WAS RUN, AND
THE ANSWER IS `ECommandResult::Failed` WITH THE PROVIDER'S OWN
`"Operation 'CheckOut' not supported by revision control provider 'Git'"`. ***
That is exactly what `GitSourceControlProvider.cpp:286-289` and `:257-260`
predicted; the prediction and the measurement agree, and the measurement is the
one that counts. The failure is produced at the worker lookup
(`GitSourceControlProvider.cpp:225-238`) **before any command runs**, so nothing
on disk was touched - and that is confirmed below by a third tool.

### 3.4 Perforce: refused by name with the SECOND engine string

```
provider in use      : 'Perforce'
  IsEnabled()        : true
  IsAvailable()      : false
  UsesCheckout()     : true
  UsesLocalReadOnly(): true

REFUSAL, THE SAME FOR EVERY ASSET BELOW: Revision control server is currently not available.
  the string is the engine's own: SourceControlHelpers.cpp:76 (the engine's own gate)
  NO PROVIDER WAS REACHED, so no state is reported and no zero is printed (P1).
```

**The second engine string is now measured *through this tool*, per asset.** Row
1.3 could only reproduce it through the engine's Python surface.

## 4. THE READ-BACK THROUGH A DIFFERENT TOOL, AND THE TRAP IT FOUND

*** A TOOL'S OWN REPORT IS NOT EVIDENCE, SO EVERY CLAIM ABOVE IS CHECKED AGAINST
`USourceControlHelpers` - A DIFFERENT CODE PATH (`SourceControlHelpers.cpp`, not
`FTurboMCPSourceControl`) - DRIVEN BY THE ENGINE'S OWN `PythonScriptCommandlet`
WITH `-SCCProvider=`. THE PROBE IS EMBEDDED IN `Tools/test-vcs.py`, WRITTEN TO A
TEMP FILE AND RUN, SO THE CROSS-CHECK IS PART OF THE SUITE RATHER THAN A NOTE.***

```
no provider : exit 0  {'present': 'True', 'current_provider': "'None'", 'is_enabled': 'False',
                       'is_available': 'False', 'check_out_file_returned': 'False',
                       'last_error_msg': 'Text("Revision control is not enabled.")'}
git         : exit 0  {'present': 'True', 'current_provider': "'Git'", 'is_enabled': 'True',
                       'is_available': 'True', 'check_out_file_returned': 'True',
                       'last_error_msg': 'Text("")'}
```

The engine agrees with this tool about the provider and about both questions, and
it reproduces `"Revision control is not enabled."` verbatim.

### AND IT FOUND A TRAP, WHICH IS THE MOST USEFUL THING IN THIS SECTION

*** `USourceControlHelpers::CheckOutFile` ANSWERS `True` ON GIT, WITH AN EMPTY
`LastErrorMsg()`, AND IT NEVER ISSUES A CHECKOUT AT ALL. ***

`CheckOutFile` returns true at `SourceControlHelpers.cpp:456-460` when the state
says `IsCheckedOut() || IsAdded()` - **and Git reports `IsCheckedOut() == true`
for every tracked file**, which is the very line this tool prints for
`TA01_HandLaid` above. So on Git the engine's helper's `true` means
*"the state says it is already checked out"*, **NOT** *"a lock was taken"*.

**THE TWO ANSWERS ARE NOT IN CONFLICT: THEY ANSWER DIFFERENT QUESTIONS, AND THE
SUITE ASSERTS BOTH SO NEITHER CAN BE MISTAKEN FOR THE OTHER.**

- the engine's helper: `true`, because Git's state says the file is checked out;
- this tool: `Failed` + `"Operation 'CheckOut' not supported"`, because it goes
  through `ISourceControlProvider::Execute` and records the **operation's** answer.

**WHY THIS MATTERS FOR `D-57`.** A write tool wired to `CheckOutFile` and reading
its `true` as "checked out, safe to write" would, on Git, be relying on a lock
that does not exist - and `Spec/03_TOOLS.md:1348-1351` says exactly that:
*"Perforce will refuse a save that is not checked out; git will not, and will
happily leave a modified file for someone else to find."* **THE ENGINE'S OWN
HELPER CANNOT DISTINGUISH THOSE TWO CASES; THIS TOOL CAN, AND IT SAYS WHICH ONE
IT IS ON.** That is the concrete finding row 10.2 has to act on.

### AND NOTHING MOVED, CONFIRMED BY A THIRD TOOL

`git status --porcelain -- TestProject/Content/Tests/Kit/TA01_HandLaid.uasset`
is **empty** before and after all three provider probes and all three commandlet
read-backs. **The `git` CLI is a third tool and it answers a question neither of
the other two can: did any of this touch a file.** It did not.

## 5. THE HAZARDS SECTION 4 OF THIS FILE NAMED, AND HOW EACH IS HANDLED

| Hazard (section 4) | How it is handled, and it is a code fact rather than a promise |
|---|---|
| `SetProvider(FName)` is **FATAL** on an unmatched name (`SourceControlModule.cpp:404-417`). | **`SetProvider` IS NEVER CALLED.** The tool uses the provider the project selected. `GetProviderNames` is still called - to **print** the registry's own list, so the provider name in the answer is the engine's answer rather than a guess. |
| `GetProvider()` dereferences `CurrentSourceControlProvider` (`:379-382`). | Guarded by `FModuleManager::LoadModulePtr<ISourceControlModule>` returning non-null, after which the engine's own first callback has already assigned it. Still **not observable here**, as section 4 said: it is a code guard, not a measured case. |
| `GetState` returns a **NULL** state that reads as "not under source control" (`DefaultSourceControlProvider.cpp:52-60`). | `GetState` is called **only after both questions pass**. A null state that arrives anyway is printed as `STATE: unknown - <reason>`, with the explicit line `THIS IS NOT 'not under source control'`. |
| `-SCCProvider=Plastic SCM` contains a **space** (`PlasticSourceControlProvider.cpp:34`). | The suite passes the switch as **one element of an argv list**, so there is no quoting to get wrong. The launch line is printed, verbatim, in the suite output. |
| The refusal must be in `Plan` and be **per asset**. | `Plan` writes `RunRefusal` into **every** asset row, and each row also carries its own `Refusal`/resolve error. The suite compares the refusal text of the Read, Preview and Edit tools **string for string**. |

**WHERE THE FIXTURE PATH COMES FROM, AND WHY IT IS NOT
`LongPackageNameToFilename`.** `TurboMCPAcceptanceAssetCommandlet.cpp:1114-1118`
and `TurboMCPTestMapCommandlet.cpp:325-334` both record that conversion answering
a path **relative to the engine directory** in this project. So the file is found
by `FPackageName::DoesPackageExist` (`Misc/PackageName.h:450`, the mount-aware
answer, which is what produced every `file:` line above), with the hand-built
`/Game` path as a checked fallback - and **`IFileManager::FileExists` verifies
whichever one wins** before it is used. The route that won is printed.

## 6. THE SUITE: 55 CHECKS, FOUR PHASES, AND NO BRANCH SKIPPED

`Tools/test-vcs.py` is the only suite this row adds. **IT OWNS THE EDITOR
LIFECYCLE**, because reaching every branch means restarting the editor with a
different `-SCCProvider=` - which is the only mechanism that sets it
(`SourceControlSettings.cpp:58-65`), and `Tools/start-editor.ps1` takes no
pass-through arguments.

| Phase | What it measures | How |
|---|---|---|
| A | "source control off": the refusal, **per asset** | the editor as handed; **if a provider is already selected the suite restarts the editor without one**, so this phase runs in every invocation |
| B1 | Git: state per asset **and the measured checkout answer** | relaunch with `-SCCProvider=Git` (14s to answer, measured) |
| B2 | Perforce: `IsEnabled()==true`, `IsAvailable()==false`, both engine refusals | relaunch with `-SCCProvider=Perforce` (32s, measured) |
| B3 | the same answers through the **engine's own helper**, plus the `git` CLI | two `PythonScriptCommandlet` runs and `git status --porcelain` |
| C | **RESTORE, ASSERTED** | relaunch through `Tools/tmcp_editor.py`, then read `IsEnabled()` back through the tool |

**IT TAKES 125 SECONDS**, well inside the runner's 600-second per-suite deadline.

### One check was REPLACED, and the reason is worth keeping

The Git phase first asserted that the two real assets got **different** states -
they do, because `TA01_HandLaid` is tracked by git and `TA14_KitDataTable` is
not. **THAT MADE THE SUITE DEPEND ON A FILE STAYING UNTRACKED, SO SOMEBODY
ELSE'S `git add` WOULD HAVE TURNED A GREEN SUITE RED FOR A REASON THAT HAS
NOTHING TO DO WITH THIS CAPABILITY.** It was replaced by a check whose failing
input is the caller's own argument - with a provider answering, the resolvable
asset must get a state and an unresolvable name must still be refused **in the
same call** - and the tracked/untracked observation is **printed rather than
asserted**:

```
    the two real assets read as: ['true', 'false']  (printed, NOT asserted - it depends on the git index)
```

**A CHECK WHOSE FAILING INPUT IS SOMEBODY ELSE'S COMMIT IS THE DEFECT THIS
PROJECT HAS RECORDED REPEATEDLY.**

### The checks were shown RED, and the break was restored

A build was made with **four deliberate breaks in
`TurboMCPSourceControl.cpp`** - the `NotEnabledText` literal paraphrased, a
`under source control:  false` line added to the refusal branch, and both no-op
sentences reworded away. **SIX named checks went red, with the wrong value
visible rather than as a bare red line:**

```
  FAIL and it names the engine's own refusal for every asset, up front  === source control: E-VCS-CHECKOUT ===
  FAIL and the refusal is THE ENGINE'S OWN STRING, verbatim  ['Revision control is disabled.', 'Revision control is disabled.']
  FAIL and NO state is reported for any of them, so no false 'not under source control' can be read  [... '      REFUSED: Revision control is disabled.\n      under source control:  false\n ...']
  FAIL and it SAYS a checkout is a no-op on this provider instead of implying a lock was taken  === source control: E-VCS-CHECKOUT ===
  FAIL and the answer says a no-op was expected HERE rather than claiming a lock was taken  ...
  FAIL and no state is reported, because asking for one would be reaching a provider that has none  === source control: E-VCS-CHECKOUT ===
RESULT: FAIL (6 of 55) -> and it names the engine's own refusal for every asset, up front, ...
```

Note the **third** one: the deliberately re-introduced `under source control:
false` on a **refused** asset is exactly the `P1` mistake the check exists to
catch, and it appears in the failure detail as the wrong value.

**THEN IT WAS RESTORED, AND THE RESTORE WAS ASSERTED RATHER THAN ASSUMED:**
`git checkout -- Source/TurboMCP/TurboMCPSourceControl.cpp`, then
`git diff --stat` showed **only** the intended `Tools/test-vcs.py` change and no
diff under `Source/`, the broken literals were confirmed absent by a `grep`, the
target was rebuilt, and the suite returned `RESULT: PASS (55 check(s) run, no
check skipped)`.

## 7. WHAT COULD NOT BE SETTLED

1. **PLASTIC SCM AND SUBVERSION CANNOT BE EXERCISED HERE AT ALL**, and this is not
   new: section 1 measured that `cm` and `svn` are not on `PATH`. Their
   `AVAILABLE` branch is unreachable, so `IsAvailable() == true` was never seen
   for either. The suite says so in its own closing lines rather than implying
   coverage.
2. **NO CHECKOUT AGAINST A LIVE DEPOT WAS EVER PERFORMED BY ANYONE.** Git's answer
   is now measured but it is a **refusal**, and Perforce's `AVAILABLE` branch
   needs a server that answers (`p4 set` still points at
   `ssl:<p4-host>:1666`). So *"what Perforce does when it is available
   headless"* - section 5 item 2 of this file - **is still unmeasured**, and it is
   the single most important thing row 10.2 needs before it wires automatic
   checkout into a write tool.
3. **The Perforce `-SCCProvider=Perforce` EDITOR path was measured only as far as
   the refusal.** The editor came up and answered in 32s, and the tool refused
   before asking anything, so **no connection attempt was made from this tool** -
   which is why the editor survived. Whether a `GetState` on Perforce would
   connect, hang, or fail is **not measured**, because reaching it would require
   the connection the tool deliberately avoids.
4. **`Tools/start-editor.ps1` CANNOT PASS `-SCCProvider=`**, and that is a fact
   about the repository rather than about the engine: it takes no
   pass-through arguments. The suite therefore launches the editor directly for
   the two provider phases. **A future harness that wants a provider selected
   will need either that argument passed through, or a
   `SourceControlSettings.ini`** - and neither exists today, which is what
   section 1 already recorded.
5. **The `GetProvider()`-before-any-provider-registers hazard remains a code fact
   and not a measured failure** (section 4 of this file). Nothing here changed
   that.

---

# ROW 10.2 BUILT AND MEASURED: THE AUTOMATIC CHECKOUT INSIDE A WRITE TOOL

**Row 10.1 built the checkout TOOL and measured the trap. This section is the half
that WIRES THE CALL INTO A WRITE TOOL, and it obeys the trap rather than repeating
it.**

*** `D-57` IS SATISFIED AND IS NOW SPENT: `E-VCS-CHECKOUT` WAS BUILT AND TESTED
(row 10.1), AND TWO WRITE TOOLS NOW CALL AN AUTOMATIC CHECKOUT. ***

## 1. WHAT WAS BUILT, AND WHICH WRITERS ARE WIRED

| File | What it is |
|---|---|
| `Source/TurboMCP/TurboMCPSourceControlWrite.h` | `FTurboMCPWriteCheckoutAsset`, `FTurboMCPWriteCheckoutPlan` and `FTurboMCPSourceControlWrite` (`Plan`, `Render`, `EnsureCheckedOut`). **ALL CHECKS ARE IN `Plan`**, so a Preview and its Edit twin cannot decide differently. |
| `Source/TurboMCP/TurboMCPSourceControlWrite.cpp` | The decision and the operation. `EnsureCheckedOut` is the only function in the row that can reach a provider with a write-intent operation. |
| `Source/TurboMCP/TurboMCPStructTools.cpp` | `P-STRUCT-FIELDS` renders the decision; `E-STRUCT-FIELDS` renders it and acts on it, BEFORE the write. |
| `Source/TurboMCP/TurboMCPCompile.cpp` | `E-SAVE`, which is the tool that actually writes a file to disk, acts on the decision before the engine's own save path runs. |
| `Source/TurboMCP/Public/Tools/TurboMCPToolsets.h` | Three doc comments extended (`P-STRUCT-FIELDS`, `E-STRUCT-FIELDS`, `E-SAVE`) and **NO new tool declared**, so the AICallable count stays at 93. No reformatting; one paragraph each. |
| `Tools/test-vcs-write.py` | 62 checks, and it OWNS THE EDITOR to reach both branches. |

**WHY THESE TWO, AND WHY NOT EVERY WRITER.** There are **32 Edit tools** in the
toolset besides the read-only `ReadTurboMCPEditQueue`. Exactly **two of them are
wired today**, and both are named above:

- **`E-SAVE`** because it is THE tool that writes a file to disk, and because
  `D-57`'s own recorded failure is about it ("made `E-SAVE` report a failure while
  nothing had been written"). It is also the tool whose engine path **already
  issues a checkout without reading anything that says whether one can mean
  anything** - section 4 measures that.
- **`P/E-STRUCT-FIELDS`** because it is the NEWEST writer in the project (row 7.1,
  commit `41a0a02`, 2026-10-11) and because it has a PREVIEW TWIN, so the rule
  "put the check in `Plan` so the Preview and the Edit twin behave identically" is
  demonstrable rather than asserted. `P-STRUCT-FIELDS` is its Preview.

**THE WRITERS THAT REMAIN UNWIRED, NAMED ONE BY ONE:**
`E-COMPILE`, `E-ENUM-ADD`, `E-VARS-ADD`, `E-WIDGETS-AS-VARS`, `E-OVERRIDE-ADD`,
`E-OVERRIDE-REMOVE`, `E-FUNCTION-ADD`, `E-FUNCTION-REMOVE`, `E-WIDGET-RETARGET`,
`E-WIDGET-BIND-ADD`, `E-COMMENT`, `E-GRAPH-DEAD`, `E-GRAPH-KNOTS`,
`E-GRAPH-TIDY`, `E-GRAPH-FIX-BACKWARD`, `E-LOCAL-VARS`, `E-CALL-LOG`,
`E-CALL-LOG-CLEAR`, `E-TRACE`, `E-GRAPH-APPEND`, `E-GRAPH-REPLACE`,
`E-GRAPH-PATCH`, `E-MAT-TIDY`, `E-MAT-LABEL-PROMOTE`, `E-MAT-BLOCKS`,
`E-DATATABLE-ROWS`, `E-GRAPH-COPY`, `E-GRAPH-COLLAPSE`, **`E-STRUCT-NEW`**, and
**`E-VCS-CHECKOUT` itself** (which is the checkout tool rather than a content
writer, and is therefore not a writer this row could wire). That is **30 unwired
write tools out of 32**, two wired.
`E-STRUCT-NEW` is left unwired ON PURPOSE and the reason is a fact about it rather
than about this row: it creates a path that **does not exist yet**, so there is no
file for source control to have an opinion about. The mechanism reports that case
in words (section 5.1) but no tool calls it from that path today.

**A HALF-WIRED MECHANISM THAT SAYS WHICH HALF IS WIRED** is the deliverable here,
and this list is the other half.

## 2. THE TRAP, AND WHAT THIS ROW READS INSTEAD

*** `USourceControlHelpers::CheckOutFile` ANSWERS `True` ON GIT AND NEVER ISSUES A
CHECKOUT *** - `SourceControlHelpers.cpp:456-460` returns early when the state
says `IsCheckedOut() || IsAdded()`, and Git reports `IsCheckedOut() == true` for
every tracked file (row 10.1, section 4).

**THIS ROW DOES NOT CALL IT.** The identifier appears **four times in the whole
mechanism and every one of them is prose** - two lines of the header's comment, one
line of the block the tool prints, and one line of a decision reason - and
`CheckOutFile(` **with a parenthesis appears ZERO times**, which is what a call
would need. The suite asserts that over its own source rather than trusting this
paragraph, and a single `CheckOutFile(` in the mechanism fails

```
ok   it never CALLS CheckOutFile - the input that fails this is any 'CheckOutFile(' in it
```

What it reads instead, and both are measured answers rather than a helper opinion:

1. **`ISourceControlProvider::UsesCheckout()`** - whether a checkout can mean
   anything on this provider at all. `GitSourceControlProvider.cpp:286-289` false,
   `PerforceSourceControlProvider.cpp:622-625` true.
2. **`ISourceControlProvider::Execute(FCheckOut)` and the OPERATION's own
   result** - `Operation->GetResultInfo()`, whose messages are repeated verbatim.
   On Git that is `ECommandResult::Failed` with
   `"Operation 'CheckOut' not supported by revision control provider 'Git'"`.

**AND THE PROOF THAT IT IS NOT THE BOOLEAN IS TWO ANSWERS TO ONE QUESTION, TAKEN
IN ONE RUN FROM TWO CODE PATHS** (section 6.1): the engine's helper answers `True`
with an EMPTY error text on Git, while this mechanism, on the same provider and the
same file in the same session, records `Failed` with a message the helper can never
see because it returns before issuing the operation.

## 3. THE DECISION THIS ROW HAD TO MAKE: WHAT A WRITE TOOL DOES WHEN `UsesCheckout() == false`

*** `D-57` DOES NOT ANSWER THIS QUESTION, AND THAT IS SAID IN THE TOOL'S OWN OUTPUT
RATHER THAN IMPLIED. *** `Spec/05_DECISIONS.md:382-388` answers the ORDERING
(`E-VCS-CHECKOUT` is built and tested before any write tool calls it - satisfied by
row 10.1) and states its REASON ("An automatic checkout wired into every write
would turn a source control problem into a 'every tool reports failure' problem,
and the cause would be invisible from any of those results"). **It says nothing
about a provider on which a checkout can never help.** The choice is this row's,
and the tool prints it:

```
    policy, provenance  : D-57 (Spec/05_DECISIONS.md:382-388) answers the ORDERING only -
    E-VCS-CHECKOUT is built and tested before any write tool calls it - and it does NOT
    answer what a write tool does when UsesCheckout() == false. That choice is row 10.2's
    own, and it is (b).
```

**THE THREE OPTIONS, AND THE ONE CHOSEN:**

| Option | Verdict |
|---|---|
| (a) skip it, and say you skipped | **NOT CHOSEN.** A skip is not a measurement, and row 10.1's rule is that an expected no-op is still worth recording. |
| **(b) attempt it and report the provider's answer** | *** CHOSEN *** |
| (c) refuse the write unless the caller opts in | **NOT CHOSEN.** On Git, and on a provider-less editor, this refuses EVERY write - which is `D-57`'s "every tool reports failure" outcome arrived at from the other side. |

**WHY (b), IN FOUR MEASURED REASONS:**

1. **It is the same answer `E-VCS-CHECKOUT` already gives.** Row 10.1 issued the
   operation on a no-op provider and recorded the provider's words rather than
   predicting a no-op. Two answers about one asset on one provider would give a
   caller two stories.
2. **It never claims a lock was taken**, and the block says so in words
   (section 5.2).
3. **The answer is a REPORT AND NEVER A GATE.** The write proceeds whether the
   checkout succeeded, failed or was refused, and the block states that in every
   answer: `THE CHECKOUT ANSWER NEVER GATES THE WRITE`. This is `D-57` obeyed
   directly - a source control problem cannot become a write failure here.
4. **It costs nothing that is not already being paid** - section 4.

**`UsesCheckout() == false` IS NOT THE ONLY REFUSAL THAT DOES NOT GATE.** With NO
provider the row is refused with the engine's own string and the write still
happens; the suite asserts both (`the file really was written, so the refusal did
not gate the save`).

## 4. THE ENGINE ALREADY ISSUES A CHECKOUT ON EVERY SAVE, AND THIS IS WHERE THAT IS

*** MEASURED THIS ROUND, AND IT IS THE REASON `E-SAVE` IS THE RIGHT FIRST WRITER. ***
`E-SAVE` calls `UEditorAssetLibrary::SaveLoadedAsset`
(`EditorAssetSubsystem.cpp:1290-1309`), which reaches
`UEditorLoadingAndSavingUtils::SavePackages` -> `FileHelpers.cpp:6019-6023` ->
`InternalCheckoutAndSavePackages` (`:5912`). That function **already issues a
checkout**, at `:5925-5929`:

```
:5925   if (ISourceControlModule::Get().IsEnabled())
:5929       FEditorFileUtils::CheckoutPackages(PackagesToCheckout, &PackagesCheckedOut, ...);
```

**IT READS NEITHER `UsesCheckout()` NOR THE OPERATION'S RESULT** - `CheckoutPackages`
builds `FinalPackageCheckoutList` / `FinalPackageMarkForAddList` and its return
value is discarded at `:5929`. So on Git the engine issues an operation that
provider cannot execute on every single save, and nothing anywhere records that
answer. Row 10.2's automatic checkout runs EARLIER and records it, which is why the
cost of option (b) is zero here: **the operation was already being issued.**

## 5. THE TOOL'S OUTPUT, VERBATIM

### 5.1 No provider: the refusal, per asset, and the write that happens anyway

```
=== automatic checkout before the write (D-57): E-STRUCT-FIELDS ===
WHAT THIS IS: the source control half of this write tool. D-57 requires the
checkout tool to be built and tested before any write tool calls it, and it is.
This is NOT USourceControlHelpers::CheckOutFile: that helper answers true on Git
without issuing anything (SourceControlHelpers.cpp:456-460). It reads
UsesCheckout() and the OPERATION's own result instead.
what this call does: the operation the plan allowed IS ISSUED here, and no file is written by this section.

    policy              : AttemptAndReport - (b) of the three options: a checkout IS issued through the provider and the provider's own answer is repeated verbatim. (a) skip-and-say and (c) refuse-unless-opted-in were both rejected. THIS ANSWER NEVER GATES THE WRITE: it is a report beside the write's own result, so a provider that cannot take a lock cannot turn a working write into a failed one (D-57).
    policy, provenance  : D-57 (Spec/05_DECISIONS.md:382-388) answers the ORDERING only - E-VCS-CHECKOUT is built and tested before any write tool calls it - and it does NOT answer what a write tool does when UsesCheckout() == false. That choice is row 10.2's own, and it is (b).

    module loaded       : true
    provider in use     : 'None'
    registered providers: 5 -> Subversion, Git, Plastic SCM, Perforce, None
    IsEnabled()         : false
    IsAvailable()       : NOT ASKED, because IsEnabled() was already false.
    UsesCheckout()      : NOT READ, because no provider was selected.

    run refusal         : Revision control is not enabled.
    the string is from  : SourceControlHelpers.cpp:67 (the engine's own gate)
    NO PROVIDER WAS REACHED, so no state is reported and no zero is printed
    (P1), and the write itself is NOT gated by this line.
assets: 1

REFUSED FOR EVERY ASSET, AND NO PROVIDER WAS REACHED: this is the engine's own
string and the same one the Preview printed, because both read it out of Plan.
    [1] /Game/Tests/TA22_VcsWrite_R4476.TA22_VcsWrite_R4476
        package: /Game/Tests/TA22_VcsWrite_R4476
        file: <REPO>/TestProject/Content/Tests/TA22_VcsWrite_R4476.uasset
        file found by: FPackageName::DoesPackageExist (PackageName.h:450)
        decision: DO NOT ATTEMPT - Revision control is not enabled.
        NOT ATTEMPTED, and no provider call was made for this asset.

    THE CHECKOUT ANSWER NEVER GATES THE WRITE. It is reported beside the write's
    own result, so a provider that cannot take a lock cannot turn a working write
    into a failed one - which is the outcome D-57 exists to prevent.
    NO ASSET FILE WAS WRITTEN BY THIS SECTION. A checkout is a source control
    operation; the write tool's own result below or above is the file write.
```

**AND THE THIRD STATE, MEASURED: A WRITE TOOL NAMED AN ASSET THAT HAS NO FILE
YET.** The very first `E-SAVE` in the suite runs against a struct that exists only
in memory, and the row says so instead of printing an empty state:

```
        decision: DO NOT ATTEMPT - '/Game/Tests/TA22_VcsWrite_R32360' resolved to package
        '/Game/Tests/TA22_VcsWrite_R32360', but NO FILE FOR IT WAS FOUND ON DISK - so there is
        no file for source control to have an opinion about
        NOT ATTEMPTED, and no provider call was made for this asset.
```

### 5.2 Git: the no-op SAID, and the provider's own refusal

```
    module loaded       : true
    provider in use     : 'Git'
    registered providers: 5 -> Subversion, Git, Plastic SCM, Perforce, None
    IsEnabled()         : true
    IsAvailable()       : true
    UsesCheckout()      : false
    UsesLocalReadOnly() : false

    run refusal         : (none) - both engine questions passed, so each asset below was asked for its state.
assets: 1

    [1] /Game/Tests/TA22_VcsWrite_R4476.TA22_VcsWrite_R4476
        package: /Game/Tests/TA22_VcsWrite_R4476
        file: <REPO>/TestProject/Content/Tests/TA22_VcsWrite_R4476.uasset
        file found by: FPackageName::DoesPackageExist (PackageName.h:450)
        the state says: under source control false, checked out here false, open for add false, modified false
        UsesCheckout() == false: A CHECKOUT IS A NO-OP ON THIS PROVIDER. It has
        no lock command, so nothing here is a claim that a lock was taken. The
        operation is issued anyway and the provider's own answer is the record.
        decision: ATTEMPT - a checkout IS issued through the provider and its own answer is repeated, even though this provider reports UsesCheckout() == false and the operation is therefore expected to be a no-op here.
        provider answer, verbatim: ECommandResult::Failed
        provider error: Operation 'CheckOut' not supported by revision control provider 'Git'

    THE CHECKOUT ANSWER NEVER GATES THE WRITE. It is reported beside the write's
    own result, so a provider that cannot take a lock cannot turn a working write
    into a failed one - which is the outcome D-57 exists to prevent.
    NO ASSET FILE WAS WRITTEN BY THIS SECTION. A checkout is a source control
    operation; the write tool's own result below or above is the file write.
```

**THE WRITE OPENED WITH `changed:` IN BOTH PHASES.** The block is APPENDED, never
prepended, so no caller's parser sees a different first word - and the suite fails
if that changes.

## 6. THE READ-BACK THROUGH DIFFERENT TOOLS, AND THE FINDING THE SUITE EARNED

**A TOOL'S OWN REPORT IS NOT EVIDENCE, so four other instruments answer for it.**

### 6.1 The engine's own helper, a different code path, in the same run

`USourceControlHelpers` is driven through the engine's own `PythonScriptCommandlet`
with `-SCCProvider=Git` (the probe is embedded in the suite and written to a temp
file, so the cross-check is part of the run rather than a note):

```
    git : exit 0, 6 value(s)  {'present': 'True', 'current_provider': "'Git'",
        'is_enabled': 'True', 'is_available': 'True',
        'check_out_file_returned': 'True', 'last_error_msg': 'Text("")'}
```

*** SO IN ONE RUN: THE ENGINE'S HELPER SAYS `True` WITH NO ERROR TEXT, AND THIS
MECHANISM SAYS `ECommandResult::Failed` WITH THE PROVIDER'S OWN
`"Operation 'CheckOut' not supported by revision control provider 'Git'"`. ***
They cannot come from one call: the helper returns at
`SourceControlHelpers.cpp:456-460` BEFORE issuing the operation it would have to
issue to see that message. That is the trap, closed by measurement rather than by
argument.

### 6.2 The filesystem: a hash before and after

`hashlib.sha256` of the fixture file, taken around every call:

- across `P-STRUCT-FIELDS` and across `E-STRUCT-FIELDS`: **UNCHANGED**, so the
  checkout section writes no file and the struct edit only marks the package dirty;
- across each `E-SAVE`: **CHANGED**, so the save really wrote, and the edited field
  name (`VcsProbeGamma`, `VcsProbeDelta`) is found **in the saved `.uasset` bytes**
  - the check `test-struct.py` uses, and one no report can fake.

### 6.3 The `git` CLI: a third tool, and it FALSIFIED A CHECK OF MINE

**THE SUITE'S OWN "NOTHING ELSE MOVED" CHECK WENT RED ON ITS FIRST RUN, AND IT WAS
RIGHT TO.** The porcelain status of the fixture was `?? path` after phase A, was
still `?? path` after the write tool's checkout in phase B - and became
**`AM path`** after `E-SAVE`. Two staged columns mean the asset had been ADDED TO
THE GIT INDEX.

*** `E-SAVE` WITH THE GIT PROVIDER STAGES A BRAND-NEW ASSET, AND IT IS THE ENGINE'S
SAVE PATH RATHER THAN THIS ROW'S OPERATION: `FileHelpers.cpp:2483` builds
`FinalPackageMarkForAddList` and `:2536-2538` executes `FMarkForAdd` on it, which
the Git provider implements as a real `git add`. *** The suite now attributes it
inside one run - the same tool's checkout left the index alone a moment earlier,
and the `FMarkForAdd` came from the engine - and it prints all three statuses.

**AND IT FOUND A DEFECT IN THIS SUITE RATHER THAN ONLY IN THE TREE:** a staged entry
OUTLIVES its file, so deleting the fixture left `AD TestProject/...uasset` in the
index for ever. The suite now unstages every `TA22_VcsWrite_R*` entry on the way IN
as well as OUT, and **asserts the restore** (`the git INDEX carries no entry for it
either`).

## 7. THE SUITE: 62 CHECKS, TWO PHASES OF PROVIDER, AND IT OWNS THE EDITOR

`Tools/test-vcs-write.py` is the only suite this row adds.

*** IT RESTARTS THE EDITOR TWICE, AND THAT IS SAID OUT LOUD IN ITS OWN OUTPUT:
once to reach `-SCCProvider=Git`, once to restore a provider-less editor. *** It
must, because `SourceControlSettings.cpp:58-65` reads that setting off the command
line (or an ini that does not exist here) and **`Tools/start-editor.ps1` cannot
pass it through** - row 10.1's finding, unchanged. The restore is ASSERTED by
reading `IsEnabled()` back through `E-VCS-CHECKOUT`.

Its phases: **A** no provider (the fixture is built, saved, previewed and edited);
**B** Git (`UsesCheckout() == false`, the operation issued, the cross-check, the
index evidence); **C** restore, asserted.

```
RESULT: PASS (62 check(s) run, no check skipped)
```

**AND THE WHOLE SET, RUN AFTER THE WIRING LANDED: `passed 55, failed 0, blocked 0, of 55`** -
the 54 suites row 10.1 left green plus this one. The two suites this row could have
broken are both in it and both green: `test-struct` (`RESULT: PASS`, because the
block is APPENDED and no line of it is a two-space-indented `name: value` line) and
`test-vcs` (`RESULT: PASS (55 check(s) run, no check skipped)`, because the
mechanism reuses row 10.1's `Plan` rather than re-deriving the two engine
questions).

### The checks, and THE INPUT THAT MAKES EACH ONE FAIL

| Check | The input that makes it fail |
|---|---|
| E-SAVE still opens with `changed:` | the block being PREPENDED to the answer |
| and the block never says `saved` | the word appearing in the block (which a prepended block causes) |
| with no provider it states the ENGINE's own refusal, verbatim | the refusal being paraphrased |
| and it prints NO provider answer it could not have obtained | `provider answer, verbatim` appearing where nothing was issued |
| the file really was written, so the refusal did not gate the save | the write being gated by the refusal |
| the preview / the edit did not write the file - hash unchanged | any of them writing the file |
| THE PREVIEW AND THE EDIT DECIDE IDENTICALLY | the two reaching different decisions |
| NO `decision` LINE IS EVER EMPTY | a row whose reason is empty (section 8 - MEASURED) |
| THE WRITE STILL HAPPENED: a failed checkout did not fail the write | the write being gated by the checkout |
| it SAYS a checkout is a no-op instead of implying a lock | that sentence going away |
| AND THE OPERATION WAS STILL ISSUED | the trap: not issuing it |
| the provider's OWN message is repeated | a paraphrase |
| THE PREVIEW ISSUED NOTHING EVEN WHERE A CHECKOUT WOULD BE ATTEMPTED | the preview reaching a provider |
| THE CHECKOUT STAGED NOTHING | the checkout staging anything in the git index |
| THE ENGINE'S OWN SAVE PATH IS WHAT STAGED IT | `FMarkForAdd` changing, or the tool staging it |
| THE ENGINE'S OWN `CheckOutFile` ANSWERS TRUE ON GIT | the helper's behaviour changing |
| so the two answers CANNOT come from one call | the tool's answer collapsing onto the helper's `True` |
| it never CALLS `CheckOutFile` | a `CheckOutFile(` call appearing in the mechanism |
| it reads the capability flag / the OPERATION's result | `UsesCheckout()` or `Execute(`/`GetResultInfo()` going away |
| BOTH wired write tools call it | either `EnsureCheckedOut` call going away |
| the struct writer's PREVIEW does not reach a provider with a checkout | `EnsureCheckedOut` appearing in the preview body |
| AND THE RESTORE IS ASSERTED (provider-less) | the restore not happening |
| AND THE RESTORE IS ASSERTED (git index) | a staged entry left behind |

**NO CHECK DEPENDS ON A SHARED ASSET'S STATE.** The fixture is built per run
(`TA22_VcsWrite_R<pid>`) and the two `git status` checks compare the fixture's OWN
status before and after, never against `""` - so somebody else's commit or `git add`
cannot turn this suite red. That is the defect row 10.1 replaced a check over, and
this suite was written after reading that note.

## 8. THE CHECKS WERE SHOWN RED, AND A RED RUN FOUND A REAL DEFECT

A build was made with **four deliberate breaks**: (1) the trap's own shape, where a
provider reporting `UsesCheckout() == false` is treated as already checked out and
nothing is issued; (2) the policy named `SkipAndSay`; (3) the provenance sentence
reworded so it no longer says `D-57` is silent on the question; (4) the block
PREPENDED instead of appended in `E-SAVE`.

**TWELVE OF 61 NAMED CHECKS WENT RED, WITH THE WRONG VALUES VISIBLE:**

```
  FAIL E-SAVE still opens with changed:, so the block was APPENDED and not prepended  === automatic checkout before the write (D-57): E-SAVE ===
  FAIL and that block names the policy taken out of the three options  === automatic checkout before the write (D-57): E-SAVE ===
  FAIL and it says plainly that D-57 does not answer the UsesCheckout() == false question
  FAIL and the block never says saved, so it cannot be confused with the save count  1] /Game/Tests/TA22_VcsWrite_R52840
  FAIL the second save opens with changed:
  FAIL *** THE PREVIEW ISSUED NOTHING EVEN WHERE A CHECKOUT WOULD BE ATTEMPTED ***
  FAIL AND THE OPERATION WAS STILL ISSUED, with the provider's own answer repeated  ...each asset below was asked for its state.
  FAIL and the provider's OWN message is repeated, not paraphrased
  FAIL E-SAVE opens with changed: with Git selected too
  FAIL and its block carries the provider's own refusal
  FAIL while THIS project's write tool, on the same provider in the same run, recorded ECommandResult::Failed with the provider's 'not supported' message
  FAIL so the two answers CANNOT come from one call: ... helper 'True' vs tool False
RESULT: FAIL (12 of 61) -> ...
```

**THE LAST LINE IS THE TRAP ITSELF GOING RED, WITH BOTH VALUES IN IT:**
`helper 'True' vs tool False`. When the mechanism stops issuing the operation, its
answer collapses toward the helper's boolean and the two become indistinguishable -
which is exactly the state row 10.2 exists to prevent.

*** AND THE SAME RED RUN FOUND A DEFECT IN THIS ROW'S OWN CODE: AN EMPTY DECISION
REASON. *** The break made the decision in two steps, and the build printed
**`decision: DO NOT ATTEMPT - `** with NOTHING after the dash - the decision
stated, its reason lost. That is `P1`'s defect in a decision: an absent explanation
reading like a decided one. The reason is now assigned once with a guard, and a new
check asserts that **no `decision` line in ANY of the seven blocks, in either
phase, is empty**. That check is one of the 62.

**THEN IT WAS RESTORED, AND THE RESTORE WAS ASSERTED RATHER THAN ASSUMED:**
`git diff --stat` showed ONLY the intended
`Source/TurboMCP/TurboMCPSourceControlWrite.cpp` change and nothing else; a `grep`
confirmed no `DELIBERATE BREAK` marker and no `SkipAndSay` remained; the target was
rebuilt (`Result: Succeeded`); and the suite returned
`RESULT: PASS (62 check(s) run, no check skipped)`.

## 9. WHAT COULD NOT BE SETTLED

1. **NO CHECKOUT AGAINST A LIVE DEPOT HAS EVER BEEN PERFORMED - NOT BY ROW 10.1,
   NOT BY THIS ROW, NOT BY ANYONE.** `p4 set` still points at
   `ssl:<p4-host>:1666`, which does not answer; `cm` and `svn` are not on
   `PATH`. **EVERY CHECKOUT ANSWER IN THIS SECTION IS THE ANSWER TO A REFUSAL OR TO
   A NO-OP PROVIDER.** The claim this row makes is narrow and measured: *the
   operation is issued through the provider, and the provider's own answer is what
   gets reported*. **IT IS NOT A CLAIM THAT AN AUTOMATIC CHECKOUT WORKS.**
2. **The `UsesCheckout() == true` branch of the ACTING path was never executed.**
   The only provider on this machine with `UsesCheckout() == true` is Perforce, and
   it is `IsAvailable() == false`, so the mechanism refuses before reaching it - by
   design, and the refusal is measured. **What a successful `FCheckOut` looks like
   is unmeasured**, and the sentence that would have to change when a depot exists
   is the "ALREADY checked out here" branch of `NotAttemptedReason`.
3. **Whether option (b) is CORRECT for a live Perforce cannot be settled here.** It
   is chosen for the four reasons in section 3, and reason 3 - that the answer never
   gates the write - is the one a live depot might argue with: on Perforce a failed
   checkout very probably means the save will fail anyway, and this row does not
   pre-empt that, because a gate that cannot be measured is exactly what `D-57`
   warns against. **That is the first thing a reachable depot should be used to
   test.**
4. **The 30 unwired writers are unwired.** Section 1 lists them. Nothing here says
   they are wrong to be unwired yet.
5. **The `FMarkForAdd` staging on Git is reported, not judged.** It is the engine's
   behaviour, it is not this row's, and this row neither prevents nor endorses it.
   What this row does is stop its own fixture from leaving an index entry behind.
