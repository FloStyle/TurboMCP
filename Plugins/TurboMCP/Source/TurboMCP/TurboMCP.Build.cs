// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
				// IPluginManager resolves shipped data from the plugin that owns this
				// module, so a renamed mount directory cannot hide EngineToolTraps.json.
				"Projects",

				// The call log installs an `IAnalyticsProviderET` over the MCP module's own
				// analytics hook, so the ET provider interface, its event attribute type and the
				// BLAKE3 hash the engine's tool identifier uses must all link.
				"AnalyticsET",

				// L-CAPTURE falls back to the engine's own screenshot when the graph
				// panel has no geometry to frame with. Its doc comment says to prefer it
				// over SceneTools.take_screenshot for editor UI, and calling it beats
				// writing a second capture path that could disagree with it.
				"SlateInspectorToolset",

				// *** THE TWO DEPENDENCIES THE KIT REFUSED `TA-18` AND `TA-19` FOR, ADDED BY THE OWNER'S DECISION. ***
				//
				// `Tools/make-kit-assets.ps1` COULD NOT BUILD EITHER FIXTURE, AND IT SAID SO BY NAME RATHER THAN
				// BUILDING SOMETHING WRONG. Its refusal named these exact modules, and the workstream that wrote it
				// stopped there because `TurboMCP.Build.cs` was outside its boundaries and the brief told it to ask.
				//
				// `TA-18` - an Animation Blueprint with a state machine - needs `AnimGraph`: the state machine is
				// authored with `UAnimGraphNode_StateMachine` and `UAnimStateTransitionNode`, both under
				// `Engine/Source/Editor/AnimGraph/Public/`, and this module could not name either type.
				// *** AND IT NEEDS A SECOND THING THAT A DEPENDENCY CANNOT SUPPLY, WHICH IS RECORDED HERE SO THE NEXT
				// READER DOES NOT EXPECT THIS LINE TO BE ENOUGH: "3 keys on ONE BONE" needs a `USkeleton` bone, and
				// every route is closed - `Animation/Skeleton.h:365-369` returns the reference skeleton CONST ON
				// PURPOSE ("to make data read only"), `ReferenceSkeleton` is protected at :301/:317, the bone-adding
				// engine functions at :797/:809/:820 all take a `USkinnedAsset` and this project holds no skeletal
				// mesh, and the last route is `FEditableSkeleton`, a PRIVATE header of a module that is not a
				// dependency. SO `TA-18` IS HALF UNBLOCKED BY THIS LINE AND STILL REFUSED BY NAMED. ***
				//
				// `TA-19` - a Niagara system with one emitter - needs `Niagara` for `UNiagaraSystem`
				// (`Niagara/Classes/NiagaraSystem.h:237`), and `NiagaraEditor` to AUTHOR one
				// (`NiagaraEditor/Public/NiagaraSystemFactoryNew.h:16`). *** AND THE PLUGIN ITSELF WAS NEVER THE
				// PROBLEM: `Engine/Plugins/FX/Niagara/Niagara.uplugin:13` IS `"EnabledByDefault" : true`, so the
				// gap was always the module dependency - which is the distinction `WORKING_PLAN.md:89` records and
				// `TurboMCPCaptureAsset.cpp:242-250` had to correct in the other direction. ***
				"AnimGraph",
				"Niagara",
				"NiagaraEditor",

				// `IPythonScriptPlugin::ExecPythonCommand`, USED BY THE TEST-MAP COMMANDLET FOR ONE REASON.
				//
				// The PIE test map needs a BLUEPRINT actor, because only the Blueprint VM raises
				// `FBlueprintCoreDelegates::OnScriptException` and therefore only a Blueprint node can make
				// `E-TRACE` fire. The only graph writer this project trusts is the engine's own
				// `BlueprintTools.write_graph_dsl`, which is a PYTHON tool - so the commandlet asks the Python
				// plugin to run a script rather than hand-building a graph, which would have been a second
				// graph writer that could disagree with the readers.
				//
				// It is already enabled in `TurboMCP.uplugin` and in the test project's `.uproject`, because
				// `07_ENGINE_NOTES.md` section 1.2 requires a module dependency to appear in BOTH the Build.cs
				// and the descriptor.
				"PythonScriptPlugin",
			});
	}
}
