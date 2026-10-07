// Copyright FloStyle. All Rights Reserved.

using UnrealBuildTool;

/// Build rules for the TurboMCP editor module.
///
/// The module is Editor-only because it registers toolsets with the engine's
/// ToolsetRegistry and calls UnrealEd directly. `01_BRIEF.md` requires that
/// nothing here ships in a game build, so the Editor target is the only one
/// allowed to compile it.
public class TurboMCP : ModuleRules
{
	public TurboMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public because the module's own headers include them: TurboMCPModule.h
		// uses Core containers, and the toolset headers derive from UObject.
		// A dependency listed here is also visible to any module added later,
		// which keeps a second module from having to repeat these.
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
				// Editor-only work: asset registry, Blueprint graphs, Slate panel.
				"AssetRegistry",
				"BlueprintGraph",
				// UDeveloperSettings, the base of the engine's MCP settings class
				// read by ReadTurboMCPStatus.
				"DeveloperSettings",
				"EditorFramework",
				"EditorSubsystem",
				"GraphEditor",
				"Json",
				"JsonUtilities",
				"Kismet",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",

				// The engine's own MCP server and tool registry. Both are
				// experimental engine plugins, enabled in TurboMCP.uplugin.
				// `07_ENGINE_NOTES.md` section 1.2: a module dependency needs
				// the entry in Build.cs AND in the .uplugin.
				"ModelContextProtocol",
				"ModelContextProtocolEngine",
				"ToolsetRegistry",

				// Used by the panel to draw and by capture tools later.
				"ImageWrapper",
				"RenderCore",
			});
	}
}
