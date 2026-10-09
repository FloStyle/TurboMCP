// Copyright FloStyle. All Rights Reserved.

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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"
#include "Slate/WidgetRenderer.h"

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

	if (!bWholeGraph && !bOneBox && !bOneNode)
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

	if (bWholeGraph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node != nullptr && !Node->IsA<UEdGraphNode_Comment>())
			{
				Targets.Add(Node);
			}
		}
		FrameLabel = FString::Printf(TEXT("the whole graph, %d node(s)"), Targets.Num());
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

	GraphEditor->SetViewLocation(Centre, Zoom);

	// The zoom the engine actually applied is read back, because the request and the
	// result are not the same thing: the panel clamps the zoom to its own limits, and a
	// panel that has not been laid out yet can refuse the move entirely. Reading it back
	// means the result reports what happened rather than what was asked for.
	FVector2D AppliedLocation = FVector2D::ZeroVector;
	float AppliedZoom = 0.0f;
	GraphEditor->GetViewLocation(AppliedLocation, AppliedZoom);

	// A tick is what makes the node panel recompute which nodes are on screen. Setting
	// the view alone is not enough: the panel culls off-screen nodes while drawing, so a
	// view that has not taken effect renders an empty canvas with the chrome around it,
	// which looks like a successful capture of an empty graph.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ForceRedrawWindow(
			FSlateApplication::Get().FindWidgetWindow(GraphEditor.ToSharedRef()).ToSharedRef());
	}

	FString Out = FString::Printf(TEXT("look: capture of %s"), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\ngraph: %s"), *Graph->GetName());
	Out += FString::Printf(TEXT("\nframed: %s"), *FrameLabel);
	Out += FString::Printf(TEXT("\nframe bounds: x %.0f to %.0f, y %.0f to %.0f"),
		Box.MinX, Box.MaxX, Box.MinY, Box.MaxY);
	Out += FString::Printf(TEXT("\nzoom: asked %.3f, applied %.3f at x %.0f y %.0f"),
		Zoom, AppliedZoom, AppliedLocation.X, AppliedLocation.Y);
	Out += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);

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

	FString ImagePath;
	FString RenderError;

	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		FVector2D(Width, Height), TF_Bilinear, /*bUseGammaCorrection=*/false);

	if (RenderTarget == nullptr)
	{
		RenderError = TEXT("could not create a render target");
	}
	else
	{
		FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false);
		Renderer->DrawWidget(RenderTarget, GraphEditor.ToSharedRef(),
			FVector2D(Width, Height), /*InScale=*/1.0f, /*bDeferRenderTargetUpdate=*/false);

		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
		ReadFlags.SetLinearToGamma(false);

		if (!RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(
				Pixels, ReadFlags) || Pixels.Num() != Width * Height)
		{
			RenderError = FString::Printf(
				TEXT("the render target read back %d pixel(s) for a %dx%d image"),
				Pixels.Num(), Width, Height);
		}
		else
		{
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
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
