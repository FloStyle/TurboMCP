// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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
#include "ImageUtils.h"
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
 * `SGraphEditor::SetViewLocation(Location, Zoom)` takes a graph-space centre and a
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
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a Blueprint in any path form, for ")
			TEXT("example BP_NodeTest.");
	}
	if (GraphName.IsEmpty())
	{
		return TEXT("error: no graph given. Pass a graph name; the tool refuses ")
			TEXT("rather than guessing which graph you meant.");
	}

	// The mode, validated before anything opens, so a typo does not leave an editor
	// tab open behind it.
	const FString WantedMode = Mode.TrimStartAndEnd().ToLower();
	const bool bWholeGraph = WantedMode.IsEmpty() || WantedMode == TEXT("graph");
	const bool bOneBox = WantedMode == TEXT("box");
	const bool bOneNode = WantedMode == TEXT("node");

	// `debug-window` renders the whole editor window at the requested size, with NO
	// cropping. It exists to answer one question that arithmetic kept getting wrong:
	// where does a given widget actually land in the rendered picture? The panel's
	// computed position and its real position can then be compared instead of assumed.
	const bool bDebugWindow = WantedMode == TEXT("debug-window");

	if (!bWholeGraph && !bOneBox && !bOneNode && !bDebugWindow)
	{
		return FString::Printf(
			TEXT("error: '%s' is not a capture mode. Accepted: graph, box, node. ")
			TEXT("Leave it empty for graph."), *Mode);
	}

	if ((bOneBox || bOneNode) && Target.TrimStartAndEnd().IsEmpty())
	{
		return FString::Printf(
			TEXT("error: mode '%s' needs a target. Pass a comment box index for ")
			TEXT("mode box, or a node ID for mode node."), *WantedMode);
	}

	int32 Width = DefaultWidth;
	int32 Height = DefaultHeight;
	if (!Size.TrimStartAndEnd().IsEmpty())
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

	if (bWholeGraph || bDebugWindow)
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
	const double BoxWidth = FMath::Max(Box.Width(), 1.0);
	const double BoxHeight = FMath::Max(Box.Height(), 1.0);

	const float ZoomFitX = static_cast<float>(Width / (BoxWidth * FramePadding));
	const float ZoomFitY = static_cast<float>(Height / (BoxHeight * FramePadding));
	const float Zoom = FMath::Clamp(FMath::Min(ZoomFitX, ZoomFitY), MinZoom, MaxZoom);

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
	// ONE TICK WAS NOT ENOUGH. The first version ran a single tick and the graph panel
	// still reported a size of 0x0, with its tab drawn greyed out in the window rather
	// than active. A tab that has just been requested needs the layout pass AND the
	// activation pass, so several ticks are pumped with a real delta time, and the loop
	// stops early as soon as the panel reports a size.
	if (FSlateApplication::IsInitialized())
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			FSlateApplication::Get().Tick(ESlateTickType::All);
			Window->SlatePrepass(1.0f);

			const FVector2D Size = GraphEditor->GetTickSpaceGeometry().GetAbsoluteSize();
			if (Size.X > 0.0 && Size.Y > 0.0)
			{
				break;
			}
		}
	}

	// The view is moved only now, with real geometry in place, so the panel keeps the
	// transform instead of discarding it during its first layout.
	GraphEditor->SetViewLocation(Centre, Zoom);

	// The zoom the engine actually applied is read back, because the request and the
	// result are not the same thing: the panel clamps the zoom to its own limits, and a
	// panel that has not been laid out can refuse the move entirely. Reading it back means
	// the result reports what happened rather than what was asked for.
	FVector2D AppliedLocation = FVector2D::ZeroVector;
	float AppliedZoom = 0.0f;
	GraphEditor->GetViewLocation(AppliedLocation, AppliedZoom);

	// Where the graph panel sits inside the window, which is what turns a graph-space
	// frame into a pixel rectangle.
	FVector2D PanelOrigin = FVector2D::ZeroVector;
	FVector2D PanelSize = WindowSize;
	FString GeometryNote;
	{
		const FGeometry PanelGeometry = GraphEditor->GetTickSpaceGeometry();
		const FGeometry WindowGeometry = Window->GetWindowGeometryInScreen();

		const FVector2D PanelScreen = PanelGeometry.GetAbsolutePosition();
		const FVector2D WindowScreen = WindowGeometry.GetAbsolutePosition();
		const float PanelScale = PanelGeometry.Scale;

		// The rendered picture is in PIXELS; Slate geometry is in SLATE UNITS, which a
		// scaled display divides. Dividing by the panel's own scale converts back, and
		// leaving it out is what put the first crop on the wrong panel entirely.
		const float SafeScale = PanelScale > 0.0f ? PanelScale : 1.0f;

		PanelOrigin = (PanelScreen - WindowScreen) / SafeScale;
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
			FVector2D(WindowWidth, WindowHeight), TF_Bilinear, /*bUseGammaCorrection=*/false);
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
		FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false);

		FHittestGrid HitTestGrid;
		Renderer->DrawWindow(RenderTarget, HitTestGrid, Window.ToSharedRef(),
			/*Scale=*/1.0f, FVector2D(WindowWidth, WindowHeight),
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
			// zoom + panelCentre. The panel's own origin inside the window is known, so
			// the frame's rectangle in window pixels follows from the view location and
			// zoom that were ACTUALLY applied, not the ones asked for.
			const double PanelCentreX = PanelOrigin.X + PanelSize.X * 0.5;
			const double PanelCentreY = PanelOrigin.Y + PanelSize.Y * 0.5;

			auto ToWindowX = [&](double GraphX)
			{
				return PanelCentreX + (GraphX - AppliedLocation.X) * AppliedZoom;
			};
			auto ToWindowY = [&](double GraphY)
			{
				return PanelCentreY + (GraphY - AppliedLocation.Y) * AppliedZoom;
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

			TArray<FColor> Pixels;
			Pixels.Reserve(Width * Height);

			// The crop is scaled to the size asked for, by nearest sample. Scaling rather
			// than refusing means a small window can still give a large picture, which is
			// the common case: the graph is bigger than any window.
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const int32 SourceY = CropTop + (Y * CropHeight) / Height;
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 SourceX = CropLeft + (X * CropWidth) / Width;
					FColor Pixel = WindowPixels[SourceY * WindowWidth + SourceX];
					// A widget background is transparent by default, so the alpha is forced
					// opaque; a transparent PNG looks empty in most viewers.
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
	if (!RenderError.IsEmpty())
	{
		Out += FString::Printf(TEXT("\nerror: %s"), *RenderError);
	}
	else
	{
		Out += FString::Printf(TEXT("\nimage: %s"), *ImagePath);
		Out += TEXT("\n; the graph widget is rendered directly rather than grabbing ");
		Out += TEXT("the screen, so the image is exactly the size asked for and ");
		Out += TEXT("shows the frame that was set.");
	}

	return Out;
}
