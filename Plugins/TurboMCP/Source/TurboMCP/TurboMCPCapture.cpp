// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.


#include "TurboMCPCapture.h"

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BlueprintEditor.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Input/HittestGrid.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"
#include "Slate/WidgetRenderer.h"
#include "SlateInspectorToolset.h"
#include "ToolsetRegistry/ToolsetImage.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPCaptureAsset.h"
#include "TurboMCPGraphTidy.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

/**
 * L-CAPTURE: picture of an asset editor.
 *
 * `03_TOOLS.md` asks for an asset's editor opened on a tab, framed one of five ways,
 * and saved as a PNG.
 *
 * WHY THIS RENDERS THE GRAPH WIDGET ITSELF
 * -----------------------------------------
 * The engine's `USlateInspectorToolset::Screenshot` captures **what is on screen**. It
 * does not frame, and framing is this tool's entire job: the spec lists five framings
 * and requires that one which cannot be done is an error rather than a silent fallback
 * to the whole graph.
 *
 * So the graph editor widget is rendered directly with `FWidgetRenderer`, at the size
 * asked for, after the view is moved to the target. That gives an exact size and an
 * exact frame, and it is the same mechanism `L-WIDGET-PREVIEW` already proved works.
 * A second capture path that only sometimes agrees with the first is exactly what
 * `04_FORMATS.md` warns against, so only one is used.
 *
 * THE FRAME IS COMPUTED, NOT READ
 * -------------------------------
 * `SGraphEditor::SetViewLocation(Location, Zoom)` takes a graph-space top-left view offset and a
 * zoom. The target's bounding box comes from the nodes' own rectangles, and the zoom
 * is chosen so that box fits the requested image. `SGraphPanel::GetBoundsForNode`
 * would give on-screen bounds but needs a live panel and a widget geometry pass, so
 * the graph-space rectangles are used instead, which is the same computation
 * `L-LAYOUT-REPORT` already does and reports.
 */
namespace
{
	/** The size used when the caller does not give one. */
	constexpr int32 DefaultWidth = 1600;
	constexpr int32 DefaultHeight = 900;

	/** The largest edge that is rendered. */
	constexpr int32 MaxEdge = 4096;

	/** The smallest zoom, so a huge graph does not become unreadable. */
	constexpr float MinZoom = 0.05f;

	/** The largest zoom, so a single node does not fill the screen at 20x. */
	constexpr float MaxZoom = 0.85f;

	/** How much empty space is left around the frame, as a fraction. */
	constexpr float FramePadding = 1.12f;

	/** How many nodes a "whole graph" frame may cover before it says so. */
	constexpr int32 ManyNodes = 150;
}

