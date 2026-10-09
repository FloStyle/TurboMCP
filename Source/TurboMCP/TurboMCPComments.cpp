// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPNodeId.h"

/**
 * R-COMMENT-LIST: comment boxes as data.
 *
 * `03_TOOLS.md` asks for every comment box with its title, colour, rectangle, its
 * parent box when it is nested, and the IDs of the nodes inside it.
 *
 * NOTHING IS READ FROM THE ENGINE FOR THE RELATIONS. A comment box does not store
 * which nodes it covers or which box contains it: both are geometry, and the editor
 * computes them from the rectangles while drawing. This tool does the same
 * computation rather than inventing a rule.
 *
 * A node can fall inside two overlapping boxes, and a box can sit inside another. A
 * caller moving a node has to know that, so both are reported instead of one being
 * picked.
 */
namespace
{
	/** How many boxes one graph may report before the list caps. */
	constexpr int32 MaxBoxes = 80;

	/** How many node IDs are printed per box before that list caps. */
	constexpr int32 MaxNodesPerBox = 60;

	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 24000;

	/** A rectangle, with the helpers the containment tests need. */
	struct FRect
	{
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;

		int32 Width() const { return MaxX - MinX; }
		int32 Height() const { return MaxY - MinY; }
		int64 Area() const
		{
			return static_cast<int64>(Width()) * static_cast<int64>(Height());
		}
	};

	/** The box a comment node occupies. */
	FRect RectOf(const UEdGraphNode_Comment* InComment)
	{
		FRect Rect;
		Rect.MinX = InComment->NodePosX;
		Rect.MinY = InComment->NodePosY;
		Rect.MaxX = InComment->NodePosX + InComment->NodeWidth;
		Rect.MaxY = InComment->NodePosY + InComment->NodeHeight;
		return Rect;
	}

	/** The box a node occupies, which is not its position alone. */
	FRect RectOfNode(const UEdGraphNode* InNode)
	{
		FRect Rect;
		Rect.MinX = InNode->NodePosX;
		Rect.MinY = InNode->NodePosY;
		Rect.MaxX = InNode->NodePosX + InNode->NodeWidth;
		Rect.MaxY = InNode->NodePosY + InNode->NodeHeight;
		return Rect;
	}

	/** True when InInner sits completely inside InOuter. */
	bool Contains(const FRect& InOuter, const FRect& InInner)
	{
		return InInner.MinX >= InOuter.MinX
			&& InInner.MinY >= InOuter.MinY
			&& InInner.MaxX <= InOuter.MaxX
			&& InInner.MaxY <= InOuter.MaxY;
	}

	/**
	 * True when a node is inside a box.
	 *
	 * A node has no meaningful size until the editor has laid it out, and an
	 * unlaid-out node reports a zero rectangle at its position. Testing the position
	 * alone is therefore what actually works, and it is also what a reader means by
	 * "inside this box".
	 */
	bool BoxCoversNode(const FRect& InBox, const UEdGraphNode* InNode)
	{
		return InNode->NodePosX >= InBox.MinX
			&& InNode->NodePosX <= InBox.MaxX
			&& InNode->NodePosY >= InBox.MinY
			&& InNode->NodePosY <= InBox.MaxY;
	}

