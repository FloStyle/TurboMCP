// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphTidy.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

// NO `BlueprintEditorUtils.h` HERE. The editor's utilities live at `Kismet2/BlueprintEditorUtils.h`
// and the bare name does not resolve, which was a fatal C1083 on the first build of this file.
#include "BlueprintEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "K2Node_Knot.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Guid.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SGraphPin.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Function.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPComment.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

namespace
{
    TMap<const UEdGraphNode*, FVector2D> CachedAlignmentNodeSizes;
    TMap<const UEdGraphPin*, FVector2D> MeasuredPinOffsets;
    TWeakPtr<SGraphEditor> AlignmentEditor;
    TWeakObjectPtr<UEdGraph> AlignmentGraph;

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

	// Splitting copies only VariableReference, not target/default/exec/split-pin state.
	// Keep all nontrivial getters intact rather than changing their meaning for layout.
	bool CanSplitGetter(UK2Node_VariableGet* Getter)
	{
		if (!Getter || Getter->GetClass() != UK2Node_VariableGet::StaticClass()
			|| !Getter->IsNodePure()) return false;
		UEdGraphPin* Value = Getter->GetValuePin();
		if (!Value || Getter->Pins.Num() != 1 || Getter->Pins[0] != Value) return false;
		return Value->Direction == EGPD_Output && Value->SubPins.Num() == 0;
	}

	/** Accept both node-id forms emitted by the graph readers. */
	bool NodeAnswersToId(const UEdGraphNode* InNode, const FString& InId)
	{
		return InNode != nullptr
			&& (TurboMCPNodeId::MakeForNode(InNode, /*bLongForm=*/true).Equals(InId, ESearchCase::CaseSensitive)
				|| TurboMCPNodeId::MakeForNode(InNode, /*bLongForm=*/false).Equals(InId, ESearchCase::CaseSensitive));
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
			if (Graph != nullptr) { Graphs.AddUnique(Graph); }
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph != nullptr) { Graphs.AddUnique(Graph); }
		}
        for (UEdGraph* Graph : Blueprint->MacroGraphs)
            if (Graph) Graphs.AddUnique(Graph);
        for (UEdGraph* Graph : Blueprint->EventGraphs)
            if (Graph) Graphs.AddUnique(Graph);
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
                if (Graphs.Num() != 1)
                {
                    OutError = TEXT("multiple graphs and no EventGraph; pass an explicit graph name");
                    return false;
                }
                OutContext.Graph = Graphs[0];
			}
			return true;
		}

        for (UEdGraph* Candidate : Graphs)
        {
            if (NameMatches(Candidate->GetName(), InGraphName))
            {
                if (OutContext.Graph)
                {
                    OutError = TEXT("ambiguous graph name; no layout planned or applied");
                    return false;
                }
                OutContext.Graph = Candidate;
            }
        }
        if (OutContext.Graph) return true;

		TArray<FString> Names;
		for (UEdGraph* Candidate : Graphs) { Names.Add(Candidate->GetName()); }
		Names.Sort();
		OutError = FString::Printf(
			TEXT("'%s' is not a graph of %s. Its graphs are: %s"),
			*InGraphName, *Resolved.ObjectPath, *FString::Join(Names, TEXT(", ")));
		return false;
	}

    // Most K2 nodes do not persist Slate geometry in NodeWidth/NodeHeight.
    // Read the graph panel's measured bounds into this plan, never into the asset.
    bool EnsureMeasuredNodeOutlines(UBlueprint* Blueprint, UEdGraph* Graph,
        TMap<const UEdGraphNode*, FVector2D>& Sizes, FString& Error)
    {
        CachedAlignmentNodeSizes.Reset(); MeasuredPinOffsets.Reset(); AlignmentGraph.Reset();
        auto Collect = [&](const TSharedPtr<SGraphEditor>& Panel)
        {
            Sizes.Reset();
            if (Panel.IsValid()) { AlignmentEditor=Panel; AlignmentGraph=Graph; }
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node) continue;
                FSlateRect Bounds;
                if (Panel.IsValid() && Panel->GetBoundsForNode(Node, Bounds, 0.f)
                    && Bounds.Right > Bounds.Left && Bounds.Bottom > Bounds.Top)
                    Sizes.Add(Node, FVector2D(Bounds.Right - Bounds.Left, Bounds.Bottom - Bounds.Top));
                else if (Node->NodeWidth > 0 && Node->NodeHeight > 0)
                    Sizes.Add(Node, FVector2D(Node->NodeWidth, Node->NodeHeight));
                else return false;
            }
            return true;
        };
        if ((!GEditor || !FSlateApplication::IsInitialized()) && Collect(nullptr)) return true;
        if (!GEditor || !FSlateApplication::IsInitialized())
        {
            Error = TEXT("Tidy needs a graphical editor to measure node outlines; no nodes moved.");
            return false;
        }
        UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (!Editors) { Error = TEXT("Asset editor subsystem unavailable; no nodes moved."); return false; }
        Editors->OpenEditorForAsset(Blueprint);
        IAssetEditorInstance* Instance = Editors->FindEditorForAsset(Blueprint, false);
        if (!Instance || (Instance->GetEditorName() != FName(TEXT("BlueprintEditor"))
            && Instance->GetEditorName() != FName(TEXT("WidgetBlueprintEditor"))))
        {
            Error = TEXT("Asset did not open a supported Blueprint editor; no unsafe editor cast attempted.");
            return false;
        }
        FBlueprintEditor* Editor = static_cast<FBlueprintEditor*>(Instance);
        TSharedPtr<SGraphEditor> Panel = Editor->OpenGraphAndBringToFront(Graph);
        if (!Panel.IsValid()) { Error = TEXT("Graph panel unavailable; no nodes moved."); return false; }
        FVector2f SavedView; float SavedZoom; Panel->GetViewLocation(SavedView,SavedZoom);
        Panel->SetViewLocation(SavedView,1.0f);
        Panel->GetGraphPanel()->Update();
        for (int32 Attempt = 0; Attempt < 8; ++Attempt)
        {
            Panel->SlatePrepass(1.0f);
            FSlateApplication::Get().Tick(ESlateTickType::All);
            if (Attempt < 2) continue;
            if (Collect(Panel))
            {
                CachedAlignmentNodeSizes=Sizes; MeasuredPinOffsets.Reset();
                for(UEdGraphNode* Node:Graph->Nodes) if(Node)
                {
                    const TSharedPtr<SGraphNode> Widget=Panel->GetGraphPanel()->GetNodeWidgetFromGuid(Node->NodeGuid);
                    if(!Widget.IsValid()) continue;
                    for(UEdGraphPin* Pin:Node->Pins) if(Pin)
                    {
                        const TSharedPtr<SGraphPin> PW=Widget->FindWidgetForPin(Pin);
                        if(PW.IsValid() && PW->GetTickSpaceGeometry().GetLocalSize().Y>0)
                            MeasuredPinOffsets.Add(Pin,Widget->GetTickSpaceGeometry().AbsoluteToLocal(PW->GetTickSpaceGeometry().LocalToAbsolute(PW->GetTickSpaceGeometry().GetLocalSize()*0.5)));
                    }
                }
                Panel->SetViewLocation(SavedView,SavedZoom);
                return true;
            }
        }
        Panel->SetViewLocation(SavedView,SavedZoom);
        Error = TEXT("Some graph widgets still have no measured bounds. Let the editor draw the graph and retry; no nodes moved.");
        return false;
    }

}

// ---------------------------------------------------------------------------------------------
// The layout
// ---------------------------------------------------------------------------------------------

namespace
{
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


}

// ---------------------------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------------------------

