# Installed-engine integration boundaries

[Documentation](../README.md) · [Engine compatibility](../engine-compatibility.md) · [Installed-engine support](../installed-engine.md)

Checked 2026-10-17. This is a source and dependency investigation. No build, editor, engine suite, packaging run or installed-engine test was performed by R16.

## Identity and evidence status

- Task: R16. Approved base: `af4d0efd97365df0f991f13ef369664b56aa927d`.
- Engine baseline: Unreal Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (`engine.lock.json`; local source checkout).
- TurboMCP plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Forward target recorded by R13: 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`; not a supported target and not runtime-tested here.
- Installed 5.8.3 distribution: **absent**. The local Launcher 5.6 installation has no `ModelContextProtocol.uplugin` or `ToolsetRegistry.uplugin` under its `Engine/Plugins` tree. This is an explicit gap, not evidence that every future installed distribution omits them.
- Current-base build/provenance report: **pending**. The integrator supplies the supervised baseline build during review. A historical build report exists at commit `084b76ceb58309ca8d17e996e0e2fa25027051ee` (not this base); it is identified separately and is not accepted as current-base evidence.

## Module, descriptor and registration

`TurboMCP.uplugin` declares one module:

| Property | Value | Consequence |
| --- | --- | --- |
| Module | `TurboMCP`, type `Editor`, loading phase `PostEngineInit` | Editor-only; not a packaged-game runtime plugin |
| `CanContainContent` | `true` | Plugin content, including `Content/Traps/EngineToolTraps.json`, must ship |
| `EnabledByDefault` | `false` | A consumer must enable the plugin explicitly |
| `ExplicitlyLoaded` | `false` | Normal module loading when the plugin is enabled |
| `Installed` | `false` | Descriptor metadata for this checkout; not proof of loadability |
| Module dependencies | See the tables below | All must compile and load |

Registration uses the supported public path:

- `FTurboMCPModule::StartupModule` waits for `FCoreDelegates::OnAllModuleLoadingPhasesComplete`, then calls `RegisterTurboMCPToolsets` (`TurboMCPModule.cpp:22-81`).
- `RegisterTurboMCPToolsets` calls `UToolsetRegistry::RegisterToolsetClass` for the four toolset classes and then installs call recording (`TurboMCPToolsets.cpp:355-392`).
- No engine patch, private manifest API or custom MCP transport is used for this registration.
- `UToolsetRegistry::IsAvailable` is checked first; when the registry is unavailable the plugin logs a warning and registers nothing.

The panel uses Slate only and is registered independently (`STurboMCPPanel.cpp:1070-1090`). The focus guard and the analytics provider are installed from the module lifecycle.

## Build.cs dependencies

Classification: **production** means a non-commandlet, non-test source; **fixture** means a commandlet or development automation; **candidate** means listed in `TurboMCP.Build.cs` but no include or symbol caller was located in this source pass. The table has one row for each of the 35 declared module entries.

| Dependency | Class | Production caller | Fixture/test caller | Header visibility | Installed evidence |
| --- | --- | --- | --- | --- | --- |
| `Core` | Public | All production sources | All fixtures | Engine public | Required; present in any editor distribution |
| `CoreUObject` | Public | All UObject and reflection code | All fixtures | Engine public | Required; present in any editor distribution |
| `Engine` | Public | All production sources | All fixtures | Engine public | Required; present in any editor distribution |
| `AssetRegistry` | Private module | Asset and capture tools | Kit, acceptance and material commandlets | Public engine module | Required; unverified in an installed 5.8.3 tree |
| `BlueprintEditorLibrary` | Private module | Function and graph tools | Kit commandlet | Public engine module | Required |
| `BlueprintGraph` | Private module | Graph tools, `K2Node_*` reflection | Kit and acceptance commandlets | Public engine module | Required |
| `KismetCompiler` | Private module | `TurboMCPCompile.cpp`, graph repoint | None located | Public engine module | Required |
| `DeveloperSettings` | Private module | `UTurboMCPSettings` | None | Public engine module | Required |
| `EditorFramework` | Private module | `FAssetEditorToolkit` in capture | None | Public engine module | Required |
| `EditorScriptingUtilities` | Private module and descriptor plugin | `UEditorAssetLibrary::SaveLoadedAsset` in compile/save | Material fixture uses material APIs, not this module | Public plugin module | Required for save; plugin is off by default but the descriptor enables it |
| `EditorSubsystem` | Private module | `UAssetEditorSubsystem` in capture/tidy | None | Public engine module | Required |
| `GraphEditor` | Private module | Capture and graph tidy | None | Public engine module | Required |
| `Json` | Private module | JSON parsing and schema/result serialization | Commandlets | Public engine module | Required |
| `JsonUtilities` | Private module | UObject JSON conversion | Commandlets | Public engine module | Required |
| `Kismet` | Private module | Blueprint compile path | Test project's fixture module also depends on it, not this module | Public engine module | Required |
| `InputCore` | Private module | No direct include or symbol located | None | Public engine module | Keep until a clean build proves it unnecessary |
| `Slate` | Private module | Panel, widget preview and capture UI | Panel probe commandlet | Public engine module | Required |
| `SlateCore` | Private module | Panel, widget preview and capture UI | Panel probe commandlet | Public engine module | Required |
| `ToolMenus` | Private module | No direct include or symbol located | None | Public engine module | Candidate; verify before removing |
| `UnrealEd` | Private module | Editor APIs across production tools | Kit, acceptance, material and test-map commandlets | Public engine module | Required |
| `ModelContextProtocol` | Private module | Registration, server state, analytics | None | Public plugin module | Mandatory; descriptor plugin is `NoRedist` and off by default |
| `ModelContextProtocolEngine` | Private module | Settings and server address in tools | None | Public plugin module | Mandatory; supplied by the `ModelContextProtocol` plugin |
| `ToolsetRegistry` | Private module | Registration and call-recorder wrapper | Commandlets use registry async results | Public plugin module | Mandatory; descriptor plugin is `NoRedist` and off by default |
| `ImageWrapper` | Private module | PNG capture encoding | None | Public engine module | Required |
| `RenderCore` | Private module | Render-target readback in capture | None | Public engine module | Required; the code documents that `RHI` symbols are not re-exported here |
| `UMG` | Private module | Widget tree, bindings and preview | Kit and acceptance commandlets | Public engine module | Required |
| `UMGEditor` | Private module | Widget tree, bindings and preview | Kit and acceptance commandlets | Public engine module | Required |
| `MaterialEditor` | Private module | Material tools | Material fixture commandlet | Public engine module | Required |
| `Projects` | Private module | `IPluginManager` in trap loading | None | Public engine module | Required |
| `AnalyticsET` | Private module | `IAnalyticsProviderET` implementation | None | Public engine module | Required |
| `SlateInspectorToolset` | Private module and descriptor plugin | `USlateInspectorToolset::Screenshot` in capture | None | Public plugin module | Required for capture; descriptor plugin is `NoRedist` and off by default |
| `AnimGraph` | Private module | None located | None located; `TurboMCPKitCommandlet.cpp:2084-2128` says the module is linked for TA-18 but the fixture is not built | Public engine module | Candidate; no include or symbol reference found |
| `Niagara` | Private module and descriptor plugin | None located; capture only compares class-name strings | None located; TA-19 is not built (`TurboMCPKitCommandlet.cpp:2132-2158`) | Public plugin module | Candidate; no typed use found |
| `NiagaraEditor` | Private module | None located | None located | Public plugin module | Candidate; no typed use found |
| `PythonScriptPlugin` | Private module and descriptor plugin | None located | `TurboMCPTestMapCommandlet.cpp:19,123` | Public plugin module | Fixture-only candidate; plugin is off by default |

No `#include ".../Private/..."` path was found in the plugin source. The engine headers used for the listed modules resolve to public engine or plugin headers. The plugin's own public headers are under `Source/TurboMCP/Public`; the module-root headers and `TurboMCPToolsetsInternal.h`, `TurboMCPSourceControl*.h`, `TurboMCPGraphStructure.h`, `TurboMCPPlayCheck.h`, `TurboMCPPureChecks.h` and `TurboMCPTrace.h` are private to the module.

