// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPComment.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPGraphTidy.h"

namespace
{
	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	/** The node key of a node, in the form the node id uses. */
	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	/** Reads a graph out of a Blueprint by name, or the first graph when the name is empty. */
	UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName, FString& OutError)
	{
		// *** ONE RESOLUTION POLICY WITH EVERY OTHER TOOL AND THE READER. ***
		// This was one of EIGHT graph resolvers. It searched UbergraphPages, FunctionGraphs and MacroGraphs;
		// the reader's TurboMCPNodeId::FindGraphByName searches those THREE PLUS EventGraphs, and nothing
		// compared them. A node id is `...:GraphName:<hash>` - the graph NAME ONLY - so a name matching
		// two graphs in different arrays would make this tool EDIT ONE AND REPORT ON ANOTHER, and no
		// caller could tell. That is the ARC06 family: identity that cannot be verified across calls.
		// It now delegates, so this tool and the reader resolve a name identically.
				OutError.Empty();
		return TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName);
	}

	/**
	 * The nodes a box encloses, computed by the rule the EDITOR uses.
	 *
	 * MEMBERSHIP IS GEOMETRIC, AND THE ENGINE'S CACHE CANNOT BE USED HERE.
	 *
	 * `UEdGraphNode_Comment::GetNodesUnderComment` returns a CACHED set that
	 * `SGraphNodeComment::HandleSelection` fills from the RENDERED widgets: it walks the graph
	 * panel's children and calls `AddNodeUnderComment` for each node whose rendered rectangle is
	 * contained in the comment's. Measured consequence: an asset read without its graph editor open
	 * has an EMPTY cache for every box, so `GetNodesUnderComment` reports nothing even for a box
	 * drawn around ten nodes. The engine exposes no function that rebuilds it.
	 *
	 * So the RULE is reproduced rather than the cache read, taken from
	 * `SGraphNodeComment::IsNodeUnderComment`, which is
	 * `FSlateRect::IsRectangleContained(CommentRect, NodeRect)` in graph space. A box's rectangle
	 * starts `TitleBarHeight` BELOW its position, because the title bar's own strip sits above it;
	 * using the position directly would claim a box contains the row of nodes drawn under its title.
	 */
	void LocalComputeMembers(UEdGraphNode_Comment* InComment, TArray<FString>& OutMembers)
	{
		OutMembers.Reset();
		UEdGraph* Graph = InComment->GetGraph();
		if (Graph == nullptr)
		{
			return;
		}

		const double CommentLeft = InComment->NodePosX;
		const double CommentTop = InComment->NodePosY + FTurboMCPComment::TitleBarHeight;
		const double CommentRight = InComment->NodePosX + InComment->NodeWidth;
		const double CommentBottom = InComment->NodePosY + InComment->NodeHeight;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr || Node == InComment
				|| Cast<UEdGraphNode_Comment>(Node) != nullptr)
			{
				// A comment inside a comment is the editor's own nesting feature and is not a
				// member in this sense; including it would make every box look populated.
				continue;
			}

			const double Left = Node->NodePosX;
			const double Top = Node->NodePosY;
			const double Right = Node->NodePosX + FTurboMCPGraphTidy::AlignmentNodeSize(Node).X;
			const double Bottom = Node->NodePosY + FTurboMCPGraphTidy::AlignmentNodeSize(Node).Y;

			// `FSlateRect::IsRectangleContained`, written out: a node counts only when it is
			// WHOLLY inside, not when it merely overlaps. A partly covered node would be moved out
			// from under the box by a tidy, which is the opposite of what a label is for.
			if (Left >= CommentLeft && Right <= CommentRight
				&& Top >= CommentTop && Bottom <= CommentBottom)
			{
				OutMembers.Add(Node->GetName());
			}
		}
		OutMembers.Sort();
	}

	/** Reads one box, computing what is inside it by the editor's own rule. */
	void ReadBox(UEdGraphNode_Comment* InComment, FTurboMCPComment::FBox& OutBox)
	{
		OutBox.Key = KeyOf(InComment);
		OutBox.NodeName = InComment->GetName();
		OutBox.Text = InComment->NodeComment;
		OutBox.X = InComment->NodePosX;
		OutBox.Y = InComment->NodePosY;
		OutBox.Width = InComment->NodeWidth;
		OutBox.Height = InComment->NodeHeight;

		LocalComputeMembers(InComment, OutBox.Members);
		OutBox.MemberCount = OutBox.Members.Num();
	}

	/** Every comment box in a graph, in the graph's own order. */
	void ReadBoxes(UEdGraph* InGraph, TArray<FTurboMCPComment::FBox>& OutBoxes)
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
				FTurboMCPComment::FBox Box;
				ReadBox(Comment, Box);
				OutBoxes.Add(MoveTemp(Box));
			}
		}
	}

	/** A box's rectangle, grown to enclose everything inside it. */
	bool ComputeFit(UEdGraphNode_Comment* InComment, double& OutX, double& OutY,
		double& OutWidth, double& OutHeight)
	{
		TArray<FString> Members;
		LocalComputeMembers(InComment, Members);
		if (Members.Num() == 0)
		{
			return false;
		}

		// The box's own top-left is the anchor the editor uses: a comment's title bar sits ABOVE
		// its rectangle, so the contents grow DOWN and RIGHT from the box's position and the fit
		// keeps the position rather than moving the box to chase its contents.
		double MaxRight = InComment->NodePosX;
		double MaxBottom = InComment->NodePosY;

		for (UEdGraphNode* Node : InComment->GetGraph()->Nodes)
		{
			if (Node == nullptr || Node == InComment
				|| Cast<UEdGraphNode_Comment>(Node) != nullptr)
			{
				continue;
			}
			if (!Members.Contains(Node->GetName()))
			{
				continue;
			}
			MaxRight = FMath::Max<double>(MaxRight, Node->NodePosX + FTurboMCPGraphTidy::AlignmentNodeSize(Node).X);
			MaxBottom = FMath::Max<double>(MaxBottom, Node->NodePosY + FTurboMCPGraphTidy::AlignmentNodeSize(Node).Y);
		}

		OutX = InComment->NodePosX;
		OutY = InComment->NodePosY;
		OutWidth = (MaxRight - OutX) + FTurboMCPComment::FitPadding;
		OutHeight = (MaxBottom - OutY) + FTurboMCPComment::FitPadding;
		return true;
	}

	/** The rectangle a box should have to enclose the named nodes, before it exists. */
	bool ComputeEnclosingRect(UEdGraph* InGraph, const TArray<FString>& InKeys,
		double& OutX, double& OutY, double& OutWidth, double& OutHeight,
		TArray<FString>& OutMissing)
	{
		bool bAny = false;
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxRight = 0.0;
		double MaxBottom = 0.0;

		for (const FString& Key : InKeys)
		{
			bool bFound = false;
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr && (KeyOf(Node).Equals(Key, ESearchCase::IgnoreCase) || TurboMCPNodeId::MakeForNode(Node, true) == Key || TurboMCPNodeId::MakeForNode(Node, false) == Key))
				{
					if (!bAny)
					{
						MinX = Node->NodePosX;
						MinY = Node->NodePosY;
						MaxRight = Node->NodePosX + FTurboMCPGraphTidy::AlignmentNodeSize(Node).X;
						MaxBottom = Node->NodePosY + FTurboMCPGraphTidy::AlignmentNodeSize(Node).Y;
						bAny = true;
					}
					else
					{
						MinX = FMath::Min<double>(MinX, Node->NodePosX);
						MinY = FMath::Min<double>(MinY, Node->NodePosY);
						MaxRight = FMath::Max<double>(
							MaxRight, Node->NodePosX + FTurboMCPGraphTidy::AlignmentNodeSize(Node).X);
						MaxBottom = FMath::Max<double>(
							MaxBottom, Node->NodePosY + FTurboMCPGraphTidy::AlignmentNodeSize(Node).Y);
					}
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				OutMissing.Add(Key);
			}
		}

		if (!bAny)
		{
			return false;
		}

		OutX = MinX - FTurboMCPComment::FitPadding;
		// The title bar sits above the rectangle, so the top is lifted by its height as well as
		// the padding, or the first row of nodes is drawn under the title.
		OutY = MinY - FTurboMCPComment::FitPadding - FTurboMCPComment::TitleBarHeight;
		OutWidth = (MaxRight - MinX) + FTurboMCPComment::FitPadding * 2.0;
		OutHeight = (MaxBottom - MinY) + FTurboMCPComment::FitPadding * 2.0
			+ FTurboMCPComment::TitleBarHeight;
		return true;
	}

	/** Parses `#RRGGBB` or `R,G,B`, each 0..255. */
	bool ParseColor(const FString& InText, FLinearColor& OutColor, FString& OutWhy)
	{
		if (InText.StartsWith(TEXT("#")))
		{
			const FString Hex = InText.Mid(1);
			if (Hex.Len() != 6)
			{
				OutWhy = TEXT("a '#' colour needs six hex digits, as #3A6EA5");
				return false;
			}
			for (const TCHAR Character : Hex)
			{
				if (!FChar::IsHexDigit(Character))
				{
					OutWhy = FString::Printf(TEXT("'%c' is not a hex digit"), Character);
					return false;
				}
			}
			OutColor = FLinearColor(
				static_cast<float>(FParse::HexNumber(*Hex.Mid(0, 2))) / 255.0f,
				static_cast<float>(FParse::HexNumber(*Hex.Mid(2, 2))) / 255.0f,
				static_cast<float>(FParse::HexNumber(*Hex.Mid(4, 2))) / 255.0f,
				1.0f);
			return true;
		}

		TArray<FString> Parts;
		InText.ParseIntoArray(Parts, TEXT(","), /*InCullEmpty=*/true);
		if (Parts.Num() != 3)
		{
			OutWhy = TEXT("a colour is #RRGGBB or three 0..255 numbers as 128,64,32");
			return false;
		}
		float Channels[3];
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 Value = FCString::Atoi(*Parts[Index].TrimStartAndEnd());
			if (Value < 0 || Value > 255)
			{
				OutWhy = FString::Printf(TEXT("'%s' is outside 0..255"), *Parts[Index]);
				return false;
			}
			Channels[Index] = static_cast<float>(Value) / 255.0f;
		}
		OutColor = FLinearColor(Channels[0], Channels[1], Channels[2], 1.0f);
		return true;
	}
}