bool FTurboMCPGraphTidy::PrepareAlignment(UBlueprint* Blueprint,UEdGraph* Graph,FString& Error)
{
    TMap<const UEdGraphNode*,FVector2D> Sizes;
    return EnsureMeasuredNodeOutlines(Blueprint,Graph,Sizes,Error);
}
FVector2D FTurboMCPGraphTidy::AlignmentNodeSize(const UEdGraphNode* Node)
{
    if(Node && Node->IsA<UEdGraphNode_Comment>()) return FVector2D(Node->NodeWidth,Node->NodeHeight);
    if(Node && AlignmentGraph.Get()==Node->GetGraph())
        if(const FVector2D* Size=CachedAlignmentNodeSizes.Find(Node)) return *Size;
    return Node?FVector2D(Node->NodeWidth,Node->NodeHeight):FVector2D::ZeroVector;
}
FVector2D FTurboMCPGraphTidy::AlignmentPinPosition(const UEdGraphPin* Pin)
{
    const UEdGraphNode* Node=Pin?Pin->GetOwningNodeUnchecked():nullptr;
    if(Node && AlignmentGraph.Get()==Node->GetGraph())
        if(const FVector2D* Offset=MeasuredPinOffsets.Find(Pin)) return FVector2D(Node->NodePosX,Node->NodePosY)+*Offset;
    return FTurboMCPLayoutReport::PinPosition(Pin);
}

