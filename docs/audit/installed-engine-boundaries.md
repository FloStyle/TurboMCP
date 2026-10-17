# Installed-engine integration boundaries

[Documentation](../README.md) · [Engine compatibility](../engine-compatibility.md) · [Installed-engine support](../installed-engine.md)

Checked 2026-10-17. This is a source and dependency investigation. No build, editor, engine suite, packaging run or installed-engine test was performed by R16.

## Identity and evidence status

- Task: R16. Approved base: `af4d0efd97365df0f991f13ef369664b56aa927d`.
- Engine baseline: Unreal Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (`engine.lock.json`; local source checkout).
- TurboMCP plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Forward target recorded by R13: 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`; not a supported target and not runtime-tested here.
- Installed 5.8.3 distribution: **absent**. The local Launcher 5.6 installation has no `ModelContextProtocol.uplugin` or `ToolsetRegistry.uplugin` under its `Engine/Plugins` tree. This is an explicit gap, not evidence that every future installed distribution omits them.
- Current-base build/provenance: **integrator-supplied and passed** at the original candidate `ff633b8e687bab7dffba45721553422339d316de` (run `7fe2db69671c429f82126beba6d529a6`). It is summarized below. It does not certify the corrected documentation commit or an installed-engine consumer. A historical build at `084b76ceb58309ca8d17e996e0e2fa25027051ee` is retained separately as historical.
- Source evidence is **partial**: the cited headers and interfaces are recorded, but acceptance-required installed-distribution and per-symbol evidence is absent. No source inspection is labeled complete.

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

The declaration table records every Build.cs entry. The evidence table gives the owning header, used surface and export or link mechanism for each entry; "no direct use located" is recorded rather than invented.

The declaration table records every Build.cs entry; the evidence tables name the owning header and the used surface or record that no use was located. A source declaration is not link evidence: the local link evidence below is the integrator build at the original candidate, which compiled and linked the plugin but does not prove installed availability or per-symbol portability.

### Declared dependency inventory

| Dependency | Class | Production reference | Fixture/test reference | Installed evidence |
| --- | --- | --- | --- | --- |
| `Core` | Public | all production sources | all fixtures | present in any editor distribution |
| `CoreUObject` | Public | all UObject and reflection code | all fixtures | present in any editor distribution |
| `Engine` | Public | all production sources | all fixtures | present in any editor distribution |
| `AssetRegistry` | Private | asset and capture tools | kit, acceptance and material commandlets | unverified in installed 5.8.3 |
| `BlueprintEditorLibrary` | Private | function and graph tools | kit commandlet | unverified in installed 5.8.3 |
| `BlueprintGraph` | Private | graph tools and K2Node reflection | kit and acceptance commandlets | unverified in installed 5.8.3 |
| `KismetCompiler` | Private | no direct include or symbol located; may be required by the UnrealEd compile path | none located | unverified in installed 5.8.3 |
| `DeveloperSettings` | Private | UTurboMCPSettings | none located | unverified in installed 5.8.3 |
| `EditorFramework` | Private | IToolkitHost::GetParentWidget in TurboMCPCaptureAsset.cpp:617-618 | none located | unverified in installed 5.8.3 |
| `EditorScriptingUtilities` | Private | UEditorAssetLibrary::SaveLoadedAsset in compile/save | none located | plugin is off by default; presence in installed 5.8.3 unverified |
| `EditorSubsystem` | Private | UEditorSubsystem base of UAssetEditorSubsystem; GetEditorSubsystem<UAssetEditorSubsystem>() in TurboMCPCapture.cpp:507,560,736 and TurboMCPCaptureAsset.cpp:568-569 | none located | unverified in installed 5.8.3 |
| `GraphEditor` | Private | capture and graph tidy | none located | unverified in installed 5.8.3 |
| `Json` | Private | JSON parsing and schema/result serialization | commandlets | unverified in installed 5.8.3 |
| `JsonUtilities` | Private | no direct include or symbol located | none located | unverified in installed 5.8.3 |
| `Kismet` | Private | no direct include or symbol located; compile path uses UnrealEd FKismetEditorUtilities | none located | unverified in installed 5.8.3 |
| `InputCore` | Private | no direct include or symbol located | none located | unverified; keep until a clean build proves it unnecessary |
| `Slate` | Private | panel, widget preview and capture UI | panel probe commandlet | unverified in installed 5.8.3 |
| `SlateCore` | Private | panel, widget preview and capture UI | panel probe commandlet | unverified in installed 5.8.3 |
| `ToolMenus` | Private | no direct include or symbol located | none located | unverified; candidate |
| `UnrealEd` | Private | FAssetEditorToolkit and FBaseToolkit::GetToolkitHost in TurboMCPCaptureAsset.cpp:600,617; UAssetEditorSubsystem in TurboMCPCapture.cpp:507,560,736 | kit, acceptance, material and test-map commandlets | unverified in installed 5.8.3 |
| `ModelContextProtocol` | Private | registration, server state and analytics | none located | mandatory; NoRedist and off by default; installed presence unverified |
| `ModelContextProtocolEngine` | Private | settings and server address | none located | mandatory; supplied by ModelContextProtocol |
| `ToolsetRegistry` | Private | registration and call-recorder wrapper | commandlets use registry async results | mandatory; NoRedist and off by default; installed presence unverified |
| `ImageWrapper` | Private | no direct include or symbol located; PNG path uses Engine FImageUtils | none located | unverified in installed 5.8.3 |
| `RenderCore` | Private | no direct RenderCore-exported symbol located; render-target readback uses Engine APIs | none located | unverified; RHI is a propagated build dependency, not a RenderCore DLL export |
| `UMG` | Private | widget tree, bindings and preview | kit and acceptance commandlets | unverified in installed 5.8.3 |
| `UMGEditor` | Private | widget tree, bindings and preview | kit and acceptance commandlets | unverified in installed 5.8.3 |
| `MaterialEditor` | Private | material tools | material fixture commandlet | unverified in installed 5.8.3 |
| `Projects` | Private | IPluginManager in trap loading | none located | unverified in installed 5.8.3 |
| `AnalyticsET` | Private | IAnalyticsProviderET implementation | none located | unverified in installed 5.8.3 |
| `SlateInspectorToolset` | Private | USlateInspectorToolset::Screenshot in capture | none located | required for capture; NoRedist and off by default; installed presence unverified |
| `AnimGraph` | Private | none located | none located; TurboMCPKitCommandlet.cpp:2084-2128 says the module is linked for TA-18 but the fixture is not built | no include or symbol reference found |
| `Niagara` | Private | none located; capture compares class-name strings only | none located; TA-19 is not built | descriptor plugin is enabled by default; no typed use found |
| `NiagaraEditor` | Private | none located | none located | no typed use found |
| `PythonScriptPlugin` | Private | none located | TurboMCPTestMapCommandlet.cpp:19,123 | plugin is off by default; presence unverified |

### Used-interface evidence

| Dependency | Owning public header | Used surface | Export/link mechanism | Local link evidence |
| --- | --- | --- | --- | --- |
| `Core` | Runtime/Core/Public/CoreMinimal.h | FString, TArray, delegates, FModuleManager | CORE_API engine import | linked in integrator build 7fe2db69671c429f82126beba6d529a6 at ff633b8 |
| `CoreUObject` | Runtime/CoreUObject/Public/UObject/Object.h | UObject, UClass, UHT reflection | COREUOBJECT_API and generated code | linked in integrator build at ff633b8 |
| `Engine` | Runtime/Engine/Public/Engine/Engine.h | GEngine, UWorld, UTextureRenderTarget2D, FRenderTarget | ENGINE_API engine import | linked in integrator build at ff633b8 |
| `AssetRegistry` | Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h | IAssetRegistry, FAssetRegistryModule | virtual interface and module API | linked in integrator build at ff633b8 |
| `BlueprintEditorLibrary` | Editor/BlueprintEditorLibrary/Public/BlueprintEditorLibrary.h | UBlueprintEditorLibrary static functions | BLUEPRINTEDITORLIBRARY_API (UE_API) | linked in integrator build at ff633b8 |
| `BlueprintGraph` | Editor/BlueprintGraph/Classes/K2Node.h | UK2Node and K2Node_* classes | module API and UHT classes | linked in integrator build at ff633b8 |
| `KismetCompiler` | none located | no direct include or symbol located | not established; keep until a build test | not separately exercised |
| `DeveloperSettings` | Runtime/Engine/Public/Engine/DeveloperSettings.h | UDeveloperSettings | ENGINE_API and UHT | linked in integrator build at ff633b8 |
| `EditorFramework` | Editor/EditorFramework/Public/Toolkits/IToolkitHost.h | IToolkitHost::GetParentWidget; the interface is obtained from FAssetEditorToolkit::GetToolkitHost | pure virtual interface; implementation in the UnrealEd host | linked in integrator build at ff633b8 |
| `EditorScriptingUtilities` | Editor/EditorScriptingUtilities/Source/EditorScriptingUtilities/Public/EditorAssetLibrary.h | UEditorAssetLibrary::SaveLoadedAsset | EDITORSCRIPTINGUTILITIES_API (UE_API) | linked in integrator build at ff633b8 |
| `EditorSubsystem` | Editor/EditorSubsystem/Public/EditorSubsystem.h | UEditorSubsystem base class and UHT reflection; UAssetEditorSubsystem is owned by UnrealEd | UCLASS MinimalAPI and UHT reflection | linked in integrator build at ff633b8 |
| `GraphEditor` | Editor/GraphEditor/Public/GraphEditor.h | SGraphEditor | GRAPHEDITOR_API | linked in integrator build at ff633b8 |
| `Json` | Runtime/Json/Public/Dom/JsonObject.h | FJsonObject, FJsonSerializer | JSON_API | linked in integrator build at ff633b8 |
| `JsonUtilities` | Runtime/JsonUtilities/Public/JsonObjectConverter.h | no direct include or symbol located | not established; keep until a build test | not separately exercised |
| `Kismet` | Editor/Kismet/Public/BlueprintCompilationManager.h | no direct include or symbol located | not established; keep until a build test | not separately exercised |
| `InputCore` | Runtime/InputCore/Public/InputCoreTypes.h | no direct include or symbol located | not established; keep until a build test | not separately exercised |
| `Slate` | Runtime/Slate/Public/Widgets/SWidget.h | SWidget and Slate widgets | SLATE_API | linked in integrator build at ff633b8 |
| `SlateCore` | Runtime/SlateCore/Public/Styling/CoreStyle.h | FSlateStyleSet and FCoreStyle | SLATECORE_API | linked in integrator build at ff633b8 |
| `ToolMenus` | Developer/ToolMenus/Public/ToolMenus.h | no direct include or symbol located | not established; candidate | not separately exercised |
| `UnrealEd` | Editor/UnrealEd/Public/Toolkits/AssetEditorToolkit.h and Editor/UnrealEd/Public/Subsystems/AssetEditorSubsystem.h | FAssetEditorToolkit, FBaseToolkit::GetToolkitHost, UAssetEditorSubsystem | UNREALED_API class/method exports and UHT | linked in integrator build at ff633b8 |
| `ModelContextProtocol` | ModelContextProtocol/Public/IModelContextProtocolModule.h and ModelContextProtocolAnalytics.h | IModelContextProtocolModule::Get/GetTools/GetAnalyticsProvider, HashToolIdentifier, ParseToolsetName | MODELCONTEXTPROTOCOL_API and virtual interfaces | linked in integrator build at ff633b8 |
| `ModelContextProtocolEngine` | ModelContextProtocolEngine/Public/ModelContextProtocolSettings.h | UModelContextProtocolSettings and server URL/port helpers | MODELCONTEXTPROTOCOLENGINE_API and UHT | linked in integrator build at ff633b8 |
| `ToolsetRegistry` | ToolsetRegistry/Public/ToolsetRegistry/UToolsetRegistry.h, ToolsetRegistry.h, Toolset.h, ToolsetRegistrySubsystem.h | RegisterToolsetClass, Find, RegisterToolset, UnregisterToolset, ForEachToolset, ExecuteTool, GetJsonSchema, GetToolsetClass | TOOLSETREGISTRY_API and virtual interfaces | linked in integrator build 7fe2db69671c429f82126beba6d529a6 at ff633b8; 5.9 GetToolsetClass removal unresolved under R14 |
| `ImageWrapper` | Runtime/ImageWrapper/Public/ImageWrapper.h | no direct include or symbol located | not established; keep until a build test | not separately exercised |
| `RenderCore` | Runtime/RenderCore/Public/RenderCore.h; RHI headers are propagated | no direct RenderCore-exported symbol located; RHI headers and link closure are propagated by RenderCore's public RHI dependency | build dependency propagation: RenderCore.Build.cs publicly depends on RHI; RHI symbols are exported by RHI, not by RenderCore | linked in integrator build at ff633b8 |
| `UMG` | Runtime/UMG/Public/Blueprint/UserWidget.h | UUserWidget and UWidgetTree | UMG_API | linked in integrator build at ff633b8 |
| `UMGEditor` | Editor/UMGEditor/Public/WidgetBlueprint.h and WidgetBlueprintOperationUtils.h | UWidgetBlueprint and FWidgetBlueprintOperationUtils | UMGEDITOR_API | linked in integrator build at ff633b8 |
| `MaterialEditor` | Editor/MaterialEditor/Public/MaterialEditingLibrary.h | UMaterialEditingLibrary | MATERIALEDITOR_API | linked in integrator build at ff633b8 |
| `Projects` | Runtime/Projects/Public/Interfaces/IPluginManager.h | IPluginManager | PROJECTS_API and virtual interface | linked in integrator build at ff633b8 |
| `AnalyticsET` | Runtime/Analytics/AnalyticsET/Public/IAnalyticsProviderET.h | IAnalyticsProviderET; FAnalyticsET::Config (inline) | virtual interface and inline config type | linked in integrator build at ff633b8 |
| `SlateInspectorToolset` | SlateInspectorToolset/Public/SlateInspectorToolset.h | USlateInspectorToolset::Screenshot | SLATEINSPECTORTOOLSET_API | linked in integrator build at ff633b8 |
| `AnimGraph` | none located; candidate header Editor/AnimGraph/Public/AnimGraphNode_StateMachine.h | no include or typed symbol located | not established; candidate | not separately exercised |
| `Niagara` | none located; candidate header Plugins/FX/Niagara/Source/Niagara/Classes/NiagaraSystem.h | no include or typed symbol located | not established; candidate | not separately exercised |
| `NiagaraEditor` | none located; candidate header Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraSystemFactoryNew.h | no include or typed symbol located | not established; candidate | not separately exercised |
| `PythonScriptPlugin` | PythonScriptPlugin/Public/IPythonScriptPlugin.h | IPythonScriptPlugin::Get and ExecPythonCommand | module interface and pure virtual method | linked in integrator build at ff633b8 for the fixture code path |

### Transitive used interfaces

| Interface | Owning module and headers | Plugin reference | Build path | Export/link mechanism | Installed evidence |
| --- | --- | --- | --- | --- | --- |
| ISourceControlModule, ISourceControlProvider, ISourceControlState, FCheckOut, FUpdateStatus | SourceControl: Developer/SourceControl/Public/ISourceControlModule.h; Developer/SourceControl/Public/ISourceControlProvider.h; Developer/SourceControl/Public/ISourceControlState.h; Developer/SourceControl/Public/SourceControlOperations.h | TurboMCPSourceControl.cpp:10-18, 191-320, 410-530; TurboMCPSourceControlWrite.cpp:10-15, 254-459 | SourceControl is a public dependency of UnrealEd (UnrealEd.Build.cs:52-68). TurboMCP privately depends on UnrealEd, so UBT propagates the SourceControl include paths and link closure. SourceControl is not a direct TurboMCP.Build.cs entry. | ISourceControlModule, ISourceControlProvider and ISourceControlState are virtual interfaces; FCheckOut and FUpdateStatus are plain operation classes created by ISourceControlOperation::Create (inline template); the single-file GetState and Execute overloads are SOURCECONTROL_API virtual. | unverified in installed 5.8.3 |
| FAnalyticsEventAttribute | Analytics: Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h | TurboMCPAnalyticsProvider.cpp:15, 202-205, 279-295, 446 | AnalyticsET.Build.cs lists Analytics in PrivateDependencyModuleNames and in PublicIncludePathModuleNames. The header is visible to include; this is not a public link dependency of AnalyticsET. | FAnalyticsEventAttribute constructors, destructor and copy/move operations are inline in the public header; no Analytics link symbol is required by the inspected uses. | unverified in installed 5.8.3 |
| FRenderTarget::ReadPixels, UTextureRenderTarget2D, RHI build closure | Engine and RenderCore/RHI: Runtime/Engine/Public/UnrealClient.h; Runtime/Engine/Public/TextureRenderTarget2D.h; Runtime/RenderCore/Public/RenderCore.h; Runtime/RHI/Public/RHI.h | TurboMCPCapture.cpp:1032; TurboMCPCaptureAsset.cpp:729; TurboMCPCaptureSelector.cpp:1008; TurboMCPWidgetPreview.cpp:286 | RenderCore.Build.cs publicly depends on RHI (line 12) and adds RHI to PublicIncludePathModuleNames. A module that depends on RenderCore receives that build closure. This is build propagation, not a RenderCore DLL re-export. | FRenderTarget::ReadPixels is ENGINE_API in UnrealClient.h:113; the plugin uses that Engine symbol and FApp::CanEverRender instead of the RHI GRHIGlobals symbol. RHI symbols are exported by RHI, not by RenderCore. | unverified in installed 5.8.3 |

### Consumed engine interface evidence

| Surface | Production/test reference | Owning public header | Used symbol or interface | Export/link mechanism | Local link | Installed |
| --- | --- | --- | --- | --- | --- | --- |
| registry registration | TurboMCPToolsets.cpp:355-392 | ToolsetRegistry/Public/ToolsetRegistry/UToolsetRegistry.h | UToolsetRegistry::RegisterToolsetClass; IsToolsetClassRegistered; GetToolsetJsonSchema | TOOLSETREGISTRY_API static functions | linked in integrator build at ff633b8 | unverified |
| recorder wrapper | TurboMCPCallRecorder.cpp:84-198 | ToolsetRegistry/Public/ToolsetRegistry/Toolset.h and ToolsetRegistry.h | FToolset::ExecuteTool, GetJsonSchema, GetToolsetClass; FToolsetRegistry::Find, RegisterToolset, UnregisterToolset, ForEachToolset | TOOLSETREGISTRY_API methods and virtual interfaces; GetToolsetClass is inline virtual at 5.8.3 and removed at 5.9 | linked in integrator build at ff633b8; 5.9 identity unresolved | unverified |
| MCP analytics interception | TurboMCPAnalyticsProvider.cpp:45, 69, 76, 103, 241, 265 | ModelContextProtocol/Public/IModelContextProtocolModule.h and ModelContextProtocolAnalytics.h | IModelContextProtocolModule::Get, GetTools, GetAnalyticsProvider; HashToolIdentifier; ParseToolsetName | MODELCONTEXTPROTOCOL_API static functions and virtual interfaces | linked in integrator build at ff633b8 | unverified |
| SlateInspector screenshot | TurboMCPCapture.cpp:940 | SlateInspectorToolset/Public/SlateInspectorToolset.h | USlateInspectorToolset::Screenshot | SLATEINSPECTORTOOLSET_API static function | linked in integrator build at ff633b8 | unverified; descriptor plugin is NoRedist and off by default |
| editor save | TurboMCPCompile.cpp:516 | EditorScriptingUtilities/Public/EditorAssetLibrary.h | UEditorAssetLibrary::SaveLoadedAsset | EDITORSCRIPTINGUTILITIES_API static function | linked in integrator build at ff633b8 | unverified; descriptor plugin is off by default |
| source-control interfaces | TurboMCPSourceControl.cpp:10-18; TurboMCPSourceControlWrite.cpp:10-15 | Developer/SourceControl/Public/ISourceControlModule.h and related | see transitive interfaces | virtual interfaces, SOURCECONTROL_API overloads, inline operation creation | linked in integrator build at ff633b8 | unverified |
| analytics event attribute | TurboMCPAnalyticsProvider.cpp:15, 202-205, 279-295 | Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h | FAnalyticsEventAttribute | inline type methods; include path via AnalyticsET PublicIncludePathModuleNames | linked through AnalyticsET at ff633b8 | unverified |
| render-target readback and RHI closure | TurboMCPCapture.cpp:1032; TurboMCPCaptureAsset.cpp:729 | Engine/Public/UnrealClient.h; Engine/Public/TextureRenderTarget2D.h | FRenderTarget::ReadPixels; UTextureRenderTarget2D | ENGINE_API readback; RenderCore publicly propagates RHI build closure; no direct RHI symbol | linked in integrator build at ff633b8 | unverified |
Within the inspected include set, no `#include` spelling containing `Private/` was found, and each located header above resolves to a public path at the checked engine revision. That does not prove every transitively included header or every referenced symbol is public. Rows with no located use are marked as such rather than filled with an invented symbol. The plugin's own public headers are under `Source/TurboMCP/Public`; the module-root headers and `TurboMCPToolsetsInternal.h`, `TurboMCPSourceControl*.h`, `TurboMCPGraphStructure.h`, `TurboMCPPlayCheck.h`, `TurboMCPPureChecks.h` and `TurboMCPTrace.h` are private to the module.

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
| Full engine rebuild | The cited used headers resolve to public paths and registration/analytics use exported module APIs. The integrator build compiled no engine modules. | Not required by the inspected build evidence. A rebuild would be needed only if engine binaries or headers are modified or missing. |
| Plugin compile | `Tools/build.ps1` invokes `Build.bat TurboMCPTestEditor Win64 Development -Project=...`. The plugin module uses `bUseUnity=false`; it compiles as a separate editor module. | Integrator build run `7fe2db…` compiled/linked TurboMCP and TurboMCPTest: 145 actions, 298.43 seconds UBT. |
| Current-base build | Integrator-owned run `7fe2db69671c429f82126beba6d529a6` at original candidate `ff633b8`, tree digest `3040f00…`: passed, exit 0, 320.367 seconds supervisor total, 145 actions, no engine-module compile actions, existing engine binaries reused. Doctor passed; 16 fixture files restored; git status clean; provenance `DirtyDiffPresent=false`, `NoOpCandidate=false`. | Passed for that candidate and tree. Does not certify the corrected documentation commit. |
| Packaging | `RunUAT BuildPlugin` against an exact engine distribution, producing a Win64 Editor package with binaries and content. | Not run. Producer requires a matching engine and compiler. |
| Blueprint-only consumer | Copy the package into the project's `Plugins` folder, enable TurboMCP, and load it with no game C++ module or engine source. | Goal is stated; no clean-install proof exists. |
| C++ consumer | Public headers plus an import library would be needed. The plugin exports only a small public surface. | Not the primary target; no proof. |
| Installed-engine consumer | Requires the exact engine build, all mandatory plugins, matching binaries, content and ABI compatibility. | Blocked by the missing installed 5.8.3 distribution. |

