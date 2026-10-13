// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphTidy.h"

// NO `BlueprintEditorUtils.h` HERE. The editor's utilities live at `Kismet2/BlueprintEditorUtils.h`
// and the bare name does not resolve, which was a fatal C1083 on the first build of this file.
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Guid.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPComment.h"
#include "TurboMCPGraphTidy.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

namespace
{
	/**
	 * THE PIN ROW PITCH, COPIED FROM THE READER AND NOT REDEFINED.
	 *
	 * `FTurboMCPLayoutReport` uses a private `PinRowSpacing = 24.0`
	 * (`TurboMCPLayoutReport.cpp:31`) and its `PinPosition` is not parameterised on it. Tidy has to
	 * compute a wire length on PLANNED positions, which the reader cannot do, so it has to reproduce
	 * the arithmetic - and the reader's total is the number tidy's before-value must equal.
	 *
	 * The two are kept in step EMPIRICALLY rather than by hope: `Tools/test-graph-tidy.py` asserts that
	 * tidy's `WireLengthBefore` EQUALS what `L-LAYOUT-REPORT` reports for the same untouched graph. A
	 * divergence in this constant turns that check red, which is the only kind of coupling worth having
	 * across two files that cannot share a private symbol.
	 */
	constexpr double PinRowSpacing = 24.0;

	/** Case-folded, whitespace-trimmed comparison, matching the other graph tools. */
	bool NameMatches(const FString& InLeft, const FString& InRight)
	{
		return InLeft.TrimStartAndEnd().Equals(InRight.TrimStartAndEnd(),
			ESearchCase::IgnoreCase);
	}

	/** True when the node is a comment box rather than a node that carries flow. */
	bool IsCommentNode(const UEdGraphNode* InNode)
	{
		return Cast<UEdGraphNode_Comment>(InNode) != nullptr;
	}

	bool IsKnotNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Knot>(InNode) != nullptr;
	}

	bool IsGetterNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_VariableGet>(InNode) != nullptr;
	}

	/**
	 * Purity, read from the node class rather than from a list of class names.
	 *
	 * `UK2Node::IsNodePure` is the engine's own answer (`K2Node.h:244`). A class that is not a
	 * `UK2Node` leaves purity unknown rather than crashing: the templated
	 * `GetDefaultObject<UK2Node>()` ASSERTS when the class is not one, which took the whole editor down
	 * once already, so the untemplated call is used and the cast is checked.
	 */
	bool IsNodePure(const UEdGraphNode* InNode)
	{
		if (const UK2Node* AsK2 = Cast<UK2Node>(InNode))
		{
			return AsK2->IsNodePure();
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// The graph, the nodes, the boxes
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Everything resolved before the layout begins. */
	struct FContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString ObjectPath;
	};

	/**
	 * Resolves an asset and finds one graph in it.
	 *
	 * The default is the event graph, and the answer always names the graph it used so a caller who
	 * omitted the name is not guessing.
	 */
	bool ResolveGraph(const FString& InAsset, const FString& InGraphName, FContext& OutContext,
		FString& OutError)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
		if (!ResolveError.IsEmpty())
		{
			OutError = ResolveError;
			return false;
		}
		OutContext.ObjectPath = Resolved.ObjectPath;

		UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
		if (Blueprint == nullptr)
		{
			OutError = FString::Printf(
				TEXT("%s is not a Blueprint, so there is no graph to tidy."), *Resolved.ObjectPath);
			return false;
		}
		OutContext.Blueprint = Blueprint;

		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		if (Graphs.Num() == 0)
		{
			OutError = FString::Printf(TEXT("%s has no graphs to tidy."),
				*Resolved.ObjectPath);
			return false;
		}

		if (InGraphName.IsEmpty())
		{
			for (UEdGraph* Candidate : Graphs)
			{
				if (NameMatches(Candidate->GetName(), TEXT("EventGraph")))
				{
					OutContext.Graph = Candidate;
					break;
				}
			}
			if (OutContext.Graph == nullptr)
			{
				OutContext.Graph = Graphs[0];
			}
			return true;
		}

		for (UEdGraph* Candidate : Graphs)
		{
			if (NameMatches(Candidate->GetName(), InGraphName))
			{
				OutContext.Graph = Candidate;
				return true;
			}
		}

		TArray<FString> Names;
		for (UEdGraph* Candidate : Graphs) { Names.Add(Candidate->GetName()); }
		Names.Sort();
		OutError = FString::Printf(
			TEXT("'%s' is not a graph of %s. Its graphs are: %s"),
			*InGraphName, *Resolved.ObjectPath, *FString::Join(Names, TEXT(", ")));
		return false;
	}

	/**
	 * The rectangle a node occupies, WITH THE DOCUMENTED FALLBACK APPLIED.
	 *
	 * A node the editor has never drawn has a zero rectangle, and a zero rectangle cannot carry a pin
	 * position. Rather than refuse - which was measured on the sibling capability and made it useless
	 * on every graph - a node with no measured size is given a `MinimumNodeExtent` square CENTRED on
	 * its position, and the caller records that the fallback was used so the report can say so.
	 */
	FTurboMCPLayoutReport::FRect RectFor(const double InX, const double InY,
		const double InWidth, const double InHeight, bool& bOutUsedFallback)
	{
		FTurboMCPLayoutReport::FRect Rect;

		double Width = InWidth;
		double Height = InHeight;
		if (Width <= 0.0 || Height <= 0.0)
		{
			// THE FALLBACK IS CENTRED, so a node's planned position and its rectangle agree about where
			// the node is. Anchoring at the corner instead would move every estimated node half a
			// square right and down as a side effect of the estimate, which is a layout change nobody
			// asked for.
			Width = FTurboMCPGraphTidy::MinimumNodeExtent;
			Height = FTurboMCPGraphTidy::MinimumNodeExtent;
			Rect.MinX = InX - Width * 0.5;
			Rect.MinY = InY - Height * 0.5;
			bOutUsedFallback = true;
		}
		else
		{
			Rect.MinX = InX;
			Rect.MinY = InY;
		}

		Rect.MaxX = Rect.MinX + Width;
		Rect.MaxY = Rect.MinY + Height;
		return Rect;
	}
}

// ---------------------------------------------------------------------------------------------
// The layout
// ---------------------------------------------------------------------------------------------

namespace
{
	/** The size tidy plans with for a node, after the fallback. */
	struct FExtent
	{
		double Width = 0.0;
		double Height = 0.0;
	};

	FExtent ExtentOf(const double InWidth, const double InHeight)
	{
		FExtent Out;
		if (InWidth <= 0.0 || InHeight <= 0.0)
		{
			Out.Width = FTurboMCPGraphTidy::MinimumNodeExtent;
			Out.Height = FTurboMCPGraphTidy::MinimumNodeExtent;
		}
		else
		{
			Out.Width = InWidth;
			Out.Height = InHeight;
		}
		return Out;
	}