bool FTurboMCPGraphTidy::Plan(const FString& InAsset, const FString& InGraphName, bool bInForce,
	bool bInMergeGetters, const TArray<FString>& InNodeIds, FPlan& OutPlan, FString& OutError, const FString& MainPath, double InHorizontalGap, double InVerticalGap, double InLanePitch)
{
    if(!FMath::IsFinite(InHorizontalGap) || !FMath::IsFinite(InVerticalGap) || !FMath::IsFinite(InLanePitch)
        || InHorizontalGap<40 || InHorizontalGap>2000 || InVerticalGap<40 || InVerticalGap>2000 || InLanePitch<100 || InLanePitch>10000)
    { OutError=TEXT("layout gaps must be finite 40..2000; lane pitch 100..10000 graph units"); return false; }
    const double SpacingX=InHorizontalGap, SpacingY=InVerticalGap;
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

	// A plan moves nodes, so it must start from Slate-measured outlines. This actively opens and
	// lays out the graph when needed, but only the postcondition below permits planning.
	TMap<const UEdGraphNode*, FVector2D> MeasuredSizes;
	if (!EnsureMeasuredNodeOutlines(Context.Blueprint, Graph, MeasuredSizes, OutError))
	{
		return false;
	}

	// -----------------------------------------------------------------------------------------
	// STEP 1: read every node once, with its measured rectangle.
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

		Entry.Width = MeasuredSizes.FindChecked(Node).X;
		Entry.Height = MeasuredSizes.FindChecked(Node).Y;

		Entry.PlannedX = Entry.X;
		Entry.PlannedY = Entry.Y;

		IndexByNode.Add(Node, OutPlan.Nodes.Num());
		OutPlan.Nodes.Add(Entry);
	}

	// A non-empty selection makes every other node a fixed anchor. Resolve every id before planning
	// so preview and edit reject the same stale selection and a partial block is never moved.
	if (InNodeIds.Num() > 0)
	{
		for (const FString& RequestedId : InNodeIds)
		{
			const FString Id = RequestedId.TrimStartAndEnd();
			const FNode* Found = OutPlan.Nodes.FindByPredicate([&Id](const FNode& Candidate)
			{
				return NodeAnswersToId(Candidate.Node, Id);
			});
			if (Found == nullptr)
			{
				OutError = FString::Printf(TEXT("no node with id '%s' in graph '%s'; tidy did not move anything."),
					*RequestedId, *OutPlan.GraphName);
				return false;
			}
			if (Found->bIsComment)
			{
				TArray<FString> Members;
				FTurboMCPComment::MembersOf(CastChecked<UEdGraphNode_Comment>(Found->Node), Members);
				for (const FNode& N : OutPlan.Nodes)
					if (N.Node && Members.Contains(N.Node->GetName())) OutPlan.SelectedNodeKeys.Add(N.Key);
				continue;
			}
			OutPlan.SelectedNodeKeys.Add(Found->Key);
		}
		if (OutPlan.SelectedNodeKeys.IsEmpty()) { OutError = TEXT("selected boxes contain no nodes; unchanged"); return false; }
		OutPlan.AnchorCount = 0;
		for (const FNode& Candidate : OutPlan.Nodes)
		{
			if (!Candidate.bIsComment && !OutPlan.SelectedNodeKeys.Contains(Candidate.Key))
			{
				++OutPlan.AnchorCount;
			}
		}
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
				// A node-to-node path is directed only from an output pin to an input pin.
				// Malformed same-direction links do not establish a dependency for layout.
				if (Other == nullptr || Other->Direction != EGPD_Input)
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

    TMap<int32,int32> MainPredecessor;
    TArray<FString> MainIds; MainPath.ParseIntoArray(MainIds,TEXT(","),true);
    if(MainPath.TrimStartAndEnd().Equals(TEXT("auto"),ESearchCase::IgnoreCase))
    {
        MainIds.Reset();
        TMap<int32,TArray<int32>> Paths; TSet<int32> Visiting;
        auto Eligible=[&](int32 I) { return !OutPlan.Nodes[I].bIsPure && !OutPlan.Nodes[I].bIsComment && (OutPlan.SelectedNodeKeys.IsEmpty() || OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[I].Key)); };
        TFunction<bool(int32)> Visit=[&](int32 I)
        {
            if(Paths.Contains(I)) return true;
            if(Visiting.Contains(I)) return false;
            Visiting.Add(I); TArray<int32> Best;
            for(const FWire& W:OutPlan.Wires) if(W.SourceNodeIndex==I && W.OutputPin->PinType.PinCategory==TEXT("exec") && Eligible(W.TargetNodeIndex))
            {
                if(!Visit(W.TargetNodeIndex)) return false;
                if(Paths[W.TargetNodeIndex].Num()>Best.Num()) Best=Paths[W.TargetNodeIndex];
            }
            Best.Insert(I,0); Paths.Add(I,Best); Visiting.Remove(I); return true;
        };
        TArray<int32> Longest;
        for(int32 I=0;I<OutPlan.Nodes.Num();++I) if(Eligible(I))
        {
            if(!Visit(I)) { OutError=TEXT("MainPath auto cannot choose through execution cycles; pass an explicit comma-separated path."); return false; }
            if(Paths[I].Num()>Longest.Num()) Longest=Paths[I];
        }
        for(int32 I:Longest) MainIds.Add(TurboMCPNodeId::MakeForNode(OutPlan.Nodes[I].Node));
    }
    for(FString& Id:MainIds) Id=Id.TrimStartAndEnd();
    int32 Previous=INDEX_NONE; TSet<int32> MainSeen;
    for(const FString& Id:MainIds)
    {
        int32 Current=INDEX_NONE;
        for(int32 I=0;I<OutPlan.Nodes.Num();++I) if(NodeAnswersToId(OutPlan.Nodes[I].Node,Id)) { Current=I; break; }
        if(Current==INDEX_NONE || MainSeen.Contains(Current) || OutPlan.Nodes[Current].bIsPure || OutPlan.Nodes[Current].bIsComment
            || (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Current].Key)))
        { OutError=TEXT("MainPath requires distinct execution nodes within the selected scope"); return false; }
        if(Previous!=INDEX_NONE)
        {
            bool Connected=false;
            for(const FWire& W:OutPlan.Wires) if(W.SourceNodeIndex==Previous && W.TargetNodeIndex==Current && W.OutputPin->PinType.PinCategory==TEXT("exec")) { Connected=true; break; }
            if(!Connected) { OutError=TEXT("MainPath nodes must be connected in order by execution wires"); return false; }
            MainPredecessor.Add(Current,Previous);
        }
        MainSeen.Add(Current); Previous=Current;
    }

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
			if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
			{
				continue;
			}
			// Exec and data pins can create parallel wires between the same nodes. They are one
			// directed dependency, so count the edge once in both adjacency and indegree.
			if (!Outgoing[Wire.SourceNodeIndex].Contains(Wire.TargetNodeIndex))
			{
				Outgoing[Wire.SourceNodeIndex].Add(Wire.TargetNodeIndex);
				++InDegree[Wire.TargetNodeIndex];
			}
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
			TArray<uint8> VisitState;
			VisitState.Init(0, OutPlan.Nodes.Num());
			TArray<int32> Path;
			TArray<int32> Cycle;
			TFunction<bool(int32)> FindCycle = [&](const int32 Current)
			{
				VisitState[Current] = 1;
				Path.Add(Current);
				for (const int32 Next : Outgoing[Current])
				{
					if (VisitState[Next] == 0 && FindCycle(Next))
					{
						return true;
					}
					if (VisitState[Next] == 1)
					{
						const int32 Start = Path.IndexOfByKey(Next);
						for (int32 Index = Start; Index < Path.Num(); ++Index)
						{
							Cycle.Add(Path[Index]);
						}
						Cycle.Add(Next);
						return true;
					}
				}
				Path.Pop();
				VisitState[Current] = 2;
				return false;
			};
			for (int32 Index = 0; Index < OutPlan.Nodes.Num() && Cycle.Num() == 0; ++Index)
			{
				if (VisitState[Index] == 0)
				{
					FindCycle(Index);
				}
			}

			TArray<FString> Titles;
			for (const int32 Index : Cycle)
			{
				Titles.Add(TurboMCPNodeId::GetNodeTitle(OutPlan.Nodes[Index].Node));
			}
			OutError = FString::Printf(
				TEXT("graph %s has a directed node cycle: %s. No column order exists; break this loop and run Tidy again."),
				*OutPlan.GraphName, *FString::Join(Titles, TEXT(" -> ")));
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
		if (OutPlan.Nodes[Index].Node) NodeIndexByKey.Add(OutPlan.Nodes[Index].Node->GetName(), Index);
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

	auto ArrangeRows = [&OutPlan, &MainPredecessor, SpacingX, SpacingY, InLanePitch](const TArray<int32>& Free, double FreeStartX, double FreeStartY)
	{
			// Execution components form separate rows; data sharing must not merge event chains.
			TMap<int32, int32> RowOf;
			for (const int32 I : Free) RowOf.Add(I, I);
			for (int32 Pass = 0; Pass < Free.Num(); ++Pass)
			{
				bool Changed = false;
				for (const FWire& W : OutPlan.Wires)
				{
					if (!W.OutputPin || W.OutputPin->PinType.PinCategory != TEXT("exec")
						|| !RowOf.Contains(W.SourceNodeIndex) || !RowOf.Contains(W.TargetNodeIndex)) continue;
					const int32 R = FMath::Min(RowOf[W.SourceNodeIndex], RowOf[W.TargetNodeIndex]);
					Changed |= RowOf[W.SourceNodeIndex] != R || RowOf[W.TargetNodeIndex] != R;
					RowOf[W.SourceNodeIndex] = RowOf[W.TargetNodeIndex] = R;
				}
				if (!Changed) break;
			}
			// Attach upstream pure expressions to their first consumer's row, including pure chains.
			for (int32 Pass = 0; Pass < Free.Num(); ++Pass)
			{
				bool Changed = false;
				for (int32 N = Free.Num() - 1; N >= 0; --N)
				{
					const int32 I = Free[N];
					if (!OutPlan.Nodes[I].bIsPure) continue;
					for (const FWire& W : OutPlan.Wires)
						if (W.SourceNodeIndex == I && W.TargetNodeIndex != I && RowOf.Contains(W.TargetNodeIndex))
						{
							Changed |= RowOf[I] != RowOf[W.TargetNodeIndex];
							RowOf[I] = RowOf[W.TargetNodeIndex]; break;
						}
				}
				if (!Changed) break;
			}
			TArray<int32> Rows;
			for (int32 I : Free) Rows.AddUnique(RowOf[I]);
            // Optimize all inter-row relationships; delegates carry extra weight.
            // Adjacent swaps reduce weighted distance without disturbing execution components.
            TMap<int32,int32> Rank;
            auto Cost=[&]()
            {
                Rank.Empty(); for(int32 I=0;I<Rows.Num();++I) Rank.Add(Rows[I],I);
                int64 Sum=0;
                for(const FWire& W:OutPlan.Wires) if(RowOf.Contains(W.SourceNodeIndex) && RowOf.Contains(W.TargetNodeIndex))
                {
                    const int32 Weight=W.OutputPin && W.OutputPin->PinType.PinCategory==TEXT("delegate")?8:1;
                    Sum+=int64(Weight)*FMath::Abs(Rank[RowOf[W.SourceNodeIndex]]-Rank[RowOf[W.TargetNodeIndex]]);
                }
                return Sum;
            };
            int64 Best=Cost();
            for(int32 Pass=0;Pass<8;++Pass)
            {
                bool Changed=false;
                for(int32 I=1;I<Rows.Num();++I)
                {
                    Rows.Swap(I-1,I); const int64 Candidate=Cost();
                    if(Candidate<Best) { Best=Candidate; Changed=true; } else Rows.Swap(I-1,I);
                }
                if(!Changed) break;
            }
			double RowY = FreeStartY;
			for (int32 Row : Rows)
			{
				double CursorX = FreeStartX;
				double Bottom = RowY;
				double LaneHeight = SpacingY;
				for (int32 I : Free) if (RowOf[I] == Row) LaneHeight = FMath::Max(LaneHeight, OutPlan.Nodes[I].Height + SpacingY);
				TMap<int32,int32> LaneOf;
                TMap<int32,double> LaneCursor;
                LaneCursor.Add(0,FreeStartX);
				for (int32 I : Free)
				{
					FNode& N = OutPlan.Nodes[I];
					if (RowOf[I] != Row || N.bIsPure) continue;
					int32 Lane = MAX_int32; const FWire* Incoming = nullptr;
					for (const FWire& W : OutPlan.Wires)
						if (W.TargetNodeIndex == I && W.OutputPin && W.OutputPin->PinType.PinCategory == TEXT("exec") && LaneOf.Contains(W.SourceNodeIndex))
						{
							int32 Branch = 0;
							for (const UEdGraphPin* Pin : OutPlan.Nodes[W.SourceNodeIndex].Node->Pins)
							{ if (Pin == W.OutputPin) break; if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == TEXT("exec")) ++Branch; }
							if(W.OutputPin->PinName==TEXT("Completed")) Branch=0;
                            const bool Preferred=MainPredecessor.Contains(I) && MainPredecessor[I]==W.SourceNodeIndex;
                            const bool SequenceEdge=OutPlan.Nodes[W.SourceNodeIndex].Node->IsA<UK2Node_ExecutionSequence>();
                            if(MainPredecessor.Contains(I) && !Preferred && !SequenceEdge) continue;
                            if(!Preferred && !SequenceEdge) for(const auto& Edge:MainPredecessor) if(Edge.Value==W.SourceNodeIndex && Edge.Key!=I) Branch=FMath::Max(1,Branch);
                            const int32 Candidate = LaneOf[W.SourceNodeIndex] + (SequenceEdge ? Branch+1 : (Preferred?0:Branch));
							if (Candidate < Lane) { Lane = Candidate; Incoming = &W; }
						}
					if (Lane == MAX_int32) Lane = 0;
					LaneOf.Add(I, Lane);
					double PureSpace=0;
                    TSet<int32> SeenPure;
                    TFunction<double(int32)> Required=[&](int32 Target)
                    {
                        if(SeenPure.Contains(Target)) return 0.0;
                        SeenPure.Add(Target); double Width=0;
                        for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && RowOf.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure)
                            Width=FMath::Max(Width,OutPlan.Nodes[W.SourceNodeIndex].Width+SpacingX+Required(W.SourceNodeIndex));
                        return Width;
                    };
                    PureSpace=Required(I);
                    // Each lane advances independently; sequence siblings share their left edge.
                    if(!LaneCursor.Contains(Lane))
                    {
                        const double Start=Incoming ? OutPlan.Nodes[Incoming->SourceNodeIndex].PlannedX+OutPlan.Nodes[Incoming->SourceNodeIndex].Width+SpacingX : FreeStartX;
                        LaneCursor.Add(Lane,Start);
                    }
                    CursorX=LaneCursor[Lane]+PureSpace;
                    N.PlannedX = CursorX; N.PlannedY = RowY + Lane * LaneHeight;
					if (Incoming && LaneOf[Incoming->SourceNodeIndex] == Lane)
					{
						const FNode& Source = OutPlan.Nodes[Incoming->SourceNodeIndex];
						N.PlannedY = FTurboMCPGraphTidy::AlignmentPinPosition(Incoming->OutputPin).Y + Source.PlannedY - Source.Y
							- (FTurboMCPGraphTidy::AlignmentPinPosition(Incoming->InputPin).Y - N.Y);
					}
					CursorX += N.Width + SpacingX;
                    LaneCursor[Lane]=CursorX;
					Bottom = FMath::Max(Bottom, N.PlannedY + N.Height);
				}
				// Place pure nodes below-left of consumers, reserving vertical space for each.
				double PureBottom = Bottom;
				TArray<int32> PlacedPure;
				for (int32 I : Free) if (RowOf[I] == Row && !OutPlan.Nodes[I].bIsPure) PlacedPure.Add(I);
				for (int32 K = Free.Num() - 1; K >= 0; --K)
				{
					const int32 I = Free[K]; FNode& N = OutPlan.Nodes[I];
					if (RowOf[I] != Row || !N.bIsPure) continue;
					N.PlannedX = FreeStartX; N.PlannedY = RowY;
                    const FWire* Chosen=nullptr;
                    for(const FWire& W:OutPlan.Wires)
                        if(W.SourceNodeIndex==I && W.TargetNodeIndex!=I && RowOf.Contains(W.TargetNodeIndex))
                            if(!Chosen || FTurboMCPGraphTidy::AlignmentPinPosition(W.InputPin).Y-OutPlan.Nodes[W.TargetNodeIndex].Y
                                < FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->InputPin).Y-OutPlan.Nodes[Chosen->TargetNodeIndex].Y) Chosen=&W;
                    if(Chosen)
                    {
                        const FNode& Consumer=OutPlan.Nodes[Chosen->TargetNodeIndex];
                        N.PlannedX=FMath::Max(FreeStartX,Consumer.PlannedX-N.Width-SpacingX);
                        N.PlannedY=FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->InputPin).Y+Consumer.PlannedY-Consumer.Y
                            -(FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->OutputPin).Y-N.Y);
                    }
					for (int32 Pass = 0; Pass <= PlacedPure.Num(); ++Pass)
					{
						double NextY = N.PlannedY;
						for (int32 P : PlacedPure)
						{
							const FNode& O = OutPlan.Nodes[P];
							if (N.PlannedX < O.PlannedX + O.Width + SpacingX && N.PlannedX + N.Width + SpacingX > O.PlannedX
								&& N.PlannedY < O.PlannedY + O.Height + SpacingY && N.PlannedY + N.Height + SpacingY > O.PlannedY)
								NextY = FMath::Max(NextY, O.PlannedY + O.Height + SpacingY);
						}
						if (NextY == N.PlannedY) break;
						N.PlannedY = NextY;
					}
					PlacedPure.Add(I);
					PureBottom = FMath::Max(PureBottom, N.PlannedY + N.Height);
				}
				double Minimum=RowY;
                for(int32 I:Free) if(RowOf[I]==Row) Minimum=FMath::Min(Minimum,OutPlan.Nodes[I].PlannedY);
                const double Shift=RowY-Minimum;
                for(int32 I:Free) if(RowOf[I]==Row) OutPlan.Nodes[I].PlannedY+=Shift;
                RowY = FMath::Max(Bottom, PureBottom) + Shift + SpacingY;
			}
            // Native Sequence lanes: stage columns are shared across outputs rather than
            // inherited from graph-wide dependency columns (which include cross-row data).
            for(int32 SeqIndex:Free)
            {
                const FNode& SeqNode=OutPlan.Nodes[SeqIndex];
                if(!SeqNode.Node->IsA<UK2Node_ExecutionSequence>()) continue;
                TArray<TArray<int32>> Lanes; TSet<int32> OwnedCalls; bool Supported=true;
                for(UEdGraphPin* Exec:SeqNode.Node->Pins) if(Exec && Exec->Direction==EGPD_Output && Exec->PinType.PinCategory==TEXT("exec"))
                {
                    int32 Next=INDEX_NONE;
                    for(const FWire& W:OutPlan.Wires) if(W.OutputPin==Exec) { if(Next!=INDEX_NONE) Supported=false; Next=W.TargetNodeIndex; }
                    TArray<int32> Lane;
                    while(Supported && Next!=INDEX_NONE)
                    {
                        if(!Free.Contains(Next) || OwnedCalls.Contains(Next) || OutPlan.Nodes[Next].bIsPure || OutPlan.Nodes[Next].Node->IsA<UK2Node_ExecutionSequence>()) { Supported=false; break; }
                        int32 Inputs=0,Outputs=0,Following=INDEX_NONE;
                        for(const FWire& W:OutPlan.Wires) if(W.OutputPin->PinType.PinCategory==TEXT("exec"))
                        {
                            if(W.TargetNodeIndex==Next) ++Inputs;
                            if(W.SourceNodeIndex==Next) { ++Outputs; Following=W.TargetNodeIndex; }
                        }
                        if(Inputs!=1 || Outputs>1) { Supported=false; break; }
                        Lane.Add(Next); OwnedCalls.Add(Next); Next=Following;
                    }
                    if(!Lane.IsEmpty()) Lanes.Add(Lane);
                }
                if(!Supported || Lanes.Num()<2) continue; // General branches retain conservative layout/guards.
                TMap<int32,int32> PureOwner;
                TFunction<void(int32,int32)> Assign=[&](int32 Target,int32 Owner)
                {
                    for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && Free.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure && !PureOwner.Contains(W.SourceNodeIndex))
                    { PureOwner.Add(W.SourceNodeIndex,Owner); Assign(W.SourceNodeIndex,Owner); }
                };
                for(const TArray<int32>& Lane:Lanes) for(int32 Call:Lane) Assign(Call,Call);
                int32 Stages=0; for(const auto& Lane:Lanes) Stages=FMath::Max(Stages,Lane.Num());
                TArray<double> Widths,Reserves,Columns; Widths.Init(0,Stages); Reserves.Init(0,Stages); Columns.Init(0,Stages);
                auto ReserveFor=[&](int32 Call)
                {
                    TSet<int32> Visiting;
                    TFunction<double(int32)> Depth=[&](int32 Target)->double
                    {
                        if(Visiting.Contains(Target)) return 0; Visiting.Add(Target); double Width=0;
                        for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && PureOwner.FindRef(W.SourceNodeIndex)==Call && PureOwner.Contains(W.SourceNodeIndex))
                            Width=FMath::Max(Width,OutPlan.Nodes[W.SourceNodeIndex].Width+SpacingX+Depth(W.SourceNodeIndex));
                        Visiting.Remove(Target); return Width;
                    };
                    return Depth(Call);
                };
                for(const auto& Lane:Lanes) for(int32 Stage=0;Stage<Lane.Num();++Stage)
                { Widths[Stage]=FMath::Max(Widths[Stage],OutPlan.Nodes[Lane[Stage]].Width); Reserves[Stage]=FMath::Max(Reserves[Stage],ReserveFor(Lane[Stage])); }
                const double Left=SeqNode.PlannedX+SeqNode.Width+SpacingX;
                double Column=Left;
                for(int32 Stage=0;Stage<Stages;++Stage) { Column+=Reserves[Stage]; Columns[Stage]=Column; Column+=Widths[Stage]+SpacingX; }
                double LaneY=SeqNode.PlannedY+SeqNode.Height+SpacingY;
                double PreviousRight=Left; TSet<int32> EarlierCalls;
                for(int32 LaneIndex=0;LaneIndex<Lanes.Num();++LaneIndex)
                {
                    const auto& Lane=Lanes[LaneIndex]; TSet<int32> Members;
                    for(int32 Call:Lane) Members.Add(Call);
                    for(const auto& Pair:PureOwner) if(Members.Contains(Pair.Value)) Members.Add(Pair.Key);
                    bool Summary=false;
                    if(LaneIndex==Lanes.Num()-1) for(const FWire& W:OutPlan.Wires)
                        if(EarlierCalls.Contains(W.SourceNodeIndex) && Members.Contains(W.TargetNodeIndex) && W.OutputPin->PinType.PinCategory!=TEXT("exec")) Summary=true;
                    const double Extra=Summary?FMath::Max(0.0,PreviousRight+SpacingX-Left):0.0;
                    double Bottom=LaneY;
                    for(int32 Stage=0;Stage<Lane.Num();++Stage)
                    {
                        FNode& N=OutPlan.Nodes[Lane[Stage]]; N.PlannedX=Columns[Stage]+Extra; N.PlannedY=LaneY;
                        if(Stage>0) for(const FWire& W:OutPlan.Wires) if(W.SourceNodeIndex==Lane[Stage-1] && W.TargetNodeIndex==Lane[Stage] && W.OutputPin->PinType.PinCategory==TEXT("exec"))
                            N.PlannedY=AlignmentPinPosition(W.OutputPin).Y+OutPlan.Nodes[Lane[Stage-1]].PlannedY-OutPlan.Nodes[Lane[Stage-1]].Y-(AlignmentPinPosition(W.InputPin).Y-N.Y);
                        Bottom=FMath::Max(Bottom,N.PlannedY+N.Height);
                    }
                    TSet<int32> Placed; for(int32 Call:Lane) Placed.Add(Call);
                    // Consumer-first traversal places pure trees using real pin offsets.
                    TFunction<void(int32)> PlaceInputs=[&](int32 Target)
                    {
                        TArray<const FWire*> Inputs;
                        for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && Members.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure) Inputs.Add(&W);
                        Inputs.Sort([](const FWire& A,const FWire& B) { return AlignmentPinPosition(A.InputPin).Y<AlignmentPinPosition(B.InputPin).Y; });
                        for(const FWire* W:Inputs)
                        {
                            const int32 I=W->SourceNodeIndex; if(Placed.Contains(I)) continue; Placed.Add(I);
                            FNode& N=OutPlan.Nodes[I]; const FNode& Consumer=OutPlan.Nodes[Target];
                            N.PlannedX=Consumer.PlannedX-N.Width-SpacingX;
                            N.PlannedY=AlignmentPinPosition(W->InputPin).Y+Consumer.PlannedY-Consumer.Y-(AlignmentPinPosition(W->OutputPin).Y-N.Y);
                            for(int32 Pass=0;Pass<Members.Num();++Pass)
                            {
                                double NextY=N.PlannedY;
                                for(int32 Other:Placed) if(Other!=I)
                                {
                                    const FNode& O=OutPlan.Nodes[Other];
                                    if(N.PlannedX<O.PlannedX+O.Width+SpacingX && N.PlannedX+N.Width+SpacingX>O.PlannedX && N.PlannedY<O.PlannedY+O.Height+SpacingY && N.PlannedY+N.Height+SpacingY>O.PlannedY)
                                        NextY=FMath::Max(NextY,O.PlannedY+O.Height+SpacingY);
                                }
                                if(NextY==N.PlannedY) break; N.PlannedY=NextY;
                            }
                            Bottom=FMath::Max(Bottom,N.PlannedY+N.Height); PlaceInputs(I);
                        }
                    };
                    for(int32 Call:Lane) PlaceInputs(Call);
                    double MinY=LaneY;
                    for(int32 I:Members) MinY=FMath::Min(MinY,OutPlan.Nodes[I].PlannedY);
                    const double ShiftY=LaneY-MinY;
                    for(int32 I:Members) { OutPlan.Nodes[I].PlannedY+=ShiftY; PreviousRight=FMath::Max(PreviousRight,OutPlan.Nodes[I].PlannedX+OutPlan.Nodes[I].Width); }
                    for(int32 Call:Lane) EarlierCalls.Add(Call);
                    LaneY=FMath::Max(LaneY+InLanePitch,Bottom+ShiftY+SpacingY);
                }
            }
	};

	for (const int32 BoxIndex : BoxOrder)
	{
		FBox& Box = OutPlan.Boxes[BoxIndex];
		if (OutPlan.SelectedNodeKeys.Num() > 0)
		{
			bool AllSelected = !Box.MemberKeys.IsEmpty();
            for (const FString& Key : Box.MemberKeys)
            {
                const int32* Index = NodeIndexByKey.Find(Key);
                if (Index && !OutPlan.Nodes[*Index].bIsComment && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[*Index].Key)) AllSelected = false;
            }
            if (!AllSelected)
            {
                for (const FString& Key : Box.MemberKeys)
                    if (const int32* I = NodeIndexByKey.Find(Key))
                        if (OwnerOfNode[*I] == BoxIndex && !OutPlan.Nodes[*I].bIsComment && OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[*I].Key))
                        { OutError = TEXT("selection crosses a comment group; select the whole comment box to preserve membership"); return false; }
                Box.bWouldFit = false; continue;
            }
		}

		// The nodes THIS box lays out: its own members, minus those a deeper box owns.
		TArray<int32> Owned;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			if (OwnerOfNode[NodeIndex] != BoxIndex || OutPlan.Nodes[NodeIndex].bIsComment)
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
		double StartY = Box.Y + FTurboMCPGraphTidy::TitleBarHeight + FTurboMCPGraphTidy::FitPadding;
        for (int32 Child : Box.ChildBoxIndices) StartY = FMath::Max(StartY, OutPlan.Boxes[Child].Y + OutPlan.Boxes[Child].PlannedHeight + SpacingY);

		ArrangeRows(Owned, StartX, StartY);

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
		const bool bScoped = OutPlan.SelectedNodeKeys.Num() > 0;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			const FNode& Candidate = OutPlan.Nodes[NodeIndex];
			if (Candidate.bIsComment || Candidate.bUnreachable)
			{
				continue;
			}
			if (bScoped)
			{
				if (OutPlan.SelectedNodeKeys.Contains(Candidate.Key) && OwnerOfNode[NodeIndex] == INDEX_NONE)
				{
					Free.Add(NodeIndex);
				}
			}
			else if (OwnerOfNode[NodeIndex] == INDEX_NONE)
			{
				Free.Add(NodeIndex);
			}
		}

		if (Free.Num() > 0)
		{
            // Moving nodes are not fixed obstacles: including their old right
            // edges makes each forced repeat push the group farther right.
            double FreeStartX = OutPlan.Nodes[Free[0]].X;
            double FreeStartY = OutPlan.Nodes[Free[0]].Y;
            TSet<const UEdGraphNode*> MovingNodes;
            for (const int32 Index : Free)
            {
                FreeStartX = FMath::Min(FreeStartX, OutPlan.Nodes[Index].X);
                FreeStartY = FMath::Min(FreeStartY, OutPlan.Nodes[Index].Y);
                MovingNodes.Add(OutPlan.Nodes[Index].Node);
            }
			for (const FNode& Candidate : OutPlan.Nodes)
			{
				if (Candidate.bIsComment || MovingNodes.Contains(Candidate.Node))
				{
					continue;
				}
				const double Right = Candidate.X + Candidate.Width + FTurboMCPGraphTidy::BoxGap;
				FreeStartX = FMath::Max(FreeStartX, Right);
			}
			for (const FBox& Box : OutPlan.Boxes)
			{
				const double Right = Box.X + Box.PlannedWidth + FTurboMCPGraphTidy::BoxGap;
				FreeStartX = FMath::Max(FreeStartX, Right);
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

			ArrangeRows(Free, FreeStartX, FreeStartY);
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 7: split variable getters that feed several consumers, BEFORE the measure, because a copy
	// changes the graph's node set and therefore its wire set.
	//
	// This is one of the two irreversible steps and it is why the rollback exists as CODE rather than
	// as a design property. What is created is recorded so `Apply` can destroy exactly it.
	// -----------------------------------------------------------------------------------------
	struct FOccupiedGetterRect { double X, Y, Width, Height; };
	TArray<FOccupiedGetterRect> GetterObstacles;
	for (const FNode& Node : OutPlan.Nodes)
	{
		if (!Node.bIsComment)
			GetterObstacles.Add({FMath::RoundToDouble(Node.PlannedX), FMath::RoundToDouble(Node.PlannedY), Node.Width, Node.Height});
	}
	for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
	{
		FNode& Entry = OutPlan.Nodes[NodeIndex];
		if (!Entry.bIsGetter || Entry.bUnreachable || Entry.Node == nullptr
			|| (OutPlan.SelectedNodeKeys.Num() > 0 && !OutPlan.SelectedNodeKeys.Contains(Entry.Key)))
		{
			continue;
		}

		UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Entry.Node);
		if (!CanSplitGetter(Getter))
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
			const int32* ConsumerPlanIndex = IndexByNode.Find(ConsumerNode);
			if (!ConsumerPlanIndex || OwnerOfNode[NodeIndex] != INDEX_NONE || OwnerOfNode[*ConsumerPlanIndex] != INDEX_NONE) continue;
			const FNode& ConsumerPlan = OutPlan.Nodes[*ConsumerPlanIndex];
			Planned.PlannedX = ConsumerPlan.PlannedX
				- Entry.Width - SpacingX;
			Planned.PlannedY = FMath::RoundToDouble(ConsumerPlan.PlannedY);
			Planned.PlannedX = FMath::RoundToDouble(Planned.PlannedX);
			// Keep existing/planned nodes fixed. Move only this new copy below
			// intersecting rectangles, reserving its space for subsequent copies.
			for (int32 Pass = 0; Pass <= GetterObstacles.Num(); ++Pass)
			{
				double NextY = Planned.PlannedY;
				for (const FOccupiedGetterRect& Obstacle : GetterObstacles)
				{
					if (Planned.PlannedX < Obstacle.X + Obstacle.Width
						&& Planned.PlannedX + Entry.Width > Obstacle.X
						&& Planned.PlannedY < Obstacle.Y + Obstacle.Height
						&& Planned.PlannedY + Entry.Height > Obstacle.Y)
						NextY = FMath::Max(NextY, FMath::CeilToDouble(Obstacle.Y + Obstacle.Height + SpacingY));
				}
				if (NextY == Planned.PlannedY) break;
				Planned.PlannedY = NextY;
			}
			if (!FMath::IsFinite(Planned.PlannedX) || !FMath::IsFinite(Planned.PlannedY)
				|| Planned.PlannedX < MIN_int32 || Planned.PlannedX > MAX_int32
				|| Planned.PlannedY < MIN_int32 || Planned.PlannedY > MAX_int32)
			{
				OutError = TEXT("Getter-copy placement exceeds graph coordinate limits; no nodes moved.");
				return false;
			}
			GetterObstacles.Add({Planned.PlannedX, Planned.PlannedY, Entry.Width, Entry.Height});
			if (OwnerOfNode[NodeIndex] == INDEX_NONE) OutPlan.GetterCopies.Add(Planned);
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
	for (FNode& N : OutPlan.Nodes) { N.PlannedX = FMath::RoundToDouble(N.PlannedX); N.PlannedY = FMath::RoundToDouble(N.PlannedY); }
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

			double SourceX = bUsePlanned ? Source.PlannedX : Source.X;
			double SourceY = bUsePlanned ? Source.PlannedY : Source.Y;
			if (bUsePlanned)
			{
				// This consumer will read from its copy, not the original getter.
				for (const FGetterCopy& Copy : OutPlan.GetterCopies)
				{
					if (Copy.OriginalValuePin == Wire.OutputPin && Copy.ConsumerPin == Wire.InputPin)
					{
						SourceX = Copy.PlannedX;
						SourceY = Copy.PlannedY;
						break;
					}
				}
			}
			const double TargetX = bUsePlanned ? Target.PlannedX : Target.X;
			const double TargetY = bUsePlanned ? Target.PlannedY : Target.Y;

			// Use exactly the report pin model, translated to planned positions.

			const FVector2D Start = FTurboMCPLayoutReport::PinPosition(Wire.OutputPin) + FVector2D(SourceX - Source.X, SourceY - Source.Y);
			const FVector2D End = FTurboMCPLayoutReport::PinPosition(Wire.InputPin) + FVector2D(TargetX - Target.X, TargetY - Target.Y);
			Total += FVector2D::Distance(Start, End);
		}
		return Total;
	};

	OutPlan.WireLengthBefore = TotalLengthOn(false);
	OutPlan.WireLengthAfter = TotalLengthOn(true);

	for (const FBox& Box : OutPlan.Boxes)
		for (const FString& Member : Box.MemberKeys)
		{
			const int32* Index = NodeIndexByKey.Find(Member);
			if (!Index) { OutError = TEXT("comment membership could not be resolved; tidy refused"); return false; }
			const FNode& N = OutPlan.Nodes[*Index];
            if (N.bIsComment) continue;
			if (N.PlannedX < Box.X || N.PlannedY < Box.Y + TitleBarHeight
				|| N.PlannedX + N.Width > Box.X + Box.PlannedWidth || N.PlannedY + N.Height > Box.Y + Box.PlannedHeight)
			{ OutError = TEXT("tidy would move a node outside its original comment box; unchanged. Use a smaller selection or explicit batch positions."); return false; }
		}

    for (int32 A=0; A<OutPlan.Nodes.Num(); ++A)
    for (int32 B=A+1; B<OutPlan.Nodes.Num(); ++B)
    {
        const FNode& L=OutPlan.Nodes[A]; const FNode& R=OutPlan.Nodes[B];
        if (L.bIsComment || R.bIsComment) continue;
        const bool Moved=L.PlannedX!=L.X || L.PlannedY!=L.Y || R.PlannedX!=R.X || R.PlannedY!=R.Y;
        if (Moved && L.PlannedX<R.PlannedX+R.Width && L.PlannedX+L.Width>R.PlannedX && L.PlannedY<R.PlannedY+R.Height && L.PlannedY+L.Height>R.PlannedY)
        { OutError=TEXT("planned tidy overlaps node rectangles; unchanged. Use a smaller group."); return false; }
    }
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
			OutPlan.NodesToMoveKeys.Add(Entry.Key);
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