## Descriptor plugin dependencies

| Plugin | Provides | `EnabledByDefault` | `NoRedist` | Role |
| --- | --- | --- | --- | --- |
| `ModelContextProtocol` | `ModelContextProtocol`, `ModelContextProtocolEngine`, editor modules | `false` | `true` | Mandatory transport and tool registration |
| `ToolsetRegistry` | `ToolsetRegistry` | `false` | `true` | Mandatory toolset registry |
| `SlateInspectorToolset` | `SlateInspectorToolset` | `false` | `true` | Capture screenshot path |
| `Niagara` | `Niagara`, `NiagaraEditor` and related modules | `true` | not set | Descriptor/module dependency; no typed production use found |
| `EditorScriptingUtilities` | `EditorScriptingUtilities` | `false` | not set | Save path |
| `PythonScriptPlugin` | `PythonScriptPlugin` | `false` | not set | Test-map fixture only |

`NoRedist` is a descriptor convention, not a runtime load gate in the inspected engine source. Whether an installed 5.8.3 distribution ships these plugins is unverified. If any mandatory dependency is absent, the plugin cannot load or compile; an engine rebuild would not create a missing distributed plugin.

## Rebuild, compile, packaging and consumer boundaries

| Boundary | Evidence | Status |
| --- | --- | --- |
| Full engine rebuild | No engine source edit is required by this plugin. All used engine headers are public; registration and analytics use exported module APIs. | Not required by source evidence. A rebuild would be needed only if engine binaries or headers are modified or missing. |
| Plugin compile | `Tools/build.ps1` invokes `Build.bat TurboMCPTestEditor Win64 Development -Project=...`. The plugin module uses `bUseUnity=false`; it compiles as a separate editor module. | Requires the engine build and compiler. Does not imply an engine rebuild. |
| Existing build path | Historical report `91c49a6cddc544b8bc0ed9e0d6e3cd28` at commit `084b76c`: `Target is up to date`, 0 actions, plugin DLL reused, no engine rebuild. It records `DirtyDiffPresent: true` and is not this base. | Historical only; current-base build is pending. |
| Packaging | `RunUAT BuildPlugin` against an exact engine distribution, producing a Win64 Editor package with binaries and content. | Not run. Producer requires a matching engine and compiler. |
| Blueprint-only consumer | Copy the package into the project's `Plugins` folder, enable TurboMCP, and load it with no game C++ module or engine source. | Goal is stated; no clean-install proof exists. |
| C++ consumer | Public headers plus an import library would be needed. The plugin exports only a small public surface. | Not the primary target; no proof. |
| Installed-engine consumer | Requires the exact engine build, all mandatory plugins, matching binaries, content and ABI compatibility. | Blocked by the missing installed 5.8.3 distribution. |