void FTurboMCPComment::MembersOf(UEdGraphNode_Comment* InComment, TArray<FString>& OutMembers)
{
	LocalComputeMembers(InComment, OutMembers);
}

bool FTurboMCPComment::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InAction, const FString& InNodes, const FString& InBox,
	const FString& InText, const FString& InColor,
	const FString& InSize, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}

	// `action` is required and named, because guessing between create and set from the presence of
	// other fields is how a caller who forgot one field gets the other operation.
	const FString ActionText = InAction.TrimStartAndEnd().ToLower();
	if (ActionText.IsEmpty())
	{
		OutError = TEXT("no action given. Pass one of create, set, fit, delete.");
		return false;
	}
	if (ActionText == TEXT("create"))      { OutPlan.Action = EAction::Create; }
	else if (ActionText == TEXT("set"))    { OutPlan.Action = EAction::Set; }
	else if (ActionText == TEXT("fit"))    { OutPlan.Action = EAction::FitAll; }
	else if (ActionText == TEXT("delete")) { OutPlan.Action = EAction::Delete; }
	else
	{
		OutError = FString::Printf(
			TEXT("'%s' is not an action. Accepted: create, set, fit, delete."), *InAction);
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is not a Blueprint, so it has no graphs to comment."), *Resolved.ObjectPath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, OutPlan.GraphName, OutError);
	if (Graph == nullptr)
	{
		return false;
	}
    if(ActionText==TEXT("fit") || ActionText==TEXT("create"))
    {
        if(!FTurboMCPGraphTidy::PrepareAlignment(Blueprint,Graph,OutError)) return false;
    }
	OutPlan.GraphName = Graph->GetName();
	OutPlan.Asset = Resolved.ObjectPath;

	OutPlan.NodeKeys = FTurboMCPAssetResolver::SplitList(InNodes);
	OutPlan.BoxKey = InBox.TrimStartAndEnd();
	if (!OutPlan.BoxKey.IsEmpty())
	{
		UEdGraphNode* Match = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
			if (Cast<UEdGraphNode_Comment>(Node) && (KeyOf(Node).Equals(OutPlan.BoxKey, ESearchCase::IgnoreCase)
				|| TurboMCPNodeId::MakeForNode(Node, true) == OutPlan.BoxKey || TurboMCPNodeId::MakeForNode(Node, false) == OutPlan.BoxKey))
			{ if (Match) { OutError = TEXT("ambiguous comment ID"); return false; } Match = Node; }
		if (!Match) { OutError = TEXT("comment ID not found in the requested graph"); return false; }
		OutPlan.BoxKey = KeyOf(Match);
	}

	OutPlan.Text = InText;
	OutPlan.bHasText = !InText.IsEmpty();
	OutPlan.bHasColor = !InColor.TrimStartAndEnd().IsEmpty();
	OutPlan.Color = InColor.TrimStartAndEnd();

	if (OutPlan.bHasColor)
	{
		FLinearColor Parsed;
		FString Why;
		if (!ParseColor(OutPlan.Color, Parsed, Why))
		{
			OutError = FString::Printf(TEXT("'%s' is not a colour: %s."),
				*OutPlan.Color, *Why);
			return false;
		}
	}

	// `WxH`, in graph units. Refused rather than ignored when it does not parse, because a caller
	// who asked for a size and silently got none has been misled.
	const FString SizeText = InSize.TrimStartAndEnd();
	if (!SizeText.IsEmpty())
	{
		FString Left;
		FString Right;
		if (!SizeText.Split(TEXT("x"), &Left, &Right) && !SizeText.Split(TEXT("X"), &Left, &Right))
		{
			OutError = FString::Printf(
				TEXT("'%s' is not a size. Pass width x height, as 400x200."), *InSize);
			return false;
		}
		const double ParsedWidth = FCString::Atod(*Left);
		const double ParsedHeight = FCString::Atod(*Right);
		if (ParsedWidth <= 0.0 || ParsedHeight <= 0.0)
		{
			OutError = FString::Printf(
				TEXT("'%s' has a side that is not a positive number."), *InSize);
			return false;
		}
		OutPlan.Width = ParsedWidth;
		OutPlan.Height = ParsedHeight;
	}

	ReadBoxes(Graph, OutPlan.Boxes);
	for (const FBox& Box : OutPlan.Boxes)
	{
		if (Box.MemberCount == 0)
		{
			OutPlan.EmptyBoxes.Add(Box);
		}
	}

	if (OutPlan.Action == EAction::Create)
	{
		if (OutPlan.NodeKeys.Num() == 0)
		{
			OutError = TEXT("create needs nodes to put the box around. Pass one or more node ")
				TEXT("keys, or a full node id, separated by commas.");
			return false;
		}

		TArray<FString> Missing;
		double X = 0.0, Y = 0.0, Width = 0.0, Height = 0.0;
		if (!ComputeEnclosingRect(Graph, OutPlan.NodeKeys, X, Y, Width, Height, Missing))
		{
			OutError = FString::Printf(
				TEXT("none of the %d node key(s) are in %s. Its node keys are listed by ")
				TEXT("R-NODE-FIND."), OutPlan.NodeKeys.Num(), *OutPlan.GraphName);
			return false;
		}
		if (Missing.Num() > 0)
		{
			// EVERY KEY IS CHECKED BEFORE ANYTHING IS MADE, so a typo in a list of ten does not
			// produce a box around the other nine. `P8`.
			OutError = FString::Printf(
				TEXT("%d node key(s) are not in %s: %s. Nothing was created."),
				Missing.Num(), *OutPlan.GraphName, *FString::Join(Missing, TEXT(", ")));
			return false;
		}

		OutPlan.Planned.X = X;
		OutPlan.Planned.Y = Y;
		OutPlan.Planned.Width = OutPlan.Width > 0.0 ? OutPlan.Width : Width;
		OutPlan.Planned.Height = OutPlan.Height > 0.0 ? OutPlan.Height : Height;
		OutPlan.Planned.MemberCount = OutPlan.NodeKeys.Num();
		OutPlan.Planned.Members = OutPlan.NodeKeys;
		return true;
	}

	if (OutPlan.Action == EAction::Delete)
	{
		if (OutPlan.BoxKey.IsEmpty())
		{
			OutError = TEXT("delete needs the box to remove. Pass its node key, as ")
				TEXT("R-COMMENT-LIST reports it. Its nodes are never removed.");
			return false;
		}
		// The box is FOUND while planning, so a key that names nothing is refused by the preview
		// too rather than only by the edit (`D-02`).
		for (const FBox& Box : OutPlan.Boxes)
		{
			if (Box.Key.Equals(OutPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		OutError = FString::Printf(
			TEXT("'%s' is not a comment box in %s. Its boxes are: %s"),
			*OutPlan.BoxKey, *OutPlan.GraphName,
			OutPlan.Boxes.Num() > 0
				? *FString::JoinBy(OutPlan.Boxes,
					TEXT(", "), [](const FBox& Box) { return Box.Key; })
				: TEXT("(none)"));
		return false;
	}

	if (OutPlan.Action == EAction::Set)
	{
		if (OutPlan.BoxKey.IsEmpty())
		{
			OutError = TEXT("set needs the box to change. Pass its node key.");
			return false;
		}
		if (!OutPlan.bHasText && !OutPlan.bHasColor
			&& OutPlan.Width <= 0.0 && OutPlan.Height <= 0.0)
		{
			OutError = TEXT("set was given nothing to change. Pass text=, color= or size=.");
			return false;
		}
		for (const FBox& Box : OutPlan.Boxes)
		{
			if (Box.Key.Equals(OutPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		OutError = FString::Printf(
			TEXT("'%s' is not a comment box in %s."), *OutPlan.BoxKey, *OutPlan.GraphName);
		return false;
	}

	if (!OutPlan.BoxKey.IsEmpty())
		OutPlan.Boxes.RemoveAll([&](const FBox& Box) { return Box.Key != OutPlan.BoxKey; });
	if (!OutPlan.NodeKeys.IsEmpty())
	{
		if (OutPlan.BoxKey.IsEmpty()) { OutError = TEXT("fit with nodes requires one Box ID"); return false; }
		TArray<FString> Missing;
		if (!ComputeEnclosingRect(Graph, OutPlan.NodeKeys, OutPlan.Planned.X, OutPlan.Planned.Y,
			OutPlan.Planned.Width, OutPlan.Planned.Height, Missing) || !Missing.IsEmpty())
		{ OutError = TEXT("fit contains missing or invalid nodes; unchanged"); return false; }
        if (OutPlan.Boxes.Num()!=1) { OutError=TEXT("explicit fit requires exactly one existing comment box"); return false; }
        const FBox& Existing=OutPlan.Boxes[0];
        // Union the requested enclosure with the entire old rectangle. Outward expansion
        // preserves old contents even when the caller supplied only some member IDs.
        const double Right=FMath::Max(Existing.X+Existing.Width,OutPlan.Planned.X+OutPlan.Planned.Width);
        const double Bottom=FMath::Max(Existing.Y+Existing.Height,OutPlan.Planned.Y+OutPlan.Planned.Height);
        OutPlan.Planned.X=FMath::Min(Existing.X,OutPlan.Planned.X);
        OutPlan.Planned.Y=FMath::Min(Existing.Y,OutPlan.Planned.Y);
        OutPlan.Planned.Width=Right-OutPlan.Planned.X;
        OutPlan.Planned.Height=Bottom-OutPlan.Planned.Y;
        OutPlan.Boxes[0].MemberCount = OutPlan.NodeKeys.Num();
	}
	// An empty Box keeps the explicit legacy fit-all behavior.
	return true;
}

FString FTurboMCPComment::RenderPreview(const FPlan& InPlan)
{
	static const TCHAR* const ActionNames[] = {
		TEXT("create"), TEXT("set"), TEXT("fit"), TEXT("delete")
	};
	const TCHAR* ActionName = ActionNames[static_cast<int32>(InPlan.Action)];

	FString Out = FString::Printf(TEXT("preview: %s in %s : %s"),
		ActionName, *InPlan.Asset, *InPlan.GraphName);

	Out += FString::Printf(TEXT("\n%d comment box(es) in the graph"), InPlan.Boxes.Num());

	if (InPlan.Action == EAction::Create)
	{
		Out += FString::Printf(
			TEXT("\nwould create a box around %d node(s), at x=%.0f y=%.0f, %.0fx%.0f"),
			InPlan.Planned.MemberCount, InPlan.Planned.X, InPlan.Planned.Y,
			InPlan.Planned.Width, InPlan.Planned.Height);
		if (InPlan.bHasText)
		{
			Out += FString::Printf(TEXT("\ntext: \"%s\""), *InPlan.Text);
		}
		else
		{
			// Named rather than left blank, because a box with no text is a rectangle whose
			// purpose nobody can recover later.
			Out += TEXT("\ntext: (none given, so the box will be a plain rectangle)");
		}
		if (InPlan.bHasColor)
		{
			Out += FString::Printf(TEXT("\ncolour: %s"), *InPlan.Color);
		}
		Out += FString::Printf(TEXT("\naround: %s"),
			*FString::Join(InPlan.Planned.Members, TEXT(", ")));
	}
	else if (InPlan.Action == EAction::Delete)
	{
		for (const FBox& Box : InPlan.Boxes)
		{
			if (Box.Key.Equals(InPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				Out += FString::Printf(
					TEXT("\nwould delete box %s \"%s\", which holds %d node(s)"),
					*Box.Key, *Box.Text, Box.MemberCount);
				Out += TEXT("\nITS NODES ARE KEPT. A box is a label, not a container: nothing ")
					TEXT("inside it is removed or moved.");
				break;
			}
		}
	}
	else if (InPlan.Action == EAction::Set)
	{
		for (const FBox& Box : InPlan.Boxes)
		{
			if (Box.Key.Equals(InPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				Out += FString::Printf(TEXT("\nbox %s is now \"%s\" at %.0fx%.0f"),
					*Box.Key, *Box.Text, Box.Width, Box.Height);
				break;
			}
		}
		if (InPlan.bHasText)  { Out += FString::Printf(TEXT("\ntext -> \"%s\""), *InPlan.Text); }
		if (InPlan.bHasColor) { Out += FString::Printf(TEXT("\ncolour -> %s"), *InPlan.Color); }
		if (InPlan.Width > 0.0)
		{
			Out += FString::Printf(TEXT("\nsize -> %.0fx%.0f"), InPlan.Width, InPlan.Height);
		}
	}
	else
	{
        if (!InPlan.NodeKeys.IsEmpty())
            Out += FString::Printf(TEXT("\nexplicit fit: expand outward only, preserving the old rectangle and all existing contents; planned rect=(%.0f,%.0f,%.0f,%.0f)"),InPlan.Planned.X,InPlan.Planned.Y,InPlan.Planned.Width,InPlan.Planned.Height);
		int32 Fitted = 0;
		int32 Empty = 0;
		for (const FBox& Box : InPlan.Boxes)
		{
			if (Box.MemberCount == 0) { ++Empty; } else { ++Fitted; }
		}
		Out += FString::Printf(
			TEXT("\nwould fit %d box(es) to their contents, leaving %.0f units of padding"),
			Fitted, FitPadding);
		if (Empty > 0)
		{
			Out += FString::Printf(
				TEXT("\n%d box(es) hold nothing and would be LEFT ALONE, because a box with no ")
				TEXT("contents has no size to fit to"), Empty);
		}
	}

	// The empty boxes are listed for EVERY action, because `03_TOOLS.md` asks for them to be
	// listed and a caller who is about to tidy a graph wants to know before choosing where to
	// look. `P1`: never return nothing.
	if (InPlan.EmptyBoxes.Num() > 0)
	{
		Out += FString::Printf(
			TEXT("\n\n%d box(es) hold NOTHING, which is usually a leftover. They are listed ")
			TEXT("because a box around no nodes is a label with nothing to label:"),
			InPlan.EmptyBoxes.Num());
		for (const FBox& Box : InPlan.EmptyBoxes)
		{
			Out += FString::Printf(TEXT("\n  %s  \"%s\""), *Box.Key, *Box.Text);
		}
	}

	else if (InPlan.Boxes.Num() > 0)
	{
		Out += TEXT("\nno box is empty.");
	}

	return Out;
}

FString FTurboMCPComment::Apply(const FPlan& InPlan)
{
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	FString FindError;
	UEdGraph* Graph = FindGraph(Blueprint, InPlan.GraphName, FindError);
	if (Graph == nullptr)
	{
		return FString::Printf(TEXT("error: %s"), *FindError);
	}

	if (InPlan.Action == EAction::Create)
	{
		UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph);
		// THE GUID IS MADE HERE, because `AddNode` is what would otherwise make it and the key is
		// read back straight afterwards. Without this every box created in one session carried the
		// key `000000`, measured: two boxes were made and BOTH reported `000000`, so a box could
		// not be named again and `set` would have hit whichever came first. A key that does not
		// identify is worse than no key, because it looks like one.
		Comment->CreateNewGuid();

		Comment->NodePosX = static_cast<int32>(InPlan.Planned.X);
		Comment->NodePosY = static_cast<int32>(InPlan.Planned.Y);
		Comment->NodeWidth = static_cast<int32>(InPlan.Planned.Width);
		Comment->NodeHeight = static_cast<int32>(InPlan.Planned.Height);
		Comment->NodeComment = InPlan.Text;
		if (InPlan.bHasColor)
		{
			FLinearColor Parsed;
			FString Why;
			ParseColor(InPlan.Color, Parsed, Why);
			Comment->CommentColor = Parsed;
		}
		Graph->Modify();
		Graph->AddNode(Comment, /*bUserAction=*/true, /*bSelectNewNode=*/false);
		Graph->MarkPackageDirty();

		FBox Made;
		ReadBox(Comment, Made);

		FString Out = FString::Printf(
			TEXT("changed: created comment box %s in %s : %s"),
			*Made.Key, *Resolved.ObjectPath, *Graph->GetName());
		Out += FString::Printf(TEXT("\nsize: %.0fx%.0f at (%.0f, %.0f)"),
			Made.Width, Made.Height, Made.X, Made.Y);
		Out += FString::Printf(TEXT("\nholds %d node(s)"), Made.MemberCount);
		Out += FString::Printf(TEXT("\n; refer to it later as %s"), *Made.Key);
		Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
		return Out;
	}

	if (InPlan.Action == EAction::Delete)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
			if (Comment == nullptr
				|| !KeyOf(Comment).Equals(InPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				continue;
			}

			TArray<FString> Members;
			LocalComputeMembers(Comment, Members);
			const int32 MemberTotal = Members.Num();
			const FString Text = Comment->NodeComment;

			Graph->Modify();
			Comment->DestroyNode();
			Graph->MarkPackageDirty();

			// THE READ-BACK RE-COUNTS THE BOXES, so "the box is gone" is checked rather than asserted.
			// *** THE NODES ARE NOT RE-MEASURED: `MemberTotal` is taken before the destroy and states
			// what the box HELD, so "its nodes were kept" rests on `DestroyNode` removing the comment
			// and nothing else. WHAT IS VERIFIED HERE IS THE BOX, NOT THE NODES. ***
			int32 BoxesLeft = 0;
			for (UEdGraphNode* Check : Graph->Nodes)
			{
				if (Cast<UEdGraphNode_Comment>(Check) != nullptr)
				{
					++BoxesLeft;
				}
			}

			FString Out = FString::Printf(
				TEXT("changed: deleted comment box '%s' from %s"), *Text, *Graph->GetName());
			Out += FString::Printf(TEXT("\nboxes in the graph now: %d (was %d)"),
				BoxesLeft, InPlan.Boxes.Num());
			Out += FString::Printf(
				TEXT("\nits %d node(s) are untouched: a box is a label, not a container."),
				MemberTotal);
			Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
			return Out;
		}

		return FString::Printf(
			TEXT("unchanged: no comment box with key '%s' is in %s, so nothing was deleted."),
			*InPlan.BoxKey, *Graph->GetName());
	}

	if (InPlan.Action == EAction::Set)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
			if (Comment == nullptr
				|| !KeyOf(Comment).Equals(InPlan.BoxKey, ESearchCase::IgnoreCase))
			{
				continue;
			}

			// The OLD values are captured first, so the result can say what changed rather than
			// only what it is now. `P4` wants a change a caller can check.
			const FString OldText = Comment->NodeComment;
			const FLinearColor OldColor = Comment->CommentColor;
			const int32 OldWidth = Comment->NodeWidth;
			const int32 OldHeight = Comment->NodeHeight;

			Graph->Modify();
			if (InPlan.bHasText)
			{
				Comment->NodeComment = InPlan.Text;
			}
			if (InPlan.bHasColor)
			{
				FLinearColor Parsed;
				FString Why;
				ParseColor(InPlan.Color, Parsed, Why);
				Comment->CommentColor = Parsed;
			}
			if (InPlan.Width > 0.0)
			{
				Comment->NodeWidth = static_cast<int32>(InPlan.Width);
			}
			if (InPlan.Height > 0.0)
			{
				Comment->NodeHeight = static_cast<int32>(InPlan.Height);
			}
			Graph->MarkPackageDirty();

			const bool bChanged = OldText != Comment->NodeComment
				|| !OldColor.Equals(Comment->CommentColor)
				|| OldWidth != Comment->NodeWidth
				|| OldHeight != Comment->NodeHeight;

			if (!bChanged)
			{
				return FString::Printf(
					TEXT("unchanged: box %s already has those values."), *InPlan.BoxKey);
			}

			FString Out = FString::Printf(TEXT("changed: updated comment box %s in %s"),
				*InPlan.BoxKey, *Graph->GetName());
			if (OldText != Comment->NodeComment)
			{
				Out += FString::Printf(TEXT("\ntext: \"%s\" -> \"%s\""),
					*OldText, *Comment->NodeComment);
			}
			if (OldWidth != Comment->NodeWidth || OldHeight != Comment->NodeHeight)
			{
				Out += FString::Printf(TEXT("\nsize: %dx%d -> %dx%d"),
					OldWidth, OldHeight, Comment->NodeWidth, Comment->NodeHeight);
			}
			if (!OldColor.Equals(Comment->CommentColor))
			{
				Out += TEXT("\ncolour: changed");
			}
			Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
			return Out;
		}

		return FString::Printf(
			TEXT("unchanged: no comment box with key '%s' is in %s, so nothing changed."),
			*InPlan.BoxKey, *Graph->GetName());
	}

	// FitAll.
	int32 Fitted = 0;
	int32 Empty = 0;
	TArray<FString> FittedLines;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
		if (Comment == nullptr || (!InPlan.BoxKey.IsEmpty() && KeyOf(Comment) != InPlan.BoxKey))
		{
			continue;
		}

		double X = 0.0, Y = 0.0, Width = 0.0, Height = 0.0;
		if (!InPlan.NodeKeys.IsEmpty())
		{ X = InPlan.Planned.X; Y = InPlan.Planned.Y; Width = InPlan.Planned.Width; Height = InPlan.Planned.Height; }
		else if (!ComputeFit(Comment, X, Y, Width, Height))
		{
			// A box with nothing in it has no size to fit to. Reported rather than resized to
			// zero, which would make it invisible and look like the tool had deleted it.
			++Empty;
			continue;
		}

		Comment->Modify();
		const int32 OldWidth = Comment->NodeWidth;
		const int32 OldHeight = Comment->NodeHeight;

		Graph->Modify();
		Comment->NodePosX = static_cast<int32>(X);
		Comment->NodePosY = static_cast<int32>(Y);
		Comment->NodeWidth = static_cast<int32>(Width);
		Comment->NodeHeight = static_cast<int32>(Height);
		++Fitted;

		FittedLines.Add(FString::Printf(TEXT("\n  %s \"%s\": %dx%d -> %dx%d"),
			*KeyOf(Comment), *Comment->NodeComment, OldWidth, OldHeight,
			Comment->NodeWidth, Comment->NodeHeight));
	}

	if (Fitted > 0)
	{
		Graph->MarkPackageDirty();
	}

	FString Out;
	if (Fitted > 0)
	{
		Out = FString::Printf(TEXT("changed: fitted %d comment box(es) in %s"),
			Fitted, *Graph->GetName());
	}
	else
	{
		// `unchanged:` because there was nothing to do, which is a different answer from a refusal
		// and must not be reported as a change.
		Out = FString::Printf(
			TEXT("unchanged: no comment box in %s needed fitting."), *Graph->GetName());
	}

	for (const FString& Line : FittedLines)
	{
		Out += Line;
	}

	if (Empty > 0)
	{
		Out += FString::Printf(
			TEXT("\n%d box(es) hold nothing and were LEFT ALONE, because a box with no contents ")
			TEXT("has no size to fit to. %s"),
			Empty,
			Fitted > 0 ? TEXT("The two counts add up to the boxes in the graph.")
				: TEXT("They are listed by P-COMMENT."));
	}

	Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
	return Out;
}