namespace
{
 bool RenderNativePreview(const FTurboMCPGraphTidy::FPlan& Plan, FString& Result)
 {
  if (!FSlateApplication::IsInitialized() || !IsInGameThread()) { Result=TEXT("Slate/game thread required"); return false; }
  UEdGraph* Original=nullptr;
  for (const auto& N:Plan.Nodes) if (N.Node) { Original=N.Node->GetGraph(); break; }
  UBlueprint* Blueprint=Original?FBlueprintEditorUtils::FindBlueprintForGraph(Original):nullptr;
  if (!Blueprint) { Result=TEXT("Blueprint graph unavailable"); return false; }
  // Copy only graph objects into a transient Blueprint shell. Duplicating the entire
  // Blueprint would invoke PostDuplicateBlueprint and can compile/reinstance classes.
  TStrongObjectPtr<UBlueprint> Scratch(NewObject<UBlueprint>(GetTransientPackage(),Blueprint->GetClass(),NAME_None,RF_Transient));
  if (!Scratch.IsValid()) { Result=TEXT("could not create transient preview owner"); return false; }
  Scratch->ParentClass=Blueprint->ParentClass;
  Scratch->GeneratedClass=Blueprint->GeneratedClass;
  Scratch->SkeletonGeneratedClass=Blueprint->SkeletonGeneratedClass;
  Scratch->NewVariables=Blueprint->NewVariables;
  TMap<UObject*,UObject*> Created;
  FObjectDuplicationParameters Parameters(Original,Scratch.Get());
  Parameters.ApplyFlags=RF_Transient;
  Parameters.FlagMask &= ~(RF_Public|RF_Standalone|RF_Transactional);
  Parameters.bAssignExternalPackages=false;
  Parameters.DuplicationSeed.Add(Blueprint,Scratch.Get());
  Parameters.CreatedObjects=&Created;
  UEdGraph* Graph=Cast<UEdGraph>(StaticDuplicateObjectEx(Parameters));
  if (!Graph) { Result=TEXT("graph copy failed"); return false; }
  if(Blueprint->FunctionGraphs.Contains(Original)) Scratch->FunctionGraphs.Add(Graph);
  else if(Blueprint->MacroGraphs.Contains(Original)) Scratch->MacroGraphs.Add(Graph);
  else Scratch->UbergraphPages.Add(Graph);
  TMap<FGuid,UEdGraphNode*> ByGuid;
  for(const auto& N:Plan.Nodes) if(N.Node)
  {
   UEdGraphNode* Copy=Cast<UEdGraphNode>(Created.FindRef(N.Node));
   if(!Copy || Copy->GetGraph()!=Graph) { Result=TEXT("preview node mapping failed"); return false; }
   ByGuid.Add(N.Node->NodeGuid,Copy);
  }
  auto FindPin=[](UEdGraphNode* N,const UEdGraphPin* P)->UEdGraphPin*
  {
   if (!N || !P) return nullptr;
   for (UEdGraphPin* Q:N->Pins) if(Q && Q->PinId==P->PinId) return Q;
   return nullptr;
  };
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("TurboMCP/Previews");
  IFileManager::Get().MakeDirectory(*Folder,true);
  const FString Stem=FGuid::NewGuid().ToString();
  Result=TEXT("Native graph previews (transient copy; no live asset changes):");
  for(int32 Side=0;Side<2;++Side)
  {
   if(Side)
   {
    for(const auto& N:Plan.Nodes) if(N.Node)
     if(UEdGraphNode** Copy=ByGuid.Find(N.Node->NodeGuid)) { (*Copy)->NodePosX=FMath::RoundToInt(N.PlannedX); (*Copy)->NodePosY=FMath::RoundToInt(N.PlannedY); }
    for(const auto& B:Plan.Boxes) if(B.Comment)
     if(UEdGraphNode** Copy=ByGuid.Find(B.Comment->NodeGuid)) { (*Copy)->NodeWidth=FMath::RoundToInt(B.PlannedWidth); (*Copy)->NodeHeight=FMath::RoundToInt(B.PlannedHeight); }
    for(const auto& C:Plan.GetterCopies)
    {
     UK2Node_VariableGet* Source=Cast<UK2Node_VariableGet>(ByGuid.FindRef(C.Original->NodeGuid));
     UEdGraphPin* Input=FindPin(ByGuid.FindRef(C.ConsumerPin->GetOwningNode()->NodeGuid),C.ConsumerPin);
     if(!Source || !Input || !Source->GetValuePin() || Source->GetGraph()!=Graph || Input->GetOwningNode()->GetGraph()!=Graph) { Result=TEXT("getter-copy preview mapping failed"); return false; }
     UK2Node_VariableGet* Copy=NewObject<UK2Node_VariableGet>(Graph,NAME_None,RF_Transient);
     Copy->VariableReference=Source->VariableReference; Copy->CreateNewGuid();
     Graph->AddNode(Copy,false,false); Copy->AllocateDefaultPins();
     if(!Copy->GetValuePin()) { Result=TEXT("getter-copy preview has no value pin"); return false; }
     Copy->NodePosX=FMath::RoundToInt(C.PlannedX); Copy->NodePosY=FMath::RoundToInt(C.PlannedY);
     Source->GetValuePin()->BreakLinkTo(Input); Copy->GetValuePin()->MakeLinkTo(Input);
    }
   }
   const TSharedRef<SGraphEditor> Editor=SNew(SGraphEditor).GraphToEdit(Graph).IsEditable(false).ShowGraphStateOverlay(false);
   const FVector2D Size(2048,1536);
   TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size,TF_Bilinear,true));
   if(!Target.IsValid()) { Result=TEXT("render target unavailable"); return false; }
   // Slate applies display gamma; keep the destination linear to avoid a second sRGB encode.
   Target->InitCustomFormat(2048,1536,PF_B8G8R8A8,true);
   Target->UpdateResourceImmediate(true);
   FWidgetRenderer Renderer(true);
   // Offscreen graphs have no active-timer tick: populate their node widgets explicitly.
   Editor->GetGraphPanel()->Update();
   Editor->SetViewLocation(FVector2f::ZeroVector,1.0f);
   Editor->SlatePrepass(1.0f);
   Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
   Editor->SlatePrepass(1.0f);
   // A second layout pass settles full-detail desired sizes before framing.
   Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
   for(UEdGraphNode* N:Graph->Nodes) if(N && !Editor->GetGraphPanel()->GetNodeWidgetFromGuid(N->NodeGuid).IsValid())
   { Result=TEXT("native preview node widgets did not populate; no blank image returned"); return false; }
   double Left=0,Top=0,Right=1,Bottom=1; bool First=true;
   for(UEdGraphNode* N:Graph->Nodes) if(N)
   {
    auto Rect=FTurboMCPLayoutReport::RectOf(N);
    FSlateRect Bounds;
    if(Editor->GetBoundsForNode(N,Bounds,0.f)) { Rect.MinX=Bounds.Left; Rect.MinY=Bounds.Top; Rect.MaxX=Bounds.Right; Rect.MaxY=Bounds.Bottom; }
    if(First) { Left=Rect.MinX; Top=Rect.MinY; Right=Rect.MaxX; Bottom=Rect.MaxY; First=false; }
    else { Left=FMath::Min(Left,Rect.MinX); Top=FMath::Min(Top,Rect.MinY); Right=FMath::Max(Right,Rect.MaxX); Bottom=FMath::Max(Bottom,Rect.MaxY); }
   }
   const FVector2D Centre((Left+Right)/2,(Top+Bottom)/2);
   const FVector2D Canvas=Editor->GetGraphPanel()?Editor->GetGraphPanel()->GetTickSpaceGeometry().GetLocalSize():Size;
   const float Wanted=FMath::Min(1.0, FMath::Min(Canvas.X/(Right-Left+160),Canvas.Y/(Bottom-Top+160)));
   float Request=Wanted,Applied=1; FVector2f Offset;
   for(int32 Try=0;Try<64;++Try)
   {
    Editor->SetViewLocation(FVector2f::ZeroVector,Request); Editor->GetViewLocation(Offset,Applied);
    if(Applied<=Wanted+KINDA_SMALL_NUMBER) break;
    Request*=0.9f;
   }
   if(Applied>Wanted+KINDA_SMALL_NUMBER || Applied<=0) { Result=TEXT("graph exceeds native zoom range; use a smaller graph or selected group"); return false; }
   Editor->SetViewLocation(FVector2f(Centre-Canvas/(2*Applied)),Applied);
   Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
   TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
   if(!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags) || Pixels.Num()!=2048*1536) { Result=TEXT("native preview readback failed"); return false; }
   for(FColor& Pixel:Pixels) Pixel.A=255;
   TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(2048,1536,Pixels,Png);
   const FString Path=Folder/(Stem+(Side?TEXT("-after.png"):TEXT("-before.png")));
   if(Png.IsEmpty() || !FFileHelper::SaveArrayToFile(Png,*Path)) { Result=TEXT("native preview image could not be saved"); return false; }
   Result+=TEXT("\n")+(Side?FString(TEXT("after: ")):FString(TEXT("before: ")))+FPaths::ConvertRelativePathToFull(Path);
  }
  return true;
 }
}