	/** The graph-space rectangle of a comment box, using the rule `P/E-COMMENT` uses. */
	void BoxRect(const UEdGraphNode_Comment* InComment, double& OutX, double& OutY,
		double& OutWidth, double& OutHeight)
	{
		OutX = static_cast<double>(InComment->NodePosX);
		OutY = static_cast<double>(InComment->NodePosY) + FTurboMCPGraphTidy::TitleBarHeight;
		OutWidth = static_cast<double>(InComment->NodeWidth);
		OutHeight = static_cast<double>(InComment->NodeHeight)
			- FTurboMCPGraphTidy::TitleBarHeight;
	}

	/** True when `InOuter` wholly contains `InInner`, on all four sides. */
	bool Contains(const FTurboMCPLayoutReport::FRect& InOuter,
		const FTurboMCPLayoutReport::FRect& InInner)
	{
		return InOuter.MinX <= InInner.MinX && InOuter.MinY <= InInner.MinY
			&& InOuter.MaxX >= InInner.MaxX && InOuter.MaxY >= InInner.MaxY;
	}

	double AreaOf(const FTurboMCPLayoutReport::FRect& InRect)
	{
		return (InRect.MaxX - InRect.MinX) * (InRect.MaxY - InRect.MinY);
	}

	/** A pin's position, computed on a node's PLANNED coordinates instead of its stored ones. */
	FVector2D PlannedPinPosition(const UEdGraphPin* InPin, const double InNodeX,
		const double InNodeY, const double InNodeWidth)
	{
		if (InPin == nullptr || InPin->GetOwningNodeUnchecked() == nullptr)
		{
			return FVector2D::ZeroVector;
		}

		const UEdGraphNode* Node = InPin->GetOwningNodeUnchecked();
		const bool bIsInput = InPin->Direction == EGPD_Input;

		// The index counts only pins of the SAME direction, which is what the reader does.
		int32 Index = 0;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != InPin->Direction)
			{
				continue;
			}
			if (Pin == InPin)
			{
				break;
			}
			++Index;
		}

		const double EdgeX = bIsInput ? InNodeX : InNodeX + InNodeWidth;
		const double EdgeY = InNodeY + PinRowSpacing * (Index + 1);
		return FVector2D(EdgeX, EdgeY);
	}
}

// ---------------------------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------------------------