	/** A colour as hex, which is short and exact. */
	FString DescribeColor(const FLinearColor& InColor)
	{
		const FColor AsByte = InColor.ToFColor(/*bSRGB=*/true);
		return FString::Printf(TEXT("#%02X%02X%02X"), AsByte.R, AsByte.G, AsByte.B);
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPCommentList(
	const FString& Asset,
	const FString& GraphName)
{
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a Blueprint in any path form, for ")
			TEXT("example BP_NodeTest.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(
		LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it has no graphs to list comment ")
			TEXT("boxes from."), *Resolved.ObjectPath);
	}

	// An empty graph name means every graph, which is the useful default: a caller
	// usually wants to know where the boxes are before choosing one.
	TArray<UEdGraph*> Graphs;
	{
		// The same graph collection R-BP-LOAD uses, so the two tools cannot disagree
		// about which graphs a Blueprint has.
		TArray<UEdGraph*> All;
		FTurboMCPBlueprintRead::CollectGraphs(Blueprint, All);

		if (GraphName.IsEmpty())
		{
			Graphs = All;
		}
		else
		{
			if (UEdGraph* Found = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName))
			{
				Graphs.Add(Found);
			}
			else
			{
				TArray<FString> Names;
				for (const UEdGraph* Graph : All)
				{
					Names.Add(Graph->GetName());
				}
				return FString::Printf(
					TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
					*Resolved.ObjectPath, *GraphName,
					Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
			}
		}
	}

	FString Out = FString::Printf(TEXT("read: comment boxes of %s"), *Resolved.ObjectPath);
	if (!GraphName.IsEmpty())
	{
		Out += FString::Printf(TEXT(", graph %s"), *GraphName);
	}

	int32 TotalBoxes = 0;
	int32 TotalNested = 0;
	int32 TotalOverlapping = 0;
	int32 BoxesWithNodes = 0;
	FString Body;

	for (UEdGraph* Graph : Graphs)
	{
		// Every comment box in this graph.
		TArray<UEdGraphNode_Comment*> Boxes;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				Boxes.Add(Comment);
			}
		}

		if (Boxes.Num() == 0)
		{
			// `P1`: a graph with no boxes says so rather than being omitted, so a
			// caller can tell "no boxes here" from "this graph was not looked at".
			Body += FString::Printf(TEXT("\n\ngraph %s: no comment boxes"), *Graph->GetName());
			continue;
		}

		TotalBoxes += Boxes.Num();
		Body += FString::Printf(TEXT("\n\ngraph %s: %d comment box(es)"),
			*Graph->GetName(), Boxes.Num());

		// Sorted by position, top to bottom then left to right, so the output reads
		// in the order the boxes appear and is stable between reads.
		Boxes.Sort([](const UEdGraphNode_Comment& A, const UEdGraphNode_Comment& B)
		{
			if (A.NodePosY != B.NodePosY)
			{
				return A.NodePosY < B.NodePosY;
			}
			return A.NodePosX < B.NodePosX;
		});

		int32 Shown = 0;
		for (UEdGraphNode_Comment* Box : Boxes)
		{
			if (Shown >= MaxBoxes)
			{
				Body += FString::Printf(TEXT("\n  (%d more box(es) not shown)"),
					Boxes.Num() - Shown);
				break;
			}
			++Shown;

			const FRect BoxRect = RectOf(Box);

			Body += FString::Printf(TEXT("\n  %s"),
				*TurboMCPNodeId::MakeForNode(Box, /*bLongForm=*/true));

			const FString Title = Box->NodeComment.IsEmpty()
				? FString(TEXT("(no title)")) : Box->NodeComment;
			Body += FString::Printf(TEXT("\n      title: %s"), *Title);
			Body += FString::Printf(TEXT("\n      colour: %s"),
				*DescribeColor(Box->CommentColor));
			Body += FString::Printf(TEXT("\n      rect: x %d, y %d, %d wide, %d tall"),
				BoxRect.MinX, BoxRect.MinY, BoxRect.Width(), BoxRect.Height());

			// Nesting, computed from the rectangles. The parent is the SMALLEST box
			// that contains this one, because a box inside two others is inside the
			// innermost.
			const UEdGraphNode_Comment* Parent = nullptr;
			FRect ParentRect;
			int64 SmallestArea = TNumericLimits<int64>::Max();

			for (const UEdGraphNode_Comment* Other : Boxes)
			{
				if (Other == Box)
				{
					continue;
				}
				const FRect OtherRect = RectOf(Other);
				if (!Contains(OtherRect, BoxRect))
				{
					continue;
				}
				// Identical rectangles must not contain each other, or two boxes
				// drawn exactly on top of each other would be each other's parent.
				if (OtherRect.MinX == BoxRect.MinX && OtherRect.MinY == BoxRect.MinY
					&& OtherRect.Width() == BoxRect.Width()
					&& OtherRect.Height() == BoxRect.Height())
				{
					continue;
				}
				if (OtherRect.Area() < SmallestArea)
				{
					SmallestArea = OtherRect.Area();
					Parent = Other;
					ParentRect = OtherRect;
				}
			}

			if (Parent != nullptr)
			{
				++TotalNested;
				Body += FString::Printf(TEXT("\n      parent box: %s"),
					*TurboMCPNodeId::MakeForNode(Parent));
			}
			else
			{
				Body += TEXT("\n      parent box: none, this is a top level box");
			}

			// The nodes inside, and any node that falls inside more than one box,
			// which a caller moving it has to know about.
			TArray<FString> Covered;
			TArray<FString> DoubledUp;
			int32 NodesShown = 0;

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
				{
					// A box is not a node inside a box. Nesting is reported through
					// the parent line instead, so a box is not counted twice.
					continue;
				}
				if (!BoxCoversNode(BoxRect, Node))
				{
					continue;
				}

				// How many boxes cover this node.
				int32 CoverCount = 0;
				for (const UEdGraphNode_Comment* Other : Boxes)
				{
					const FRect OtherRect = RectOf(Other);
					if (OtherRect.MinX == BoxRect.MinX && OtherRect.MinY == BoxRect.MinY
						&& OtherRect.Width() == BoxRect.Width()
						&& OtherRect.Height() == BoxRect.Height())
					{
						continue;
					}
					if (BoxCoversNode(OtherRect, Node))
					{
						++CoverCount;
					}
				}

				if (NodesShown < MaxNodesPerBox)
				{
					Covered.Add(TurboMCPNodeId::MakeForNode(Node));
					++NodesShown;
				}
				else
				{
					++NodesShown;
				}

				if (CoverCount > 0)
				{
					++TotalOverlapping;
					DoubledUp.Add(FString::Printf(TEXT("%s (in %d other box(es))"),
						*TurboMCPNodeId::MakeForNode(Node), CoverCount));
				}
			}

