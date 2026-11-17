# Launcher 5.8.4 dependency audit (R12, part 1)

Investigation record for the installed Launcher distribution. It answers one question: are the dependencies TurboMCP needs present in the exact installed build? It does not change the plugin, the descriptor or the pin, and it makes no support claim.

## Verdict

**`ready-for-package`**

## Engine identity

| Field | Value |
| --- | --- |
| MajorVersion | 5 |
| MinorVersion | 8 |
| PatchVersion | 4 |
| Changelist | 58902052 |
| CompatibleChangelist | 55116800 |
| IsLicenseeVersion | 0 |
| IsPromotedBuild | 1 |
| BranchName | ++UE5+Release-5.8 |
| Engine/Binaries/Win64/UnrealEditor.modules BuildId | 55116800 |
| LauncherInstalled.dat AppVersion | 5.8.4-58902052+++UE5+Release-5.8-Windows |

`BuildId` equals `CompatibleChangelist`, not `Changelist`. A Launcher build has one fixed BuildId that every installation of that build shares, so a binary built against it is not tied to this machine.

## Descriptor dependencies

| Plugin | Descriptor | EnabledByDefault | Installed | Experimental | Beta | Modules | Plugin BuildId | DLL | Import lib |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ModelContextProtocol | Engine\Plugins\Experimental\ModelContextProtocol\ModelContextProtocol.uplugin | False | False | True | False | 6 | 55116800 | present | present |
| ToolsetRegistry | Engine\Plugins\Experimental\ToolsetRegistry\ToolsetRegistry.uplugin | False | False | True | False | 1 | 55116800 | present | present |
| SlateInspectorToolset | Engine\Plugins\Experimental\Toolsets\SlateInspectorToolset\SlateInspectorToolset.uplugin | False | False | True | False | 2 | 55116800 | present | present |
| Niagara | Engine\Plugins\FX\Niagara\Niagara.uplugin | True | False | None | False | 8 | 55116800 | present | present |
| EditorScriptingUtilities | Engine\Plugins\Editor\EditorScriptingUtilities\EditorScriptingUtilities.uplugin | False | False | None | True | 1 | 55116800 | present | present |
| PythonScriptPlugin | Engine\Plugins\Experimental\PythonScriptPlugin\PythonScriptPlugin.uplugin | False | False | None | True | 2 | 55116800 | present | present |
| EnhancedInput | Engine\Plugins\EnhancedInput\EnhancedInput.uplugin | True | False | None | False | 3 | 55116800 | present | present |

All seven descriptors are present, every module DLL and import library resolves, and every plugin `BuildId` equals the engine `BuildId`. Five default to disabled (`EnabledByDefault` false): the three Experimental plugins, `EditorScriptingUtilities` and `PythonScriptPlugin`. `Niagara` and `EnhancedInput` default to enabled.

## Build.cs modules

All 35 modules named by `TurboMCP.Build.cs` were resolved in the distribution:

| # | Module | DLL | Import library |
| --- | --- | --- | --- |
| 1 | Core | present | present |
| 2 | CoreUObject | present | present |
| 3 | Engine | present | present |
| 4 | AssetRegistry | present | present |
| 5 | BlueprintEditorLibrary | present | present |
| 6 | BlueprintGraph | present | present |
| 7 | KismetCompiler | present | present |
| 8 | DeveloperSettings | present | present |
| 9 | EditorFramework | present | present |
| 10 | EditorScriptingUtilities | present | present |
| 11 | EditorSubsystem | present | present |
| 12 | GraphEditor | present | present |
| 13 | Json | present | present |
| 14 | JsonUtilities | present | present |
| 15 | Kismet | present | present |
| 16 | InputCore | present | present |
| 17 | Slate | present | present |
| 18 | SlateCore | present | present |
| 19 | ToolMenus | present | present |
| 20 | UnrealEd | present | present |
| 21 | ModelContextProtocol | present | present |
| 22 | ModelContextProtocolEngine | present | present |
| 23 | ToolsetRegistry | present | present |
| 24 | ImageWrapper | present | present |
| 25 | RenderCore | present | present |
| 26 | UMG | present | present |
| 27 | UMGEditor | present | present |
| 28 | MaterialEditor | present | present |
| 29 | Projects | present | present |
| 30 | AnalyticsET | present | present |
| 31 | SlateInspectorToolset | present | present |
| 32 | AnimGraph | present | present |
| 33 | Niagara | present | present |
| 34 | NiagaraEditor | present | present |
| 35 | PythonScriptPlugin | present | present |

## Public headers