The integrator build artifact for the original candidate is `TestProject/Binaries/Win64/UnrealEditor-TurboMCP.dll`, 3,741,184 bytes, SHA-256 `14DEE2F2B7DE4278BA02CDB090511EA1CBB5760BD84CD7665A29ADD398C03AC0`, built with MSVC 14.50.35728 and Windows SDK 10.0.22621.0. Those values belong to `ff633b8`, not to the correction commit.

Integrator-owned run `7fe2db69671c429f82126beba6d529a6` used `TURBOMCP_ENGINE=C:\UnrealEngine; python Tools/execute-tests.py --profile build --max-seconds 900 --execute`, target `TurboMCPTestEditor Win64 Development`, engine `396c9f0…`, and tree digest `3040f00…`. It passed with 145 actions; TurboMCP and TurboMCPTest compiled/linked and no engine module was compiled. Engine binaries were reused. Doctor passed, the supervisor restored 16 fixture files, git status was clean, and provenance recorded `DirtyDiffPresent=false`, `NoOpCandidate=false`, `BuildExitCode=0`. This is a source-engine target build, not packaging, an installed-engine test, a Blueprint-only consumer test or 5.9 support. It does not certify the corrected documentation commit.

The historical build is retained separately:

```text
target  : TurboMCPTestEditor Win64 Development
Result  : Succeeded
Target is up to date
0 action(s)
plugin DLL: TestProject/Binaries/Win64/UnrealEditor-TurboMCP.dll
```