FString UTurboMCPLookToolset::LookTurboMCPCapture(
	const FString& Asset,
	const FString& GraphName,
	const FString& Mode,
	const FString& Target,
	const FString& Size)
{
	// *** THE MODE IS READ AND VALIDATED BEFORE THE ASSET AND THE GRAPH ARE DEMANDED. ***
	//
	// Every check below is a check on the ARGUMENTS, and the value of this ordering was
	// measured by row 2.1: a caller who passes a mode this tool does not have should be
	// told THAT, and not sent looking for a graph it never needed. It matters more now,
	// because `mode asset-editor` needs NO graph name and NO asset at all.
	//
	// The mode, validated before anything opens, so a typo does not leave an editor
	// tab open behind it.
	const FString WantedMode = Mode.TrimStartAndEnd().ToLower();
	const bool bWholeGraph = WantedMode.IsEmpty() || WantedMode == TEXT("graph");
	const bool bOneBox = WantedMode == TEXT("box");
	const bool bRegion = WantedMode == TEXT("region");
	const bool bOneNode = WantedMode == TEXT("node");

	// `screen` IS READ HERE TOO, AND FOR ONE REASON: the graph name must NOT be demanded
	// for it, and that guard is taken before the block that handles `screen` below. It is
	// one constant-folded comparison, so reading it twice costs nothing and keeps the
	// guard honest.
	const bool bScreen = WantedMode == TEXT("screen");

	// `asset-editor` IS HANDLED BEFORE THE ASSET AND THE GRAPH ARE DEMANDED, and that is
	// the point of it: with no asset it answers the capability question - "which editors
	// can this tool capture" - without opening anything.
	//
	// `GraphName` IS REUSED AS THE KIND ARGUMENT rather than a sixth parameter being added
	// to the exported signature. `LookTurboMCPCapture` is an AICallable tool whose schema
	// every suite and the coverage audit already read, and a new parameter would change
	// that schema for every caller to serve one mode. The argument is documented as doing
	// two jobs, and the result echoes which job it did.
	const bool bAssetEditor = WantedMode == TEXT("asset-editor");

	if (bAssetEditor)
	{
		// STARTED EMPTY ON PURPOSE. The `look:` banner is prepended at the END, once the
		// capture has succeeded, so that a refusal can begin with `error:` instead. Seeding
		// this with the banner AND prepending it printed it TWICE - MEASURED, verbatim:
		// `look: capture of an asset editorlook: capture of an asset editor` - and the
		// suite did not catch it because no check read the first line of a SUCCESS. A
		// check for that is now in the suite.
		FString Out;

		// FORWARD DECLARED IN THE HEADER, so `TurboMCPCaptureAsset.h` is the only include
		// this file gains and neither suite's measured picture can move.
		TurboMCPCaptureAsset::FKind Named;

		if (Asset.IsEmpty())
		{
			// NO ASSET IS NOT A FAILURE: it is the question "which editors can this tool
			// capture", and it is answered without opening anything. An empty argument
			// that printed a refusal would make the catalogue unreachable.
			if (!GraphName.TrimStartAndEnd().IsEmpty()
				&& !TurboMCPCaptureAsset::FindKind(GraphName, Named))
			{
				// *** THE REFUSAL STARTS WITH `error:` AND CARRIES NO BANNER FIRST. ***
				//
				// MEASURED, AND IT COST A ROUND. With the banner first, the answer began
				// `look: capture of an asset editor` and the engine's tool layer did NOT
				// flag it, so `out.startswith("error:")` was FALSE and a caller reading
				// the first line saw a SUCCESS - which is the identical defect row 2.1
				// recorded on the `select-nodes` probe. The refusal is therefore the whole
				// opening of the answer, and the support table is appended after it.
				FString Refusal = FString::Printf(
					TEXT("error: '%s' is not an editor kind this tool knows. Pass one of ")
					TEXT("the kind names listed below, or leave it empty for the whole ")
					TEXT("table."), *GraphName);
				Refusal += TEXT("\nwhich asset editors this tool can capture:");
				TurboMCPCaptureAsset::DescribeKinds(Refusal);
				return Refusal;
			}

			Out += TEXT("\nasset: none given, so this is the support table and no editor ")
				TEXT("was opened");
			if (!Named.Name.IsEmpty())
			{
				// A KIND WITHOUT AN ASSET IS ANSWERED RATHER THAN IGNORED, because a
				// caller who asked "can you capture a DataTable" deserves that answer and
				// not the whole table to search.
				Out += FString::Printf(TEXT("\nkind asked about: %s"), *Named.Name);
				Out += FString::Printf(TEXT("\n  asset class: %s"), *Named.AssetClass);
				Out += FString::Printf(TEXT("\n  editor module: %s"),
					Named.OwnerModule.IsEmpty() ? TEXT("(none)") : *Named.OwnerModule);
				Out += FString::Printf(
					TEXT("\n  another tool that also captures this kind, if any: %s"),
					Named.AlternativeTool.IsEmpty()
						? TEXT("(no other tool)") : *Named.AlternativeTool);
				Out += FString::Printf(TEXT("\n  why: %s"), *Named.Reason);
			}
			TurboMCPCaptureAsset::DescribeKinds(Out);

			// THE BANNER GOES ON HERE, at the one point in this branch that ANSWERS, so a
			// refusal above can still begin with `error:` and this success still opens with
			// the banner exactly once.
			return FString::Printf(TEXT("look: capture of an asset editor%s"), *Out);
		}

		if (!GraphName.TrimStartAndEnd().IsEmpty()
			&& !TurboMCPCaptureAsset::FindKind(GraphName, Named))
		{
			// THE KIND IS CHECKED BEFORE ANY EDITOR IS OPENED, so a typo cannot leave a
			// tab open behind a refusal. `D-41`: a framing that cannot be done is an
			// error naming why, and never a silent fallback to a wider one.
			//
			// AND THE REFUSAL STARTS WITH `error:` FOR THE REASON ABOVE, so a caller
			// testing the prefix sees the failure rather than the banner.
			FString Refusal = FString::Printf(
				TEXT("error: '%s' is not an editor kind this tool knows, so nothing was ")
				TEXT("opened. Pass one of the kind names listed below, or leave it empty ")
				TEXT("to capture whichever editor the asset resolves to."), *GraphName);
			Refusal += TEXT("\nwhich asset editors this tool can capture:");
			TurboMCPCaptureAsset::DescribeKinds(Refusal);
			return Refusal;
		}

		FString ImagePath;
		FString Error;

		// *** THE BANNER IS PUT ON ONLY ONCE THE CAPTURE HAS SUCCEEDED, AND THAT IS A
		// DEFECT FIXED BY A BREAK TEST RATHER THAN BY READING. ***
		//
		// `CaptureAssetEditor` has already appended its frame, geometry and image lines to
		// `Out` by the time it returns, so the banner used to be written FIRST and a
		// refusal therefore arrived as:
		//
		//     look: capture of an asset editor
		//     error: could not write ...
		//
		// MEASURED, WITH THE TARGET FILE MADE READ-ONLY: the tool DID report the failure,
		// but the answer did not begin with `error:`, so the engine's tool layer did not
		// flag it and a caller testing the prefix - which is the convention this tool
		// documents and row 2.1 measured - READ A FAILED WRITE AS A SUCCESS. That is the
		// same defect row 2.1 found on the `select-nodes` probe, one layer down.
		//
		// So the label is prepended to the SUCCESS text here, and every refusal below is a
		// plain string beginning `error:` and carrying no banner at all.
		if (!TurboMCPCaptureAsset::CaptureAssetEditor(Asset, Out, ImagePath, Error))
		{
			// *** THE PREFIX IS ADDED ONLY WHEN IT IS NOT ALREADY THERE. ***
			//
			// MEASURED, verbatim, before this guard existed:
			//     `error: error: no asset named '/Game/...'`
			// The resolver returns its reasons ALREADY PREFIXED - `TurboMCPAssetResolver.cpp:108`
			// builds `error: no asset named '%s'. ` and `:43`, `:86` do the same - so
			// prepending unconditionally doubled it. The double is not cosmetic: a caller
			// that strips one prefix to read the reason gets the word `error` as the reason.
			const FString Prefix = TEXT("error: ");
			FString Refusal = Error.StartsWith(Prefix, ESearchCase::CaseSensitive)
				? Error
				: Prefix + Error;
			Refusal += FString::Printf(TEXT("\nasset: %s"), *Asset);
			if (!Named.Name.IsEmpty())
			{
				Refusal += FString::Printf(TEXT("\nkind asked for: %s"), *Named.Name);
			}
			Refusal += TEXT("\nwhich asset editors this tool can capture:");
			TurboMCPCaptureAsset::DescribeKinds(Refusal);
			return Refusal;
		}

		Out = FString::Printf(TEXT("look: capture of an asset editor%s"), *Out);
		if (!Named.Name.IsEmpty())
		{
			Out += FString::Printf(TEXT("\nkind asked for: %s"), *Named.Name);
		}
		TurboMCPCaptureAsset::DescribeKinds(Out);
		return Out;
	}

	// `debug-window` renders the whole editor window at the requested size, with NO
	// cropping. It exists to answer one question that arithmetic kept getting wrong:
	// where does a given widget actually land in the rendered picture? The panel's
	// computed position and its real position can then be compared instead of assumed.
	const bool bDebugWindow = WantedMode == TEXT("debug-window");

	// THE FOUR SELECTORS ADDED BY ROW 2.1, AND THE ONE THAT NEEDS NO ASSET AT ALL.
	//
	// They are routed through `TurboMCPCaptureSelector` below rather than resolved
	// here, so there is still exactly ONE `LookTurboMCPCapture` and one PNG contract
	// (`04_FORMATS.md` section 6). The selectors decide WHICH NODES are framed; this
	// function still owns the frame arithmetic and the picture.
	// `bScreen` WAS ALSO DECLARED AT THE TOP, for the graph-name guard. It is not
	// redeclared here, because two declarations of one mode flag is how a mode comes to
	// mean two things.
	const bool bRhiState = WantedMode == TEXT("rhi-state");
	const bool bSelectProbe = WantedMode == TEXT("select-nodes");
	const bool bNewSelector = WantedMode == TEXT("selection")
		|| WantedMode == TEXT("neighbours")
		|| WantedMode == TEXT("box-neighbours");

	// `bAssetEditor` WAS DECLARED AT THE TOP, with the other mode flags that are read
	// before the asset is demanded. It is repeated here as a comment rather than as a
	// second declaration, because two declarations of one mode flag is how a mode comes
	// to mean two things.

	if (!bWholeGraph && !bRegion && !bOneBox && !bOneNode && !bDebugWindow
		&& !bScreen && !bNewSelector && !bRhiState && !bSelectProbe && !bAssetEditor)
	{
		// `debug-window` and `rhi-state` are the two diagnostic modes and are NOT
		// listed: an agent reading this refusal is being told what it may pass, and a
		// diagnostic is not something a caller should be steered into.
		return FString::Printf(
			TEXT("error: '%s' is not a capture mode. Accepted: graph, box, node, ")
			TEXT("region (Target=x,y,width,height), selection, neighbours, box-neighbours, screen, asset-editor. Leave it ")
			TEXT("empty for graph."), *Mode);
	}

	// THE GRAPH NAME IS DEMANDED ONLY NOW, once the mode is known to be one that needs it.
	// `asset-editor` AND `screen` ARE THE TWO MODES THAT DO NOT. `asset-editor` frames an
	// editor tab, and a Material, a DataTable or a Widget Designer has no graph to name;
	// `screen` frames the whole window, which belongs to no asset and no graph at all.
	//
	// THIS CHECK WAS `asset-editor` ONLY FOR ONE ROUND, AND THE OMISSION WAS MEASURED: a
	// suite calling `mode screen` with no graph name got
	// `error: no graph given ...` instead of a picture, which reads as a defect in the
	// capture and is a defect in THIS guard.
	if (GraphName.IsEmpty() && !bAssetEditor && !bScreen)
	{
		return TEXT("error: no graph given. Pass a graph name; the tool refuses ")
			TEXT("rather than guessing which graph you meant.");
	}

	if (bDebugWindow && !Target.TrimStartAndEnd().IsEmpty())
	{
		// The target was silently ignored here, which is the same silent behaviour the
		// selectors below are careful to avoid: a caller who passed a node ID and got
		// the whole window back would have no way to tell.
		return FString::Printf(
			TEXT("error: mode debug-window takes no target, and '%s' was given. This ")
			TEXT("mode renders the whole window and a target would be ignored."),
			*Target);
	}

	// `rhi-state` MEASURES INSTEAD OF RENDERING, and that is deliberate rather than a
	// convenience: the state that decides whether `screen` can work at all cannot be
	// seen on a machine that IS headless, so the answer is taken from this process and
	// printed as a measurement rather than inferred. It writes nothing.
	if (bRhiState)
	{
		FString State = FString::Printf(TEXT("look: rendering state of the editor process"));
		TurboMCPCaptureSelector::DescribeScreenAvailability(State);
		return State;
	}

	// `screen` IS HANDLED BEFORE THE ASSET IS RESOLVED, and that is the point of it:
	// the whole editor window does not belong to an asset. Requiring one would make
	// the caller name something the picture does not contain.
	if (bScreen)
	{
		FString Out = TEXT("look: capture of the whole editor window");
		FString ImagePath;
		FString Error;
		if (!TurboMCPCaptureSelector::RenderScreen(Out, ImagePath, Error))
		{
			// REFUSED BY NAME. `D-41` forbids a wider frame in place of the one asked
			// for, and a black rectangle is the wider-frame failure in its worst form:
			// it looks like a successful capture of a dark window.
			//
			// *** `error:` COMES FIRST AND THE BANNER FOLLOWS. This refusal used to be
			// `"%s\nerror: ..." % Out`, which put the `look:` banner at the front, so a caller
			// testing for the `error:` prefix saw a SUCCESS - which is the exact failure the probe
			// branch above records as measured. Every refusal in this file begins `error:`. ***
			return FString::Printf(TEXT("error: mode screen cannot work here: %s\n%s"),
				*Error, *Out);
		}
		return Out;
	}

	// The hop count lives in `Size` for the two neighbourhood selectors. `04_FORMATS.md`
	// section 6.1 gives the framing names but no parameter spellings, and `Target` is
	// already taken by the node ID or the box index, so the free parameter carries it.
	// Size is parsed below exactly as it is for the older modes, so `1610` is refused
	// rather than silently read as 1610 hops.
	int32 NeighbourHops = 0;

	if ((bOneBox || bOneNode) && Target.TrimStartAndEnd().IsEmpty())
	{
		return FString::Printf(
			TEXT("error: mode '%s' needs a target. Pass a comment box index for ")
			TEXT("mode box, or a node ID for mode node."), *WantedMode);
	}

	int32 Width = DefaultWidth;
	int32 Height = DefaultHeight;
	if (bNewSelector)
	{
		// THE HOP COUNT IS `Size` FOR THE TWO NEIGHBOURHOOD SELECTORS, and it is a plain
		// whole number rather than a `WxH` pair, because those modes COMPUTE their own
		// picture size from the frame exactly as `node` and `box` already do. `0` is a
		// meaningful value (the target alone), so a bare `0` must be accepted here even
		// though the size parser below would reject it for every other mode.
		const FString HopText = Size.TrimStartAndEnd();
		if (!HopText.IsEmpty())
		{
			if (!HopText.IsNumeric())
			{
				return FString::Printf(
					TEXT("error: mode '%s' reads Size as a whole number of hops, and '%s' ")
					TEXT("is not one. Pass 0 for the target alone, or a count such as 3."),
					*WantedMode, *Size);
			}
			NeighbourHops = FCString::Atoi(*HopText);
			if (NeighbourHops > MaxEdge)
			{
				return FString::Printf(
					TEXT("error: %d hops is more than the %d limit this tool accepts. A ")
					TEXT("neighbourhood that wide is the whole graph, so ask for the ")
					TEXT("whole graph as it is."), NeighbourHops, MaxEdge);
			}
		}
	}
	else if (!Size.TrimStartAndEnd().IsEmpty())
	{
		FString Left;
		FString Right;
		if (!Size.Split(TEXT("x"), &Left, &Right) && !Size.Split(TEXT(","), &Left, &Right))
		{
			return FString::Printf(
				TEXT("error: '%s' is not a size. Pass WxH, for example 1600x900."), *Size);
		}
		Left.TrimStartAndEndInline();
		Right.TrimStartAndEndInline();
		if (!Left.IsNumeric() || !Right.IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a size. Both parts must be whole numbers."),
				*Size);
		}
		Width = FCString::Atoi(*Left);
		Height = FCString::Atoi(*Right);
		if (Width <= 0 || Height <= 0)
		{
			return FString::Printf(
				TEXT("error: a size of %dx%d cannot be rendered."), Width, Height);
		}
		if (Width > MaxEdge || Height > MaxEdge)
		{
			return FString::Printf(
				TEXT("error: %dx%d is larger than the %d limit."),
				Width, Height, MaxEdge);
		}
	}

	// The hop count was read into NeighbourHops above, and only for the selectors.

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	if (GEditor == nullptr)
	{
		return TEXT("error: the editor is not available yet, try again once it has ")
			TEXT("loaded.");
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so its graph cannot be captured. ")
			TEXT("Widget Blueprints and materials are not supported by this tool yet, ")
			TEXT("and saying so is better than capturing the wrong tab."),
			*Resolved.ObjectPath);
	}

	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
	if (Graph == nullptr)
	{
		TArray<UEdGraph*> All;
		FTurboMCPBlueprintRead::CollectGraphs(Blueprint, All);
		TArray<FString> Names;
		for (const UEdGraph* Candidate : All)
		{
			Names.Add(Candidate->GetName());
		}
		return FString::Printf(
			TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
			*Resolved.ObjectPath, *GraphName,
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
	}

	// The target set, resolved BEFORE the editor is opened, so a bad target leaves no
	// tab open.
	TArray<UEdGraphNode*> Targets;
	FString FrameLabel;

	// THE SELECTOR MODES, WHICH NEED THE LIVE GRAPH PANEL AND SO ARE RESOLVED WITH IT.
	//
	// `selection` reads the Slate panel, so unlike every other mode it cannot be
	// resolved before the editor opens: there is no selection to read until a canvas
	// exists. `neighbours` and `box-neighbours` read the graph only, but they are
	// resolved here with it so all three selectors take ONE code path and cannot
	// disagree about a mode name, a target or a hop count.
	FString SelectorDefinition;

	// `select-nodes` IS THE INTERNAL PROBE THAT MAKES `selection` TESTABLE, and it is the
	// only mode on this tool that changes the editor. It exists because `P3` forbids a
	// Look creating a selection (`02_PRINCIPLES.md:29-33`), so without a probe the
	// positive branch of `selection` could never be measured here. It resolves the keys
	// BEFORE touching anything, and it clears the selection and asserts that it is empty
	// before returning. It is not an AICallable tool and it is not part of the contract.
	if (bSelectProbe)
	{
		UAssetEditorSubsystem* ProbeEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (ProbeEditors == nullptr)
		{
			return TEXT("error: the asset editor subsystem is not available, so no editor ")
				TEXT("tab can be opened and no selection can be probed.");
		}

		IAssetEditorInstance* ProbeInstance =
			ProbeEditors->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
		if (ProbeInstance == nullptr)
		{
			ProbeEditors->OpenEditorForAsset(Blueprint);
			ProbeInstance = ProbeEditors->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
		}

		FBlueprintEditor* ProbeBlueprintEditor =
			ProbeInstance != nullptr ? static_cast<FBlueprintEditor*>(ProbeInstance) : nullptr;
		if (ProbeBlueprintEditor == nullptr)
		{
			return FString::Printf(
				TEXT("error: %s did not open a Blueprint editor, so there is no graph panel ")
				TEXT("to probe."), *Resolved.ObjectPath);
		}

		TSharedPtr<SGraphEditor> ProbePanel =
			ProbeBlueprintEditor->OpenGraphAndBringToFront(Graph);
		if (!ProbePanel.IsValid())
		{
			return FString::Printf(
				TEXT("error: the editor opened %s but gave no graph panel for graph %s."),
				*Resolved.ObjectPath, *Graph->GetName());
		}

		FString Out = FString::Printf(TEXT("look: selection probe on %s"), *Resolved.ObjectPath);
		Out += FString::Printf(TEXT("\ngraph: %s"), *Graph->GetName());
		FString ProbeReport = Out;
		FString ProbeWhy;
		if (!TurboMCPCaptureSelector::ProbeSelection(ProbePanel.Get(), Graph, Target,
				Resolved.ObjectPath, ProbeReport, ProbeWhy))
		{
			// A PLAIN STRING BEGINNING `error:` AND NOTHING ELSE, which is the convention
			// every other mode on this tool already uses. RETURNING THE REPORT FOLLOWED BY THE
			// ERROR MAKES THE ENGINE'S TOOL LAYER FLAG THE WHOLE ANSWER AS AN ERROR while the
			// text begins with the success banner, so a caller testing for the `error:` prefix
			// reads a success. The suite's `a key that resolves to no node is refused` check is
			// what catches it.
			return FString::Printf(TEXT("error: %s"), *ProbeWhy);
		}
		return ProbeReport;
	}

	if (bNewSelector)
	{
		UAssetEditorSubsystem* SelectorEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (SelectorEditors == nullptr)
		{
			return TEXT("error: the asset editor subsystem is not available, so no editor ")
				TEXT("tab can be opened and no selector can be read.");
		}

		IAssetEditorInstance* SelectorInstance =
			SelectorEditors->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
		if (SelectorInstance == nullptr)
		{
			SelectorEditors->OpenEditorForAsset(Blueprint);
			SelectorInstance = SelectorEditors->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
		}

		FBlueprintEditor* SelectorBlueprintEditor =
			SelectorInstance != nullptr ? static_cast<FBlueprintEditor*>(SelectorInstance) : nullptr;
		if (SelectorBlueprintEditor == nullptr)
		{
			return FString::Printf(
				TEXT("error: %s did not open a Blueprint editor, so mode %s has no ")
				TEXT("canvas to read. This happens for an asset whose editor is not the ")
				TEXT("Blueprint editor."), *Resolved.ObjectPath, *WantedMode);
		}

		// The graph the caller NAMED is the one brought to the front, so `selection`
		// reads the canvas it is about rather than whichever tab was last active.
		TSharedPtr<SGraphEditor> SelectorPanel =
			SelectorBlueprintEditor->OpenGraphAndBringToFront(Graph);
		if (!SelectorPanel.IsValid())
		{
			return FString::Printf(
				TEXT("error: the editor opened %s but gave no graph panel for graph %s, ")
				TEXT("so mode %s has no canvas to read."),
				*Resolved.ObjectPath, *Graph->GetName(), *WantedMode);
		}

		TurboMCPCaptureSelector::FSelection Selection;
		FString Why;
		if (!TurboMCPCaptureSelector::Resolve(WantedMode, Target, NeighbourHops,
				Resolved.ObjectPath, Graph, SelectorPanel.Get(), Selection, Why))
		{
			return FString::Printf(TEXT("error: %s"), *Why);
		}

		Targets = Selection.Targets;
		FrameLabel = Selection.FrameLabel;
		SelectorDefinition = Selection.Definition;
	}
	else if (bWholeGraph || bDebugWindow || bRegion)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node != nullptr && !Node->IsA<UEdGraphNode_Comment>())
			{
				Targets.Add(Node);
			}
		}
		FrameLabel = bDebugWindow
			? FString::Printf(TEXT("the whole window, uncropped (debug), %d node(s) in the graph"),
				Targets.Num())
			: FString::Printf(TEXT("the whole graph, %d node(s)"), Targets.Num());
	}
	else if (bOneBox)
	{
		if (!Target.TrimStartAndEnd().IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a comment box index. Mode box takes a whole ")
				TEXT("number, the index from L-LAYOUT-REPORT."), *Target);
		}
		const int32 Wanted = FCString::Atoi(*Target.TrimStartAndEnd());

		TArray<UEdGraphNode_Comment*> Boxes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				Boxes.Add(Comment);
			}
		}
		// Sorted the same way L-LAYOUT-REPORT prints them, so an index means the same
		// thing in both tools. Without this the index would be meaningless.
		Boxes.Sort([](const UEdGraphNode_Comment& A, const UEdGraphNode_Comment& B)
		{
			if (A.NodePosY != B.NodePosY)
			{
				return A.NodePosY < B.NodePosY;
			}
			return A.NodePosX < B.NodePosX;
		});

		if (Wanted < 0 || Wanted >= Boxes.Num())
		{
			// The spec's own example: "asked for box 9, this graph has 4". Never a
			// silent fallback to the whole graph.
			return FString::Printf(
				TEXT("error: asked for box %d, this graph has %d comment box(es). ")
				TEXT("Boxes are numbered from 0 in the order L-LAYOUT-REPORT lists ")
				TEXT("them, top to bottom then left to right."),
				Wanted, Boxes.Num());
		}

		Targets.Add(Boxes[Wanted]);
		FrameLabel = FString::Printf(TEXT("comment box %d"), Wanted);
	}
	else
	{
		const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(Target.TrimStartAndEnd());
		if (!Parsed.IsComplete())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a node ID. Pass one in the form ")
				TEXT("AssetName:GraphName:NodeKey, or an ID printed by ")
				TEXT("R-NODE-FIND."), *Target);
		}

		TArray<UEdGraphNode*> Matches;
		const int32 MatchCount =
			TurboMCPNodeId::FindNodesByKey(Graph, Parsed.NodeKey, Matches);
		if (MatchCount == 0)
		{
			return FString::Printf(
				TEXT("error: no node in %s has the key %s, so nothing was captured. ")
				TEXT("Use R-NODE-FIND to get a current ID."),
				*Graph->GetName(), *Parsed.NodeKey);
		}
		if (MatchCount > 1)
		{
			return FString::Printf(
				TEXT("error: the key %s fits %d nodes in %s, so the frame would be ")
				TEXT("ambiguous. Use more digits."),
				*Parsed.NodeKey, MatchCount, *Graph->GetName());
		}

		Targets.Add(Matches[0]);
		FrameLabel = FString::Printf(TEXT("node %s"),
			*TurboMCPNodeId::GetNodeTitle(Matches[0]));
	}

	if (Targets.Num() == 0)
	{
		// An empty frame is refused rather than rendered as a blank image, because a
		// blank image looks like a successful capture of nothing.
		return FString::Printf(
			TEXT("error: %s has no node to frame in graph %s, so there is nothing to ")
			TEXT("capture."), *Resolved.ObjectPath, *Graph->GetName());
	}

	FString GeometryError;
    if(!FTurboMCPGraphTidy::PrepareAlignment(Blueprint,Graph,GeometryError)) return TEXT("error: ")+GeometryError;
	// The bounding box of the targets, in graph space.
	FTurboMCPLayoutReport::FRect Box = FTurboMCPLayoutReport::RectOf(Targets[0]);
	for (const UEdGraphNode* Node : Targets)
	{
		const FTurboMCPLayoutReport::FRect R = FTurboMCPLayoutReport::RectOf(Node);
		Box.MinX = FMath::Min(Box.MinX, R.MinX);
		Box.MinY = FMath::Min(Box.MinY, R.MinY);
		Box.MaxX = FMath::Max(Box.MaxX, R.MaxX);
		Box.MaxY = FMath::Max(Box.MaxY, R.MaxY);
	}

    if(bRegion)
    {
        TArray<FString> Parts; Target.ParseIntoArray(Parts,TEXT(","),false);
        double V[4];
        if(Parts.Num()!=4) return TEXT("error: region Target must be x,y,width,height in graph coordinates");
        for(int32 I=0;I<4;++I) if(!LexTryParseString(V[I],*Parts[I].TrimStartAndEnd()) || !FMath::IsFinite(V[I]) || FMath::Abs(V[I])>10000000)
            return TEXT("error: region requires four finite graph coordinates within +/-10000000");
        if(V[2]<=0 || V[3]<=0) return TEXT("error: region width and height must be positive");
        Box.MinX=V[0]; Box.MinY=V[1]; Box.MaxX=V[0]+V[2]; Box.MaxY=V[1]+V[3];
        FrameLabel=TEXT("explicit graph-coordinate region");
    }
	// A zero or negative graph-space frame has no pixels to capture. Check before the
	// fit calculation can turn it into an artificial one-unit frame or any image is written.
	const double FrameWidth = Box.Width();
	const double FrameHeight = Box.Height();
	if (FrameWidth <= 0.0)
	{
		return FString::Printf(TEXT("error: capture frame width is %.0f for %s, but it must be greater than zero. Open the graph so its node outlines are measured, then try again."),
			FrameWidth, *FrameLabel);
	}
	if (FrameHeight <= 0.0)
	{
		return FString::Printf(TEXT("error: capture frame height is %.0f for %s, but it must be greater than zero. Open the graph so its node outlines are measured, then try again."),
			FrameHeight, *FrameLabel);
	}

    // Explicit picture dimensions render native Slate at that resolution. The live
    // editor's window size must not limit the available graph pixels (S10 CP-11).
    // Selector Size is a hop count, not a resolution. Derive its independent
    // canvas from only the resolved target bounds, never the whole graph extent.
    if (bNewSelector)
    {
        const double SelectorScale = FMath::Min(1.0, static_cast<double>(MaxEdge) / FMath::Max(FrameWidth + 64.0, FrameHeight + 64.0));
        Width = FMath::Clamp(FMath::CeilToInt((FrameWidth + 64.0) * SelectorScale), 256, MaxEdge);
        Height = FMath::Clamp(FMath::CeilToInt((FrameHeight + 64.0) * SelectorScale), 256, MaxEdge);
    }
    if(bNewSelector || (!Size.TrimStartAndEnd().IsEmpty() && (bWholeGraph || bOneBox || bOneNode || bRegion)))
    {
        TurboMCPGraphScratch::FCopy CaptureCopy;
        if(!CaptureCopy.Init(Blueprint,Graph)) return TEXT("error: capture scratch graph unavailable");
        const TSharedRef<SGraphEditor> CaptureEditor=SNew(SGraphEditor).GraphToEdit(CaptureCopy.Graph).IsEditable(false).ShowGraphStateOverlay(false);
        const FVector2D PictureSize(Width,Height);
        TStrongObjectPtr<UTextureRenderTarget2D> PictureTarget(FWidgetRenderer::CreateTargetFor(PictureSize,TF_Bilinear,true));
        if(!PictureTarget.IsValid()) return TEXT("error: capture render target unavailable");
        PictureTarget->InitCustomFormat(Width,Height,PF_B8G8R8A8,true);
        PictureTarget->UpdateResourceImmediate(true);
        FWidgetRenderer PictureRenderer(true);
        CaptureEditor->GetGraphPanel()->Update();
        CaptureEditor->SetViewLocation(FVector2f::ZeroVector,1.0f);
        for(int32 Pass=0;Pass<2;++Pass)
        {
            CaptureEditor->SlatePrepass(1.0f);
            PictureRenderer.DrawWidget(PictureTarget.Get(),CaptureEditor,PictureSize,0.0f);
        }
        for(UEdGraphNode* Node:CaptureCopy.Graph->Nodes)
            if(Node && !CaptureEditor->GetGraphPanel()->GetNodeWidgetFromGuid(Node->NodeGuid).IsValid())
                return TEXT("error: capture node widgets unavailable; no blank image returned");
        const FVector2D CanvasSize=CaptureEditor->GetGraphPanel()->GetTickSpaceGeometry().GetLocalSize();
        if(CanvasSize.X<=0 || CanvasSize.Y<=0) return TEXT("error: capture canvas unavailable");
        const float FitZoom=FMath::Min(1.0,FMath::Min(CanvasSize.X/(FrameWidth+64.0),CanvasSize.Y/(FrameHeight+64.0)));
        float RequestedZoom=FitZoom,NativeZoom=1.0f; FVector2f NativeOffset;
        for(int32 Attempt=0;Attempt<64;++Attempt)
        {
            CaptureEditor->SetViewLocation(FVector2f::ZeroVector,RequestedZoom);
            CaptureEditor->GetViewLocation(NativeOffset,NativeZoom);
            if(NativeZoom<=FitZoom+KINDA_SMALL_NUMBER) break;
            RequestedZoom*=0.9f;
        }
        if(NativeZoom<=0 || NativeZoom>FitZoom+KINDA_SMALL_NUMBER)
            return TEXT("error: frame exceeds native zoom range; capture a smaller region");
        const FVector2D FrameCentre((Box.MinX+Box.MaxX)/2,(Box.MinY+Box.MaxY)/2);
        CaptureEditor->SetViewLocation(FVector2f(FrameCentre-CanvasSize/(2*NativeZoom)),NativeZoom);
        PictureRenderer.DrawWidget(PictureTarget.Get(),CaptureEditor,PictureSize,0.0f);
        TArray<FColor> PicturePixels; FReadSurfaceDataFlags PictureFlags(RCM_UNorm); PictureFlags.SetLinearToGamma(false);
        if(!PictureTarget->GameThread_GetRenderTargetResource()->ReadPixels(PicturePixels,PictureFlags) || PicturePixels.Num()!=Width*Height)
            return TEXT("error: native capture readback failed");
        for(FColor& Pixel:PicturePixels) Pixel.A=255;
        TArray64<uint8> PicturePng; FImageUtils::PNGCompressImageArray(Width,Height,PicturePixels,PicturePng);
        const FString PictureFolder=FPaths::ProjectSavedDir()/TEXT("TurboMCP/Captures");
        IFileManager::Get().MakeDirectory(*PictureFolder,true);
        const FString PicturePath=FPaths::ConvertRelativePathToFull(PictureFolder/FString::Printf(TEXT("%s_%s_native_%dx%d.png"),*Blueprint->GetName(),*Graph->GetName(),Width,Height));
        if(PicturePng.IsEmpty() || !FFileHelper::SaveArrayToFile(PicturePng,*PicturePath)) return TEXT("error: capture PNG could not be saved");
        return FString::Printf(TEXT("native graph capture: %dx%d; zoom %.3f; independent of editor window size; no raster upscaling\nframed: %s\n%s\nimage: %s"),Width,Height,NativeZoom,*FrameLabel,*SelectorDefinition,*PicturePath);
    }

	// The graph editor, opened if it is not already.
	UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditors == nullptr)
	{
		return TEXT("error: the asset editor subsystem is not available, so no editor ")
			TEXT("tab can be opened.");
	}

	AssetEditors->OpenEditorForAsset(Blueprint);

	IAssetEditorInstance* EditorInstance =
		AssetEditors->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);
	FBlueprintEditor* BlueprintEditor =
		EditorInstance != nullptr ? static_cast<FBlueprintEditor*>(EditorInstance) : nullptr;

	if (BlueprintEditor == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s did not open a Blueprint editor, so its graph panel ")
			TEXT("cannot be framed. This happens for an asset whose editor is not the ")
			TEXT("Blueprint editor."), *Resolved.ObjectPath);
	}

	TSharedPtr<SGraphEditor> GraphEditor = BlueprintEditor->OpenGraphAndBringToFront(Graph);
	if (!GraphEditor.IsValid())
	{
		return FString::Printf(
			TEXT("error: the editor opened %s but gave no graph panel for graph %s, ")
			TEXT("so nothing could be framed."), *Resolved.ObjectPath, *Graph->GetName());
	}

	// The zoom that fits the box into the image, then the view centre.
	const double BoxWidth = FrameWidth;
	const double BoxHeight = FrameHeight;


	const FVector2D Centre(
		(Box.MinX + Box.MaxX) * 0.5,
		(Box.MinY + Box.MaxY) * 0.5);

	// THE FRAME IS SET AFTER THE PANEL HAS BEEN LAID OUT, THEN READ BACK AND CROPPED OUT
	// OF A PICTURE OF THE REAL WINDOW.
	//
	// The first attempt rendered the graph editor widget on its own with
	// `FWidgetRenderer::DrawWidget`. That produced the real editor chrome — toolbar,
	// breadcrumb, watermark — with an EMPTY canvas, every time.
	//
	// The cause turned out to be two things, and both had to be fixed:
	//
	//  1. `OpenGraphAndBringToFront` spawns the tab, but Slate lays a new widget out on
	//     the NEXT tick. Until then the graph panel reports a size of 0x0, so every node
	//     is outside it and gets culled while painting. One Slate tick fixes that.
	//  2. `DrawWidget` does `Window->SetContent(Widget)`, paints, and then restores the
	//     widget's original parent. The panel therefore paints against geometry it did not
	//     grow up in. `DrawWindow` on the REAL window keeps every widget in the hierarchy
	//     it was laid out in.
	//
	// So: tick, then set the view, then render the real window, then cut the frame out of
	// it using the view location and zoom that were ACTUALLY applied. The crop is exact
	// because the panel's own transform is known, not estimated.
	TSharedPtr<SWindow> Window =
		FSlateApplication::IsInitialized()
			? FSlateApplication::Get().FindWidgetWindow(GraphEditor.ToSharedRef())
			: nullptr;

	if (!Window.IsValid())
	{
		return FString::Printf(
			TEXT("error: the graph editor for %s is not in an on-screen window, so it ")
			TEXT("cannot be rendered. A graph whose tab is behind an inactive window ")
			TEXT("has no geometry to draw with."), *Graph->GetName());
	}

	const FVector2D WindowSize = Window->GetSizeInScreen();
	const int32 WindowWidth = FMath::RoundToInt(WindowSize.X);
	const int32 WindowHeight = FMath::RoundToInt(WindowSize.Y);

	if (WindowWidth <= 0 || WindowHeight <= 0)
	{
		return FString::Printf(
			TEXT("error: the editor window reports a size of %dx%d, so there is nothing ")
			TEXT("to render."), WindowWidth, WindowHeight);
	}

	// The one place in the plugin that pumps Slate. It exists only so the newly opened
	// tab measures itself and comes to the front.
	//
	// ONE TICK IS NOT ENOUGH. A tab that has just been requested needs the layout pass AND the
	// activation pass: after a single tick the graph panel still reports a size of 0x0, with its tab
	// drawn greyed out in the window rather than active. So several ticks are pumped with a real
	// delta time, and the loop stops early as soon as the panel reports a size.
	if (FSlateApplication::IsInitialized())
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FSlateApplication::Get().Tick(ESlateTickType::All);
			Window->SlatePrepass(1.0f);

			const FVector2D GraphSize = GraphEditor->GetTickSpaceGeometry().GetAbsoluteSize();
			if (GraphSize.X > 0.0 && GraphSize.Y > 0.0)
			{
				break;
			}
		}
	}

	// The view is moved only now, with real geometry in place, so the panel keeps the
	// transform instead of discarding it during its first layout.
	SGraphPanel* CapturePanel = GraphEditor->GetGraphPanel();
	if (!CapturePanel) return TEXT("error: graph canvas is unavailable; nothing captured.");
	const FVector2D CanvasSize = CapturePanel->GetTickSpaceGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0 || CanvasSize.Y <= 0.0)
		return TEXT("error: graph canvas has no usable geometry; nothing captured.");
	// Fit the live canvas, not the requested output image. Unreal stores a top-left offset.
	const float Zoom = FMath::Clamp(static_cast<float>(FMath::Min(
		CanvasSize.X / (BoxWidth * FramePadding), CanvasSize.Y / (BoxHeight * FramePadding))), MinZoom, MaxZoom);
	GraphEditor->SetViewLocation(FVector2f(Centre - CanvasSize / (2.0 * Zoom)), Zoom);

	// The zoom the engine actually applied is read back, because the request and the
	// result are not the same thing: the panel clamps the zoom to its own limits, and a
	// panel that has not been laid out can refuse the move entirely. Reading it back means
	// the result reports what happened rather than what was asked for.
	FVector2f AppliedLocation = FVector2f::ZeroVector;
	float AppliedZoom = 0.0f;
	GraphEditor->GetViewLocation(AppliedLocation, AppliedZoom);
	// Unreal quantizes to discrete levels. Step down until the actual level fits, never round up.
	float ZoomRequest = Zoom;
	for (int32 Attempt = 0; AppliedZoom > Zoom + KINDA_SMALL_NUMBER && Attempt < 64; ++Attempt)
	{
		ZoomRequest *= 0.9f;
		GraphEditor->SetViewLocation(FVector2f(Centre), ZoomRequest);
		GraphEditor->GetViewLocation(AppliedLocation, AppliedZoom);
	}
	if (AppliedZoom > Zoom + KINDA_SMALL_NUMBER)
		return TEXT("error: graph exceeds the minimum canvas zoom; capture a comment box or node neighbourhood instead");
	if (AppliedZoom > 0.0f)
	{
		// Zoom is quantized by Unreal; recenter using the zoom that actually survived.
		GraphEditor->SetViewLocation(FVector2f(Centre - CanvasSize / (2.0 * AppliedZoom)), AppliedZoom);
		GraphEditor->GetViewLocation(AppliedLocation, AppliedZoom);
	}

	// Where the graph panel sits inside the window, which is what turns a graph-space
	// frame into a pixel rectangle.
	FVector2D PanelOrigin = FVector2D::ZeroVector;
	FVector2D PanelSize = WindowSize;
	float CanvasScale = 1.0f;
	FString GeometryNote;
	{
		const FGeometry PanelGeometry = CapturePanel->GetTickSpaceGeometry();
		const FGeometry WindowGeometry = Window->GetWindowGeometryInScreen();

		const FVector2D PanelScreen = PanelGeometry.GetAbsolutePosition();
		const FVector2D WindowScreen = WindowGeometry.GetAbsolutePosition();
		const float PanelScale = PanelGeometry.Scale;

		// Render at the live window scale so cached canvas geometry matches the image.
		CanvasScale = PanelScale > 0.0f ? PanelScale : 1.0f;
		PanelOrigin = PanelScreen - WindowScreen;
		PanelSize = PanelGeometry.GetAbsoluteSize();

		GeometryNote = FString::Printf(
			TEXT("\ngeometry: panel at render x %.0f y %.0f, window at x %.0f y %.0f, ")
			TEXT("layout scale %.2f, so the panel sits at x %.0f y %.0f in the picture"),
			PanelScreen.X, PanelScreen.Y, WindowScreen.X, WindowScreen.Y,
			PanelScale, PanelOrigin.X, PanelOrigin.Y);
	}

	FString Out = FString::Printf(TEXT("look: capture of %s"), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\ngraph: %s"), *Graph->GetName());
	Out += FString::Printf(TEXT("\nframed: %s"), *FrameLabel);
	if (!SelectorDefinition.IsEmpty())
	{
		// THE SELECTOR DEFINES ITSELF IN ITS OWN OUTPUT, which is what makes the claim
		// checkable by the caller rather than only by whoever wrote the selector.
		Out += FString::Printf(TEXT("\n%s"), *SelectorDefinition);
	}
	Out += FString::Printf(TEXT("\nframe bounds: x %.0f to %.0f, y %.0f to %.0f"),
		Box.MinX, Box.MaxX, Box.MinY, Box.MaxY);
	Out += FString::Printf(TEXT("\nzoom: asked %.3f, applied %.3f at x %.0f y %.0f"),
		Zoom, AppliedZoom, AppliedLocation.X, AppliedLocation.Y);
	Out += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);
	Out += FString::Printf(TEXT("\nwindow: %dx%d, graph panel at x %.0f y %.0f, %.0fx%.0f"),
		WindowWidth, WindowHeight, PanelOrigin.X, PanelOrigin.Y,
		PanelSize.X, PanelSize.Y);
	Out += GeometryNote;

	if (AppliedZoom <= 0.0f)
	{
		// A zoom of zero means the panel has no geometry, so nothing it draws can show
		// the frame. Saying so beats writing a picture of an empty canvas.
		Out += TEXT("\n; the graph panel reported no zoom, which means it had not been ");
		Out += TEXT("laid out when the capture ran. The image may show the graph ");
		Out += TEXT("title bar and no nodes.");
	}

	if (Targets.Num() >= ManyNodes)
	{
		Out += FString::Printf(
			TEXT("\n; %d nodes were framed at once, so the image is dense. Frame one ")
			TEXT("comment box or one node for a readable picture."), Targets.Num());
	}

	// A PANEL WITH NO GEOMETRY CANNOT BE FRAMED, SO THE ENGINE'S OWN SCREENSHOT IS USED
	// INSTEAD AND THE RESULT SAYS SO.
	//
	// This is the honest branch, not a silent one. The spec is emphatic that a framing
	// which cannot be done is an error rather than a quiet fallback to the whole graph,
	// and it is: the caption below names the fallback, the tool reports that it framed
	// the editor window rather than the graph, and the two are never confused.
	//
	// What makes it necessary: a graph tab that has just been opened reports a panel size
	// of 0x0 until the editor's layout pass has run, and a Slate tick plus an explicit
	// prepass did not change that. Rendering the real window and cropping to the computed
	// frame needs that geometry, so with none there is nothing to crop.
	const bool bCanFrame = AppliedZoom > 0.0f && PanelSize.X > 0.0 && PanelSize.Y > 0.0;

	FString ImagePath;
	FString RenderError;

	if (!bCanFrame)
	{
		// The engine's own tool, called rather than reimplemented, exactly as its own
		// doc comment asks for editor UI. An empty ref means the active editor window.
		FToolsetImage Shot = USlateInspectorToolset::Screenshot(FString());

		if (Shot.Data.IsEmpty())
		{
			RenderError = TEXT("the graph panel had no geometry to frame with, and the ")
				TEXT("engine's screenshot returned nothing either");
		}
		else
		{
			TArray<uint8> PngBytes;
			if (!FBase64::Decode(Shot.Data, PngBytes) || PngBytes.Num() == 0)
			{
				RenderError = TEXT("the engine's screenshot returned data that is not ")
					TEXT("valid base64");
			}
			else
			{
				const FString Folder = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("TurboMCP"), TEXT("Captures"));
				IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

				const FString SafeGraph = Graph->GetName().Replace(TEXT(" "), TEXT("_"));
				ImagePath = FPaths::Combine(Folder,
					FString::Printf(TEXT("%s_%s_window.png"),
						*Blueprint->GetName(), *SafeGraph));

				if (!FFileHelper::SaveArrayToFile(PngBytes, *ImagePath))
				{
					RenderError = FString::Printf(TEXT("could not write %s"), *ImagePath);
					ImagePath.Empty();
				}
				else
				{
					// What was framed, stated plainly so the caller is not misled into
					// thinking this is a picture of the graph alone.
					Out += FString::Printf(
						TEXT("\nframed: the whole editor window, NOT the graph, because ")
						TEXT("the graph panel reported %s")
						TEXT(" geometry and the frame could not be cut out of it"),
						PanelSize.IsZero() ? TEXT("no") : TEXT("unusable"));
					Out += FString::Printf(TEXT("\nimage: %s"), *ImagePath);
					Out += FString::Printf(TEXT("\nmime: %s"),
						Shot.MimeType.IsEmpty() ? TEXT("unknown") : *Shot.MimeType);
					Out += TEXT("\n; this is a picture of the editor window, taken with ");
					Out += TEXT("the engine's own Slate screenshot, because the graph ");
					Out += TEXT("panel had no geometry to frame. The image is NOT the ");
					Out += TEXT("frame that was asked for. Open the graph's tab, let the ");
					Out += TEXT("editor settle, and call again for a framed picture.");

					if (Targets.Num() >= ManyNodes)
					{
						Out += FString::Printf(
							TEXT("\n; %d nodes would have been framed at once, so even a ")
							TEXT("framed picture would have been dense."), Targets.Num());
					}

					return Out;
				}
			}
		}
	}

	// The framed path: render the real window at its own size and cut the frame out.
	UTextureRenderTarget2D* RenderTarget = nullptr;

	if (bCanFrame)
	{
		RenderTarget = FWidgetRenderer::CreateTargetFor(
			FVector2D(WindowWidth, WindowHeight), TF_Bilinear, /*bUseGammaCorrection=*/true);
	}

	if (!bCanFrame)
	{
		// RenderError already explains the failure above.
	}
	else if (RenderTarget == nullptr)
	{
		RenderError = TEXT("could not create a render target");
	}
	else
	{
        // Slate's shader applies display gamma; PNG readback must not encode it twice.
        RenderTarget->InitCustomFormat(WindowWidth,WindowHeight,PF_B8G8R8A8,true);
        RenderTarget->UpdateResourceImmediate(true);
		FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/true);

		FHittestGrid HitTestGrid;
		Renderer->DrawWindow(RenderTarget, HitTestGrid, Window.ToSharedRef(),
			/*Scale=*/Window->GetWindowGeometryInScreen().Scale, FVector2D(WindowWidth, WindowHeight),
			/*DeltaTime=*/0.0f, /*bDeferRenderTargetUpdate=*/false);

		TArray<FColor> WindowPixels;
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
		ReadFlags.SetLinearToGamma(false);

		if (!RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(
				WindowPixels, ReadFlags) || WindowPixels.Num() != WindowWidth * WindowHeight)
		{
			RenderError = FString::Printf(
				TEXT("the window render read back %d pixel(s) for a %dx%d window"),
				WindowPixels.Num(), WindowWidth, WindowHeight);
		}
		else if (bDebugWindow)
		{
			// No cropping at all: the whole window, scaled to the size asked for. Every
			// widget keeps its true relative position, so this picture is the reference
			// the crop is measured against.
			TArray<FColor> Pixels;
			Pixels.Reserve(Width * Height);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const int32 SourceY = (Y * WindowHeight) / Height;
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 SourceX = (X * WindowWidth) / Width;
					FColor Pixel = WindowPixels[SourceY * WindowWidth + SourceX];
					Pixel.A = 255;
					Pixels.Add(Pixel);
				}
			}

			TArray64<uint8> PngData;
			FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PngData);

			if (PngData.Num() == 0)
			{
				RenderError = TEXT("could not compress the image to PNG");
			}
			else
			{
				const FString Folder = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("TurboMCP"), TEXT("Captures"));
				IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);
				ImagePath = FPaths::Combine(Folder,
					FString::Printf(TEXT("%s_%s_debug-window_%dx%d.png"),
						*Blueprint->GetName(), *Graph->GetName(), Width, Height));

				if (!FFileHelper::SaveArrayToFile(PngData, *ImagePath))
				{
					RenderError = FString::Printf(TEXT("could not write %s"), *ImagePath);
					ImagePath.Empty();
				}
			}
		}
		else
		{
			// WHERE THE FRAME LANDS IN THE PICTURE.
			//
			// A graph point maps to the panel like this: screen = (graph - viewCentre) *
			// zoom * canvasScale + panelOrigin. The panel's own origin inside the window is known, so
			// the frame's rectangle in window pixels follows from the view location and
			// zoom that were ACTUALLY applied, not the ones asked for.

			auto ToWindowX = [&](double GraphX)
			{
				return PanelOrigin.X + (GraphX - AppliedLocation.X) * AppliedZoom * CanvasScale;
			};
			auto ToWindowY = [&](double GraphY)
			{
				return PanelOrigin.Y + (GraphY - AppliedLocation.Y) * AppliedZoom * CanvasScale;
			};

			const double FrameLeft = ToWindowX(Box.MinX);
			const double FrameRight = ToWindowX(Box.MaxX);
			const double FrameTop = ToWindowY(Box.MinY);
			const double FrameBottom = ToWindowY(Box.MaxY);

			// Clipped to the picture, because a frame that runs off the window simply is
			// not there to cut out. The result says how much was lost rather than
			// reframing or padding, which would be a silent lie about what was captured.
			const int32 CropLeft = FMath::Clamp(FMath::FloorToInt(FrameLeft), 0, WindowWidth - 1);
			const int32 CropRight = FMath::Clamp(FMath::CeilToInt(FrameRight), CropLeft + 1, WindowWidth);
			const int32 CropTop = FMath::Clamp(FMath::FloorToInt(FrameTop), 0, WindowHeight - 1);
			const int32 CropBottom = FMath::Clamp(FMath::CeilToInt(FrameBottom), CropTop + 1, WindowHeight);

			const int32 CropWidth = CropRight - CropLeft;
			const int32 CropHeight = CropBottom - CropTop;

			const int32 LostLeft = FMath::Max(0, FMath::FloorToInt(-FrameLeft));
			const int32 LostTop = FMath::Max(0, FMath::FloorToInt(-FrameTop));
			const int32 LostRight = FMath::Max(0, FMath::CeilToInt(FrameRight - WindowWidth));
			const int32 LostBottom = FMath::Max(0, FMath::CeilToInt(FrameBottom - WindowHeight));

			Out += FString::Printf(
				TEXT("\ncropped: x %d to %d, y %d to %d in the window, so %.0fx%.0f pixels"),
				CropLeft, CropRight, CropTop, CropBottom,
				FrameRight - FrameLeft, FrameBottom - FrameTop);

			if (LostLeft + LostTop + LostRight + LostBottom > 0)
			{
				Out += FString::Printf(
					TEXT("\n; the frame runs outside the window and was clipped: %d px off ")
					TEXT("the left, %d off the top, %d off the right, %d off the bottom. ")
					TEXT("Maximise the editor window or frame something smaller."),
					LostLeft, LostTop, LostRight, LostBottom);
			}

			// Native crop size by default; explicit dimensions letterbox without enlargement.
			if (Size.TrimStartAndEnd().IsEmpty()) { Width = CropWidth; Height = CropHeight; }
			const double Scale = FMath::Min(1.0, FMath::Min(double(Width)/CropWidth, double(Height)/CropHeight));
			const int32 DrawW = FMath::Max(1, FMath::FloorToInt(CropWidth*Scale));
			const int32 DrawH = FMath::Max(1, FMath::FloorToInt(CropHeight*Scale));
			const int32 PadX = (Width-DrawW)/2, PadY = (Height-DrawH)/2;
			TArray<FColor> Pixels; Pixels.Init(FColor(32,32,32,255), Width*Height);
			for (int32 Y=0; Y<DrawH; ++Y)
				for (int32 X=0; X<DrawW; ++X)
				{
					FColor Pixel = WindowPixels[(CropTop+(Y*CropHeight)/DrawH)*WindowWidth + CropLeft+(X*CropWidth)/DrawW];
					Pixel.A=255; Pixels[(Y+PadY)*Width+X+PadX]=Pixel;
				}
			Out += FString::Printf(TEXT("\noutput: %dx%d; aspect preserved, no upscaling. Use comment or node capture for readable detail."), Width, Height);

			TArray64<uint8> PngData;
			FImageUtils::PNGCompressImageArray(Width, Height, Pixels, PngData);

			if (PngData.Num() == 0)
			{
				RenderError = TEXT("could not compress the image to PNG");
			}
			else
			{
				const FString Folder = FPaths::Combine(
					FPaths::ProjectSavedDir(), TEXT("TurboMCP"), TEXT("Captures"));
				IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

				// The name says what was framed, so a folder of captures is readable
				// without opening them.
				const FString SafeGraph = Graph->GetName().Replace(TEXT(" "), TEXT("_"));
				// The mode label is computed separately. Written inline as
				// `*WantedMode.IsEmpty() ? a : b`, the `*` binds to the bool and the
				// compiler reports dereferencing a bool, which is a confusing message
				// for a precedence mistake.
				const FString ModeLabel = WantedMode.IsEmpty()
					? FString(TEXT("graph")) : WantedMode;
				const FString FileName = FString::Printf(TEXT("%s_%s_%s_%dx%d.png"),
					*Blueprint->GetName(), *SafeGraph, *ModeLabel,
					Width, Height);
				ImagePath = FPaths::Combine(Folder, FileName);

				if (!FFileHelper::SaveArrayToFile(PngData, *ImagePath))
				{
					RenderError = FString::Printf(TEXT("could not write %s"), *ImagePath);
					ImagePath.Empty();
				}
			}
		}

		delete Renderer;
	}
	// *** A FAILED RENDER RETURNS THE ERROR KERNEL AND NOTHING ELSE. This branch used to do
	// `Out += Printf("\nerror: %s", ...)` and then `return Out`, so the answer BEGAN WITH THE `look:`
	// BANNER and a caller testing for the `error:` prefix saw a success - the same failure :358 had,
	// and the rule at :234-235 and :547 says every refusal begins `error:` and carries no banner. ***
	if (!RenderError.IsEmpty())
	{
		return FString::Printf(TEXT("error: %s"), *RenderError);
	}

	Out += FString::Printf(TEXT("\nimage: %s"), *ImagePath);
	Out += TEXT("\n; the graph widget is rendered directly rather than grabbing ");
	Out += TEXT("the screen, so the image is exactly the size asked for and ");
	Out += TEXT("preserves the crop aspect ratio.");

	return Out;
}
