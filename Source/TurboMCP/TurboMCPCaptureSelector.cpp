// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCapture.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "K2Node_Knot.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"
#include "Widgets/SWindow.h"

namespace
{
	/** The largest edge rendered, the same constant `TurboMCPCapture.cpp` uses. */
	constexpr int32 ScreenMaxEdge = 4096;

	/** The folder every capture goes to. */
	const FString CaptureFolderName = TEXT("Captures");

	/** Said in words, so an absent measurement never prints as a zero (`P1`). */
	const TCHAR* const NotAvailable = TEXT("NOT AVAILABLE");

	/** True when the widget is a plain reroute knot. */
	bool IsKnot(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->IsA<UK2Node_Knot>();
	}

	/**
	 * The nodes one link away from InNode, WITHOUT COUNTING A KNOT AS A STEP.
	 *
	 * THIS IS `D-42` IN CODE. The engine stores a wire that runs through a reroute
	 * knot as two links: A to the knot, and the knot to B. The reader hides knots
	 * (`04_FORMATS.md` section 2.1 rule 7), so from the graph the reader prints that
	 * is ONE link, and the hop count here has to agree with it.
	 *
	 * A KNOT IS NEVER ADDED TO THE RESULT. A frame containing a knot would contain a
	 * node the agent never saw named, which is the failure `D-42` was written about.
	 */
	void DirectNeighbours(const UEdGraphNode* InNode, TArray<UEdGraphNode*>& OutNodes)
	{
		TSet<UEdGraphNode*> Seen;
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked == nullptr)
				{
					continue;
				}
				UEdGraphNode* Far = Linked->GetOwningNodeUnchecked();
				if (Far == nullptr)
				{
					continue;
				}

				// Walked THROUGH a knot rather than stopping on it. The loop is
				// written with an explicit guard so a knot wired to itself cannot
				// spin: an already visited knot is skipped rather than followed.
				TSet<UEdGraphNode*> Guard;
				while (IsKnot(Far))
				{
					if (Guard.Contains(Far))
					{
						Far = nullptr;
						break;
					}
					Guard.Add(Far);

					UEdGraphNode* Next = nullptr;
					for (const UEdGraphPin* KnotPin : Far->Pins)
					{
						if (KnotPin == nullptr)
						{
							continue;
						}
						for (const UEdGraphPin* KnotLinked : KnotPin->LinkedTo)
						{
							if (KnotLinked == nullptr)
							{
								continue;
							}
							UEdGraphNode* Candidate = KnotLinked->GetOwningNodeUnchecked();
							if (Candidate != nullptr && Candidate != Far)
							{
								Next = Candidate;
								break;
							}
						}
						if (Next != nullptr)
						{
							break;
						}
					}
					Far = Next;
				}

