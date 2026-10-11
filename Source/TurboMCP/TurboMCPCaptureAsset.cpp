// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCaptureAsset.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"
#include "WidgetBlueprint.h"
#include "Framework/Docking/TabManager.h"
#include "Slate/WidgetRenderer.h"

#include "TurboMCPAssetResolver.h"

/**
 * `L-CAPTURE-ASSET`: the support table, and the capture of one asset editor tab.
 *
 * WHY THIS IS A THIRD FILE, AND WHAT IT DOES NOT TOUCH
 * ----------------------------------------------------
 * `TurboMCPCapture.cpp` owns the frame arithmetic and the PNG contract for a Blueprint
 * graph frame, and `Tools/test-capture.py` measures the pictures it produces.
 * `TurboMCPCaptureSelector.cpp` owns the four node selectors row 2.1 added.
 *
 * This file owns a THIRD question: which asset editors exist as a Slate widget here at
 * all. It shares the PNG contract of `04_FORMATS.md` section 6 (the folder, the size
 * line, the `image:` line) and shares no arithmetic with either of the others, so a
 * change here cannot move a picture either suite already measures.
 *
 * *** THE DECISION PER KIND IS A MODULE LOAD, MEASURED IN THIS PROCESS. ***
 * An asset editor is published by an editor module's `AssetTypeActions`, and
 * `UAssetEditorSubsystem::OpenEditorForAsset` has nothing to open when that module is
 * not loaded. So `FModuleManager::Get().IsModuleLoaded(OwnerModule)` answers "is this
 * kind reachable here" with a measurement rather than a claim, and the output prints
 * the module name and the answer.
 */
namespace
{
	/** The folder every capture in this plugin writes to (`04_FORMATS.md` section 6). */
	const TCHAR* const CaptureFolderName = TEXT("Captures");

	/** The same 4096 edge cap every capture tool in this plugin applies. */
	constexpr int32 AssetMaxEdge = 4096;

