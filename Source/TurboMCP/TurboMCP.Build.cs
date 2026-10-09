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
				// UBlueprintEditorLibrary::ListEvents and ListFunctions. The engine
				// already computes the set of overridable functions and events,
				// interface members included, and R-OVERRIDES uses it rather than
				// reproducing the rule.
				"BlueprintEditorLibrary",
				"BlueprintGraph",
				// UEdGraphNode and its pins live in Engine, which is a public
				// dependency above, so it is not repeated here.
				//
				// UK2Node_CustomEvent and UK2Node_CallFunction, needed for the node
				// type ID rule and for matching a node against search words. Kismet
				// is also listed further down for the compiler API, so this entry
				// carries only the node-class reason.
				"KismetCompiler",
				// UDeveloperSettings, the base of the engine's MCP settings class
				// read by ReadTurboMCPStatus.
				"DeveloperSettings",
				"EditorFramework",
				// UEditorAssetLibrary::SaveLoadedAsset, the same save path the
				// engine's own editor toolset uses. FEditorFileUtils tries to
				// check out from source control and fails headless, which made
				// E-SAVE report failure while nothing was written.
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"GraphEditor",
				"Json",
				"JsonUtilities",
				"Kismet",

				// INPUTCORE IS REQUIRED BY THE PANEL'S LIST AND COMBO WIDGETS, AND ITS ABSENCE IS A
				// LINK ERROR RATHER THAN A COMPILE ERROR - which is why it survived every earlier
				// build. `SListView` and `SComboBox` read `EKeys::Enter`, `EKeys::BackSpace`,
				// `EKeys::LeftMouseButton` and eight more, and `EKeys` lives in
				// `InputCore/Public/InputCoreTypes.h:345`. `Slate` and `SlateCore` were already
				// listed and are NOT enough: twelve `LNK2019: unresolved external symbol` errors named
				// the keys, which reads like a Slate problem and is not one.
				"InputCore",
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

				// L-WIDGET-PREVIEW: FWidgetRenderer draws a widget into a render
				// target off screen, and UUserWidget is what gets instantiated.
				// UMGEditor carries UBaseWidgetBlueprint, which is where a Widget
				// Blueprint's WidgetTree lives.
				"UMG",
				"UMGEditor",

				// The material readers call `UMaterialEditingLibrary` and the expression classes, so
				// the editor module that owns them must link. Its header is INCLUDED as
				// `MaterialEditingLibrary.h` and NOT as `MaterialEditor/MaterialEditingLibrary.h`,
				// because it sits at that module's Public ROOT - checked, and the slash form failed
				// the build with "No such file or directory".
				"MaterialEditor",

				// The call log installs an `IAnalyticsProviderET` over the MCP module's own
				// analytics hook, so the ET provider interface, its event attribute type and the
				// BLAKE3 hash the engine's tool identifier uses must all link.
				"AnalyticsET",

				// L-CAPTURE falls back to the engine's own screenshot when the graph
				// panel has no geometry to frame with. Its doc comment says to prefer it
				// over SceneTools.take_screenshot for editor UI, and calling it beats
				// writing a second capture path that could disagree with it.
				"SlateInspectorToolset",
			});
	}
}