FString FTurboMCPGraphTidy::RenderPreview(const FPlan& InPlan)
{
	TArray<FString> Lines;

	// THE NUMBERS GO ON THE FIRST LINE, as the spec requires, AND THEY ARE LABELLED AS THE THING THAT
	// WAS MEASURED. `D-LAY-4` settles that tidy is judged on TOTAL WIRE LENGTH and knots on WIRES
	// CROSSING NODES, so a reader comparing the two tools' numbers is comparing two quantities - and
	// the label is what stops that.
	Lines.Add(FString::Printf(TEXT("preview: %s in %s : total wire length %.1f -> %.1f across %d wire(s)"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireLengthAfter, InPlan.WireCount));

	if (InPlan.SelectedNodeKeys.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("scope: %d selected node(s) are selected; %d surrounding node(s) stay fixed. Box members stay grouped; selected boxes may grow without moving their corners."),
			InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount));
	}
	else
	{
		Lines.Add(TEXT("scope: whole graph; no fixed-node selection was supplied."));
	}

	if (InPlan.bAlreadyTidy)
	{
		Lines.Add(TEXT("the graph is ALREADY TIDY: no node would move, no box would change size, and no getter would be split. The edit would answer unchanged:"));
	}
	else if (InPlan.bWouldApply)
	{
		Lines.Add(FString::Printf(TEXT("the edit WOULD APPLY: %d node(s) would move, %d box(es) would be fitted, %d getter(s) would be split."), InPlan.NodesToMove, InPlan.BoxesToFit, InPlan.GetterCopies.Num()));
		if (InPlan.SelectedNodeKeys.Num() > 0)
		{
			Lines.Add(FString::Printf(TEXT("scope moves only: %s"), *FString::Join(InPlan.NodesToMoveKeys, TEXT(", "))));
		}
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

	// `Plan` refuses before reaching this point unless the graph panel has measured every outline.
	Lines.Add(TEXT("geometry: every node has a Slate-measured size; no estimate was used."));


    FString Images;
    if (!RenderNativePreview(InPlan, Images)) Lines.Add(TEXT("preview images unavailable: ") + Images);
    else Lines.Add(Images);
	return FString::Join(Lines, TEXT("\n"));
}