				if (Far != nullptr && Far != InNode)
				{
					Seen.Add(Far);
				}
			}
		}

		OutNodes.Reserve(OutNodes.Num() + Seen.Num());
		for (UEdGraphNode* Node : Seen)
		{
			OutNodes.Add(Node);
		}
	}

	/**
	 * The nodes within InHops hops of InStart, with the root always included.
	 *
	 * InHops counts HOPS, so `0` frames the node alone and `1` adds everything one
	 * link away. `OutDistance` reports the hop count each node was reached at, which
	 * is what the result prints.
	 */
	void CollectNeighbourhood(
		UEdGraphNode* InStart, int32 InHops,
		TArray<UEdGraphNode*>& OutNodes, TMap<UEdGraphNode*, int32>& OutDistance)
	{
		OutNodes.Reset();
		OutDistance.Reset();
		if (InStart == nullptr)
		{
			return;
		}

		OutNodes.Add(InStart);
		OutDistance.Add(InStart, 0);

		TArray<UEdGraphNode*> Frontier;
		Frontier.Add(InStart);

		for (int32 Hop = 1; Hop <= InHops; ++Hop)
		{
			TArray<UEdGraphNode*> Next;
			for (UEdGraphNode* Node : Frontier)
			{
				TArray<UEdGraphNode*> Direct;
				DirectNeighbours(Node, Direct);
				for (UEdGraphNode* One : Direct)
				{
					if (OutDistance.Contains(One))
					{
						continue;
					}
					OutDistance.Add(One, Hop);
					OutNodes.Add(One);
					Next.Add(One);
				}
			}
			Frontier = Next;
			if (Frontier.Num() == 0)
			{
				break;
			}
		}
	}

	/** True when A sits inside B, with no tolerance: a box owns what it covers. */
	bool Contains(const TurboMCPCaptureSelector::FBox& InOuter, const FTurboMCPLayoutReport::FRect& InInner)
	{
		return InInner.MinX >= InOuter.MinX && InInner.MaxX <= InOuter.MaxX
			&& InInner.MinY >= InOuter.MinY && InInner.MaxY <= InOuter.MaxY;
	}

	/** The box a node belongs to, or `INDEX_NONE` when it is in no box. */
	int32 OwningBox(
		const TArray<TurboMCPCaptureSelector::FBox>& InBoxes,
		const FTurboMCPLayoutReport::FRect& InNode)
	{
		int32 Best = INDEX_NONE;
		double BestArea = 0.0;
		for (const TurboMCPCaptureSelector::FBox& Box : InBoxes)
		{
			if (!Contains(Box, InNode))
			{
				continue;
			}
			// THE SMALLEST BOX WINS, so a nested box owns its own nodes and a node
			// is never in two boxes at once. Two boxes reported as one region is a
			// different tool's job (`L-LAYOUT-REPORT` reports nesting).
			const double Area = (Box.MaxX - Box.MinX) * (Box.MaxY - Box.MinY);
			if (Best == INDEX_NONE || Area < BestArea)
			{
				Best = Box.Index;
				BestArea = Area;
			}
		}
		return Best;
	}

	/** The node IDs of a node list, in the order given. */
	void NodeIdsOf(const TArray<UEdGraphNode*>& InNodes, TArray<FString>& OutIds)
	{
		for (const UEdGraphNode* Node : InNodes)
		{
			OutIds.Add(TurboMCPNodeId::MakeForNode(Node));
		}
	}
}

void TurboMCPCaptureSelector::CollectCommentBoxes(UEdGraph* InGraph, TArray<FBox>& OutBoxes)
{
	OutBoxes.Reset();
	if (InGraph == nullptr)
	{
		return;
	}

	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			FBox Box;
			Box.Node = Comment;
			const FTurboMCPLayoutReport::FRect Rect = FTurboMCPLayoutReport::RectOf(Comment);
			Box.MinX = Rect.MinX;
			Box.MinY = Rect.MinY;
			Box.MaxX = Rect.MaxX;
			Box.MaxY = Rect.MaxY;

			// The position is taken BEFORE the vector is re-sorted, because the
			// comparison below reads the rectangles this loop is filling in.
			Box.Index = OutBoxes.Num();
			OutBoxes.Add(Box);
		}
	}

	OutBoxes.Sort([](const FBox& A, const FBox& B)
	{
		if (A.MinY != B.MinY)
		{
			return A.MinY < B.MinY;
		}
		return A.MinX < B.MinX;
	});

	// The index is assigned AFTER the sort, because an index has to mean the same
	// thing in `L-CAPTURE` as it does in `L-LAYOUT-REPORT`, and that listing is
	// ordered top to bottom then left to right.
	for (int32 Index = 0; Index < OutBoxes.Num(); ++Index)
	{
		OutBoxes[Index].Index = Index;
	}
}