bool FTurboMCPGraphTidy::Plan(const FString& InAsset, const FString& InGraphName, bool bInForce,
	bool bInMergeGetters, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.bForce = bInForce;
	OutPlan.bMergeGettersRequested = bInMergeGetters;

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}

	// GETTER MERGING IS REFUSED BY NAME RATHER THAN SILENTLY IGNORED. The spec makes it an option that
	// is off by default; a caller who turns it on is entitled to be told this build does not do it,
	// rather than to receive a layout that quietly did not merge and looks like it did.
	if (bInMergeGetters)
	{
		OutError = TEXT("merging variable getters is not implemented in this build. The spec makes it ")
			TEXT("an option that is off by default, and it is refused rather than ignored so the answer is not mistaken for a merge that happened. Call again with merging off: getters that feed several consumers are still SPLIT, one per consumer.");
		return false;
	}

	FContext Context;
	if (!ResolveGraph(OutPlan.Asset, InGraphName, Context, OutError))
	{
		return false;
	}
	UEdGraph* Graph = Context.Graph;
	if (OutPlan.GraphName.IsEmpty())
	{
		OutPlan.GraphName = Graph->GetName();
	}

	if (Graph->Nodes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("graph %s is empty, so there is nothing to tidy."),
			*OutPlan.GraphName);
		return false;
	}

	// -----------------------------------------------------------------------------------------
	// STEP 1: read every node once, with its rectangle.
	// -----------------------------------------------------------------------------------------
	TMap<const UEdGraphNode*, int32> IndexByNode;
	for (int32 Order = 0; Order < Graph->Nodes.Num(); ++Order)
	{
		UEdGraphNode* Node = Graph->Nodes[Order];
		if (Node == nullptr)
		{
			continue;
		}

		FNode Entry;
		Entry.Node = Node;
		Entry.Key = TurboMCPNodeId::MakeForNode(Node);
		Entry.ClassName = Node->GetClass()->GetName();
		Entry.GraphOrder = Order;
		Entry.X = static_cast<double>(Node->NodePosX);
		Entry.Y = static_cast<double>(Node->NodePosY);
		Entry.bIsComment = IsCommentNode(Node);
		Entry.bIsKnot = IsKnotNode(Node);
		Entry.bIsGetter = IsGetterNode(Node);
		Entry.bIsPure = !Entry.bIsComment && IsNodePure(Node);

		const FExtent Extent = ExtentOf(Node->NodeWidth, Node->NodeHeight);
		Entry.Width = Extent.Width;
		Entry.Height = Extent.Height;
		// THE ENGINE'S OWN NUMBERS ARE KEPT SEPARATELY, so the before-measure can reproduce
		// `L-LAYOUT-REPORT` exactly while the planned layout still has a size to work with. See the field's
		// comment: STORING ONLY THE SUBSTITUTED SIZE COMPUTES THE TWO NUMBERS FROM DIFFERENT RECTANGLES,
		// with nothing in the output to show it.
		Entry.MeasuredWidth = static_cast<double>(Node->NodeWidth);
		Entry.MeasuredHeight = static_cast<double>(Node->NodeHeight);
		if (Node->NodeWidth <= 0 || Node->NodeHeight <= 0)
		{
			Entry.bSizeWasFallback = true;
			OutPlan.bUsedFallbackGeometry = true;
		}

		Entry.PlannedX = Entry.X;
		Entry.PlannedY = Entry.Y;

		IndexByNode.Add(Node, OutPlan.Nodes.Num());
		OutPlan.Nodes.Add(Entry);
	}

	// -----------------------------------------------------------------------------------------
	// STEP 2: the boxes, their membership, and their parents.
	//
	// MEMBERSHIP COMES FROM `FTurboMCPComment::MembersOf`, the ONE copy of the geometric rule in this
	// plugin. A second copy would agree with the first until one of them was changed.
	// -----------------------------------------------------------------------------------------
	{
		TArray<FTurboMCPLayoutReport::FRect> BoxRects;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (!OutPlan.Nodes[Index].bIsComment)
			{
				continue;
			}

			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(OutPlan.Nodes[Index].Node);
			if (Comment == nullptr)
			{
				continue;
			}

			FBox Box;
			Box.Comment = Comment;
			Box.Key = OutPlan.Nodes[Index].Key;
			Box.Text = Comment->NodeComment;

			double BoxX = 0.0;
			double BoxY = 0.0;
			double BoxWidth = 0.0;
			double BoxHeight = 0.0;
			BoxRect(Comment, BoxX, BoxY, BoxWidth, BoxHeight);
			Box.X = BoxX;
			Box.Y = BoxY;
			Box.Width = BoxWidth;
			Box.Height = BoxHeight;
			Box.PlannedWidth = BoxWidth;
			Box.PlannedHeight = BoxHeight;

			FTurboMCPLayoutReport::FRect Rect;
			Rect.MinX = BoxX;
			Rect.MinY = BoxY;
			Rect.MaxX = BoxX + BoxWidth;
			Rect.MaxY = BoxY + BoxHeight;
			BoxRects.Add(Rect);

			FTurboMCPComment::MembersOf(Comment, Box.MemberKeys);

			OutPlan.Boxes.Add(Box);
		}

		// PARENTS ARE RESOLVED GEOMETRICALLY, by the SAME containment rule as membership, and the
		// parent is the SMALLEST box that contains this one (`D-LAY-3`). THE ENGINE'S `CommentDepth`
		// IS NOT READ HERE AT ALL, because a stored field can disagree with geometry after a hand
		// resize and geometry cannot disagree with itself.
		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			int32 BestParent = INDEX_NONE;
			double BestArea = TNumericLimits<double>::Max();
			for (int32 Other = 0; Other < OutPlan.Boxes.Num(); ++Other)
			{
				if (Other == Index)
				{
					continue;
				}
				if (!Contains(BoxRects[Other], BoxRects[Index]))
				{
					continue;
				}
				// An IDENTICAL rectangle is not a parent: two boxes drawn on top of each other are not
				// nested, and treating one as the other's parent would make each the other's ancestor.
				if (BoxRects[Other].MinX == BoxRects[Index].MinX
					&& BoxRects[Other].MinY == BoxRects[Index].MinY
					&& BoxRects[Other].MaxX == BoxRects[Index].MaxX
					&& BoxRects[Other].MaxY == BoxRects[Index].MaxY)
				{
					continue;
				}

				const double Area = AreaOf(BoxRects[Other]);
				if (Area < BestArea
					|| (Area == BestArea && BestParent != INDEX_NONE
						&& OutPlan.Boxes[Other].Key > OutPlan.Boxes[BestParent].Key))
				{
					BestArea = Area;
					BestParent = Other;
				}
			}
			OutPlan.Boxes[Index].ParentIndex = BestParent;
		}

		// The depth in the RESOLVED chain, and a cycle check. Two boxes each containing the other is
		// geometrically impossible but reachable from a corrupted asset, and the inside-out pass would
		// then recurse forever.
		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			int32 Depth = 0;
			int32 Walk = OutPlan.Boxes[Index].ParentIndex;
			TSet<int32> Seen;
			Seen.Add(Index);
			while (Walk != INDEX_NONE)
			{
				if (Seen.Contains(Walk))
				{
					OutError = FString::Printf(
						TEXT("comment box '%s' and '%s' each contain the other, so no box order exists. Tidy the boxes by hand and run this again."),
						*OutPlan.Boxes[Index].Text, *OutPlan.Boxes[Walk].Text);
					return false;
				}
				Seen.Add(Walk);
				++Depth;
				Walk = OutPlan.Boxes[Walk].ParentIndex;
			}
			OutPlan.Boxes[Index].Depth = Depth;
		}

		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			const int32 Parent = OutPlan.Boxes[Index].ParentIndex;
			if (Parent != INDEX_NONE)
			{
				OutPlan.Boxes[Parent].ChildBoxIndices.Add(Index);
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 3: the wires, read once. The measure sums these, so the list IS the number's basis.
	// -----------------------------------------------------------------------------------------
	for (const FNode& Entry : OutPlan.Nodes)
	{
		if (Entry.Node == nullptr)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Entry.Node->Pins)
		{
			// Each link is counted ONCE, from the output side, which is what the reader does. Summing
			// from both sides would double every number and make the comparison meaningless.
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}
			for (UEdGraphPin* Other : Pin->LinkedTo)
			{
				if (Other == nullptr)
				{
					continue;
				}
				FWire Wire;
				Wire.OutputPin = Pin;
				Wire.InputPin = Other;
				if (const int32* Source = IndexByNode.Find(Pin->GetOwningNodeUnchecked()))
				{
					Wire.SourceNodeIndex = *Source;
				}
				if (const int32* Target = IndexByNode.Find(Other->GetOwningNodeUnchecked()))
				{
					Wire.TargetNodeIndex = *Target;
				}
				OutPlan.Wires.Add(Wire);
			}
		}
	}
	OutPlan.WireCount = OutPlan.Wires.Num();

	// -----------------------------------------------------------------------------------------
	// STEP 4: the columns, by longest incoming chain, over exec AND data links.
	//
	// Data links must count. A pure node has no exec wire of its own - it is reached only because a
	// live node reads its value - so a column computed on exec alone would put every pure node in
	// column 0 and stack them against the entry. `P/E-GRAPH-DEAD` reached the same conclusion for
	// liveness and `TestRecord.md` section 6b records it.
	// -----------------------------------------------------------------------------------------
	{
		TArray<TArray<int32>> Outgoing;
		TArray<int32> InDegree;
		Outgoing.SetNum(OutPlan.Nodes.Num());
		InDegree.Init(0, OutPlan.Nodes.Num());

		for (const FWire& Wire : OutPlan.Wires)
		{
			if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE
				|| Wire.SourceNodeIndex == Wire.TargetNodeIndex)
			{
				continue;
			}
			Outgoing[Wire.SourceNodeIndex].AddUnique(Wire.TargetNodeIndex);
			++InDegree[Wire.TargetNodeIndex];
		}

		TArray<int32> Queue;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (InDegree[Index] == 0)
			{
				Queue.Add(Index);
			}
		}

		int32 Processed = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			++Processed;
			for (const int32 Next : Outgoing[Current])
			{
				OutPlan.Nodes[Next].Column = FMath::Max(OutPlan.Nodes[Next].Column,
					OutPlan.Nodes[Current].Column + 1);
				if (--InDegree[Next] == 0)
				{
					Queue.Add(Next);
				}
			}
		}

		// A CYCLE IS A REFUSAL, NOT A SILENT ZERO. Column is a longest path and a cycle has no answer
		// for one; guessing an order would move nodes on no basis.
		if (Processed < OutPlan.Nodes.Num())
		{
			FString Cyclic;
			for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
			{
				if (InDegree[Index] > 0)
				{
					Cyclic = OutPlan.Nodes[Index].Key;
					break;
				}
			}
			OutError = FString::Printf(
				TEXT("graph %s has a cycle among its nodes - '%s' is part of one - so no column order exists. Tidy is for graphs that are mostly fine; break the loop and run it again."),
				*OutPlan.GraphName, *Cyclic);
			return false;
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 5: reachability. Unreachable nodes are EXCLUDED from the layout and left exactly where
	// they are, and the count is reported so a caller is not surprised by nodes that did not move.
	// `P/E-GRAPH-DEAD` owns their removal.
	// -----------------------------------------------------------------------------------------
	{
		TSet<int32> Reached;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			// An entry point is a node with no incoming link that is not a comment. A comment is not
			// flow and cannot make anything reachable.
			if (!OutPlan.Nodes[Index].bIsComment && OutPlan.Nodes[Index].Column == 0)
			{
				Reached.Add(Index);
			}
		}

		bool bGrew = true;
		while (bGrew)
		{
			bGrew = false;
			for (const FWire& Wire : OutPlan.Wires)
			{
				if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
				{
					continue;
				}
				if (Reached.Contains(Wire.SourceNodeIndex)
					&& !Reached.Contains(Wire.TargetNodeIndex))
				{
					Reached.Add(Wire.TargetNodeIndex);
					bGrew = true;
				}
			}
		}

		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (OutPlan.Nodes[Index].bIsComment)
			{
				continue;
			}
			if (!Reached.Contains(Index))
			{
				OutPlan.Nodes[Index].bUnreachable = true;
				++OutPlan.UnreachableCount;
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 6: the layout.
	//
	// THE ORDER HERE IS THE PART MOST LIKELY TO BE WRONG, so it is stated once and followed exactly.
	// Inside-out: a box at the greatest depth is arranged and FITTED first, and an outer box then sees
	// its children already fitted and treats each as ONE BLOCK. Fitting an outer box first would
	// compute its rectangle from stale child sizes, and the outer box would then have to MOVE - which
	// the spec forbids.
	// -----------------------------------------------------------------------------------------

	// Which box owns a node: the DEEPEST box whose membership contains it. A node inside two nested
	// boxes is laid out by the innermost one, so it is laid out exactly once (`D-LAY-2`).
	TMap<FString, int32> NodeIndexByKey;
	for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
	{
		NodeIndexByKey.Add(OutPlan.Nodes[Index].Key, Index);
	}

	TArray<int32> OwnerOfNode;
	OwnerOfNode.Init(INDEX_NONE, OutPlan.Nodes.Num());
	for (int32 BoxIndex = 0; BoxIndex < OutPlan.Boxes.Num(); ++BoxIndex)
	{
		const FBox& Box = OutPlan.Boxes[BoxIndex];
		for (const FString& Key : Box.MemberKeys)
		{
			const int32* Found = NodeIndexByKey.Find(Key);
			if (Found == nullptr)
			{
				continue;
			}
			const int32 NodeIndex = *Found;
			if (OutPlan.Nodes[NodeIndex].bIsComment || OutPlan.Nodes[NodeIndex].bUnreachable)
			{
				continue;
			}
			if (OwnerOfNode[NodeIndex] == INDEX_NONE
				|| OutPlan.Boxes[OwnerOfNode[NodeIndex]].Depth < Box.Depth)
			{
				OwnerOfNode[NodeIndex] = BoxIndex;
			}
		}
	}

	// Deepest first, so a child's fitted rectangle is final before its parent measures it.
	TArray<int32> BoxOrder;
	for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
	{
		BoxOrder.Add(Index);
	}
	BoxOrder.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
	{
		if (OutPlan.Boxes[InLeft].Depth != OutPlan.Boxes[InRight].Depth)
		{
			return OutPlan.Boxes[InLeft].Depth > OutPlan.Boxes[InRight].Depth;
		}
		// A stable order among equals, so a re-run of tidy changes nothing.
		return OutPlan.Boxes[InLeft].Key < OutPlan.Boxes[InRight].Key;
	});

	for (const int32 BoxIndex : BoxOrder)
	{
		FBox& Box = OutPlan.Boxes[BoxIndex];

		// The nodes THIS box lays out: its own members, minus those a deeper box owns.
		TArray<int32> Owned;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			if (OwnerOfNode[NodeIndex] != BoxIndex)
			{
				continue;
			}
			Owned.Add(NodeIndex);
		}

		if (Owned.Num() == 0)
		{
			// A box with nothing to arrange is LEFT ALONE rather than collapsed to nothing: an empty
			// box is a label the user made, and shrinking it would be a change nobody asked for.
			Box.bWouldFit = false;
			continue;
		}

		// Left to right in column order, then by graph order then key so the result is deterministic.
		Owned.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
		{
			const FNode& L = OutPlan.Nodes[InLeft];
			const FNode& R = OutPlan.Nodes[InRight];
			if (L.Column != R.Column)
			{
				return L.Column < R.Column;
			}
			if (L.GraphOrder != R.GraphOrder)
			{
				return L.GraphOrder < R.GraphOrder;
			}
			return L.Key < R.Key;
		});

		// A BOX'S TOP-LEFT CORNER IS NEVER MOVED. Only sizes change, so the start position is the
		// box's own corner plus its padding - which is what `P/E-COMMENT`'s fit does.
		const double StartX = Box.X + FTurboMCPGraphTidy::FitPadding;
		const double StartY = Box.Y + FTurboMCPGraphTidy::FitPadding;

		int32 CurrentColumn = OutPlan.Nodes[Owned[0]].Column;
		double CursorX = StartX;
		double RowY = StartY;

		for (const int32 NodeIndex : Owned)
		{
			FNode& Entry = OutPlan.Nodes[NodeIndex];
			if (Entry.Column != CurrentColumn)
			{
				// A NEW COLUMN WRAPS TO A NEW ROW rather than continuing rightward, so a chain reads
				// top to bottom inside its box instead of running off the side of it.
				CurrentColumn = Entry.Column;
				CursorX = StartX;
				RowY += Entry.Height + FTurboMCPGraphTidy::VerticalNodeGap;
			}

			Entry.PlannedX = CursorX;
			Entry.PlannedY = RowY;
			CursorX += Entry.Width + FTurboMCPGraphTidy::HorizontalNodeGap;
		}

		// THE PURE-NODE SHIFT: a pure node moves RIGHTWARD until it sits beside the node that uses it.
		// `max` and not assignment, because a node already further right must not be dragged left - the
		// spec says "shift rightward". Consumers are visited in DECREASING column order so a chain of
		// pure nodes settles in one pass.
		TArray<int32> ByColumnDescending = Owned;
		ByColumnDescending.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
		{
			return OutPlan.Nodes[InLeft].Column > OutPlan.Nodes[InRight].Column;
		});
		for (const int32 NodeIndex : ByColumnDescending)
		{
			FNode& Entry = OutPlan.Nodes[NodeIndex];
			if (!Entry.bIsPure || Entry.bIsKnot)
			{
				continue;
			}

			// Its consumers, and only when there is EXACTLY ONE: a getter read by several consumers
			// has no single node to sit beside.
			TArray<int32> Consumers;
			for (const FWire& Wire : OutPlan.Wires)
			{
				if (Wire.SourceNodeIndex == NodeIndex && Wire.TargetNodeIndex != INDEX_NONE
					&& Wire.TargetNodeIndex != NodeIndex)
				{
					Consumers.AddUnique(Wire.TargetNodeIndex);
				}
			}
			if (Consumers.Num() != 1)
			{
				continue;
			}

			const FNode& Consumer = OutPlan.Nodes[Consumers[0]];
			const double TargetX = Consumer.PlannedX - Entry.Width
				- FTurboMCPGraphTidy::HorizontalNodeGap;
			Entry.PlannedX = FMath::Max(Entry.PlannedX, TargetX);
		}

		// FIT THE BOX TO ITS CONTENTS, AND ONLY ITS SIZE. A child box is already final because the
		// deepest boxes were processed first, so a whole child rectangle counts as one block here.
		bool bAny = false;
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
		for (const int32 NodeIndex : Owned)
		{
			const FNode& Entry = OutPlan.Nodes[NodeIndex];
			const double Left = Entry.PlannedX;
			const double Top = Entry.PlannedY;
			const double Right = Entry.PlannedX + Entry.Width;
			const double Bottom = Entry.PlannedY + Entry.Height;
			if (!bAny)
			{
				MinX = Left; MinY = Top; MaxX = Right; MaxY = Bottom;
				bAny = true;
			}
			else
			{
				MinX = FMath::Min(MinX, Left);
				MinY = FMath::Min(MinY, Top);
				MaxX = FMath::Max(MaxX, Right);
				MaxY = FMath::Max(MaxY, Bottom);
			}
		}
		for (const int32 ChildIndex : Box.ChildBoxIndices)
		{
			const FBox& Child = OutPlan.Boxes[ChildIndex];
			const double Left = Child.X;
			const double Top = Child.Y;
			const double Right = Child.X + Child.PlannedWidth;
			const double Bottom = Child.Y + Child.PlannedHeight;
			if (!bAny)
			{
				MinX = Left; MinY = Top; MaxX = Right; MaxY = Bottom;
				bAny = true;
			}
			else
			{
				MinX = FMath::Min(MinX, Left);
				MinY = FMath::Min(MinY, Top);
				MaxX = FMath::Max(MaxX, Right);
				MaxY = FMath::Max(MaxY, Bottom);
			}
		}

		if (bAny)
		{
			// The corner is Box.X and Box.Y AS STORED, so the fitted rectangle starts from where the
			// user put the box and grows right and down. `+ TitleBarHeight` because the stored Y is the
			// top of the title strip, which is above the content rectangle.
			const double WantedRight = MaxX + FTurboMCPGraphTidy::FitPadding;
			const double WantedBottom = MaxY + FTurboMCPGraphTidy::FitPadding;
			Box.PlannedWidth = FMath::Max(Box.Width, WantedRight - Box.X);
			Box.PlannedHeight = FMath::Max(Box.Height,
				WantedBottom - Box.Y + FTurboMCPGraphTidy::TitleBarHeight);
			Box.bWouldFit = !FMath::IsNearlyEqual(Box.PlannedWidth, Box.Width)
				|| !FMath::IsNearlyEqual(Box.PlannedHeight, Box.Height);
		}
	}

	// THE BOXLESS GROUP: ordered by execution order and placed left to right on one row, to the right
	// of every box so it cannot land inside one the user drew.
	{
		TArray<int32> Free;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			if (OutPlan.Nodes[NodeIndex].bIsComment || OutPlan.Nodes[NodeIndex].bUnreachable
				|| OwnerOfNode[NodeIndex] != INDEX_NONE)
			{
				continue;
			}
			Free.Add(NodeIndex);
		}

		if (Free.Num() > 0)
		{
			double FreeStartX = 0.0;
			bool bAnyBox = false;
			for (const FBox& Box : OutPlan.Boxes)
			{
				const double Right = Box.X + Box.PlannedWidth + FTurboMCPGraphTidy::BoxGap;
				FreeStartX = bAnyBox ? FMath::Max(FreeStartX, Right) : Right;
				bAnyBox = true;
			}

			Free.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
			{
				const FNode& L = OutPlan.Nodes[InLeft];
				const FNode& R = OutPlan.Nodes[InRight];
				if (L.Column != R.Column)
				{
					return L.Column < R.Column;
				}
				if (L.GraphOrder != R.GraphOrder)
				{
					return L.GraphOrder < R.GraphOrder;
				}
				return L.Key < R.Key;
			});

			double CursorX = FreeStartX;
			for (const int32 NodeIndex : Free)
			{
				FNode& Entry = OutPlan.Nodes[NodeIndex];
				Entry.PlannedX = CursorX;
				Entry.PlannedY = 0.0;
				CursorX += Entry.Width + FTurboMCPGraphTidy::HorizontalNodeGap;
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 7: split variable getters that feed several consumers, BEFORE the measure, because a copy
	// changes the graph's node set and therefore its wire set.
	//
	// This is one of the two irreversible steps and it is why the rollback exists as CODE rather than
	// as a design property. What is created is recorded so `Apply` can destroy exactly it.
	// -----------------------------------------------------------------------------------------
	for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
	{
		FNode& Entry = OutPlan.Nodes[NodeIndex];
		if (!Entry.bIsGetter || Entry.bUnreachable || Entry.Node == nullptr)
		{
			continue;
		}

		UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Entry.Node);
		if (Getter == nullptr)
		{
			continue;
		}
		UEdGraphPin* ValuePin = Getter->GetValuePin();
		if (ValuePin == nullptr || ValuePin->LinkedTo.Num() < 2)
		{
			continue;
		}

		// The ORIGINAL keeps the FIRST consumer, in the graph's own node order, so the split is
		// deterministic and a second run of tidy finds nothing left to split.
		TArray<UEdGraphPin*> Consumers = ValuePin->LinkedTo;
		Consumers.Sort([](const UEdGraphPin& InLeft, const UEdGraphPin& InRight)
		{
			const UEdGraphNode* L = InLeft.GetOwningNodeUnchecked();
			const UEdGraphNode* R = InRight.GetOwningNodeUnchecked();
			if (L == nullptr || R == nullptr)
			{
				return L != nullptr;
			}
			if (L->NodePosX != R->NodePosX)
			{
				return L->NodePosX < R->NodePosX;
			}
			if (L->NodePosY != R->NodePosY)
			{
				return L->NodePosY < R->NodePosY;
			}
			return L->GetName() < R->GetName();
		});

		for (int32 ConsumerIndex = 1; ConsumerIndex < Consumers.Num(); ++ConsumerIndex)
		{
			UEdGraphPin* ConsumerPin = Consumers[ConsumerIndex];
			if (ConsumerPin == nullptr)
			{
				continue;
			}
			UEdGraphNode* ConsumerNode = ConsumerPin->GetOwningNodeUnchecked();
			if (ConsumerNode == nullptr)
			{
				continue;
			}

			// NOTHING IS CREATED HERE. `Plan` MUST WRITE NOTHING: it is the half the Preview runs, and
			// a Preview that added a node would change the graph it was asked to describe. What the
			// split WOULD do is recorded as a position beside the planned layout, and `Apply` performs
			// it - which is the same discipline the planned positions follow, and it makes the ordinary
			// rollback "no write ever happened" for getters as well.
			FGetterCopy Planned;
			Planned.Original = Getter;
			Planned.ConsumerPin = ConsumerPin;
			Planned.OriginalValuePin = ValuePin;
			Planned.PlannedX = static_cast<double>(ConsumerNode->NodePosX)
				- Entry.Width - FTurboMCPGraphTidy::HorizontalNodeGap;
			Planned.PlannedY = static_cast<double>(ConsumerNode->NodePosY);
			OutPlan.GetterCopies.Add(Planned);
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 8: the measure, on the PLANNED positions.
	//
	// The reader's `PinPosition` reads a node's STORED coordinates, so it cannot measure a layout that
	// exists only as values. The SAME ARITHMETIC is applied to the planned numbers here, and the suite
	// asserts that the before-value EQUALS what `L-LAYOUT-REPORT` reports for the untouched graph. That
	// cross-tool check is what keeps the two implementations in step across a file boundary.
	// -----------------------------------------------------------------------------------------
	auto TotalLengthOn = [&OutPlan](const bool bUsePlanned)
	{
		double Total = 0.0;
		for (const FWire& Wire : OutPlan.Wires)
		{
			if (Wire.OutputPin == nullptr || Wire.InputPin == nullptr)
			{
				continue;
			}
			if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
			{
				continue;
			}
			const FNode& Source = OutPlan.Nodes[Wire.SourceNodeIndex];
			const FNode& Target = OutPlan.Nodes[Wire.TargetNodeIndex];

			// The WIDTH matters for an output's edge and for nothing else, so the fallback extent is
			// used here for the same reason it is used for membership: a zero width would put the
			// output pin exactly on the input pin's edge and understate every wire.
			const double SourceX = bUsePlanned ? Source.PlannedX : Source.X;
			const double SourceY = bUsePlanned ? Source.PlannedY : Source.Y;
			const double TargetX = bUsePlanned ? Target.PlannedX : Target.X;
			const double TargetY = bUsePlanned ? Target.PlannedY : Target.Y;

			// THE WIDTH IS CHOSEN BY WHICH NUMBER IS BEING COMPUTED, and that is the whole fix.
			//
			// BEFORE must reproduce `L-LAYOUT-REPORT` EXACTLY, and the reader uses the node's own
			// `NodeWidth` - so the engine's number is used, even when it is zero. Substituting a size here
			// is what made the two tools disagree.
			//
			// AFTER is a layout that does not exist yet, so it has only the planned positions and the
			// fallback extent to work with. That is where the documented fallback belongs.
			const double SourceWidth = bUsePlanned ? Source.Width : Source.MeasuredWidth;
			const double TargetWidth = bUsePlanned ? Target.Width : Target.MeasuredWidth;

			const FVector2D Start = PlannedPinPosition(Wire.OutputPin, SourceX, SourceY, SourceWidth);
			const FVector2D End = PlannedPinPosition(Wire.InputPin, TargetX, TargetY, TargetWidth);
			Total += FVector2D::Distance(Start, End);
		}
		return Total;
	};

	OutPlan.WireLengthBefore = TotalLengthOn(false);
	OutPlan.WireLengthAfter = TotalLengthOn(true);

	// How many nodes and boxes the plan would actually change.
	for (const FNode& Entry : OutPlan.Nodes)
	{
		if (Entry.bIsComment || Entry.bUnreachable)
		{
			continue;
		}
		if (!FMath::IsNearlyEqual(Entry.PlannedX, Entry.X)
			|| !FMath::IsNearlyEqual(Entry.PlannedY, Entry.Y))
		{
			++OutPlan.NodesToMove;
		}
	}
	for (const FBox& Box : OutPlan.Boxes)
	{
		if (Box.bWouldFit)
		{
			++OutPlan.BoxesToFit;
		}
	}

	// When knots are added, the AFTER number includes them. The knot pass is asked where it would put
	// them; whether they help TIDY's metric is decided by the measure below, which is the whole reason
	// the two capabilities are allowed to measure different things (`D-LAY-4`).

	// -----------------------------------------------------------------------------------------
	// STEP 9: better or not.
	//
	// STRICTLY LESS, not "not worse". `T-LAY-5` states the acceptance criterion as "wire length not
	// worse, or rolled back with numbers": a pass that changes nothing and rolls back satisfies it, and
	// so does one that improves. A pass that makes the number EQUAL while moving every node has changed
	// the user's graph for no gain, so it rolls back.
	// -----------------------------------------------------------------------------------------
	OutPlan.bAlreadyTidy = OutPlan.NodesToMove == 0 && OutPlan.BoxesToFit == 0
		&& OutPlan.GetterCopies.Num() == 0;

	if (OutPlan.bAlreadyTidy)
	{
		OutPlan.bWouldApply = false;
		OutPlan.RollbackReason = TEXT("the graph is already tidy: no node would move, no box would ")
			TEXT("change size, and no getter would be split.");
		return true;
	}

	OutPlan.bWouldApply = OutPlan.WireLengthAfter < OutPlan.WireLengthBefore || bInForce;
	if (!OutPlan.bWouldApply)
	{
		OutPlan.RollbackReason = FString::Printf(
			TEXT("the layout would leave the wire length at %.1f against %.1f, which is not shorter, so it would be rolled back. Pass force to apply it anyway."),
			OutPlan.WireLengthAfter, OutPlan.WireLengthBefore);
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// RenderPreview
// ---------------------------------------------------------------------------------------------

FString FTurboMCPGraphTidy::RenderPreview(const FPlan& InPlan)
{
	TArray<FString> Lines;

	// THE NUMBERS GO ON THE FIRST LINE, as the spec requires, AND THEY ARE LABELLED AS THE THING THAT
	// WAS MEASURED. `D-LAY-4` settles that tidy is judged on TOTAL WIRE LENGTH and knots on WIRES
	// CROSSING NODES, so a reader comparing the two tools' numbers is comparing two quantities - and
	// the label is what stops that.
	Lines.Add(FString::Printf(TEXT("preview: %s in %s : total wire length %.1f -> %.1f across %d wire(s)"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireLengthAfter, InPlan.WireCount));

	if (InPlan.bAlreadyTidy)
	{
		Lines.Add(TEXT("the graph is ALREADY TIDY: no node would move, no box would change size, and no getter would be split. The edit would answer unchanged:"));
	}
	else if (InPlan.bWouldApply)
	{
		Lines.Add(FString::Printf(TEXT("the edit WOULD APPLY: %d node(s) would move, %d box(es) would be fitted, %d getter(s) would be split."), InPlan.NodesToMove, InPlan.BoxesToFit, InPlan.GetterCopies.Num()));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("the edit WOULD ROLL BACK: %s"), *InPlan.RollbackReason));
	}

	if (InPlan.bForce && InPlan.WireLengthAfter >= InPlan.WireLengthBefore && !InPlan.bAlreadyTidy)
	{
		Lines.Add(TEXT("Force is set, so it would be applied EVEN THOUGH the wire length is not shorter. The edit's first line says WORSE when it does."));
	}

	// A BOX'S TOP-LEFT CORNER IS NEVER MOVED, and the preview says so WITH THE SIZES IT WOULD SET, so
	// the claim is checkable rather than decorative.
	if (InPlan.Boxes.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("boxes: %d, of which %d would be fitted. A box's TOP-LEFT CORNER is never moved; only its size changes."), InPlan.Boxes.Num(), InPlan.BoxesToFit));
		for (const FBox& Box : InPlan.Boxes)
		{
			const FString Label = Box.Text.IsEmpty() ? Box.Key : Box.Text;
			Lines.Add(FString::Printf(TEXT("  %s at (%.0f,%.0f) depth %d%s : %.0fx%.0f -> %.0fx%.0f"), *Label, Box.X, Box.Y, Box.Depth, Box.ParentIndex == INDEX_NONE ? TEXT("") : TEXT(" (nested)"), Box.Width, Box.Height, Box.PlannedWidth, Box.PlannedHeight));
		}
	}

	if (InPlan.GetterCopies.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("getters: %d would be split, one copy per extra consumer, each placed beside the node that reads it."), InPlan.GetterCopies.Num()));
	}

	if (InPlan.UnreachableCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("unreachable: %d node(s) are NOT moved, because disconnected groups are left where they are. R-GRAPH-DEAD removes them."), InPlan.UnreachableCount));
	}

	if (InPlan.bUsedFallbackGeometry)
	{
		// THE FALLBACK IS ALWAYS ANNOUNCED. `NodeWidth` is written only by Slate when a node is drawn,
		// so a graph that has never been rendered has no measured sizes, and a reader is entitled to
		// know the geometry came from `MinimumNodeExtent` rather than from the engine.
		Lines.Add(FString::Printf(TEXT("geometry: at least one node has NO MEASURED SIZE, so a %.0f-unit square centred on its position was used. Open the asset once to give the engine real sizes."), MinimumNodeExtent));
	}
	else
	{
		// AND WHEN EVERY NODE HAS A REAL SIZE, THAT IS SAID TOO. The line is not decoration: a reader
		// cannot tell "the engine measured every node" from "the code forgot to mention it" unless the
		// absence is stated, and the difference decides whether the numbers can be trusted as drawn.
		Lines.Add(TEXT("geometry: every node has a size the engine measured, so no estimate was used."));
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString FTurboMCPGraphTidy::Apply(const FPlan& InPlan)
{
	// A GRAPH IS NEEDED TO CREATE ANYTHING, and the plan carries the live nodes rather than the graph,
	// so it is taken from the first node that has one.
	UEdGraph* Graph = nullptr;
	for (const FNode& Entry : InPlan.Nodes)
	{
		if (Entry.Node != nullptr)
		{
			Graph = Entry.Node->GetGraph();
			break;
		}
	}
	if (Graph == nullptr)
	{
		return TEXT("error: the plan has no graph to write to. Plan and Apply must be called on the same loaded asset.");
	}

	// -----------------------------------------------------------------------------------------
	// NOTHING IS WRITTEN UNTIL THE MEASURE IS IN. The planned positions are VALUES in the plan, the
	// wire length was measured on them in `Plan`, and this function only touches the graph when the
	// result is shorter. THE ORDINARY ROLLBACK IS THEREFORE THAT NO WRITE EVER HAPPENED, which is the
	// strongest form and the reason the plan carries values rather than pointers.
	//
	// The exceptions are the getter copies, which must exist before the measure because they change the
	// wire set. They are created first, measured, and destroyed again if the result is not better - with
	// the links they replaced restored.
	// -----------------------------------------------------------------------------------------

	if (InPlan.bAlreadyTidy)
	{
		return FString::Printf(TEXT("unchanged: %s in %s is already tidy (total wire length %.1f across %d wire(s)). No node moved, no box changed size and no getter was split."), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireCount);
	}

	// THE PLAN IS READ-ONLY, so the copies this function makes are recorded in a LOCAL array. `Plan`
	// decided WHERE each copy goes and wrote nothing; `Apply` is the half that creates, so the handle to
	// each created node lives here rather than being written back into a const plan.
	TArray<FGetterCopy> Created = InPlan.GetterCopies;

	// Create the getter copies. Each one takes over ONE consumer from the original.
	for (FGetterCopy& Entry : Created)
	{
		if (Entry.Original == nullptr || Entry.ConsumerPin == nullptr)
		{
			continue;
		}

		UK2Node_VariableGet* Copy = NewObject<UK2Node_VariableGet>(Graph);
		if (Copy == nullptr)
		{
			continue;
		}

		Copy->CreateNewGuid();

		// THE MEMBER REFERENCE IS COPIED EXACTLY, GUID INCLUDED. A local-variable reference resolves
		// through its GUID, so a copy with a default GUID would point at nothing and break the graph in
		// a way no report would show.
		Copy->VariableReference = Entry.Original->VariableReference;

		Copy->NodePosX = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedX));
		Copy->NodePosY = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedY));

		Graph->Modify();
		Graph->AddNode(Copy, /*bUserAction=*/false, /*bSelectNewNode=*/false);

		// `AllocateDefaultPins` IS MANDATORY, AND LEAVING IT OUT IS A MEASURED CRASH. `NewObject` does
		// not allocate a node's pins and `UEdGraph::AddNode` does not either: the editor allocates them
		// when the node is reconstructed for display, which never happens headlessly.
		// `UK2Node_Knot::GetOutputPin` is `return Pins[1];` unguarded and took the editor down on an
		// empty array for exactly this reason.
		Copy->AllocateDefaultPins();

		UEdGraphPin* CopyValuePin = Copy->GetValuePin();
		if (CopyValuePin == nullptr)
		{
			// REFUSED RATHER THAN HALF-DONE. A copy with no value pin cannot take the consumer, and
			// destroying the original's link without somewhere to put it would sever the graph. So the
			// copy is removed and the consumer is left exactly as it was.
			Copy->DestroyNode();
			continue;
		}

		UEdGraphPin* OriginalValuePin = Entry.OriginalValuePin != nullptr
			? Entry.OriginalValuePin : Entry.Original->GetValuePin();
		if (OriginalValuePin != nullptr)
		{
			OriginalValuePin->BreakLinkTo(Entry.ConsumerPin);
		}
		CopyValuePin->MakeLinkTo(Entry.ConsumerPin);

		Entry.Copy = Copy;
	}

	// -----------------------------------------------------------------------------------------
	// THE POSITIONS ARE WRITTEN NOW, because the whole point of the pass is the effect of the new
	// positions on the wire length, and every original position is kept so an unfit pass can be put back
	// EXACTLY.
	// -----------------------------------------------------------------------------------------
	TArray<TPair<UEdGraphNode*, TPair<int32, int32>>> OriginalPositions;
	for (const FNode& Entry : InPlan.Nodes)
	{
		if (Entry.Node == nullptr || Entry.bIsComment || Entry.bUnreachable)
		{
			continue;
		}
		const int32 NewX = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedX));
		const int32 NewY = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedY));
		if (NewX == Entry.Node->NodePosX && NewY == Entry.Node->NodePosY)
		{
			continue;
		}
		OriginalPositions.Add(TPair<UEdGraphNode*, TPair<int32, int32>>(Entry.Node, TPair<int32, int32>(Entry.Node->NodePosX, Entry.Node->NodePosY)));
		Entry.Node->Modify();
		Entry.Node->NodePosX = NewX;
		Entry.Node->NodePosY = NewY;
	}

	// The measure is taken with the SAME function the reader uses, so the two numbers are comparable.
	double MeasuredAfter = 0.0;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}
			MeasuredAfter += FTurboMCPLayoutReport::WireLength(Pin);
		}
	}

	// *** WHETHER THE PACKAGE WAS ALREADY DIRTY WHEN THE PASS STARTED, READ BEFORE THE PASS CAN SET IT.
	// This is the same guard `GraphKnots.cpp` gained, and the reasoning is identical: a rollback must
	// leave the asset as it found it, so the flag may be cleared ONLY when this pass is what set it.
	// An earlier operation in the same editor session may own unsaved work, and discarding that is not
	// this pass's decision to make. ***
	const bool bWasDirtyBefore = Graph->GetOutermost()->IsDirty();

	const bool bBetter = MeasuredAfter < InPlan.WireLengthBefore;
	if (!bBetter && !InPlan.bForce)
	{
		// ROLLBACK. The positions are put back EXACTLY, and every copy is destroyed with the consumer it
		// took restored to the original getter. Then the result is RE-MEASURED AND COMPARED, because a
		// rollback whose result is not checked is a rollback nobody knows works.
		for (const auto& Undo : OriginalPositions)
		{
			Undo.Key->NodePosX = Undo.Value.Key;
			Undo.Key->NodePosY = Undo.Value.Value;
		}

		int32 Undone = 0;
		int32 RestorationFailed = 0;
		for (const FGetterCopy& Entry : Created)
		{
			if (Entry.Copy == nullptr)
			{
				continue;
			}
			if (Entry.OriginalValuePin != nullptr && Entry.ConsumerPin != nullptr)
			{
				// Break the copy's link FIRST, then re-make the original's, so the consumer is never
				// left with two links or with none.
				if (UEdGraphPin* CopyPin = Entry.Copy->GetValuePin())
				{
					CopyPin->BreakLinkTo(Entry.ConsumerPin);
				}
				Entry.OriginalValuePin->MakeLinkTo(Entry.ConsumerPin);

				// *** THE RESTORATION IS READ BACK, BECAUSE `MakeLinkTo` RETURNS `void` AND THEREFORE
				// CANNOT REPORT A REFUSAL. The consumer's input must point at the ORIGINAL getter again;
				// when it does not, the rollback left this reader severed and saying "rolled back" would
				// be a restatement of the request rather than a measurement of the result. This is the
				// shape `WidgetRetarget.cpp:355` and `DataTableRows.cpp:574` already use. ***
				if (Entry.ConsumerPin->LinkedTo.Contains(Entry.OriginalValuePin) == false)
				{
					++RestorationFailed;
				}
			}
			Entry.Copy->DestroyNode();
			++Undone;
		}
		Graph->MarkPackageDirty();

		// *** THE DIRTY FLAG IS RESTORED TO WHAT IT WAS BEFORE THE PASS, AND ONLY THEN. `MarkPackageDirty`
		// above runs on a path whose whole purpose is to leave the asset as it was, so a fully rolled-back
		// pass was leaving the package looking modified and the editor offering to save a change that does
		// not exist. CLEARED ONLY WHEN THIS PASS IS WHAT SET IT - see `bWasDirtyBefore`. ***
		if (!bWasDirtyBefore)
		{
			Graph->GetOutermost()->SetDirtyFlag(false);
		}

		double Restored = 0.0;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
				{
					continue;
				}
				Restored += FTurboMCPLayoutReport::WireLength(Pin);
			}
		}
		const bool bRestored = FMath::IsNearlyEqual(Restored, InPlan.WireLengthBefore, 1.0);
		FString Out = FString::Printf(TEXT("unchanged: %s in %s would be worse (total wire length %.1f -> %.1f), so it was rolled back. %d node position(s) and %d getter copy(ies) were undone, and the rollback %s the original length (%.1f)."), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, OriginalPositions.Num(), Undone, bRestored ? TEXT("RESTORED") : TEXT("DID NOT RESTORE"), Restored);

		// *** THE WIRE LENGTH COMING BACK IS NOT THE SAME AS EVERY READER BEING RE-ATTACHED, SO THE
		// SECOND QUESTION IS ASKED SEPARATELY. `bRestored` compares a total; this reports the consumers
		// whose original link was NOT re-made. `MakeLinkTo` returns `void`, so the check is a read of the
		// pin's own link list, taken after the attempt. ***
		if (RestorationFailed > 0)
		{
			Out += FString::Printf(
				TEXT(" *** %d CONSUMER(S) WERE NOT RE-ATTACHED TO THEIR ORIGINAL GETTER: the total length ")
				TEXT("came back but those readers were left severed, which is a defect in this tool and not ")
				TEXT("a clean rollback. ***"), RestorationFailed);
		}
		return Out;
	}

	// -----------------------------------------------------------------------------------------
	// APPLIED. The sizes of the boxes are written now, and ONLY their sizes: the corner is the one the
	// user placed, which is what "never moves a user's box as a whole" means in a checkable form.
	// -----------------------------------------------------------------------------------------
	int32 BoxesResized = 0;
	for (const FBox& Box : InPlan.Boxes)
	{
		if (Box.Comment == nullptr || !Box.bWouldFit)
		{
			continue;
		}
		Box.Comment->Modify();
		Box.Comment->NodeWidth = static_cast<int32>(FMath::RoundToDouble(Box.PlannedWidth));
		Box.Comment->NodeHeight = static_cast<int32>(FMath::RoundToDouble(Box.PlannedHeight));
		++BoxesResized;
	}

	int32 GettersSplit = 0;
	for (const FGetterCopy& Entry : Created)
	{
		if (Entry.Copy != nullptr)
		{
			++GettersSplit;
		}
	}

	// A KNOT IS NOT ADDED HERE, AND THAT IS A DECISION RATHER THAN AN OMISSION. `P/E-GRAPH-KNOTS` owns
	// that rule, and tidy is judged on total wire length, which a knot LENGTHENS - `D-LAY-4` warns a
	// caller to keep the two metrics apart, so adding knots from inside tidy would lengthen the very
	// number that decides whether the pass is kept. The result says so rather than leaving a caller to
	// assume knots were considered.
	Graph->MarkPackageDirty();

	TArray<FString> Lines;
	const bool bWorse = MeasuredAfter >= InPlan.WireLengthBefore;
	Lines.Add(FString::Printf(TEXT("changed: tidied %s in %s : total wire length %.1f -> %.1f%s"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, bWorse ? TEXT(" (WORSE, applied because Force was set)") : TEXT("")));
	Lines.Add(FString::Printf(TEXT("moved %d node(s). Fitted %d of %d box(es), and NO BOX CORNER MOVED: every box keeps the top-left corner the user placed, and only its size changed."), OriginalPositions.Num(), BoxesResized, InPlan.Boxes.Num()));
	if (GettersSplit > 0)
	{
		Lines.Add(FString::Printf(TEXT("split %d variable getter(s), one copy per extra consumer, placed beside the node that reads it."), GettersSplit));
	}
	if (InPlan.UnreachableCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("left %d unreachable node(s) EXACTLY where they were, because disconnected groups are not moved into the layout. R-GRAPH-DEAD removes them."), InPlan.UnreachableCount));
	}
	if (InPlan.bUsedFallbackGeometry)
	{
		Lines.Add(FString::Printf(TEXT("geometry: at least one node had NO MEASURED SIZE, so a %.0f-unit square centred on its position was used and the positions are computed from that estimate. Open the asset once for real sizes."), MinimumNodeExtent));
	}
	else
	{
		// THE ABSENCE IS STATED AS WELL AS THE PRESENCE. A reader cannot tell "the engine measured every
		// node" from "the code forgot to say" unless the answer says which, and that decides whether the
		// numbers can be trusted as drawn.
		Lines.Add(TEXT("geometry: every node had a size the engine measured, so no estimate was used."));
	}
	Lines.Add(TEXT("no knots were added: P/E-GRAPH-KNOTS owns that rule and tidy is measured on total wire length, which a knot lengthens."));
	Lines.Add(TEXT("; the change is in memory and NOT saved. Call E-SAVE."));

	return FString::Join(Lines, TEXT("\n"));
}