FString FTurboMCPGraphTidy::Apply(const FPlan& InPlan)
{
    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: tidy requires an idle editor with undo available, outside PIE and active transactions");
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
	for (const FGetterCopy& Copy : InPlan.GetterCopies)
	{
		bool bConsumerLive = false;
		for (const UEdGraphNode* Node : Graph->Nodes)
			if (Node && Copy.ConsumerPin && Node->Pins.Contains(Copy.ConsumerPin)) { bConsumerLive = true; break; }
		if (!Copy.OriginalValuePin || !Graph->Nodes.Contains(Copy.Original) || !CanSplitGetter(Copy.Original)
			|| Copy.Original->GetValuePin() != Copy.OriginalValuePin || !bConsumerLive
			|| !Copy.OriginalValuePin->LinkedTo.Contains(Copy.ConsumerPin)
			|| !Copy.ConsumerPin->LinkedTo.Contains(Copy.OriginalValuePin))
			return TEXT("unchanged: getter split plan is stale; preview again before applying");
	}
	const bool bWasDirtyBefore = Graph->GetOutermost()->IsDirty();
	const int32 NodesBefore = Graph->Nodes.Num();
    FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphTidy", "TurboMCP tidy graph"));
    if (!Transaction.IsOutstanding()) return TEXT("unchanged: could not open tidy undo transaction");
    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph)) Blueprint->Modify();
    Graph->Modify();
    // Capture both ends of rewired links as well as moved nodes and comments.
    for (UEdGraphNode* Node : Graph->Nodes) if (Node) Node->Modify();
	TArray<FGetterCopy> Created = InPlan.GetterCopies;
	FString CopyFailure;

	// Create the getter copies. Each one takes over ONE consumer from the original.
	for (FGetterCopy& Entry : Created)
	{
		if (!CanSplitGetter(Entry.Original) || Entry.ConsumerPin == nullptr)
		{
			CopyFailure = TEXT("a planned getter copy could not be created");
			break;
		}

		UK2Node_VariableGet* Copy = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
		if (Copy == nullptr)
		{
			CopyFailure = TEXT("a planned getter copy could not be created");
			break;
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
		if (CopyValuePin == nullptr || CopyValuePin->PinType != Entry.OriginalValuePin->PinType)
		{
			// REFUSED RATHER THAN HALF-DONE. A copy with no value pin cannot take the consumer, and
			// destroying the original's link without somewhere to put it would sever the graph. So the
			// copy is removed and the consumer is left exactly as it was.
			Copy->DestroyNode();
			CopyFailure = TEXT("a planned getter copy could not be created");
			break;
		}

		UEdGraphPin* OriginalValuePin = Entry.OriginalValuePin != nullptr
			? Entry.OriginalValuePin : Entry.Original->GetValuePin();
		if (OriginalValuePin != nullptr)
		{
			OriginalValuePin->BreakLinkTo(Entry.ConsumerPin);
		}
		CopyValuePin->MakeLinkTo(Entry.ConsumerPin);

		Entry.Copy = Copy;
		if (!CopyValuePin->LinkedTo.Contains(Entry.ConsumerPin)
			|| !Entry.ConsumerPin->LinkedTo.Contains(CopyValuePin)
			|| Entry.ConsumerPin->LinkedTo.Contains(OriginalValuePin))
		{
			CopyFailure = TEXT("getter-copy connection read-back failed");
			break;
		}
	}

	// -----------------------------------------------------------------------------------------
	// THE POSITIONS ARE WRITTEN NOW, because the whole point of the pass is the effect of the new
	// positions on the wire length, and every original position is kept so an unfit pass can be put back
	// EXACTLY.
	// -----------------------------------------------------------------------------------------
	TArray<TPair<UEdGraphNode*, TPair<int32, int32>>> OriginalPositions;
	TArray<FString> ActuallyMovedKeys;
	for (const FNode& Entry : InPlan.Nodes)
	{
		if (!CopyFailure.IsEmpty()) break;
		if (Entry.Node == nullptr || Entry.bIsComment || Entry.bUnreachable
			|| (InPlan.SelectedNodeKeys.Num() > 0 && !InPlan.SelectedNodeKeys.Contains(Entry.Key)))
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
		ActuallyMovedKeys.Add(Entry.Key);
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

	const bool bBetter = MeasuredAfter < InPlan.WireLengthBefore;
	if (!CopyFailure.IsEmpty() || (!bBetter && !InPlan.bForce))
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
			else
			{
				++RestorationFailed;
			}
			Entry.Copy->DestroyNode();
			++Undone;
		}
		Graph->MarkPackageDirty();

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
		bool bPositionsRestored = true;
		for (const auto& Undo : OriginalPositions)
			bPositionsRestored &= Undo.Key->NodePosX == Undo.Value.Key && Undo.Key->NodePosY == Undo.Value.Value;
		const bool bRollbackComplete = bRestored && RestorationFailed == 0
			&& bPositionsRestored && Graph->Nodes.Num() == NodesBefore;
		if (bRollbackComplete && !bWasDirtyBefore) Graph->GetOutermost()->SetDirtyFlag(false);
		FString Out = FString::Printf(TEXT("%s: %s in %s did not complete an acceptable layout (total wire length %.1f -> %.1f), so rollback was attempted. Preview predicted %.1f; the post-write measurement was %.1f. %d node position(s) and %d getter copy(ies) were undone, and the rollback %s the original length (%.1f)."), bRollbackComplete ? TEXT("unchanged") : TEXT("partial"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, InPlan.WireLengthAfter, MeasuredAfter, OriginalPositions.Num(), Undone, bRestored ? TEXT("RESTORED") : TEXT("DID NOT RESTORE"), Restored);
		if (InPlan.SelectedNodeKeys.Num() > 0)
		{
			Out += FString::Printf(TEXT(" Scope was %d selected node(s); %d surrounding node(s) and all comment boxes were fixed anchors."), InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount);
		}

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
		if (!CopyFailure.IsEmpty()) Out += TEXT(" Cause: ") + CopyFailure + TEXT(". Force does not bypass split failures.");
		if (!bRollbackComplete) Out += TEXT(" rollback=incomplete; package remains dirty; inspect before retrying or saving.");
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
	const bool bScoreMatchesPlan = FMath::IsNearlyEqual(MeasuredAfter, InPlan.WireLengthAfter, 1.0);
	Lines.Add(FString::Printf(TEXT("changed: tidied %s in %s : total wire length %.1f -> %.1f%s"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, bWorse ? TEXT(" (WORSE, applied because Force was set)") : TEXT("")));
	Lines.Add(FString::Printf(TEXT("score: preview predicted %.1f; post-edit measured %.1f (%s)."), InPlan.WireLengthAfter, MeasuredAfter, bScoreMatchesPlan ? TEXT("MATCHES THE PREVIEW") : TEXT("DOES NOT MATCH THE PREVIEW")));
	Lines.Add(FString::Printf(TEXT("moved %d node(s). Fitted %d of %d box(es), and NO BOX CORNER MOVED: every box keeps the top-left corner the user placed, and only its size changed."), OriginalPositions.Num(), BoxesResized, InPlan.Boxes.Num()));
	if (InPlan.SelectedNodeKeys.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("scope: only the %d selected node(s) were eligible to move; %d surrounding node(s) and all comment boxes remained fixed anchors."), InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount));
		Lines.Add(FString::Printf(TEXT("scope moved: %s"), *FString::Join(ActuallyMovedKeys, TEXT(", "))));
	}
	if (GettersSplit > 0)
	{
		Lines.Add(FString::Printf(TEXT("split %d variable getter(s), one copy per extra consumer, placed beside the node that reads it."), GettersSplit));
	}
	if (InPlan.UnreachableCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("left %d unreachable node(s) EXACTLY where they were, because disconnected groups are not moved into the layout. R-GRAPH-DEAD removes them."), InPlan.UnreachableCount));
	}
	Lines.Add(TEXT("geometry: every node had a Slate-measured size; no estimate was used."));
	Lines.Add(TEXT("no knots were added: P/E-GRAPH-KNOTS owns that rule and tidy is measured on total wire length, which a knot lengthens."));
	Lines.Add(TEXT("undo=one; compile=not_run; save=not_requested"));
	Lines.Add(TEXT("; the change is in memory and NOT saved. Call E-SAVE."));

	return FString::Join(Lines, TEXT("\n"));
}