bool TurboMCPCaptureSelector::Resolve(
	const FString& InMode,
	const FString& InTarget,
	int32 InHops,
	const FString& InAsset,
	UEdGraph* InGraph,
	SGraphEditor* InEditor,
	FSelection& OutSelection,
	FString& OutWhy)
{
	OutSelection = FSelection();
	OutSelection.Mode = InMode;
	OutWhy.Empty();

	if (InGraph == nullptr)
	{
		// A reason is never allowed to be empty (`P1`). This branch is reachable in
		// tests and would otherwise print a bare `error: `.
		OutWhy = FString::Printf(
			TEXT("no graph was resolved for %s, so no selector could be read from it"),
			InAsset.IsEmpty() ? TEXT("the asset") : *InAsset);
		return false;
	}

	if (InHops < 0)
	{
		OutWhy = FString::Printf(
			TEXT("the size '%d' is not a hop count. Pass a whole number of hops, 0 or more."),
			InHops);
		return false;
	}

	if (InMode == TEXT("selection"))
	{
		// WHICH SELECTION, ANSWERED RATHER THAN ASSUMED.
		//
		// The engine keeps node selection in the Slate graph panel, not on the node:
		// `FGraphPanelSelectionSet` is `TSet<UObject*>` (`GraphEditor.h:54`) and it is
		// reached through `SGraphEditor::GetSelectedNodes` (`GraphEditor.h:394-406`
		// <- `SNodePanel.h:244`). `GEditor->GetSelectedActors()`
		// (`Editor/EditorEngine.h:1969`) and `GetSelectedComponents()` (`:2000`) are
		// DIFFERENT selections, and this mode reads neither.
		if (InEditor == nullptr)
		{
			OutWhy = FString::Printf(
				TEXT("mode selection reads the graph canvas selection, and no graph editor ")
				TEXT("panel is open for %s, so there is no canvas selection to read. Open ")
				TEXT("the graph or name a different mode."),
				*InAsset);
			return false;
		}

		TArray<UEdGraphNode*> Selected;
		for (UObject* Object : InEditor->GetSelectedNodes())
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				Selected.Add(Node);
			}
		}

		// Deterministic order, so the same selection prints the same frame twice. The
		// panel's set order is not defined; the graph's own node order is.
		TArray<UEdGraphNode*> Ordered;
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Selected.Contains(Node))
			{
				Ordered.Add(Node);
			}
		}

		if (Ordered.Num() == 0)
		{
			// NOT a fallback to the whole graph: `D-41` forbids it and the spec's own
			// table says so in words.
			OutWhy = FString::Printf(
				TEXT("nothing is selected in the graph canvas of %s in graph %s, so there ")
				TEXT("is nothing to frame. Select one or more nodes, or name a different ")
				TEXT("mode. This is not a fallback to the whole graph."),
				*InAsset, *InGraph->GetName());
			return false;
		}

		OutSelection.Targets = Ordered;
		OutSelection.FrameLabel = FString::Printf(
			TEXT("the graph canvas selection, %d node(s)"), Ordered.Num());

		TArray<FString> Ids;
		NodeIdsOf(Ordered, Ids);
		OutSelection.Definition = FString::Printf(
			TEXT("selection means the nodes selected on the GRAPH CANVAS of the open ")
			TEXT("graph editor, read from SGraphEditor::GetSelectedNodes (GraphEditor.h:394). ")
			TEXT("It is NOT the level actor selection (GEditor->GetSelectedActors, ")
			TEXT("Editor/EditorEngine.h:1969), NOT the component selection ")
			TEXT("(GetSelectedComponents, :2000) and NOT the Content Browser asset ")
			TEXT("selection. Those are different selections and this selector reads none ")
			TEXT("of them, so a capture of actors is refused rather than framed.")
			TEXT("\nnodes: %s"), *FString::Join(Ids, TEXT(", ")));
		return true;
	}

	if (InMode == TEXT("neighbours"))
	{
		if (InTarget.TrimStartAndEnd().IsEmpty())
		{
			OutWhy = FString::Printf(
				TEXT("mode neighbours needs a target node ID. Pass one in the form ")
				TEXT("AssetName:GraphName:NodeKey, or an ID printed by R-NODE-FIND."));
			return false;
		}

		const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(InTarget.TrimStartAndEnd());
		if (!Parsed.IsComplete())
		{
			OutWhy = FString::Printf(
				TEXT("'%s' is not a node ID. Pass one in the form ")
				TEXT("AssetName:GraphName:NodeKey, or an ID printed by R-NODE-FIND."),
				*InTarget);
			return false;
		}

		TArray<UEdGraphNode*> Matches;
		const int32 MatchCount = TurboMCPNodeId::FindNodesByKey(InGraph, Parsed.NodeKey, Matches);
		if (MatchCount == 0)
		{
			OutWhy = FString::Printf(
				TEXT("no node in %s has the key %s, so there is no neighbourhood to frame. ")
				TEXT("Use R-NODE-FIND to get a current ID."),
				*InGraph->GetName(), *Parsed.NodeKey);
			return false;
		}
		if (MatchCount > 1)
		{
			OutWhy = FString::Printf(
				TEXT("the key %s fits %d nodes in %s, so the neighbourhood would be ")
				TEXT("ambiguous. Use more digits."),
				*Parsed.NodeKey, MatchCount, *InGraph->GetName());
			return false;
		}

		UEdGraphNode* Root = Matches[0];
		TMap<UEdGraphNode*, int32> Distance;
		CollectNeighbourhood(Root, InHops, OutSelection.Targets, Distance);

		OutSelection.FrameLabel = FString::Printf(
			TEXT("node %s plus every node within %d hop(s), %d node(s)"),
			*TurboMCPNodeId::GetNodeTitle(Root), InHops, OutSelection.Targets.Num());

		// THE DEFINITION IS THE OUTPUT (`D-42`). A hop counts non-rendered reroute
		// knots as nothing, and a knot is never framed, so the number an agent reads
		// here is the number it can act on in the graph the reader printed.
		OutSelection.Definition = FString::Printf(
			TEXT("neighbours: a hop follows ONE LINK in the graph the reader prints. A link ")
			TEXT("that runs through a plain reroute knot is therefore ONE hop and not two ")
			TEXT("(D-42, 04_FORMATS.md section 6.2), because the reader hides knots ")
			TEXT("(04_FORMATS.md section 2.1 rule 7), and a knot is NEVER framed because it ")
			TEXT("is a node the agent never saw named.")
			TEXT("\nroot: %s (hop 0)")
			TEXT("\nhops asked: %d")
			TEXT("\nnodes framed: %d"),
			*TurboMCPNodeId::MakeForNode(Root), InHops, OutSelection.Targets.Num());

		TArray<FString> Lines;
		for (UEdGraphNode* Node : OutSelection.Targets)
		{
			Lines.Add(FString::Printf(TEXT("%s at hop %d"),
				*TurboMCPNodeId::MakeForNode(Node), Distance.Contains(Node) ? Distance[Node] : -1));
		}
		OutSelection.Definition += FString::Printf(TEXT("\n%s"), *FString::Join(Lines, TEXT("\n")));
		return true;
	}

	if (InMode == TEXT("box-neighbours"))
	{
		if (InTarget.TrimStartAndEnd().IsEmpty())
		{
			OutWhy = FString::Printf(
				TEXT("mode box-neighbours needs a target. Pass a comment box index, the ")
				TEXT("index from L-LAYOUT-REPORT, in Target, and the number of linked boxes ")
				TEXT("in Size. Pass size 0 for the box alone."));
			return false;
		}
		if (!InTarget.TrimStartAndEnd().IsNumeric())
		{
			OutWhy = FString::Printf(
				TEXT("'%s' is not a comment box index. Mode box-neighbours takes a whole ")
				TEXT("number, the index from L-LAYOUT-REPORT."), *InTarget);
			return false;
		}

		const int32 Wanted = FCString::Atoi(*InTarget.TrimStartAndEnd());

		TArray<FBox> Boxes;
		CollectCommentBoxes(InGraph, Boxes);

		if (Wanted < 0 || Wanted >= Boxes.Num())
		{
			// The spec's own example, "asked for box 9, this graph has 4", in the words
			// this mode uses. Never a silent fallback to the whole graph.
			OutWhy = FString::Printf(
				TEXT("asked for box %d, this graph has %d comment box(es). Boxes are ")
				TEXT("numbered from 0 in the order L-LAYOUT-REPORT lists them, top to ")
				TEXT("bottom then left to right."),
				Wanted, Boxes.Num());
			return false;
		}

		const int32 Seed = Boxes[Wanted].Index;

		// WHICH BOX EACH NODE IS IN, so that "the boxes it links to" is decided from
		// the graph and not from a guess. The smallest enclosing box wins.
		TMap<int32, TSet<int32>> BoxAdjacency;
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || Node->IsA<UEdGraphNode_Comment>())
			{
				continue;
			}
			const int32 From = OwningBox(Boxes, FTurboMCPLayoutReport::RectOf(Node));
			if (From == INDEX_NONE)
			{
				continue;
			}

			TArray<UEdGraphNode*> Direct;
			DirectNeighbours(Node, Direct);
			for (UEdGraphNode* Far : Direct)
			{
				if (Far == nullptr || Far->IsA<UEdGraphNode_Comment>())
				{
					continue;
				}
				const int32 To = OwningBox(Boxes, FTurboMCPLayoutReport::RectOf(Far));
				if (To == INDEX_NONE || To == From)
				{
					continue;
				}
				BoxAdjacency.FindOrAdd(From).Add(To);
				BoxAdjacency.FindOrAdd(To).Add(From);
			}
		}

		// The same breadth first walk as `neighbours`, over boxes instead of nodes.
		TSet<int32> InBoxes;
		InBoxes.Add(Seed);
		TMap<int32, int32> BoxDistance;
		BoxDistance.Add(Seed, 0);

		TArray<int32> Frontier;
		Frontier.Add(Seed);
		for (int32 Hop = 1; Hop <= InHops; ++Hop)
		{
			TArray<int32> Next;
			for (int32 Box : Frontier)
			{
				if (!BoxAdjacency.Contains(Box))
				{
					continue;
				}
				for (int32 Linked : BoxAdjacency[Box])
				{
					if (InBoxes.Contains(Linked))
					{
						continue;
					}
					InBoxes.Add(Linked);
					BoxDistance.Add(Linked, Hop);
					Next.Add(Linked);
				}
			}
			Frontier = Next;
			if (Frontier.Num() == 0)
			{
				break;
			}
		}

		// The nodes of every selected box, even a node that is only in that box.
		OutSelection.Targets.Reset();
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || Node->IsA<UEdGraphNode_Comment>())
			{
				continue;
			}
			const int32 Owner = OwningBox(Boxes, FTurboMCPLayoutReport::RectOf(Node));
			if (Owner != INDEX_NONE && InBoxes.Contains(Owner))
			{
				OutSelection.Targets.Add(Node);
			}
		}

		if (OutSelection.Targets.Num() == 0)
		{
			OutWhy = FString::Printf(
				TEXT("box %d and the %d box(es) it links to hold no node, so there is ")
				TEXT("nothing to frame. An empty frame is refused rather than written as ")
				TEXT("a blank image."),
				Wanted, InHops);
			return false;
		}

		OutSelection.TargetBox = Seed;
		OutSelection.FrameLabel = FString::Printf(
			TEXT("comment box %d plus the %d box(es) it links to, %d box(es) and %d node(s)"),
			Wanted, InHops, InBoxes.Num(), OutSelection.Targets.Num());

		TArray<FString> BoxLines;
		for (const FBox& Box : Boxes)
		{
			if (InBoxes.Contains(Box.Index))
			{
				BoxLines.Add(FString::Printf(TEXT("%d at hop %d"),
					Box.Index, BoxDistance.Contains(Box.Index) ? BoxDistance[Box.Index] : -1));
			}
		}

		OutSelection.Definition = FString::Printf(
			TEXT("box-neighbours: the space and the bounds are the GRAPH SPACE rectangles of ")
			TEXT("the comment boxes, taken from the same rectangle L-LAYOUT-REPORT uses for a ")
			TEXT("node. A node belongs to the SMALLEST box that contains it, so a node is ")
			TEXT("never in two boxes at once. Two boxes are neighbours when at least one node ")
			TEXT("in one is linked to a node in the other, a knot does not count as a step ")
			TEXT("(D-42), and a link between two nodes inside the SAME box is not a box link.")
			TEXT("\nseed box: %d (hop 0)")
			TEXT("\nlinked boxes asked: %d")
			TEXT("\nboxes framed: %d")
			TEXT("\nnodes framed: %d")
			TEXT("\n%s")
			TEXT("\nrefused combinations: a box derived from the CURRENT SELECTION and a box ")
			TEXT("derived from a PRIOR CAPTURE are both refused, because neither is a box ")
			TEXT("index L-LAYOUT-REPORT printed and an index that means something else is a ")
			TEXT("silent lie. Pass the index in Target."),
			Wanted, InHops, InBoxes.Num(), OutSelection.Targets.Num(),
			*FString::Join(BoxLines, TEXT(", ")));
		return true;
	}

	OutWhy = FString::Printf(
		TEXT("'%s' is not a selector this function resolves. It resolves selection, ")
		TEXT("neighbours and box-neighbours; screen is rendered by RenderScreen."),
		*InMode);
	return false;
}