The build report's provenance also records `EngineIdentity` `396c9f059903aed5fec78ecd3d437a40c6415368`, artifact size `3,740,672` bytes and artifact SHA-256 `6A20846E6ADCD22BB879A1557D93A5BAE6A278A60C9A16AB7174DDC2469902C9`. Those values belong to the old candidate and must not be presented as the R16 base artifact.

## TurboMCP public and binary surface

This section describes TurboMCP's own exports, not the engine interfaces it consumes; consumed engine surfaces and their export mechanisms are recorded above.

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
| Isolated version adapter | Candidate for 5.9, not a demonstrated replacement. `FToolset::GetToolsetClass` is removed; the public name-based path is not proven authoritative. R14 must resolve the public identity path. No protected manifest access. | **Proposal under R14; not implemented by R16** |
| Optional development module extraction | Would separate commandlets, tests and fixture-only dependencies such as `PythonScriptPlugin` from the production editor package. | **Defer** until packaging evidence proves it is necessary; do not remove dependencies speculatively |
| Direct per-tool `IModelContextProtocolTool` registration | Would bypass the toolset registry and duplicate schema/registration behavior. | Not preferred |
| Custom transport, replacement server or engine patch | Explicitly outside scope and unsafe as a shortcut. | Rejected |

## Bounded follow-ups