The only explicit historical build evidence is:

```text
target  : TurboMCPTestEditor Win64 Development
Result  : Succeeded
Target is up to date
0 action(s)
plugin DLL: TestProject/Binaries/Win64/UnrealEditor-TurboMCP.dll
```

The build report's provenance also records `EngineIdentity` `396c9f059903aed5fec78ecd3d437a40c6415368`, artifact size `3,740,672` bytes and artifact SHA-256 `6A20846E6ADCD22BB879A1557D93A5BAE6A278A60C9A16AB7174DDC2469902C9`. Those values belong to the old candidate and must not be presented as the R16 base artifact.

## Public and binary surface

- The module entry is `IMPLEMENT_MODULE(FTurboMCPModule, TurboMCP)` (`TurboMCPModule.cpp:134`).
- `TURBOMCP_API` exports are limited to `UTurboMCPSettings` (`TurboMCPSettings.h:29`), `FTurboMCPMutationTransaction` (`TurboMCPMutation.h:124`) and `LogTurboMCP` (`Logging.h:27`).
- The four toolset UCLASSes are registered internally and are not marked for external C++ use. A Blueprint-only consumer does not need their headers or an import library.
- The prebuilt editor DLL must carry the module entry, UHT reflection data and the public exports above. Dependency module DLLs must be present at load time.
- The plugin's public include path is `Source/TurboMCP/Public`. Private module headers are not part of the consumer surface.

## Blueprint-only consumer constraints

1. Install the plugin under the project's `Plugins/TurboMCP`; the consumer does not need `AdditionalPluginDirectories`.
2. Enable TurboMCP explicitly. The plugin is `EnabledByDefault: false`.
3. Supply a Win64 Editor binary built for the exact engine build and compatible build configuration. A version string alone is not ABI proof.
4. Ensure every descriptor dependency exists in the installed engine, especially `ModelContextProtocol`, `ToolsetRegistry` and `SlateInspectorToolset`. Missing dependencies block load.
5. Ship `Content/Traps/EngineToolTraps.json` and `Resources/Icon128.png`; the descriptor references the icon and the trap loader resolves plugin content.
6. The project may have no `Source` directory; it must not be asked to compile game C++ or engine source. The producer may still need a compiler and matching engine.
7. Runtime proof of discovery, tool calls, save, undo/rollback, shutdown and reload is still required; a copied package is not acceptance evidence.