void TurboMCPCaptureSelector::DescribeRenderingState(FString& InOutDescription)
{
	// *** WHY THE `-nullrhi` TEST IS NOT `GUsingNullRHI`, MEASURED. ***
	//
	// `GUsingNullRHI` is `GRHIGlobals.UsingNullRHI` (`RHIGlobals.h:894`), and it is the
	// exact flag `LaunchEngineLoop.cpp:3343` tests before choosing the Slate NULL
	// renderer at `:3345-3347`. READING IT FROM HERE DOES NOT LINK:
	//
	//     error LNK2019: unresolved external symbol
	//     "__declspec(dllimport) struct FRHIGlobals GRHIGlobals"
	//     referenced in function DescribeRenderingState
	//
	// `TurboMCP.Build.cs` does not list the `RHI` module. `RenderCore` lists it as a
	// PUBLIC dependency (`RenderCore.Build.cs:12`), which makes the HEADER visible
	// through `RenderCore` but does NOT export the symbol through `RenderCore`.
	//
	// So the answer is taken from flags that ARE reachable, and each one is printed as
	// the thing it actually is rather than relabelled:
	//
	//   `-nullrhi` on the command line  - `FParse::Param`, which is the same call the
	//                                     engine itself makes (`App.h:405`).
	//   FApp::CanEverRender()           - `App.h:400-408`, which returns false when
	//                                     that flag is set, in a commandlet and on a
	//                                     dedicated server.
	//   FSlateApplication::IsInitialized()
	//
	// NO DEPENDENCY WAS ADDED for this, and the limit is stated in the output because a
	// reader who wants the engine's own flag deserves to know this is not it.
	const bool bNullRhiFlag = FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
	const bool bAppRendering = FApp::CanEverRender();
	const bool bSlate = FSlateApplication::IsInitialized();

	// THE THREE LABELS ARE COMPUTED INTO LOCALS RATHER THAN WRITTEN AS A TERNARY OF TWO
	// `TEXT` LITERALS INSIDE THE VARIADIC CALL. That form is rejected by the engine's
	// compile time format checker with `error C7595` on this project's UE 5.8.3, which is
	// a trap the brief names and which has now fired twice in this file.
	const TCHAR* const NullRhiLabel = bNullRhiFlag ? TEXT("true") : TEXT("false");
	const TCHAR* const AppRenderingLabel = bAppRendering ? TEXT("true") : TEXT("false");
	const TCHAR* const SlateLabel = bSlate ? TEXT("true") : TEXT("false");

	InOutDescription += TEXT("\nrendering state, measured in this process rather than assumed:");
	InOutDescription += FString::Printf(
		TEXT("\n-nullrhi on the command line: %s  (FParse::Param, the call App.h:405 makes)"),
		NullRhiLabel);
	InOutDescription += FString::Printf(
		TEXT("\nFApp::CanEverRender(): %s  (App.h:400-408)"),
		AppRenderingLabel);
	InOutDescription += FString::Printf(
		TEXT("\nSlate initialized: %s  (LaunchEngineLoop.cpp:3343 installs the renderer)"),
		SlateLabel);
	InOutDescription += TEXT(
		"\n; GUsingNullRHI (RHIGlobals.h:894) is the engine flag LaunchEngineLoop.cpp:3343 ")
		TEXT("tests, and this plugin CANNOT read it: the RHI module is not in TurboMCP.Build.cs ")
		TEXT("and RenderCore does not re-export the symbol. The three lines above are what ")
		TEXT("was measured instead, so this is a measured proxy and NOT the engine's own ")
		TEXT("flag. No dependency was added for it.");

	if (bNullRhiFlag || !bAppRendering)
	{
		InOutDescription += TEXT(
			"\n; this process does not render, so the engine installs the Slate NULL ")
			TEXT("renderer (LaunchEngineLoop.cpp:3345-3347 chose it because the null RHI path ")
			TEXT("was taken). There is no rendered pixel to read: mode screen is REFUSED ")
			TEXT("rather than returning a black image. This is the state every -nullrhi run ")
			TEXT("is in.");
	}
	else if (!bSlate)
	{
		InOutDescription += TEXT(
			"\n; the RHI is the real one but Slate is not initialized, which is the state ")
			TEXT("of a commandlet: LaunchEngineLoop.cpp:3343 guards ")
			TEXT("FSlateApplication::Get().InitializeRenderer(...) with !IsRunningCommandlet(). ")
			TEXT("mode screen is REFUSED for that reason.");
	}
	else
	{
		InOutDescription += TEXT(
			"\n; the real RHI is active and Slate is initialized, so mode screen CAN ")
			TEXT("render the editor window here.");
	}
}