| ID | Scope | Acceptance |
| --- | --- | --- |
| T16-ADAPTER-1 | Proposal under R14. The public 5.9 `FToolset` no longer exposes `GetToolsetClass`. `GetToolsetName`, `ForEachToolset` and `Find` are public, but reconstructed-name identity is not demonstrated authoritative. R14 must identify a public class-to-registered-name path or explicitly leave identity unresolved; do not use protected manifest access. Requirements: preserve unrelated handlers; skip unavailable or filtered toolsets; avoid duplicate wrapping; restore the original handler after a failed swap; retain filtering, enable state and recording through shutdown/reload. | R14 resolves the identity path and proves both engine builds. Negative checks: name mismatch, missing or filtered handler, repeated install, failed wrapper registration. No adapter is implemented by R16. |
| T16-DEPS-1 | Verify the listed-but-unused `AnimGraph`, `Niagara`, `NiagaraEditor`, `ToolMenus` and `InputCore` dependencies with a clean build and symbol search. Remove only dependencies proven unused, preserving fixture authoring requirements. | Clean plugin build with each dependency absent; no production or fixture regression; evidence recorded. |
| T16-DEV-1 | If the prebuilt package cannot require fixture-only dependencies, move commandlets and development automation into a separate editor/development module while preserving commandlet names and tests. | Production package loads without `PythonScriptPlugin`; commandlets still runnable from the development build. |
| T16-PKG-1 / R12 | Build a Win64 Editor package with `RunUAT BuildPlugin`, install it into a fresh Blueprint-only project and test discovery, calls, save, undo/rollback, shutdown and reload. | Clean install on the exact engine build; producer and consumer requirements recorded; no game C++ or engine source compile. |
| T16-META-1 | Replace the deleted-public-repository `DocsURL` and `SupportURL` only after a live publication target exists. | Valid metadata target recorded; no invented URL. |