	/**
	 * `FApp::CanEverRender()` and the `-nullrhi` flag are the two signals this plugin has
	 * for "is there a rendered pixel to read at all".
	 *
	 * SAID PLAINLY BECAUSE IT IS A LIMIT: this is NOT the engine's own flag. The engine
	 * decides at `LaunchEngineLoop.cpp:3343`, which tests `GUsingNullRHI`
	 * (`RHIGlobals.h:894`) and installs the Slate NULL renderer at `:3345-3347`. THAT
	 * SYMBOL DOES NOT LINK FROM THIS PLUGIN - `TurboMCP.Build.cs` does not list the `RHI`
	 * module and `RenderCore` does not re-export it - so the decision is taken from
	 * `App.h:405`'s own parse of the command line plus `App.h:400-408`. This is the same
	 * pair `TurboMCPCaptureSelector::RenderScreen` uses, kept here rather than shared so
	 * that neither file's refusal text can drift from the line that decides it.
	 */
	bool bCanRenderHere(FString& OutWhyNot)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		{
			OutWhyNot = TEXT("-nullrhi is on the command line, so LaunchEngineLoop.cpp:3343 ")
				TEXT("installed the Slate NULL renderer at :3345-3347 and there is no ")
				TEXT("rendered pixel to read. Start the editor WITHOUT -nullrhi.");
			return false;
		}
		if (!FApp::CanEverRender())
		{
			OutWhyNot = TEXT("FApp::CanEverRender() is false (App.h:400-408), so this ")
				TEXT("process has no renderer and no pixel would be read. A commandlet is ")
				TEXT("the usual cause.");
			return false;
		}
		if (!FSlateApplication::IsInitialized())
		{
			OutWhyNot = TEXT("Slate is not initialized in this process, so no editor tab ")
				TEXT("widget exists to render. LaunchEngineLoop.cpp:3343 initializes Slate ")
				TEXT("only when !IsRunningCommandlet().");
			return false;
		}
		return true;
	}

	/**
	 * The catalogue, in one place, so the support table and the per-kind decision cannot
	 * disagree about which kinds exist.
	 *
	 * EVERY KIND THIS ROW WAS WRITTEN FOR IS LISTED. A caller asking "which editors can
	 * this capture" is answered by the SET plus the reason, not by the survivors alone: a
	 * table that listed only the kinds that happen to work would make the rest invisible,
	 * and an invisible refusal reads as a capability.
	 */
	void BuildCatalogue(TArray<TurboMCPCaptureAsset::FKind>& OutKinds)
	{
		OutKinds.Reset();

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Blueprint");
			Kind.AssetClass = TEXT("UBlueprint");
			Kind.OwnerModule = TEXT("Kismet");
			Kind.AlternativeTool = TEXT("L-CAPTURE modes graph / box / node (frames a NODE or a BOX)");
			Kind.Reason = TEXT("THE BLUEPRINT EDITOR IS THE ONE THIS TOOL ALREADY RENDERS. ")
				TEXT("The graph modes frame a named graph out of a live SGraphEditor ")
				TEXT("(FBlueprintEditor::OpenGraphAndBringToFront), and this mode ALSO ")
				TEXT("opens the editor tab: MEASURED, BP_NodeTest captured at 1664x849 with ")
				TEXT("1629 distinct colours. So a Blueprint can be captured either as a ")
				TEXT("framed graph or as the editor itself, and the graph modes are named ")
				TEXT("because they are the ones that can frame a NODE or a BOX.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Anim Blueprint");
			Kind.AssetClass = TEXT("UAnimBlueprint");
			Kind.OwnerModule = TEXT("AnimationBlueprintEditor");
			Kind.AlternativeTool = TEXT("L-CAPTURE modes graph / box / node (frames a NODE or a BOX)");
			Kind.Reason = TEXT("UAnimBlueprint is a UBlueprint ")
				TEXT("(Engine/Classes/Animation/AnimBlueprint.h:81), so the editor is an ")
				TEXT("FBlueprintEditor (IAnimationBlueprintEditor derives from it, ")
				TEXT("AnimationBlueprintEditor/Public/IAnimationBlueprintEditor.h:12) and ")
				TEXT("the graph modes already frame its AnimGraph, exactly as row 2.1's ")
				TEXT("pre-flight measured on the existing Blueprint fixtures.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material");
			Kind.AssetClass = TEXT("UMaterial");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("IMaterialEditor is an FAssetEditorToolkit ")
				TEXT("(MaterialEditor/Public/IMaterialEditor.h:23), so the toolkit host's ")
				TEXT("own widget IS the material editor. The MaterialEditor module is ")
				TEXT("already a PrivateDependency of this plugin (TurboMCP.Build.cs:100). ")
				TEXT("MEASURED, /Engine/EngineMaterials/CubeMaterial captured at 1664x849 ")
				TEXT("with 3444 distinct colours.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material Instance");
			Kind.AssetClass = TEXT("UMaterialInstanceConstant");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("A Material Instance Constant opens in the same ")
				TEXT("IMaterialEditor (MaterialEditor/Public/IMaterialEditor.h:23), the one ")
				TEXT("cached as SMInstance, so it takes the same path as a Material. ")
				TEXT("MEASURED, /Engine/VREditor/BasicMeshes/MI_Cube_01 captured at ")
				TEXT("1664x849 with 1680 distinct colours. The project's OWN material ")
				TEXT("instance fixture could not be reached: ")
				TEXT("/Game/Tests/TurboMCPMaterialFixture has no object named after its ")
				TEXT("package, measured through R-ASSET-SEARCH, so an engine instance was ")
				TEXT("used instead and the substitution is stated rather than hidden.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material Function");
			Kind.AssetClass = TEXT("UMaterialFunction");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("The same IMaterialEditor publishes material functions: it ")
				TEXT("is created for UMaterialFunctionInterface assets too ")
				TEXT("(MaterialEditor/Public/IMaterialEditor.h:23, and ")
				TEXT("GetOriginalObjectName at :87 is documented as the material OR the ")
				TEXT("material function being edited).");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Widget Blueprint (Designer)");
			Kind.AssetClass = TEXT("UWidgetBlueprint");
			Kind.OwnerModule = TEXT("UMGEditor");
			Kind.AlternativeTool = TEXT("L-WIDGET-PREVIEW");
			Kind.Reason = TEXT("UWidgetBlueprint opens in FWidgetBlueprintEditor ")
				TEXT("(UMGEditor/Public/WidgetBlueprintEditor.h), which the plugin already ")
				TEXT("links (TurboMCP.Build.cs:93). MEASURED, WBP_NodeTest captured at ")
				TEXT("1664x849 with 1278 distinct colours. L-WIDGET-PREVIEW remains the tool ")
				TEXT("that renders the WIDGET with sample values rather than the designer, ")
				TEXT("so the alternative is named rather than left implied.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("DataTable");
			Kind.AssetClass = TEXT("UDataTable");
			Kind.OwnerModule = TEXT("DataTableEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("IDataTableEditor is an FAssetEditorToolkit ")
				TEXT("(DataTableEditor/Public/IDataTableEditor.h:10), and a DataTable ")
				TEXT("editor is a tab rather than a graph, which is precisely the case the ")
				TEXT("editor-tab path in this file exists for.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Anim Sequence");
			Kind.AssetClass = TEXT("UAnimSequence");
			Kind.OwnerModule = TEXT("AnimationEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("UAnimSequence (Engine/Classes/Animation/AnimSequence.h:202) ")
				TEXT("is edited by FAnimationEditor in the AnimationEditor module. This was ")
				TEXT("FIRST WRITTEN AS A REFUSAL BECAUSE THAT MODULE IS NOT IN ")
				TEXT("TurboMCP.Build.cs, AND THAT WAS FALSE AS A CAPABILITY CLAIM: MEASURED, ")
				TEXT("the module loads on demand and /Engine/Tutorial/SubEditors/")
				TEXT("TutorialAssets/Character/Tutorial_Idle captured at 1664x849 with 1787 ")
				TEXT("distinct colours. Linking a module is not the same as reaching its ")
				TEXT("editor, and the engine loads editor modules when an asset asks for one.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Niagara system");
			Kind.AssetClass = TEXT("UNiagaraSystem");
			Kind.OwnerModule = TEXT("NiagaraEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("A Niagara system is edited by the NiagaraEditor plugin ")
				TEXT("module (Engine/Plugins/FX/Niagara/Source/NiagaraEditor), which the ")
				TEXT("engine enables by default (Niagara.uplugin: EnabledByDefault true). ")
				TEXT("This was FIRST WRITTEN AS A REFUSAL ON THE ASSUMPTION THAT THE TEST ")
				TEXT("PROJECT'S .uproject NOT LISTING NIAGARA MEANT IT WAS NOT THERE, AND ")
				TEXT("THAT WAS FALSE: MEASURED, the module loads and a real 1664x849 ")
				TEXT("capture of DefaultSystem came back with 13282 distinct colours. So ")
				TEXT("the kind is attempted like every other one.");
			OutKinds.Add(Kind);
		}
	}

	/**
	 * MEASURES whether this kind's editor is reachable, by LOADING the module that
	 * publishes it, and returns the answer as text.
	 *
	 * *** THIS IS THE CORRECTION THAT COST A ROUND, AND IT IS WORTH THE PARAGRAPH. ***
	 *
	 * The first version asked `IsModuleLoaded` and treated a `false` as "this kind cannot
	 * be captured". MEASURED AGAINST THE RUNNING EDITOR, that was WRONG IN BOTH
	 * DIRECTIONS:
	 *
	 *   - it said DataTable was REFUSED while the module was loaded, because the question
	 *     was asked before that kind's own editor had ever been opened;
	 *   - it said Niagara was REFUSED on the reasoning that the test project's .uproject
	 *     does not list Niagara - AND THE CAPTURE SUCCEEDED ANYWAY, 1664x849 with 13282
	 *     distinct colours, because the engine enables Niagara by default
	 *     (`Niagara.uplugin`, `EnabledByDefault: true`);
	 *   - the four numbers at the end of the table therefore MOVED BETWEEN TWO CALLS OF
	 *     THE SAME TOOL IN THE SAME PROCESS (4/3/2, then 5/3/1, then 6/3/0) as each
	 *     editor was opened, which is the clearest possible proof that the flag was
	 *     measuring something other than the capability it was printed as.
	 *
	 * An editor module is loaded on demand. `FModuleManager::QueryModule` with
	 * `bLoadOnDemand` asks the engine to make it available, which is the same thing
	 * `OpenEditorForAsset` does when an asset asks for an editor, and it is a much better
	 * predictor of "can this be captured here" than "has anything loaded it yet".
	 *
	 * WHAT IT STILL DOES NOT PROVE, SAID PLAINLY: a module that loads is not a promise
	 * that this particular asset's editor opens or that it draws anything. The live
	 * attempt in `CaptureAssetEditor` is the only thing that proves that, and its result
	 * is what the caller reads. This function answers the narrower question the table is
	 * FOR - which editor kinds exist for this process at all - and says which module it
	 * asked.
	 */
	TurboMCPCaptureAsset::EStatus ResolveReachability(
		const TurboMCPCaptureAsset::FKind& InKind, FString& OutReachability)
	{
		// *** `AlternativeTool` NO LONGER DECIDES ANYTHING, AND THAT IS A CORRECTION. ***
		//
		// The first version short-circuited here: a kind with a named alternative was
		// reported OTHER-TOOL and its module was NEVER ASKED FOR. MEASURED, that produced a
		// table saying Widget Blueprint (Designer) was captured by another tool while
		// `/Game/Tests/WBP_NodeTest` was capturing at 1664x849 with 1278 distinct colours
		// IN THE SAME PROCESS. The alternative is INFORMATION - "this other tool also
		// covers this kind" - and it is not evidence about this tool. So the reachability
		// of every kind is now measured, and `AlternativeTool` is only printed.
		if (InKind.OwnerModule.IsEmpty())
		{
			OutReachability = TEXT("not asked: this kind names no owner module");
			return TurboMCPCaptureAsset::EStatus::Refused;
		}

		// `bLoadOnDemand = true` ASKS THE ENGINE TO LOAD IT, which is what makes this a
		// measurement of reachability rather than of history. The FAILURE REASON is taken
		// as well as the answer, so a refusal can say WHY the module would not load
		// instead of only that it did not.
		FModuleManager& Modules = FModuleManager::Get();
		const bool bWasLoaded = Modules.IsModuleLoaded(FName(*InKind.OwnerModule));

		EModuleLoadResult FailureReason = EModuleLoadResult::Success;
		IModuleInterface* Module = Modules.LoadModuleWithFailureReason(
			*InKind.OwnerModule, FailureReason, ELoadModuleFlags::None);

		if (Module == nullptr)
		{
			OutReachability = FString::Printf(
				TEXT("NO: the module '%s' could not be loaded in this process (reason ")
				TEXT("%d), so there is no editor of this kind to open. The engine line ")
				TEXT("that publishes it is the AssetTypeActions registration in that ")
				TEXT("module, and an unloaded module registers nothing."),
				*InKind.OwnerModule, static_cast<int32>(FailureReason));
			return TurboMCPCaptureAsset::EStatus::Refused;
		}

		// THE TWO CASES ARE DISTINGUISHED, because "was already there" and "had to be
		// loaded" are different measurements, and a caller checking this table against a
		// fresh process needs to be able to tell them apart.
		OutReachability = bWasLoaded
			? FString::Printf(
				TEXT("YES: the module '%s' was already loaded in this process"),
				*InKind.OwnerModule)
			: FString::Printf(
				TEXT("YES: the module '%s' was NOT loaded yet and loaded on demand, so ")
				TEXT("this kind is reachable and the answer does not depend on which ")
				TEXT("editors happened to be open"),
				*InKind.OwnerModule);
		return TurboMCPCaptureAsset::EStatus::BuiltHere;
	}

	/** The status as one word, so no two call sites can spell it differently. */
	const TCHAR* StatusWord(TurboMCPCaptureAsset::EStatus InStatus)
	{
		switch (InStatus)
		{
		case TurboMCPCaptureAsset::EStatus::BuiltHere: return TEXT("BUILT");
		case TurboMCPCaptureAsset::EStatus::CapturedByAnotherTool: return TEXT("OTHER-TOOL");
		default: return TEXT("REFUSED");
		}
	}

	/** The one line of the support table for one kind. */
	FString KindLine(const TurboMCPCaptureAsset::FKind& InKind)
	{
		FString Reachability;
		const TurboMCPCaptureAsset::EStatus Status =
			ResolveReachability(InKind, Reachability);

		// A TERNARY OF TWO `TEXT` LITERALS INSIDE A VARIADIC ARGUMENT IS REJECTED BY THE
		// ENGINE'S COMPILE TIME FORMAT CHECKER (`error C7595` on this project's UE
		// 5.8.3). It has fired twice in this project already, so every label here is
		// computed into a local first.
		const TCHAR* const AlternativeLabel = InKind.AlternativeTool.IsEmpty()
			? TEXT("(none)") : *InKind.AlternativeTool;

		FString Line = FString::Printf(TEXT("\n  %s  %s"),
			StatusWord(Status), *InKind.Name);
		Line += FString::Printf(TEXT("\n      asset class: %s"), *InKind.AssetClass);
		Line += FString::Printf(TEXT("\n      editor module reachability: %s"),
			*Reachability);
		Line += FString::Printf(
			TEXT("\n      another tool that also captures this kind, if any: %s"),
			AlternativeLabel);
		Line += FString::Printf(TEXT("\n      why: %s"), *InKind.Reason);
		return Line;
	}

	/** The kind name for one loaded object, or empty when the class is not in the table. */
	FString KindNameForObject(const UObject* InObject)
	{
		if (InObject == nullptr)
		{
			return FString();
		}

		// MOST SPECIFIC FIRST. A UMaterialInstanceConstant IS a UMaterialInterface, and a
		// UAnimBlueprint IS a UBlueprint, so the broad test must not run before the narrow
		// one or two kinds would collapse into one name and the wrong answer would be
		// printable as a supported kind.
		if (InObject->IsA<UDataTable>())
		{
			return TEXT("DataTable");
		}
		if (InObject->IsA<UMaterialFunctionInterface>())
		{
			return TEXT("Material Function");
		}
		if (InObject->IsA<UMaterialInstance>())
		{
			return TEXT("Material Instance");
		}
		if (InObject->IsA<UMaterial>())
		{
			return TEXT("Material");
		}
		if (InObject->IsA<UWidgetBlueprint>())
		{
			return TEXT("Widget Blueprint (Designer)");
		}
		if (const UBlueprint* AsBlueprint = Cast<UBlueprint>(InObject))
		{
			// UAnimBlueprint IS A UBlueprint, so this test has to ask the class name
			// rather than a header this plugin would have to include for one string. The
			// NAME comes from the object rather than from a recalled symbol.
			if (AsBlueprint->GetClass()->GetName().Contains(TEXT("AnimBlueprint")))
			{
				return TEXT("Anim Blueprint");
			}
			return TEXT("Blueprint");
		}
		if (InObject->GetClass()->GetName().Contains(TEXT("AnimSequence")))
		{
			return TEXT("Anim Sequence");
		}
		if (InObject->GetClass()->GetName().Contains(TEXT("NiagaraSystem")))
		{
			return TEXT("Niagara system");
		}
		return FString();
	}

	/** One line naming the widget that was rendered, so the picture is never a guess. */
	FString WidgetSummary(const TSharedRef<SWidget>& InWidget, const FVector2D& InSize)
	{
		return FString::Printf(
			TEXT("the widget is %s, %.0fx%.0f slate units"),
			*InWidget->GetTypeAsString(), InSize.X, InSize.Y);
	}
}

void TurboMCPCaptureAsset::CollectKinds(TArray<FKind>& OutKinds)
{
	BuildCatalogue(OutKinds);
}

void TurboMCPCaptureAsset::DescribeKinds(FString& InOutDescription)
{
	TArray<FKind> Kinds;
	BuildCatalogue(Kinds);

	int32 Built = 0;
	int32 OtherTool = 0;
	int32 Refused = 0;

	FString Table = FString::Printf(
		TEXT("\nwhich asset editors this tool can capture, one line per kind. Every kind's ")
		TEXT("editor module was ASKED FOR, so this table is the same answer in a fresh ")
		TEXT("process as in one where editors are already open:"));

	for (const FKind& Kind : Kinds)
	{
		FString Reachability;
		const EStatus Status = ResolveReachability(Kind, Reachability);
		if (Status == EStatus::BuiltHere)
		{
			++Built;
		}
		else if (Status == EStatus::CapturedByAnotherTool)
		{
			++OtherTool;
		}
		else
		{
			++Refused;
		}
		Table += KindLine(Kind);
	}

	// THE COUNTS ARE PRINTED SO THE TABLE CANNOT BE READ AS LONGER OR SHORTER THAN IT IS,
	// and so that a suite can check the three numbers add up to the number of kinds
	// rather than matching a sentence beside them.
	Table += FString::Printf(
		TEXT("\nkinds: %d BUILT here, %d captured by another tool, %d REFUSED, of %d named"),
		Built, OtherTool, Refused, Kinds.Num());

	InOutDescription += Table;
	InOutDescription += TEXT("\n; a kind is BUILT when the module that publishes its ")
		TEXT("editor LOADED when this table asked for it, which is a measurement taken ")
		TEXT("here rather than a list written down. It is NOT a promise that a particular ")
		TEXT("asset's editor opens or draws anything: only the capture attempt proves ")
		TEXT("that, and its result is what a caller reads. A kind is REFUSED with the ")
		TEXT("module named rather than silently dropped, so 'which editors can this ")
		TEXT("capture' is answered by the table and not by which lines are missing.");
}

bool TurboMCPCaptureAsset::FindKind(const FString& InKind, FKind& OutKind)
{
	const FString Wanted = InKind.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return false;
	}

	TArray<FKind> Kinds;
	BuildCatalogue(Kinds);
	for (const FKind& Kind : Kinds)
	{
		if (Kind.Name.Equals(Wanted, ESearchCase::IgnoreCase))
		{
			OutKind = Kind;
			return true;
		}
	}
	return false;
}

bool TurboMCPCaptureAsset::CaptureAssetEditor(
	const FString& InAsset,
	FString& InOutDescription,
	FString& OutImagePath,
	FString& OutError)
{
	OutImagePath.Empty();
	OutError.Empty();

	// THE RENDERER IS CHECKED BEFORE ANYTHING IS OPENED, so a process that cannot render
	// does not leave an editor tab open behind a refusal it could have made first.
	FString WhyNot;
	if (!bCanRenderHere(WhyNot))
	{
		OutError = FString::Printf(
			TEXT("this editor cannot render, so no asset editor could be turned into a ")
			TEXT("picture: %s A capture that cannot render REFUSES BY NAME rather than ")
			TEXT("returning a black image, because a black image looks like a successful ")
			TEXT("capture of a dark editor."), *WhyNot);
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	if (Loaded == nullptr)
	{
		// AN ABSENT MEASUREMENT IS NEVER A ZERO (`P1`): the asset did not load, and that is
		// what is said, rather than reporting an empty frame or a wrong tab.
		OutError = FString::Printf(
			TEXT("%s exists in the asset registry but did not load, so its editor cannot ")
			TEXT("be opened. An asset whose module is not loaded cannot be loaded either, ")
			TEXT("and that is the usual cause."), *Resolved.ObjectPath);
		return false;
	}

	UAssetEditorSubsystem* AssetEditors = GEditor != nullptr
		? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditors == nullptr)
	{
		OutError = TEXT("the asset editor subsystem is not available, so no editor tab ")
			TEXT("can be opened.");
		return false;
	}

	AssetEditors->OpenEditorForAsset(Loaded);

	IAssetEditorInstance* Instance =
		AssetEditors->FindEditorForAsset(Loaded, /*bFocusIfOpen=*/false);
	if (Instance == nullptr)
	{
		OutError = FString::Printf(
			TEXT("no editor opened for %s, so there is no tab to render. The editor for ")
			TEXT("this asset class is published by a module that is not loaded in this ")
			TEXT("process."), *Resolved.ObjectPath);
		return false;
	}

	// *** THE ONE PATH THAT REACHES EVERY KIND: THE TOOLKIT HOST'S OWN WIDGET. ***
	//
	// Every asset editor is an FAssetEditorToolkit (Toolkits/AssetEditorToolkit.h:115),
	// and `IToolkitHost::GetParentWidget()` (Toolkits/IToolkitHost.h:25) is the host's
	// own widget: for a STANDALONE asset editor that is
	// SStandaloneAssetEditorToolkitHost, whose own implementation is `return AsShared()`
	// (SStandaloneAssetEditorToolkitHost.cpp:359-362), so it IS that editor's content.
	// `FWidgetRenderer::DrawWidget` renders a widget, so ONE path reaches every kind this
	// process can open instead of six class-specific ones that would each need their own
	// engine module and could each disagree about the size.
	FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);

	// A TERNARY OF TWO `TEXT` LITERALS INSIDE A VARIADIC ARGUMENT IS REJECTED BY THE
	// ENGINE'S COMPILE TIME FORMAT CHECKER (`error C7595` on this project's UE 5.8.3),
	// AND SO IS THE SAME TERNARY ASSIGNED TO AN `FString`. It has fired twice in this
	// project already, so this picks between two FStrings built in separate statements
	// rather than between two literals.
	FString WhichWidget = TEXT("the toolkit host's own widget for this editor: the whole ")
		TEXT("editor tab, uncropped");
	TSharedPtr<SWidget> Content;

	// A TOOLKIT HOST IS REQUIRED, AND ITS ABSENCE IS A REFUSAL. `FBaseToolkit` exposes
	// `GetToolkitHost()` (BaseToolkit.h:46) and there is no asset editor without a host,
	// so a null host means this instance is not a hosted toolkit and there is nothing to
	// render. Saying so beats dereferencing it.
	if (Instance != nullptr)
	{
		const TSharedRef<IToolkitHost> Host = Toolkit->GetToolkitHost();
		Content = Host->GetParentWidget();
	}

	if (!Content.IsValid())
	{
		OutError = FString::Printf(
			TEXT("the editor for %s opened but its toolkit host gave no widget, so there ")
			TEXT("is nothing to render. This is a refusal rather than a picture of an ")
			TEXT("empty frame."), *Resolved.ObjectPath);
		return false;
	}

	const TSharedRef<SWidget> Widget = Content.ToSharedRef();

	// ONE SLATE TICK LOOP, for the same measured reason TurboMCPCapture.cpp has one: a
	// tab that has just been spawned is laid out on the NEXT tick and reports 0x0 until
	// then, so a widget rendered before that would paint nothing. The window is prepared
	// too, so the widget's own geometry is computed against a real parent.
	if (FSlateApplication::IsInitialized())
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FSlateApplication::Get().Tick(ESlateTickType::All);

			const TSharedPtr<SWindow> HostWindow =
				FSlateApplication::Get().FindWidgetWindow(Widget);
			if (HostWindow.IsValid())
			{
				HostWindow->SlatePrepass(1.0f);
			}
			else
			{
				Widget->SlatePrepass(1.0f);
			}

			const FVector2D Size = Widget->GetTickSpaceGeometry().GetAbsoluteSize();
			if (Size.X > 0.0 && Size.Y > 0.0)
			{
				break;
			}
		}
	}

	FVector2D WidgetSize = Widget->GetTickSpaceGeometry().GetAbsoluteSize();
	FString SizeNote;
	if (WidgetSize.X <= 0.0 || WidgetSize.Y <= 0.0)
	{
		// ASKING THE WINDOW IS NOT THE SAME MEASUREMENT, and the result says so. A widget
		// that reports no geometry has not been laid out; rendering it anyway gives an
		// empty image, and an empty image is the failure `D-41` forbids being passed off
		// as a capture.
		const TSharedPtr<SWindow> HostWindow =
			FSlateApplication::Get().FindWidgetWindow(Widget);
		if (!HostWindow.IsValid())
		{
			OutError = FString::Printf(
				TEXT("the editor widget for %s is not in an on-screen window and reports ")
				TEXT("no geometry of its own, so it cannot be rendered. An editor tab ")
				TEXT("behind an inactive window has no geometry to draw with."),
				*Resolved.ObjectPath);
			return false;
		}
		const FVector2D WindowSize = HostWindow->GetSizeInScreen();
		if (WindowSize.X <= 0.0 || WindowSize.Y <= 0.0)
		{
			OutError = FString::Printf(
				TEXT("the editor widget for %s reports no geometry and its window reports ")
				TEXT("%.0fx%.0f, so there is nothing to render."),
				*Resolved.ObjectPath, WindowSize.X, WindowSize.Y);
			return false;
		}
		WidgetSize = WindowSize;
		SizeNote = TEXT("\n; the widget had not been laid out and reported no size of its ")
			TEXT("own, so the HOST WINDOW's size was used. The picture is still the editor ")
			TEXT("widget, rendered at that size.");
	}

	int32 Width = FMath::Max(1, FMath::RoundToInt(WidgetSize.X));
	int32 Height = FMath::Max(1, FMath::RoundToInt(WidgetSize.Y));
	FString CapNote;
	if (Width > AssetMaxEdge || Height > AssetMaxEdge)
	{
		const double Scale = FMath::Min(
			static_cast<double>(AssetMaxEdge) / static_cast<double>(Width),
			static_cast<double>(AssetMaxEdge) / static_cast<double>(Height));
		Width = FMath::Max(1, FMath::FloorToInt(Width * Scale));
		Height = FMath::Max(1, FMath::FloorToInt(Height * Scale));
		CapNote = FString::Printf(
			TEXT("\n; the widget is larger than the %d edge cap, so it was scaled to ")
			TEXT("%dx%d with the aspect ratio unchanged."), AssetMaxEdge, Width, Height);
	}

	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		FVector2D(Width, Height), TF_Bilinear, /*bUseGammaCorrection=*/false);
	if (RenderTarget == nullptr)
	{
		OutError = FString::Printf(
			TEXT("could not create a %dx%d render target for the editor widget"),
			Width, Height);
		return false;
	}

	FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false);
	Renderer->DrawWidget(RenderTarget, Widget, /*Scale=*/1.0f,
		FVector2D(Width, Height), /*DeltaTime=*/0.0f, /*bDeferRenderTargetUpdate=*/false);

	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
	ReadFlags.SetLinearToGamma(false);

	const bool bRead =
		RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags);
	delete Renderer;

	if (!bRead || Pixels.Num() < Width * Height)
	{
		OutError = FString::Printf(
			TEXT("the editor widget read back %d pixel(s) for a %dx%d image, so the ")
			TEXT("picture is incomplete and was not written."),
			Pixels.Num(), Width, Height);
		return false;
	}

	// *** THE EMPTY FRAME IS REFUSED, AND `D-41` IS WHY IT IS A REFUSAL AND NOT A FILE. ***
	//
	// A Slate widget paints transparent by default, so a widget that was never laid out
	// renders as ONE flat colour. `03_TOOLS.md` section 18 is explicit that an empty frame
	// is refused rather than written as a blank image, "because a blank image looks like a
	// successful capture of nothing". The count of distinct colours is computed and
	// PRINTED either way, so a caller reads the measurement rather than a claim.
	int32 Distinct = 0;
	{
		TSet<FColor> Seen;
		for (int32 Index = 0; Index < Width * Height; ++Index)
		{
			Seen.Add(Pixels[Index]);
		}
		Distinct = Seen.Num();
	}

	if (Distinct <= 1)
	{
		OutError = FString::Printf(
			TEXT("the editor widget rendered as ONE flat colour (%d distinct colour(s) in ")
			TEXT("a %dx%d image), so nothing was drawn and no file was written. An empty ")
			TEXT("frame is a refusal rather than a blank PNG."),
			Distinct, Width, Height);
		return false;
	}

	// The alpha is forced opaque because a Slate widget background is transparent by
	// default and a transparent PNG reads as an empty file in most viewers.
	for (int32 Index = 0; Index < Width * Height; ++Index)
	{
		Pixels[Index].A = 255;
	}

	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PngData);
	if (PngData.Num() == 0)
	{
		OutError = TEXT("could not compress the editor widget image to PNG");
		return false;
	}

	const FString Folder = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("TurboMCP"), CaptureFolderName);
	IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

	// The name says which asset was captured, so a folder of captures is readable
	// without opening one. A per-run name would be wrong here: the asset IS the identity.
	const FString SafeAsset = FPaths::GetBaseFilename(Resolved.ObjectPath)
		.Replace(TEXT(" "), TEXT("_"));
	OutImagePath = FPaths::Combine(Folder,
		FString::Printf(TEXT("%s_asset-editor_%dx%d.png"), *SafeAsset, Width, Height));

	if (!FFileHelper::SaveArrayToFile(PngData, *OutImagePath))
	{
		OutError = FString::Printf(TEXT("could not write %s"), *OutImagePath);
		OutImagePath.Empty();
		return false;
	}

	InOutDescription += FString::Printf(
		TEXT("\nframed: %s"), *WhichWidget);
	InOutDescription += FString::Printf(TEXT("\nwidget: %s"),
		*WidgetSummary(Widget, WidgetSize));
	InOutDescription += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);
	InOutDescription += FString::Printf(
		TEXT("\nreadback: %d distinct colour(s) in %d pixel(s)"),
		Distinct, Pixels.Num());
	InOutDescription += FString::Printf(TEXT("\nimage: %s"), *OutImagePath);
	InOutDescription += SizeNote;
	InOutDescription += CapNote;
	InOutDescription += TEXT("\n; this is the editor widget rendered off screen with ")
		TEXT("FWidgetRenderer::DrawWidget, NOT a grab of the screen, so the picture is ")
		TEXT("the editor tab even when its window is behind another one.");
	return true;
}