void TurboMCPCaptureSelector::DescribeScreenAvailability(FString& InOutDescription)
{
	InOutDescription += TEXT("\ncan mode screen work here: ");

	const bool bRenders = FApp::CanEverRender()
		&& !FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
	if (!bRenders)
	{
		InOutDescription += TEXT("NO, and the reason is the renderer rather than the window");
	}
	else if (!FSlateApplication::IsInitialized())
	{
		InOutDescription += TEXT("NO, because Slate is not initialized in this process");
	}
	else if (!FSlateApplication::Get().GetActiveTopLevelWindow().IsValid())
	{
		InOutDescription += TEXT("NO, because no top level editor window is open");
	}
	else
	{
		const FVector2D Size =
			FSlateApplication::Get().GetActiveTopLevelWindow()->GetSizeInScreen();
		InOutDescription += FString::Printf(
			TEXT("YES, the active window reports %dx%d"),
			FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y));
	}

	InOutDescription += TEXT("\n; this line changes nothing and writes no image, so a ")
		TEXT("suite can measure the answer without touching the machine's screen.");
	DescribeRenderingState(InOutDescription);
}

bool TurboMCPCaptureSelector::RenderScreen(
	FString& InOutDescription, FString& OutImagePath, FString& OutError)
{
	OutImagePath.Empty();
	OutError.Empty();

	// *** THE NO-RENDER REFUSAL, WITH THE LINE THAT DECIDES IT. ***
	//
	// `LaunchEngineLoop.cpp:3343` tests `GUsingNullRHI` and `:3345-3347` installs the
	// Slate NULL renderer when it is set, so `:3354` initializes a renderer that draws
	// nothing. WITH NO RENDERED PIXEL THERE IS NOTHING TO READ: a screen capture in that
	// state would hand back a black rectangle, and a black rectangle looks exactly like
	// a successful capture of a dark window. So it is REFUSED BY NAME, here, before any
	// render target is created.
	//
	// The flag is read from the command line rather than from `GUsingNullRHI` for the
	// reason stated at length in `DescribeRenderingState`: the RHI module is not linked,
	// so that symbol does not resolve. The limit is printed by `mode rhi-state`, and the
	// refusal below says which signal it used.
	const bool bNullRhiFlag = FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
	if (bNullRhiFlag || !FApp::CanEverRender())
	{
		// Computed into a local: a ternary of two `TEXT` literals in a variadic
		// argument is the `error C7595` trap.
		const TCHAR* const WhichSignal = bNullRhiFlag
			? TEXT("-nullrhi is on the command line")
			: TEXT("FApp::CanEverRender() is false");

		OutError = FString::Printf(
			TEXT("the editor is not rendering (%s), so Slate has the NULL renderer and ")
			TEXT("there is no rendered pixel to read. The engine line that decides it is ")
			TEXT("LaunchEngineLoop.cpp:3343, which installs the null Slate renderer at ")
			TEXT(":3345-3347 when the null RHI path is taken (GUsingNullRHI, ")
			TEXT("RHIGlobals.h:894; this plugin reads the -nullrhi command line flag and ")
			TEXT("FApp::CanEverRender instead, because the RHI module is not linked). mode ")
			TEXT("screen is REFUSED rather than returning a black image. Start the editor ")
			TEXT("WITHOUT -nullrhi to capture the screen."),
			WhichSignal);
		return false;
	}

	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Slate is not initialized in this process, so there is no editor ")
			TEXT("window to render. In a commandlet it never is: ")
			TEXT("LaunchEngineLoop.cpp:3343 guards ")
			TEXT("FSlateApplication::Get().InitializeRenderer(...) with !IsRunningCommandlet().");
		return false;
	}

	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Window.IsValid())
	{
		// SAID PLAINLY rather than rendering something else. An absent window is an
		// absent measurement, not a zero and not a false (`P1`).
		OutError = TEXT("no top level editor window is open, so there is nothing on screen ")
			TEXT("to capture. Slate is initialized and the RHI is the real one, so the ")
			TEXT("window is the missing piece.");
		return false;
	}

	const FVector2D WindowSize = Window->GetSizeInScreen();
	const int32 WindowWidth = FMath::RoundToInt(WindowSize.X);
	const int32 WindowHeight = FMath::RoundToInt(WindowSize.Y);

	if (WindowWidth <= 0 || WindowHeight <= 0)
	{
		OutError = FString::Printf(
			TEXT("the editor window reports a size of %dx%d, so there is nothing to render."),
			WindowWidth, WindowHeight);
		return false;
	}

	// "AT ITS CURRENT SIZE" is the window's own size, and the ONLY change made is the
	// 4096 edge cap this project applies to every capture, applied to BOTH edges so
	// the aspect ratio cannot change.
	int32 Width = WindowWidth;
	int32 Height = WindowHeight;
	FString CapNote;
	if (Width > ScreenMaxEdge || Height > ScreenMaxEdge)
	{
		const double Scale = FMath::Min(
			static_cast<double>(ScreenMaxEdge) / static_cast<double>(Width),
			static_cast<double>(ScreenMaxEdge) / static_cast<double>(Height));
		Width = FMath::Max(1, FMath::FloorToInt(Width * Scale));
		Height = FMath::Max(1, FMath::FloorToInt(Height * Scale));
		CapNote = FString::Printf(
			TEXT("\n; the window is %dx%d and the %d edge cap scaled it to %dx%d, so the ")
			TEXT("aspect ratio is unchanged."),
			WindowWidth, WindowHeight, ScreenMaxEdge, Width, Height);
	}

	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(
		FVector2D(Width, Height), TF_Bilinear, /*bUseGammaCorrection=*/false);
	if (RenderTarget == nullptr)
	{
		OutError = FString::Printf(
			TEXT("could not create a %dx%d render target for the editor window"), Width, Height);
		return false;
	}

	FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false);
	FHittestGrid HitTestGrid;

	// The overload takes NO vertex colour: the signature is the one
	// `TurboMCPCapture.cpp` already calls (`Slate/WidgetRenderer.h:92-99`), so the two
	// window renders cannot drift apart. The base vertex colour is
	// `FSlateColor::UseForeground()`, which is what the proven path already uses.
	Renderer->DrawWindow(RenderTarget, HitTestGrid, Window.ToSharedRef(),
		/*Scale=*/1.0f, FVector2D(Width, Height), /*DeltaTime=*/0.0f,
		/*bDeferRenderTargetUpdate=*/false);

	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
	ReadFlags.SetLinearToGamma(false);

	const bool bRead = RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags);
	delete Renderer;

	if (!bRead || Pixels.Num() < Width * Height)
	{
		OutError = FString::Printf(
			TEXT("the window render read back %d pixel(s) for a %dx%d window, so the ")
			TEXT("picture is incomplete and was not written."),
			Pixels.Num(), Width, Height);
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
		OutError = TEXT("could not compress the window image to PNG");
		return false;
	}

	const FString Folder = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("TurboMCP"), CaptureFolderName);
	IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);

	OutImagePath = FPaths::Combine(Folder,
		FString::Printf(TEXT("screen_%dx%d.png"), Width, Height));

	if (!FFileHelper::SaveArrayToFile(PngData, *OutImagePath))
	{
		OutError = FString::Printf(TEXT("could not write %s"), *OutImagePath);
		OutImagePath.Empty();
		return false;
	}

	InOutDescription += FString::Printf(
		TEXT("\nframed: the whole editor window, uncropped, at its current size"));
	InOutDescription += FString::Printf(
		TEXT("\nwindow: %dx%d, rendered at %dx%d"), WindowWidth, WindowHeight, Width, Height);
	InOutDescription += FString::Printf(TEXT("\nsize: %dx%d"), Width, Height);
	// A TERNARY OF TWO `TEXT` LITERALS INSIDE A VARIADIC ARGUMENT IS REJECTED BY THE
	// ENGINE'S COMPILE TIME FORMAT CHECKER (`error C7595` on this project's UE 5.8.3),
	// so each label is computed into a local first. The same trap is recorded in the
	// brief and has now fired twice here.
	const TCHAR* const RenderLabel =
		FApp::CanEverRender() ? TEXT("rendering") : TEXT("not rendering");
	const TCHAR* const SlateLabel =
		FSlateApplication::IsInitialized() ? TEXT("yes") : TEXT("no");

	InOutDescription += FString::Printf(
		TEXT("\nrenderer state: %s, Slate initialized %s"), RenderLabel, SlateLabel);
	InOutDescription += FString::Printf(TEXT("\nimage: %s"), *OutImagePath);
	InOutDescription += TEXT("\n; screen is the whole editor window composited by the ")
		TEXT("Slate renderer, NOT the level viewport frame. The level viewport needs rows ")
		TEXT("2.3 (L-CAPTURE-LEVEL), and this tool does not stand in for it.");
	InOutDescription += CapNote;
	return true;
}
