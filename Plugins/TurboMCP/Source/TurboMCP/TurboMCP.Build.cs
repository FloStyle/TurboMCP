// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
// Editor-only module. Dependencies must also be enabled in the plugin descriptor.
using UnrealBuildTool;

public class TurboMCP : ModuleRules
{
	public TurboMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// Keep private helpers isolated; unity grouping otherwise creates name collisions.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
                "SourceControl",
				"BlueprintEditorLibrary",
				"BlueprintGraph",
				"KismetCompiler",
				"DeveloperSettings",
                "Settings",
				"EditorFramework",
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"GraphEditor",
				"Json",
				"JsonUtilities",
				"Kismet",

				"InputCore",
                "EnhancedInput",
                "InputBlueprintNodes",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",

				"ModelContextProtocol",
				"ModelContextProtocolEngine",
				"ToolsetRegistry",

				"ImageWrapper",
				"RenderCore",
				"RHI",

				"UMG",
				"UMGEditor",

				"MaterialEditor",
				"Projects",

				"AnalyticsET",

				"SlateInspectorToolset",

				"AnimGraph",
				"Niagara",
				"NiagaraEditor",

				"PythonScriptPlugin",
			});
	}
}