			const int32 CoveredCount = Covered.Num();
			Body += FString::Printf(TEXT("\n      nodes inside: %d"), NodesShown);
			if (NodesShown == 0)
			{
				// Being empty is a real fact about the box, not a gap.
				Body += TEXT("\n          (this box covers no node)");
			}
			if (NodesShown > CoveredCount)
			{
				Body += FString::Printf(TEXT("\n          (%d more not shown)"),
					NodesShown - CoveredCount);
			}
			for (const FString& Id : Covered)
			{
				Body += FString::Printf(TEXT("\n          %s"), *Id);
			}

			if (DoubledUp.Num() > 0)
			{
				Body += FString::Printf(
					TEXT("\n      WARNING: %d node(s) in this box also fall inside ")
					TEXT("another box, so they are covered twice:"), DoubledUp.Num());
				for (const FString& Line : DoubledUp)
				{
					Body += FString::Printf(TEXT("\n          %s"), *Line);
				}
			}

			if (NodesShown > 0)
			{
				++BoxesWithNodes;
			}
		}
	}

	Out += FString::Printf(
		TEXT("\ncounts: %d box(es), %d nested, %d with nodes, %d node(s) inside more than one box"),
		TotalBoxes, TotalNested, BoxesWithNodes, TotalOverlapping);

	if (TotalBoxes == 0)
	{
		Out += TEXT("\nNo comment box exists in the graphs that were read. The graph ")
			TEXT("text writer prints one comment line per box when there are any.");
	}

	Out += Body;

	if (TotalOverlapping > 0)
	{
		// Stated once at the end as well, because it is the one fact that changes
		// what a caller should do next.
		Out += FString::Printf(
			TEXT("\n; %d node(s) fall inside more than one box. Moving one of them ")
			TEXT("changes the membership of every box that covers it."),
			TotalOverlapping);
	}

	if (Out.Len() > MaxResultChars)
	{
		const int32 Dropped = Out.Len() - MaxResultChars;
		Out = Out.Left(MaxResultChars);
		Out += FString::Printf(
			TEXT("\n\n; (%d more character(s) were dropped; name one graph to read ")
			TEXT("fewer boxes at a time)"), Dropped);
	}

	return Out;
}