Ownership: R14 owns forward compatibility and the unresolved 5.9 identity path; R12 owns packaging and the Blueprint-only consumer; R04 owns behavioral auditing where a behavior question remains. The T16 identifiers are proposals, not dispatch-ready tasks.

## Defects and evidence gaps

- **Release metadata defect:** `DocsURL` and `SupportURL` point at `https://github.com/FloStyle/TurboMCP` and its `/issues` path, but the public repository is deleted. `MarketplaceURL` is empty. No replacement URL is invented here.
- **Dependency metadata drift:** `TurboMCP.Build.cs` lists `AnimGraph`, `Niagara` and `NiagaraEditor`, while `TurboMCPKitCommandlet.cpp` still says those modules are absent for TA-18/TA-19. No include or typed symbol use was located. `Kismet`, `KismetCompiler`, `JsonUtilities`, `ImageWrapper`, `ToolMenus` and `InputCore` also have no located direct use in this pass; they are retained and marked for verification rather than removed.
- **Fixture coupling:** six commandlets and three development automation tests compile into the same editor module as production code. `PythonScriptPlugin` is used only by the test-map commandlet. No module split exists.
- **Forward compile break:** `TurboMCPCallRecorder.cpp:98,170` uses `FToolset::GetToolsetClass`, removed at the pinned 5.9 target. The identity replacement is unresolved and belongs to R14; no adapter is implemented here.
- **Installed-engine gap:** no installed 5.8.3 distribution; the local 5.6 tree lacks the mandatory MCP and registry plugins. R12 cannot certify compatibility.
- **Build evidence boundary:** the integrator build passed at original candidate `ff633b8` and tree `3040f00…`; it did not build the corrected documentation commit and did not exercise packaging, an installed engine or a Blueprint-only consumer.
- **Per-symbol limitation:** the integrator build proves the plugin linked for its source-engine target. It does not prove every consumed engine symbol is individually exported from an installed distribution.
- **Consumer gap:** no packaged plugin, no Blueprint-only clean install, no runtime discovery/save/undo proof.
- **Execution-lock gap:** cross-harness engine/port locking remains unverified; the integrator owns the supervised build run.