## Integration options

| Option | Assessment | Decision |
| --- | --- | --- |
| Direct supported registry registration | Current path: `UToolsetRegistry::RegisterToolsetClass` and public MCP module APIs. No engine patch or custom transport. | **Recommended, smallest supported integration** |
| Isolated version adapter | Needed for the 5.9 `FToolset::GetToolsetClass` removal in `TurboMCPCallRecorder.cpp`. A compile-time adapter can use class identity on 5.8.3 and registered-name lookup on 5.9. | **Required follow-up for 5.9**; no engine patch |
| Optional development module extraction | Would separate commandlets, tests and fixture-only dependencies such as `PythonScriptPlugin` from the production editor package. | **Defer** until packaging evidence proves it is necessary; do not remove dependencies speculatively |
| Direct per-tool `IModelContextProtocolTool` registration | Would bypass the toolset registry and duplicate schema/registration behavior. | Not preferred |
| Custom transport, replacement server or engine patch | Explicitly outside scope and unsafe as a shortcut. | Rejected |

## Bounded follow-ups

| ID | Scope | Acceptance |
| --- | --- | --- |
| T16-ADAPTER-1 | Add a compile-time adapter in `TurboMCPCallRecorder.cpp` for 5.8.3 class identity and 5.9 registered-name identity; conditionally compile the removed override. | 5.8.3 and 5.9 compile; four toolsets wrapped once; start/end records unchanged; unregister/reload leaves no stale handler. |
| T16-DEPS-1 | Verify the listed-but-unused `AnimGraph`, `Niagara`, `NiagaraEditor`, `ToolMenus` and `InputCore` dependencies with a clean build and symbol search. Remove only dependencies proven unused, preserving fixture authoring requirements. | Clean plugin build with each dependency absent; no production or fixture regression; evidence recorded. |
| T16-DEV-1 | If the prebuilt package cannot require fixture-only dependencies, move commandlets and development automation into a separate editor/development module while preserving commandlet names and tests. | Production package loads without `PythonScriptPlugin`; commandlets still runnable from the development build. |
| T16-PKG-1 / R12 | Build a Win64 Editor package with `RunUAT BuildPlugin`, install it into a fresh Blueprint-only project and test discovery, calls, save, undo/rollback, shutdown and reload. | Clean install on the exact engine build; producer and consumer requirements recorded; no game C++ or engine source compile. |
| T16-META-1 | Replace the deleted-public-repository `DocsURL` and `SupportURL` only after a live publication target exists. | Valid metadata target recorded; no invented URL. |

## Defects and evidence gaps

- **Release metadata defect:** `DocsURL` and `SupportURL` point at `https://github.com/FloStyle/TurboMCP` and its `/issues` path, but the public repository is deleted. `MarketplaceURL` is empty. No replacement URL is invented here.
- **Dependency metadata drift:** `TurboMCP.Build.cs` lists `AnimGraph`, `Niagara` and `NiagaraEditor`, while `TurboMCPKitCommandlet.cpp` still says those modules are absent for TA-18/TA-19. No include or typed symbol use was located. Recorded for verification; not removed.
- **Fixture coupling:** six commandlets and three development automation tests compile into the same editor module as production code. `PythonScriptPlugin` is used only by the test-map commandlet. No module split exists.
- **Forward compile break:** `TurboMCPCallRecorder.cpp:98,170` uses `FToolset::GetToolsetClass`, removed at the pinned 5.9 target. This is a production code change for a follow-up, not R16 implementation.
- **Installed-engine gap:** no installed 5.8.3 distribution; the local 5.6 tree lacks the mandatory MCP and registry plugins. R12 cannot certify compatibility.
- **Build evidence gap:** no current-base build or provenance report; the historical report is at `084b76c` and has a dirty tree.
- **Consumer gap:** no packaged plugin, no Blueprint-only clean install, no runtime discovery/save/undo proof.
- **Execution-lock gap:** cross-harness engine/port locking remains unverified; the integrator owns the supervised build run.
