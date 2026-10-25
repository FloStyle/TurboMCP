// Copyright (c) 2026 FloStyle
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
#include "GraphEditor.h"
#include "IMaterialEditor.h"
#include "RHI.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
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

/** Lists editor kinds and captures their editor presentation. Module availability
 * is checked separately from opening an asset and rendering its editor widget.
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
			Kind.AlternativeTool = TEXT("LookTurboMCPCapture modes graph, box or node");
			Kind.Reason = TEXT("Captures the Blueprint editor presentation. Use graph, node or box modes to frame graph content.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Anim Blueprint");
			Kind.AssetClass = TEXT("UAnimBlueprint");
			Kind.OwnerModule = TEXT("AnimationBlueprintEditor");
			Kind.AlternativeTool = TEXT("LookTurboMCPCapture modes graph, box or node");
			Kind.Reason = TEXT("Attempts the Animation Blueprint editor presentation. Module availability does not prove that a particular animation graph can be captured.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material");
			Kind.AssetClass = TEXT("UMaterial");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Captures the material editor presentation.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material Instance");
			Kind.AssetClass = TEXT("UMaterialInstanceConstant");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Captures the material instance editor presentation.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Material Function");
			Kind.AssetClass = TEXT("UMaterialFunction");
			Kind.OwnerModule = TEXT("MaterialEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Captures the material function editor presentation.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Widget Blueprint (Designer)");
			Kind.AssetClass = TEXT("UWidgetBlueprint");
			Kind.OwnerModule = TEXT("UMGEditor");
			Kind.AlternativeTool = TEXT("LookTurboMCPWidgetPreview (requires Execution=allow)");
			Kind.Reason = TEXT("Captures the Widget Blueprint editor presentation. The visible tab may be the designer or a graph. Widget preview renders a transient instance and requires explicit execution consent.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("DataTable");
			Kind.AssetClass = TEXT("UDataTable");
			Kind.OwnerModule = TEXT("DataTableEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Captures the DataTable editor presentation.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Anim Sequence");
			Kind.AssetClass = TEXT("UAnimSequence");
			Kind.OwnerModule = TEXT("AnimationEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Attempts the animation sequence editor presentation when its editor module can be loaded.");
			OutKinds.Add(Kind);
		}

		{
			TurboMCPCaptureAsset::FKind Kind;
			Kind.Name = TEXT("Niagara system");
			Kind.AssetClass = TEXT("UNiagaraSystem");
			Kind.OwnerModule = TEXT("NiagaraEditor");
			Kind.AlternativeTool = TEXT("");
			Kind.Reason = TEXT("Attempts the Niagara system editor presentation when its editor module can be loaded.");
			OutKinds.Add(Kind);
		}
	}

	/**
	 * MEASURES whether this kind's editor is reachable, by LOADING the module that
	 * publishes it, and returns the answer as text.
	 *
	 * *** THIS IS THE CORRECTION THAT COST A ROUND, AND IT IS WORTH THE PARAGRAPH. ***
	 *
	 * ASKING `IsModuleLoaded` AND TREATING A `false` AS "this kind cannot be captured" IS
	 * WRONG IN BOTH DIRECTIONS AGAINST A RUNNING EDITOR:
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
		// SHORT-CIRCUITING HERE REPORTS A KIND WITH A NAMED ALTERNATIVE AS OTHER-TOOL AND NEVER
		// ASKS FOR ITS MODULE, which produces a table saying Widget Blueprint (Designer) was
		// captured by another tool while
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
			TEXT("\n      related tool or mode: %s"),
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

	FString Table = TEXT("\nAsset-editor module availability by kind. This query may load editor modules. ")
		TEXT("It does not capture an asset or verify that an editor can render it:");

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
	InOutDescription += TEXT("\n; BUILT means the editor module loaded successfully; it is not capture acceptance. ")
		TEXT("REFUSED means the module could not be loaded. A capture attempt must separately ")
		TEXT("open the requested editor and produce an image.");
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
	FString& OutError,
	bool bFitGraph)
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
		// THE RESOLVER RETURNS ITS REASON ALREADY PREFIXED - `TurboMCPAssetResolver.cpp:108`
		// builds `error: no asset named '%s'. ` and `:43` and `:86` do the same - so it is
		// passed THROUGH here rather than prefixed again. The caller adds the prefix only
		// if it is missing, and doing it in both places is what produced the measured
		// `error: error: no asset named ...`.
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

	if (bFitGraph && !Loaded->IsA<UMaterial>())
	{
		OutError = TEXT("Target=fit currently supports Material assets only. Use an empty Target to capture this asset editor at its current view.");
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

	// Capture the asset toolkit's host widget through Slate's live renderer.
	// Do not reparent live widgets into a temporary offscreen renderer.
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


	if (bFitGraph)
	{
		// Inspect only visible graphs belonging to the requested asset's editor.
		// Never change graph contents or selection to obtain a wider image.
		if (Instance->GetEditorName() != FName(TEXT("MaterialEditor")))
		{
			OutError = TEXT("Target=fit requires the standard Material Editor. Use an empty Target for this editor.");
			return false;
		}
		UMaterialInterface* EditedMaterial = static_cast<IMaterialEditor*>(Toolkit)->GetMaterialInterface();
		TArray<TSharedRef<SGraphEditor>> GraphEditors;
		TFunction<void(TSharedRef<SWidget>)> Visit = [&](TSharedRef<SWidget> Current)
		{
			if (!Current->GetVisibility().IsVisible()) { return; }
			if (Current->GetType() == FName(TEXT("SGraphEditor")))
			{
				auto Editor = StaticCastSharedRef<SGraphEditor>(Current);
				if (Editor->GetCurrentGraph() && Editor->GetCurrentGraph()->GetTypedOuter<UMaterial>() == EditedMaterial)
				{ GraphEditors.Add(Editor); }
			}
			FChildren* Children = Current->GetChildren();
			for (int32 Index = 0; Index < Children->Num(); ++Index) { Visit(Children->GetChildAt(Index)); }
		};
		Visit(Widget);
		if (GraphEditors.Num() != 1 || !GraphEditors[0]->GetGraphPanel())
		{
			OutError = TEXT("Could not identify exactly one visible material graph. Open its graph tab and retry Target=fit. No image was written.");
			return false;
		}
		const auto Editor = GraphEditors[0];
		SGraphPanel* Panel = Editor->GetGraphPanel();
		FBox2D Bounds(ForceInit);
		for (UEdGraphNode* Node : Editor->GetCurrentGraph()->Nodes)
		{
			FVector2D Min, Max;
			if (!Node || !Panel->GetBoundsForNode(Node, Min, Max, 48.0f))
			{
				OutError = TEXT("Material graph node bounds are not ready. Retry after the editor finishes drawing. No image was written.");
				return false;
			}
			Bounds += Min; Bounds += Max;
		}
		const FVector2D ViewSize(Panel->GetCachedGeometry().GetLocalSize());
		if (!Bounds.bIsValid || Bounds.GetSize().X <= 0 || Bounds.GetSize().Y <= 0 || ViewSize.X <= 0 || ViewSize.Y <= 0)
		{
			OutError = TEXT("Material graph has no measurable nodes or visible canvas. No image was written.");
			return false;
		}
		const float Zoom = FMath::Min(1.0, FMath::Min(ViewSize.X / Bounds.GetSize().X, ViewSize.Y / Bounds.GetSize().Y));
		// Slate rounds requested zoom upward to a preset. Step down until the actual
		// zoom fits, then center using that measured zoom rather than the request.
		float RequestedZoom = Zoom;
		float ActualZoom = 1.0f;
		FVector2D ActualLocation;
		for (int32 Attempt = 0; Attempt < 32; ++Attempt)
		{
			Editor->SetViewLocation(Bounds.Min, RequestedZoom);
			Editor->GetViewLocation(ActualLocation, ActualZoom);
			if (ActualZoom <= Zoom) { break; }
			RequestedZoom *= 0.8f;
		}
		if (ActualZoom <= 0 || ActualZoom > Zoom)
		{
			OutError = TEXT("The material graph is too large to fit at the editor's minimum zoom. No image was written. Use smaller graphs or the current-view capture.");
			return false;
		}
		Editor->SetViewLocation(Bounds.GetCenter() - ViewSize / (2.0 * ActualZoom), ActualZoom);
		FSlateApplication::Get().Tick(ESlateTickType::All);
		Widget->SlatePrepass(1.0f);
		InOutDescription += TEXT("\nmaterial framing: fit all measured graph nodes; graph view changed, node positions and selection preserved.");
	}
	if (Loaded->IsA<UMaterialInterface>())
	{
		// The Material Editor renders a working copy. Query that copy rather than
		// the source asset; opening its tab may have just queued shader compilation.
		UMaterial* PreviewMaterial = nullptr;
		if (Instance->GetEditorName() == FName(TEXT("MaterialEditor")))
		{
			PreviewMaterial = Cast<UMaterial>(static_cast<IMaterialEditor*>(Toolkit)->GetMaterialInterface());
		}
		if (!PreviewMaterial)
		{
			InOutDescription += TEXT("\nmaterial shaders: unknown; this editor does not expose the supported preview material.");
		}
		else if (PreviewMaterial->IsCompiling())
		{
			InOutDescription += TEXT("\nmaterial shaders: compiling; this image may show a fallback material. Retry after compilation finishes.");
		}
		else if (PreviewMaterial->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform))
		{
			InOutDescription += TEXT("\nmaterial shaders: unavailable for the current shader platform. Inspect the Material Editor errors before relying on the preview.");
		}
		else
		{
			InOutDescription += TEXT("\nmaterial shaders: available for the editor material on the current shader platform.");
		}
		InOutDescription += TEXT("\n; Shader state is sampled before capture. It does not prove the displayed frame or material appearance is correct. After opening or activating the editor, allow normal editor ticks and capture again if the preview is still a fallback. This tool does not wait for or force shader compilation.");
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

	// Capture the live widget through Slate's own window renderer. Rendering it in
	// a temporary offscreen window changes live invalidation/hit-test state and can
	// leave nested grids with incompatible dimensions on the next editor tick.
	TArray<FColor> Pixels;
	FIntVector CapturedSize;
	if (!FSlateApplication::Get().TakeScreenshot(Widget, Pixels, CapturedSize)
		|| CapturedSize.X <= 0 || CapturedSize.Y <= 0
		|| int64(Pixels.Num()) != int64(CapturedSize.X) * CapturedSize.Y)
	{
		OutError = TEXT("Slate could not capture the live editor widget completely. No image was written.");
		return false;
	}
	// Size comes from the screenshot, not the pre-capture layout estimate.
	Width = CapturedSize.X;
	Height = CapturedSize.Y;
	CapNote.Empty();
	const int32 EdgeLimit = 4096;
	if (Width > EdgeLimit || Height > EdgeLimit)
	{
		const double Scale = FMath::Min(double(EdgeLimit) / Width, double(EdgeLimit) / Height);
		const int32 NewWidth = FMath::Max(1, FMath::FloorToInt(Width * Scale));
		const int32 NewHeight = FMath::Max(1, FMath::FloorToInt(Height * Scale));
		TArray<FColor> Scaled;
		FImageUtils::ImageResize(Width, Height, Pixels, NewWidth, NewHeight, Scaled, false);
		CapNote = FString::Printf(TEXT("\n; native screenshot %dx%d downscaled to %dx%d for the 4096 edge limit."), Width, Height, NewWidth, NewHeight);
		Pixels = MoveTemp(Scaled); Width = NewWidth; Height = NewHeight;
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

	// Keep asset identity readable while preserving every capture as separate evidence.
	const FString SafeAsset = FPaths::GetBaseFilename(Resolved.ObjectPath)
		.Replace(TEXT(" "), TEXT("_"));
	OutImagePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Folder,
		FString::Printf(TEXT("%s_asset-editor_%dx%d_%s.png"), *SafeAsset, Width, Height,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits))));

	if (!FFileHelper::SaveArrayToFile(PngData, *OutImagePath))
	{
		OutError = FString::Printf(TEXT("could not write %s"), *OutImagePath);
		OutImagePath.Empty();
		return false;
	}

	InOutDescription += FString::Printf(
		TEXT("\nframed: %s"), *WhichWidget);

	// *** WHICH KIND THIS ASSET RESOLVED TO, SO A CALLER NEVER HAS TO GUESS. ***
	//
	// `03_TOOLS.md` section 18 requires each editor kind to be "either supported or
	// REFUSED BY NAME, listed in the tool's own answer, so 'which editors can this
	// capture' is never a guess". A refusal covers one half of that; the other half is
	// that a SUCCESSFUL capture says which kind it was, which is what this line is for. A
	// class that is not one of the named kinds says so instead of being folded into one
	// that would be wrong.
	const FString ResolvedKind = KindNameForObject(Loaded);
	InOutDescription += ResolvedKind.IsEmpty()
		? FString::Printf(
			TEXT("\nkind: none of the named kinds. The asset is a %s, which no row of the ")
			TEXT("support table names, so it was captured through the generic editor-tab ")
			TEXT("path without being classified."),
			*Loaded->GetClass()->GetName())
		: FString::Printf(TEXT("\nkind: %s"), *ResolvedKind);
	InOutDescription += FString::Printf(TEXT("\nwidget: %s"),
		*WidgetSummary(Widget, WidgetSize));
	InOutDescription += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);
	InOutDescription += FString::Printf(
		TEXT("\nreadback: %d distinct colour(s) in %d pixel(s)"),
		Distinct, Pixels.Num());
	InOutDescription += FString::Printf(TEXT("\nimage: %s"), *OutImagePath);
	InOutDescription += SizeNote;
	InOutDescription += CapNote;
	InOutDescription += TEXT("\n; Captured the live editor widget through Slate. Embedded viewports show their latest rendered frame, which may lag behind newly activated tabs or shader updates.");
	return true;
}