| Surface | Header | Resolved |
| --- | --- | --- |
| registry registration | Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/UToolsetRegistry.h | yes |
| recorder wrapper | Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/Toolset.h | yes |
| recorder wrapper | ToolsetRegistry.h | yes |
| MCP analytics interception | Engine/Plugins/Experimental/ModelContextProtocol/Source/ModelContextProtocol/Public/IModelContextProtocolModule.h | yes |
| MCP analytics interception | ModelContextProtocolAnalytics.h | yes |
| SlateInspector screenshot | Engine/Plugins/Experimental/Toolsets/SlateInspectorToolset/Source/SlateInspectorToolset/Public/SlateInspectorToolset.h | yes |
| editor save | Engine/Plugins/Editor/EditorScriptingUtilities/Source/EditorScriptingUtilities/Public/EditorAssetLibrary.h | yes |
| source-control interfaces | Developer/SourceControl/Public/ISourceControlModule.h | yes |
| analytics event attribute | Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h | yes |
| render-target readback and RHI closure | Engine/Source/Runtime/Engine/Public/UnrealClient.h | yes |
| render-target readback and RHI closure | Engine/Source/Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h | yes |

## Exported symbols

`dumpbin /exports` on the owning DLL. A header declaration is not evidence; the matching decorated export is recorded in the JSON record.

| Surface | Symbol | Owning module | Exported |
| --- | --- | --- | --- |
| registry registration | `UToolsetRegistry::RegisterToolsetClass` | ToolsetRegistry | yes |
| registry registration | `UToolsetRegistry::IsToolsetClassRegistered` | ToolsetRegistry | yes |
| registry registration | `UToolsetRegistry::GetToolsetJsonSchema` | ToolsetRegistry | yes |
| recorder wrapper | `FToolset::ExecuteTool` | ToolsetRegistry | yes |
| recorder wrapper | `FToolset::GetJsonSchema` | ToolsetRegistry | yes |
| recorder wrapper | `FToolsetRegistry::Find` | ToolsetRegistry | yes |
| recorder wrapper | `FToolsetRegistry::RegisterToolset` | ToolsetRegistry | yes |
| recorder wrapper | `FToolsetRegistry::ForEachToolset` | ToolsetRegistry | yes |
| MCP analytics interception | `IModelContextProtocolModule::Get` | ModelContextProtocol | yes |
| MCP analytics interception | `ParseToolsetName` | ModelContextProtocol | yes |
| MCP analytics interception | `HashToolIdentifier` | ModelContextProtocol | yes |
| SlateInspector screenshot | `USlateInspectorToolset::Screenshot` | SlateInspectorToolset | yes |
| editor save | `UEditorAssetLibrary::SaveLoadedAsset` | EditorScriptingUtilities | yes |
| source-control interfaces | `ISourceControlModule::Get (inline accessor)` | Core | yes |
| source-control interfaces | `ISourceControlProvider` | SourceControl | yes |
| analytics event attribute | `FAnalyticsEventAttribute` | Analytics | yes |
| render-target readback | `FRenderTarget::ReadPixels` | Engine | yes |
| render-target readback | `UTextureRenderTarget2D` | Engine | yes |

The class `ISourceControlModule` has no export of its own because its accessor is inline: `ISourceControlModule.h:394` defines `static inline ISourceControlModule& Get()` through `FModuleManager::LoadModuleChecked`. The row above therefore checks the mechanism the inline body calls, `FModuleManager::LoadModule` in Core, which is exported, and `ISourceControlProvider`, which is also exported. This is a mechanism fact, not a gap.

## Toolchain

| Requirement | Launcher value | Local value |
| --- | --- | --- |
| Windows SDK | 10.0.22621.0 | 10.0.22621.0 |
| Preferred MSVC | 14.50.35717-14.50.99999 | 14.50.35728 |
| Banned MSVC | 14.50.0-14.50.35722 | not in a banned range |

## Trial build

| Field | Value |
| --- | --- |
| Exit code | 0 |
| Actions | 253 |
| Plugin actions | 248 |
| Actions outside the plugin | 5 |
| Engine modules compiled | none |
| Package .modules BuildId | 55116800 |
| DLL | C:\Dev\_b\.local\launcher-audit\package\Binaries\Win64\UnrealEditor-TurboMCP.dll |
| DLL SHA-256 | 19129c0866c307ed8e3ce233bc6e26ad5004e5820b012b22a840e43c25899db5 |
| Log | C:\Dev\_b\.local\launcher-audit\buildplugin.log |

The first attempt exited 1 because the command line split `-Plugin=` from its value; the process was rerun correctly, and that failed attempt's log was overwritten and not kept. No code or environment fault was involved.

The 5 actions outside the plugin name are the module's own generated inputs, not engine modules: `Compile Resource [x64] Default.rc2`, `Compile [x64] SharedPCH.UnrealEd.Cpp20.cpp`, `Compile [x64] Logging.cpp`, `Compile [x64] PerModuleInline.gen.cpp`. No engine module compiled, so the build reused the distribution's prebuilt binaries.

Nothing under the engine folder changed during the build: no file under `Engine/Intermediate`, `Engine/Binaries` or `Engine/Programs/UnrealBuildTool` has a modification time inside the build window.

The package `.modules` BuildId is `55116800`, the engine BuildId, because a Launcher build ships one fixed BuildId. A DLL built against this distribution therefore matches every installation of the same Launcher build, unlike the local source build whose BuildId is generated per machine.

## What this does not prove

- It does not certify runtime behavior, clean-consumer installation or Launcher support.
- It does not authorize module extraction; that remains a separate, reviewed change.
- It inspects only what the installed distribution contains and what the trial build compiled.

