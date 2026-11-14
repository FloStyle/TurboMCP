// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphAppend.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Math/NumericLimits.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPComment.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPGraphTidy.h"

/**
 * Deterministic placement for the forms an append creates.
 *
 * Attached blocks follow their dependency lanes; detached forms use a separate column.
 * Created footprints come from scratch Slate measurement when supplied. Missing existing dimensions
 * retain an explicit 320x160 estimate. Every edge, extent and addition is widened to int64
 * and checked before the int32 cast; a coordinate outside the node coordinate range refuses.
 */
namespace
{
	constexpr int64 PlacementWidth = 320;
	constexpr int64 PlacementHeight = 160;
	constexpr int64 PlacementHorizontalGap = 160;
	constexpr int64 PlacementVerticalGap = 64;

	/** Add two widened values, reporting an int64 overflow instead of wrapping. */
	bool AddWide(int64 InA, int64 InB, int64& OutSum)
	{
		if ((InB > 0 && InA > MAX_int64 - InB) || (InB < 0 && InA < MIN_int64 - InB))
		{
			return false;
		}
		OutSum = InA + InB;
		return true;
	}

	/** A node position is an int32; INT32_MIN is a legal coordinate when the arithmetic fits. */
	bool FitsCoordinate(int64 InValue)
	{
		return InValue >= static_cast<int64>(MIN_int32) && InValue <= static_cast<int64>(MAX_int32);
	}
}

bool FTurboMCPGraphAppend::PlanTailTranslation(const UEdGraph* Graph,
    const TArray<FString>& MovableNodeIds, const TArray<FPlacementObstacle>& NewBounds,
    TArray<FExistingPosition>& OutMoves, FString& OutError)
{
    OutMoves.Reset(); OutError.Empty();
    if (!Graph || MovableNodeIds.IsEmpty() || NewBounds.IsEmpty())
    { OutError = TEXT("insertion requires a graph, explicit movable node IDs and new node bounds"); return false; }

    TMap<FString, const UEdGraphNode*> ById;
    for (const UEdGraphNode* Node : Graph->Nodes)
        if (Node) ById.Add(TurboMCPNodeId::MakeForNode(Node, true), Node);
    TSet<const UEdGraphNode*> Allowed;
    for (const FString& Id : MovableNodeIds)
    {
        const UEdGraphNode* const* Found = ById.Find(Id);
        if (!Found || Allowed.Contains(*Found))
        { OutError = TEXT("insertion has an unknown or duplicate movable node ID: ") + Id; return false; }
        Allowed.Add(*Found);
    }
    auto Bounds = [](const UEdGraphNode* Node, int64 Shift)
    {
        return FPlacementObstacle{int64(Node->NodePosX) + Shift, Node->NodePosY,
            Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth,
            Node->NodeHeight > 0 ? Node->NodeHeight : PlacementHeight};
    };
    auto Overlaps = [](const FPlacementObstacle& A, const FPlacementObstacle& B)
    {
        return A.X < B.X + B.Width && B.X < A.X + A.Width
            && A.Y < B.Y + B.Height && B.Y < A.Y + A.Height;
    };
    TMap<const UEdGraphNode*, TArray<const UEdGraphNode*>> CommentGroups;
    for (const UEdGraphNode* Node : Allowed)
        if (const UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Node))
        {
            TArray<FString> Members;
            FTurboMCPComment::MembersOf(const_cast<UEdGraphNode_Comment*>(Box), Members);
            TArray<const UEdGraphNode*>& Group = CommentGroups.Add(Box);
            const FPlacementObstacle Outer = Bounds(Box, 0);
            for (const UEdGraphNode* Candidate : Graph->Nodes)
            {
                if (!Candidate || Candidate == Box) continue;
                const FPlacementObstacle Inner = Bounds(Candidate, 0);
                const bool NestedBox = Candidate->IsA<UEdGraphNode_Comment>()
                    && Inner.X >= Outer.X && Inner.Y >= Outer.Y
                    && Inner.X + Inner.Width <= Outer.X + Outer.Width
                    && Inner.Y + Inner.Height <= Outer.Y + Outer.Height;
                if (!Members.Contains(Candidate->GetName()) && !NestedBox) continue;
                if (!Allowed.Contains(Candidate))
                { OutError = TEXT("Moving a comment requires explicit authorization for its contents: ") + TurboMCPNodeId::MakeForNode(Candidate, true); return false; }
                Group.Add(Candidate);
            }
        }
    for (const FPlacementObstacle& Rect : NewBounds)
        if (Rect.Width <= 0 || Rect.Height <= 0 || !FitsCoordinate(Rect.X)
            || !FitsCoordinate(Rect.Y) || Rect.Width > MAX_int32 || Rect.Height > MAX_int32
            || !FitsCoordinate(Rect.X + Rect.Width) || !FitsCoordinate(Rect.Y + Rect.Height))
        { OutError = TEXT("insertion contains invalid or overflowing new node bounds"); return false; }

    TSet<const UEdGraphNode*> Moving;
    for (const UEdGraphNode* Node : Graph->Nodes)
        if (Node && (!Node->IsA<UEdGraphNode_Comment>() || Allowed.Contains(Node)))
            for (const FPlacementObstacle& Rect : NewBounds)
                if (Overlaps(Bounds(Node, 0), Rect))
                {
                    if (!Allowed.Contains(Node))
                    { OutError = TEXT("insertion is blocked by fixed node: ") + TurboMCPNodeId::MakeForNode(Node, true); return false; }
                    Moving.Add(Node);
                }
    if (Moving.IsEmpty()) return true;

    // Grow only the necessary subset of the caller's upper bound. A uniform shift
    // preserves all relative positions within the tail. Each iteration either
    // adds a node or stabilizes, so graph cycles cannot cause an endless walk.
    int64 Shift = 0;
    for (int32 Iteration = 0; Iteration <= Allowed.Num(); ++Iteration)
    {
        // A movable box and its explicitly authorized contents translate as one
        // group. Repeat closure for nested boxes before computing the shared shift.
        bool Expanded;
        do
        {
            Expanded = false;
            for (const auto& Pair : CommentGroups)
            {
                bool Touched = Moving.Contains(Pair.Key);
                for (const UEdGraphNode* Member : Pair.Value) Touched |= Moving.Contains(Member);
                if (!Touched) continue;
                if (!Moving.Contains(Pair.Key)) { Moving.Add(Pair.Key); Expanded = true; }
                for (const UEdGraphNode* Member : Pair.Value)
                    if (!Moving.Contains(Member)) { Moving.Add(Member); Expanded = true; }
            }
        } while (Expanded);
        for (const UEdGraphNode* Node : Moving)
        {
            const FPlacementObstacle Original = Bounds(Node, 0);
            for (const FPlacementObstacle& Rect : NewBounds)
                if (Original.Y < Rect.Y + Rect.Height && Rect.Y < Original.Y + Original.Height
                    && Original.X < Rect.X + Rect.Width)
                    Shift = FMath::Max(Shift, Rect.X + Rect.Width + PlacementHorizontalGap - Original.X);
        }
        TSet<const UEdGraphNode*> Additions;
        for (const UEdGraphNode* Node : Moving)
        {
            const FPlacementObstacle Moved = Bounds(Node, Shift);
            if (!FitsCoordinate(Moved.X) || !FitsCoordinate(Moved.X + Moved.Width))
            { OutError = TEXT("insertion would overflow coordinates for: ") + TurboMCPNodeId::MakeForNode(Node, true); return false; }
            for (const UEdGraphNode* Other : Graph->Nodes)
                if (Other && !Moving.Contains(Other) && !Other->IsA<UEdGraphNode_Comment>()
                    && Overlaps(Moved, Bounds(Other, 0))) Additions.Add(Other);
            for (const UEdGraphPin* Pin : Node->Pins)
                if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                    for (const UEdGraphPin* Target : Pin->LinkedTo)
                        if (Target && Target->GetOwningNode() && !Moving.Contains(Target->GetOwningNode())
                            && Moved.X + Moved.Width + PlacementHorizontalGap > Target->GetOwningNode()->NodePosX)
                            Additions.Add(Target->GetOwningNode());
        }
        if (Additions.IsEmpty()) break;
        for (const UEdGraphNode* Node : Additions)
        {
            if (!Allowed.Contains(Node))
            { OutError = TEXT("insertion would move or cross a fixed downstream node: ") + TurboMCPNodeId::MakeForNode(Node, true); return false; }
            Moving.Add(Node);
        }
    }
    TArray<FExistingPosition> Candidate;
    for (const UEdGraphNode* Node : Moving)
    {
        // Moving an output past a fixed data consumer creates a new backward
        // cable. Do not silently add that shared consumer to the execution tail.
        for (const UEdGraphPin* Pin : Node->Pins)
            if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                for (const UEdGraphPin* Input : Pin->LinkedTo)
                    if (Input && Input->GetOwningNode() && !Moving.Contains(Input->GetOwningNode()))
                    {
                        const UEdGraphNode* Consumer = Input->GetOwningNode();
                        const FPlacementObstacle Original = Bounds(Node, 0);
                        if (Original.X + Original.Width <= Consumer->NodePosX
                            && Original.X + Original.Width + Shift > Consumer->NodePosX)
                        { OutError = TEXT("insertion would cross a fixed data consumer: ") + TurboMCPNodeId::MakeForNode(Consumer, true); return false; }
                    }
        FExistingPosition Move;
        Move.NodeId = TurboMCPNodeId::MakeForNode(Node, true);
        Move.OldX = Node->NodePosX; Move.OldY = Node->NodePosY;
        Move.NewX = int32(int64(Node->NodePosX) + Shift); Move.NewY = Node->NodePosY;
        Candidate.Add(Move);
    }
    Candidate.Sort([](const FExistingPosition& A, const FExistingPosition& B) { return A.NodeId < B.NodeId; });
    OutMoves = MoveTemp(Candidate);
    return true;
}

bool FTurboMCPGraphAppend::PlanInsertionComment(UEdGraph* Graph, const FString& ContainingCommentId,
    const TArray<FExistingPosition>& Moves, const TArray<FPlacementObstacle>& NewBounds,
    FCommentGrowth& OutGrowth, FString& OutError)
{
    OutGrowth = FCommentGrowth(); OutError.Empty();
    UBlueprint* Blueprint = Graph ? Graph->GetTypedOuter<UBlueprint>() : nullptr;
    if (!Blueprint)
    { OutError = TEXT("insertion comment validation requires a Blueprint graph"); return false; }
    UEdGraphNode_Comment* Container = nullptr;
    if (!ContainingCommentId.IsEmpty())
    {
        Container = Cast<UEdGraphNode_Comment>(TurboMCPGraphScratch::Find(Graph, ContainingCommentId));
        if (!Container || Container->NodeWidth <= 0 || Container->NodeHeight <= 0)
        { OutError = TEXT("insertion containing comment is missing or has invalid dimensions: ") + ContainingCommentId; return false; }
    }
    // Use the shared membership implementation on a temporary graph. No live
    // coordinates, comment caches, package dirtiness or undo records are changed.
    TurboMCPGraphScratch::FCopy Scratch;
    if (!Scratch.Init(Blueprint, Graph))
    { OutError = TEXT("insertion could not create a scratch graph for comment validation"); return false; }
    auto ContainsBox = [](const UEdGraphNode_Comment* Outer, const UEdGraphNode_Comment* Inner)
    {
        return Inner->NodePosX >= Outer->NodePosX && Inner->NodePosY >= Outer->NodePosY
            && int64(Inner->NodePosX) + Inner->NodeWidth <= int64(Outer->NodePosX) + Outer->NodeWidth
            && int64(Inner->NodePosY) + Inner->NodeHeight <= int64(Outer->NodePosY) + Outer->NodeHeight;
    };
    auto BoxesOverlap = [](const UEdGraphNode_Comment* A, const UEdGraphNode_Comment* B)
    {
        return int64(A->NodePosX) < int64(B->NodePosX) + B->NodeWidth
            && int64(B->NodePosX) < int64(A->NodePosX) + A->NodeWidth
            && int64(A->NodePosY) < int64(B->NodePosY) + B->NodeHeight
            && int64(B->NodePosY) < int64(A->NodePosY) + A->NodeHeight;
    };
    TMap<FGuid, TArray<FString>> Before;
    for (UEdGraphNode* Node : Scratch.Graph->Nodes)
        if (UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Node))
            FTurboMCPComment::MembersOf(Box, Before.Add(Box->NodeGuid));
    TSet<FGuid> Seen;
    for (const FExistingPosition& Move : Moves)
    {
        UEdGraphNode* Original = TurboMCPGraphScratch::Find(Graph, Move.NodeId);
        UEdGraphNode* Copy = Original ? TurboMCPGraphScratch::ByGuid(Scratch.Graph, Original->NodeGuid) : nullptr;
        if (!Copy || Seen.Contains(Original->NodeGuid) || Original->NodePosX != Move.OldX
            || Original->NodePosY != Move.OldY || Move.NewX < Move.OldX || Move.NewY != Move.OldY)
        { OutError = TEXT("insertion has stale, duplicate or non-horizontal movement: ") + Move.NodeId; return false; }
        if (Original == Container)
        { OutError = TEXT("The containing comment must keep its top-left fixed; remove it from the movable node list."); return false; }
        Seen.Add(Original->NodeGuid);
        Copy->NodePosX = Move.NewX; Copy->NodePosY = Move.NewY;
    }
    UEdGraphNode_Comment* CopyContainer = Container
        ? Cast<UEdGraphNode_Comment>(TurboMCPGraphScratch::ByGuid(Scratch.Graph, Container->NodeGuid)) : nullptr;
    if (CopyContainer)
    {
        int64 Right = int64(Container->NodePosX) + Container->NodeWidth;
        const int64 Left = Container->NodePosX;
        const int64 Top = int64(Container->NodePosY) + int64(FTurboMCPComment::TitleBarHeight);
        const int64 Bottom = int64(Container->NodePosY) + Container->NodeHeight;
        for (const FPlacementObstacle& Rect : NewBounds)
        {
            if (Rect.Width <= 0 || Rect.Height <= 0 || !FitsCoordinate(Rect.X) || !FitsCoordinate(Rect.Y)
                || Rect.Width > MAX_int32 || Rect.Height > MAX_int32
                || Rect.X < Left || Rect.Y < Top || Rect.Y + Rect.Height > Bottom)
            { OutError = TEXT("insertion does not fit the named comment vertically or to its right; its top-left and height stay fixed"); return false; }
            Right = FMath::Max(Right, Rect.X + Rect.Width + 32);
        }
        const TArray<FString>& OriginalMembers = Before.FindChecked(Container->NodeGuid);
        for (UEdGraphNode* Node : Scratch.Graph->Nodes)
            if (Node && Seen.Contains(Node->NodeGuid) && OriginalMembers.Contains(Node->GetName()))
            {
                // Use the same existing-node bounds as tail collision planning.
                // Scratch copies do not carry the live Slate measurement cache.
                const int64 Width = Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth;
                Right = FMath::Max(Right, int64(Node->NodePosX) + Width + 32);
            }
        // Membership helpers cover ordinary nodes. Nested comments also need
        // their complete frame preserved, including its empty right-hand padding.
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* Inner = Cast<UEdGraphNode_Comment>(Node); Inner && Inner != Container && Seen.Contains(Inner->NodeGuid) && ContainsBox(Container, Inner))
            {
                const UEdGraphNode* Moved = TurboMCPGraphScratch::ByGuid(Scratch.Graph, Inner->NodeGuid);
                Right = FMath::Max(Right, int64(Moved->NodePosX) + Inner->NodeWidth + 32);
            }
        if (!FitsCoordinate(Right) || Right - Left > MAX_int32)
        { OutError = TEXT("insertion comment growth would overflow graph coordinates"); return false; }
        CopyContainer->NodeWidth = int32(Right - Left);
        // Growth must not cover a neighboring comment or escape an existing ancestor.
        for (UEdGraphNode* Node : Graph->Nodes)
            if (UEdGraphNode_Comment* Other = Cast<UEdGraphNode_Comment>(Node); Other && Other != Container)
            {
                const int64 OtherRight = int64(Other->NodePosX) + Other->NodeWidth;
                const int64 OtherBottom = int64(Other->NodePosY) + Other->NodeHeight;
                const bool Ancestor = Other->NodePosX <= Left && Other->NodePosY <= Container->NodePosY
                    && OtherRight >= int64(Container->NodePosX) + Container->NodeWidth && OtherBottom >= Bottom;
                const bool Nested = ContainsBox(Container, Other);
                const bool HitsGrowth = Right > int64(Container->NodePosX) + Container->NodeWidth
                    && Other->NodePosX < Right && OtherRight > int64(Container->NodePosX) + Container->NodeWidth
                    && Other->NodePosY < Bottom && OtherBottom > Container->NodePosY;
                if ((Ancestor && Right > OtherRight) || (!Ancestor && !Nested && HitsGrowth))
                { OutError = TEXT("insertion comment growth is blocked by fixed comment: ") + TurboMCPNodeId::MakeForNode(Other, true); return false; }
            }
    }
    // Preserve nested relationships and prevent translated frames from crossing
    // formerly disjoint frames. Checking ordinary members alone misses empty boxes.
    for (UEdGraphNode* NodeA : Graph->Nodes)
        if (const auto* A = Cast<UEdGraphNode_Comment>(NodeA))
            for (UEdGraphNode* NodeB : Graph->Nodes)
                if (const auto* B = Cast<UEdGraphNode_Comment>(NodeB); B && A != B)
                {
                    const auto* AfterA = Cast<UEdGraphNode_Comment>(TurboMCPGraphScratch::ByGuid(Scratch.Graph, A->NodeGuid));
                    const auto* AfterB = Cast<UEdGraphNode_Comment>(TurboMCPGraphScratch::ByGuid(Scratch.Graph, B->NodeGuid));
                    if (!AfterA || !AfterB || ContainsBox(A, B) != ContainsBox(AfterA, AfterB)
                        || (!BoxesOverlap(A, B) && BoxesOverlap(AfterA, AfterB)))
                    {
                        OutError = TEXT("Insertion would change comment nesting or overlap: ")
                            + TurboMCPNodeId::MakeForNode(A, true) + TEXT(" and ") + TurboMCPNodeId::MakeForNode(B, true);
                        return false;
                    }
                }
    for (UEdGraphNode* Node : Scratch.Graph->Nodes)
        if (UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(Node))
        {
            TArray<FString> After;
            FTurboMCPComment::MembersOf(Box, After);
            if (Before.FindChecked(Box->NodeGuid) != After)
            { OutError = TEXT("insertion would change existing comment membership: ") + Box->GetName(); return false; }
        }
    if (CopyContainer && CopyContainer->NodeWidth != Container->NodeWidth)
    {
        OutGrowth.NodeId = TurboMCPNodeId::MakeForNode(Container, true);
        OutGrowth.OldWidth = Container->NodeWidth; OutGrowth.NewWidth = CopyContainer->NodeWidth;
    }
    return true;
}

bool FTurboMCPGraphAppend::PlanInsertion(UEdGraph* Graph, FPlan& Plan,
    const TArray<FNewLink>& LayoutLinks, FString& OutError)
{
    Plan.PlannedPositions.Reset(); Plan.ExistingPositions.Reset(); Plan.CommentGrowth = FCommentGrowth();
    OutError.Empty();
    UBlueprint* Blueprint = Graph ? Graph->GetTypedOuter<UBlueprint>() : nullptr;
    if (!Blueprint || Plan.MovableNodeIds.IsEmpty())
    { OutError = TEXT("Insertion requires a Blueprint graph and explicit movable node IDs."); return false; }
    TSet<FGuid> Movable;
    for (FString& Id : Plan.MovableNodeIds)
    {
        const UEdGraphNode* Node = TurboMCPGraphScratch::Find(Graph, Id);
        if (!Node || Movable.Contains(Node->NodeGuid))
        { OutError = TEXT("Unknown or repeated movable node: ") + Id; return false; }
        Movable.Add(Node->NodeGuid);
        Id = TurboMCPNodeId::MakeForNode(Node, true);
    }
    const UEdGraphNode* Anchor = nullptr;
    int32 AnchorForm = INDEX_NONE;
    for (const FNewLink& Link : LayoutLinks)
    {
        if (Link.FromFormIndex != INDEX_NONE || !Plan.Nodes.IsValidIndex(Link.ToFormIndex)
            || !Plan.Nodes[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin)) continue;
        const UEdGraphNode* Candidate = TurboMCPGraphScratch::Find(Graph, Link.FromNodeKey);
        if (!Candidate || Movable.Contains(Candidate->NodeGuid)
            || (Anchor && (Candidate != Anchor || AnchorForm != Link.ToFormIndex)))
        { OutError = TEXT("Insertion requires one fixed execution attachment; conflicting source: ") + Link.FromNodeKey; return false; }
        Anchor = Candidate; AnchorForm = Link.ToFormIndex;
    }
    if (!Anchor) { OutError = TEXT("Insertion requires an existing execution source connected to the new chain."); return false; }

    // Remove only authorized obstacles from a disposable layout view. The full
    // live graph is used below to compute movement and validate comment containment.
    TurboMCPGraphScratch::FCopy Scratch;
    if (!Scratch.Init(Blueprint, Graph)) { OutError = TEXT("Could not create insertion layout scratch graph."); return false; }
    TArray<FNewLink> ScratchLinks = LayoutLinks;
    for (FNewLink& Link : ScratchLinks)
    {
        for (FString* Key : {&Link.FromNodeKey, &Link.ToNodeKey})
        {
            if (Key->IsEmpty()) continue;
            if (const UEdGraphNode* Original = TurboMCPGraphScratch::Find(Graph, *Key))
                if (const UEdGraphNode* Copy = TurboMCPGraphScratch::ByGuid(Scratch.Graph, Original->NodeGuid))
                    *Key = TurboMCPNodeId::MakeForNode(Copy, true);
        }
    }
    Scratch.Graph->Nodes.RemoveAll([&](const UEdGraphNode* Node)
    { return Node && (Movable.Contains(Node->NodeGuid) || Node->IsA<UEdGraphNode_Comment>()); });
    if (!PlanPlacement(Scratch.Graph, Plan.Nodes, Plan.PlannedPositions,
        Plan.bPlacementUsedEstimatedSize, OutError, &ScratchLinks)) return false;
    const FPlannedPosition* First = Plan.PlannedPositions.FindByPredicate([&](const FPlannedPosition& P)
    { return P.FormIndex == AnchorForm; });
    if (!First) { OutError = TEXT("Insertion placement did not include its execution attachment."); return false; }
    // Keep insertion in the attachment lane. A fixed obstacle must cause a
    // refusal rather than silently relocating the new block beyond the tail.
    const int64 DeltaX = int64(Anchor->NodePosX)
        + (Anchor->NodeWidth > 0 ? Anchor->NodeWidth : PlacementWidth)
        + PlacementHorizontalGap - First->X;
    const int64 DeltaY = int64(Anchor->NodePosY) - First->Y;
    TArray<FPlacementObstacle> Bounds;
    for (FPlannedPosition& Position : Plan.PlannedPositions)
    {
        const FNewNode& Form = Plan.Nodes[Position.FormIndex];
        const int64 X = int64(Position.X) + DeltaX;
        const int64 Width = Form.MeasuredWidth > 0 ? Form.MeasuredWidth : PlacementWidth;
        const int64 Y = int64(Position.Y) + DeltaY;
        const int64 Height = Form.MeasuredHeight > 0 ? Form.MeasuredHeight : PlacementHeight;
        if (!FitsCoordinate(X) || !FitsCoordinate(X + Width) || !FitsCoordinate(Y) || !FitsCoordinate(Y + Height))
        { OutError = TEXT("Insertion geometry exceeds the supported coordinate range."); return false; }
        Position.X = int32(X); Position.Y = int32(Y);
        Bounds.Add({X, Y, Width, Height});
    }
    if (!PlanTailTranslation(Graph, Plan.MovableNodeIds, Bounds, Plan.ExistingPositions, OutError)) return false;
    return PlanInsertionComment(Graph, Plan.ContainingCommentId, Plan.ExistingPositions, Bounds, Plan.CommentGrowth, OutError);
}

bool FTurboMCPGraphAppend::PlanPlacement(const UEdGraph* InGraph, const TArray<FNewNode>& InNodes,
	TArray<FPlannedPosition>& OutPositions, bool& bOutUsedEstimatedSize, FString& OutRefusal, const TArray<FNewLink>* InLinks, bool bPlaceOutside, bool bPerAnchor, const TArray<FPlacementObstacle>* Reserved)
{
	OutPositions.Reset();
	bOutUsedEstimatedSize = false;
	OutRefusal.Empty();

	if (InGraph == nullptr)
	{
		OutRefusal = TEXT("placement: the graph is unavailable, so no stored or estimated bounds exist");
		return false;
	}

	int32 CreatedCount = 0;
	for (const FNewNode& Form : InNodes)
	{
		if (!Form.bLinksToExisting)
		{
			++CreatedCount;
		}
	}
	if (CreatedCount == 0)
	{
		return true;
	}

    // Components include data links: shared pure producers must never be duplicated.
    // Keep original form indices so preview and apply refer to the same input forms.
    if (bPerAnchor)
    {
        TSet<int32> Visited;
        TArray<FPlacementObstacle> Occupied;
        if (Reserved) Occupied = *Reserved;
        for (int32 Seed = 0; Seed < InNodes.Num(); ++Seed)
        {
            if (InNodes[Seed].bLinksToExisting || Visited.Contains(Seed)) continue;
            TSet<int32> Members;
            TArray<int32> Pending;
            Pending.Add(Seed); Members.Add(Seed); Visited.Add(Seed);
            for (int32 Cursor = 0; Cursor < Pending.Num(); ++Cursor)
                if (InLinks) for (const FNewLink& Link : *InLinks)
                {
                    const int32 Other = Link.FromFormIndex == Pending[Cursor] ? Link.ToFormIndex
                        : Link.ToFormIndex == Pending[Cursor] ? Link.FromFormIndex : INDEX_NONE;
                    if (InNodes.IsValidIndex(Other) && !InNodes[Other].bLinksToExisting && !Members.Contains(Other))
                    { Members.Add(Other); Visited.Add(Other); Pending.Add(Other); }
                }
            TArray<FNewNode> Forms = InNodes;
            for (int32 I = 0; I < Forms.Num(); ++I)
                if (!Members.Contains(I)) Forms[I].bLinksToExisting = true;
            TArray<FNewLink> Links;
            TSet<FString> ExecAnchors;
            bool bHasAnchor = false;
            if (InLinks) for (const FNewLink& Link : *InLinks)
                if (Members.Contains(Link.FromFormIndex) || Members.Contains(Link.ToFormIndex))
                {
                    Links.Add(Link);
                    if (Link.FromFormIndex == INDEX_NONE && Members.Contains(Link.ToFormIndex))
                    {
                        bHasAnchor = true;
                        if (Forms[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin)) ExecAnchors.Add(Link.FromNodeKey);
                    }
                    if (Link.ToFormIndex == INDEX_NONE && Members.Contains(Link.FromFormIndex)) bHasAnchor = true;
                }
            if (!bHasAnchor)
            {
                OutPositions.Reset();
                OutRefusal = FString::Printf(TEXT("per-anchor placement: component starting at line %d has %s. Split the request or use explicit local/outside placement; no nodes were moved."),
                    InNodes[Seed].Line, !bHasAnchor ? TEXT("no fixed connection") : TEXT("multiple fixed execution sources"));
                return false;
            }
            TArray<FPlannedPosition> Positions;
            bool bEstimated = false;
            FString Refusal;
            // Try each source deterministically. A candidate must satisfy every
            // incoming execution attachment, not merely the first link in the text.
            TArray<FString> AnchorKeys = ExecAnchors.Array();
            AnchorKeys.Sort();
            if (AnchorKeys.IsEmpty()) AnchorKeys.Add(FString());
            bool bPlaced = false;
            for (const FString& AnchorKey : AnchorKeys)
            {
                TArray<FNewLink> OrderedLinks;
                for (const FNewLink& Link : Links)
                    if (Link.FromFormIndex == INDEX_NONE && Link.FromNodeKey == AnchorKey) OrderedLinks.Add(Link);
                for (const FNewLink& Link : Links)
                    if (!(Link.FromFormIndex == INDEX_NONE && Link.FromNodeKey == AnchorKey)) OrderedLinks.Add(Link);
                if (!PlanPlacement(InGraph, Forms, Positions, bEstimated, Refusal, &OrderedLinks, false, false, &Occupied)) continue;
                bool bCompatible = true;
                if (ExecAnchors.Num() > 1) for (const FNewLink& Link : Links)
                {
                    if (Link.FromFormIndex != INDEX_NONE || !Members.Contains(Link.ToFormIndex)
                        || !Forms[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin)) continue;
                    const UEdGraphNode* Source = nullptr;
                    for (const UEdGraphNode* Node : InGraph->Nodes)
                        if (Node && TurboMCPNodeId::MakeForNode(Node, true) == Link.FromNodeKey) { Source = Node; break; }
                    const FPlannedPosition* Target = Positions.FindByPredicate([&](const FPlannedPosition& P) { return P.FormIndex == Link.ToFormIndex; });
                    if (!Source || !Target) { bCompatible = false; break; }
                    const int64 Width = FMath::Max<int64>(PlacementWidth, Forms[Link.ToFormIndex].MeasuredWidth);
                    const int64 Height = FMath::Max<int64>(PlacementHeight, Forms[Link.ToFormIndex].MeasuredHeight);
                    const int64 SourceRight = int64(Source->NodePosX) + (Source->NodeWidth > 0 ? Source->NodeWidth : PlacementWidth);
                    // Local compatibility permits two footprint strides, but never
                    // a backward execution wire or a remote source-to-lane jump.
                    const int64 DX = int64(Target->X) - SourceRight;
                    const int64 DY = FMath::Abs(int64(Target->Y) - int64(Source->NodePosY));
                    if (DX < 0 || DX > 2 * (Width + PlacementHorizontalGap)
                        || DY > 2 * (Height + PlacementVerticalGap)) { bCompatible = false; break; }
                }
                if (bCompatible) { bPlaced = true; break; }
            }
            if (!bPlaced)
            {
                OutPositions.Reset();
                OutRefusal = FString::Printf(TEXT("per-anchor component starting at line %d: %s"), InNodes[Seed].Line,
                    ExecAnchors.Num() > 1 ? TEXT("multiple fixed execution sources have no collision-free compatible local placement; no nodes were moved") : *Refusal);
                return false;
            }
            bOutUsedEstimatedSize |= bEstimated;
            for (const FPlannedPosition& Position : Positions)
            {
                const FNewNode& Form = InNodes[Position.FormIndex];
                Occupied.Add({Position.X, Position.Y, Form.MeasuredWidth > 0 ? Form.MeasuredWidth : PlacementWidth,
                    Form.MeasuredHeight > 0 ? Form.MeasuredHeight : PlacementHeight});
                OutPositions.Add(Position);
            }
        }
        OutPositions.Sort([](const FPlannedPosition& A, const FPlannedPosition& B) { return A.FormIndex < B.FormIndex; });
        return true;
    }
    const auto ClearsReserved = [&](int64 X, int64 Y, int64 W, int64 H)
    {
        if (Reserved) for (const FPlacementObstacle& Box : *Reserved)
            if (X < Box.X + Box.Width + PlacementHorizontalGap && X + W + PlacementHorizontalGap > Box.X
                && Y < Box.Y + Box.Height + PlacementVerticalGap && Y + H + PlacementVerticalGap > Box.Y) return false;
        return true;
    };

    int64 CellWidth = 0, CellHeight = 0;
    for (const FNewNode& Form : InNodes)
        if (!Form.bLinksToExisting)
        {
            CellWidth = FMath::Max<int64>(CellWidth, Form.MeasuredWidth > 0 ? Form.MeasuredWidth : PlacementWidth);
            CellHeight = FMath::Max<int64>(CellHeight, Form.MeasuredHeight > 0 ? Form.MeasuredHeight : PlacementHeight);
            bOutUsedEstimatedSize |= Form.MeasuredWidth <= 0 || Form.MeasuredHeight <= 0;
        }
    const int64 CellStride = CellHeight + PlacementVerticalGap;
	bool bHasExistingBounds = false;
	int64 Top = 0;
	int64 Right = 0;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		int64 Width = Node->NodeWidth;
		int64 Height = Node->NodeHeight;
		// EACH DIMENSION IS INDEPENDENT: a known positive width stays authoritative even when the
		// height is missing, so the block cannot be placed inside a known wide bound.
		if (Width <= 0)
		{
			Width = PlacementWidth;
			bOutUsedEstimatedSize = true;
		}
		if (Height <= 0)
		{
			Height = PlacementHeight;
			bOutUsedEstimatedSize = true;
		}

		int64 NodeRight = 0;
		if (!AddWide(Node->NodePosX, Width, NodeRight))
		{
			OutRefusal = FString::Printf(
				TEXT("placement: the right edge of an existing bound cannot be represented in 64 bits"));
			return false;
		}

		const int64 NodeTop = Node->NodePosY;
		if (!bHasExistingBounds)
		{
			bHasExistingBounds = true;
			Top = NodeTop;
			Right = NodeRight;
		}
		else
		{
			Top = FMath::Min(Top, NodeTop);
			Right = FMath::Max(Right, NodeRight);
		}
	}


    // Attachment-relative layout changes positions of new forms only. Keep the
    // conservative legacy column only for detached blocks.
    const UEdGraphNode* Attachment = nullptr;
    int32 AttachmentForm = INDEX_NONE;
    if (InLinks)
        for (const FNewLink& Link : *InLinks)
            if (Link.FromFormIndex == INDEX_NONE && InNodes.IsValidIndex(Link.ToFormIndex)
                && !InNodes[Link.ToFormIndex].bLinksToExisting
                && InNodes[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin))
            {
                for (const UEdGraphNode* Node : InGraph->Nodes)
                    if (Node && TurboMCPNodeId::MakeForNode(Node, true) == Link.FromNodeKey)
                    { Attachment = Node; AttachmentForm = Link.ToFormIndex; break; }
                if (Attachment) break;
            }
    bool bDataAttachment = false;
    // A pure block has no execution attachment. Anchor its result before the
    // existing consumer rather than treating connected calculations as detached.
    const bool bAllNewPure = !InNodes.ContainsByPredicate([](const FNewNode& Node)
    { return !Node.bLinksToExisting && (!Node.ProbeExecInputs.IsEmpty() || !Node.ProbeExecOutputs.IsEmpty()); });
    if (!Attachment && InLinks && bAllNewPure)
        for (const FNewLink& Link : *InLinks)
            if (InNodes.IsValidIndex(Link.FromFormIndex) && !InNodes[Link.FromFormIndex].bLinksToExisting
                && Link.ToFormIndex == INDEX_NONE)
            {
                for (const UEdGraphNode* Node : InGraph->Nodes)
                    if (Node && TurboMCPNodeId::MakeForNode(Node, true) == Link.ToNodeKey)
                    { Attachment = Node; AttachmentForm = Link.FromFormIndex; bDataAttachment = true; break; }
                if (Attachment) break;
            }
    // A single inserted execution node should use the available gap in its own
    // flow before the general block layout searches below unrelated events.
    // Existing nodes remain fixed. If no local slot fits, retain the general fallback.
    if (!bPlaceOutside && Attachment && !bDataAttachment && CreatedCount == 1 && InLinks)
    {
        const UEdGraphNode* Consumer = nullptr;
        for (const FNewLink& Link : *InLinks)
            if (Link.FromFormIndex == INDEX_NONE && Link.ToFormIndex == AttachmentForm)
                for (const UEdGraphPin* Pin : Attachment->Pins)
                    if (Pin && Pin->Direction == EGPD_Output && Pin->PinName.ToString() == Link.FromPin
                        && Pin->PinType.PinCategory == TEXT("exec") && Pin->LinkedTo.Num() == 1 && Pin->LinkedTo[0])
                        Consumer = Pin->LinkedTo[0]->GetOwningNode();
        if (Consumer && Consumer != Attachment)
        {
            const int64 MinX = int64(Attachment->NodePosX) + (Attachment->NodeWidth > 0 ? Attachment->NodeWidth : PlacementWidth) + 32;
            const int64 MaxX = int64(Consumer->NodePosX) - CellWidth - 32;
            if (MinX <= MaxX)
            {
                const int64 PreferredX = MinX + (MaxX - MinX) / 2;
                const int64 PreferredY = Attachment->NodePosY;
                bool Found = false;
                int64 BestX = 0, BestY = 0, BestScore = MAX_int64;
                for (int64 CandidateX : {PreferredX, MinX, MaxX})
                    for (int32 Row = -4; Row <= 4; ++Row)
                    {
                        const int64 CandidateY = PreferredY + int64(Row) * 64;
                        if (!FitsCoordinate(CandidateX) || !FitsCoordinate(CandidateX + CellWidth)
                            || !FitsCoordinate(CandidateY) || !FitsCoordinate(CandidateY + CellHeight)) continue;
                        bool Clear = ClearsReserved(CandidateX, CandidateY, CellWidth, CellHeight);
                        for (const UEdGraphNode* Node : InGraph->Nodes)
                        {
                            if (!Node) continue;
                            const int64 W = Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth;
                            const int64 H = Node->NodeHeight > 0 ? Node->NodeHeight : PlacementHeight;
                            if (CandidateX < int64(Node->NodePosX) + W + 16 && CandidateX + CellWidth + 16 > Node->NodePosX
                                && CandidateY < int64(Node->NodePosY) + H + 16 && CandidateY + CellHeight + 16 > Node->NodePosY)
                            { Clear = false; break; }
                        }
                        const int64 Score = FMath::Abs(CandidateY - PreferredY) * 4 + FMath::Abs(CandidateX - PreferredX);
                        if (Clear && Score < BestScore) { Found = true; BestX = CandidateX; BestY = CandidateY; BestScore = Score; }
                    }
                if (Found)
                {
                    FPlannedPosition Position;
                    Position.FormIndex = AttachmentForm; Position.X = int32(BestX); Position.Y = int32(BestY);
                    Position.bEstimatedSize = InNodes[AttachmentForm].MeasuredWidth <= 0 || InNodes[AttachmentForm].MeasuredHeight <= 0;
                    OutPositions.Add(Position);
                    return true;
                }
            }
        }
    }
    if (Attachment && InLinks)
    {
        TArray<int32> Degree, Depth, Lane, Order;
        Degree.Init(0, InNodes.Num()); Depth.Init(0, InNodes.Num()); Lane.Init(0, InNodes.Num());
        const auto IsNew = [&](int32 I) { return InNodes.IsValidIndex(I) && !InNodes[I].bLinksToExisting; };
        for (const FNewLink& Link : *InLinks)
            if (IsNew(Link.FromFormIndex) && IsNew(Link.ToFormIndex)) ++Degree[Link.ToFormIndex];
        for (int32 I = 0; I < InNodes.Num(); ++I) if (IsNew(I) && Degree[I] == 0) Order.Add(I);
        for (int32 Cursor = 0; Cursor < Order.Num(); ++Cursor)
            for (const FNewLink& Link : *InLinks)
                if (Link.FromFormIndex == Order[Cursor] && IsNew(Link.ToFormIndex))
                {
                    Depth[Link.ToFormIndex] = FMath::Max(Depth[Link.ToFormIndex], Depth[Order[Cursor]] + 1);
                    if (--Degree[Link.ToFormIndex] == 0) Order.Add(Link.ToFormIndex);
                }
        if (Order.Num() != CreatedCount)
        {
            // A dependency cycle has no topological order. Use iterative DFS
            // reverse finish order; backward edges become layout return wires.
            // This is a position-only classification: no link is removed or changed.
            TArray<uint8> Visit;
            Visit.Init(0, InNodes.Num());
            TArray<int32> Roots, Finished, Stack, EdgeCursor;
            Roots.Add(AttachmentForm);
            for (int32 I = 0; I < InNodes.Num(); ++I) if (IsNew(I) && I != AttachmentForm) Roots.Add(I);
            for (int32 Root : Roots)
            {
                if (Visit[Root] != 0) continue;
                Visit[Root] = 1; Stack.Add(Root); EdgeCursor.Add(0);
                while (!Stack.IsEmpty())
                {
                    const int32 Current = Stack.Last();
                    bool bDescended = false;
                    while (EdgeCursor.Last() < InLinks->Num())
                    {
                        const FNewLink& Link = (*InLinks)[EdgeCursor.Last()++];
                        if (Link.FromFormIndex != Current || !IsNew(Link.ToFormIndex) || Visit[Link.ToFormIndex] != 0) continue;
                        Visit[Link.ToFormIndex] = 1;
                        Stack.Add(Link.ToFormIndex); EdgeCursor.Add(0);
                        bDescended = true;
                        break;
                    }
                    if (!bDescended)
                    {
                        Visit[Current] = 2; Finished.Add(Current);
                        Stack.RemoveAt(Stack.Num() - 1); EdgeCursor.RemoveAt(EdgeCursor.Num() - 1);
                    }
                }
            }
            Order.Reset();
            for (int32 I = Finished.Num() - 1; I >= 0; --I) Order.Add(Finished[I]);
        }
        TArray<int32> LayoutRank;
        LayoutRank.Init(INDEX_NONE, InNodes.Num());
        for (int32 I = 0; I < Order.Num(); ++I) LayoutRank[Order[I]] = I;
        const auto IsForward = [&](const FNewLink& Link)
        {
            return IsNew(Link.FromFormIndex) && IsNew(Link.ToFormIndex)
                && LayoutRank[Link.FromFormIndex] < LayoutRank[Link.ToFormIndex];
        };
        Depth.Init(0, InNodes.Num());
        for (int32 I : Order)
            for (const FNewLink& Link : *InLinks)
                if (Link.FromFormIndex == I && IsForward(Link))
                    Depth[Link.ToFormIndex] = FMath::Max(Depth[Link.ToFormIndex], Depth[I] + 1);
        if (Order.Num() == CreatedCount)
        {
            int32 NextLane = 1;
            TArray<int32> LaneParent;
            LaneParent.Add(INDEX_NONE);
            const auto SharedParentLane = [&](int32 A, int32 B)
            {
                TSet<int32> Ancestors;
                for (int32 L = A; L != INDEX_NONE; L = LaneParent[L]) Ancestors.Add(L);
                for (int32 L = B; L != INDEX_NONE; L = LaneParent[L])
                    if (Ancestors.Contains(L)) return L;
                return 0;
            };
            TSet<int32> AssignedLanes;
            for (int32 I : Order)
            {
                // Output pin order keeps the first connected continuation on its parent's row.
                bool bFirstConnectedOutput = true;
                for (int32 PinIndex = 0; PinIndex < InNodes[I].ProbeExecOutputs.Num(); ++PinIndex)
                {
                    int32 OutputLane = INDEX_NONE;
                    for (const FNewLink& Link : *InLinks)
                        if (Link.FromFormIndex == I && IsForward(Link)
                            && Link.FromPin == InNodes[I].ProbeExecOutputs[PinIndex]
                            && InNodes[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin))
                        {
                            if (OutputLane == INDEX_NONE)
                            {
                                if (bFirstConnectedOutput) OutputLane = Lane[I];
                                else { OutputLane = NextLane++; LaneParent.Add(Lane[I]); }
                                bFirstConnectedOutput = false;
                            }
                            if (AssignedLanes.Contains(Link.ToFormIndex))
                                Lane[Link.ToFormIndex] = SharedParentLane(Lane[Link.ToFormIndex], OutputLane);
                            else { Lane[Link.ToFormIndex] = OutputLane; AssignedLanes.Add(Link.ToFormIndex); }
                        }
                }
            }
            // Pure inputs inherit the consumer's lane, traversed consumer-first.
            for (int32 Cursor = Order.Num() - 1; Cursor >= 0; --Cursor)
            {
                const int32 I = Order[Cursor];
                if (!InNodes[I].ProbeExecInputs.IsEmpty() || !InNodes[I].ProbeExecOutputs.IsEmpty()) continue;
                for (const FNewLink& Link : *InLinks)
                    if (Link.FromFormIndex == I && IsForward(Link)) { Lane[I] = Lane[Link.ToFormIndex]; break; }
            }
            const int64 BaseX = bDataAttachment
                ? static_cast<int64>(Attachment->NodePosX) - (static_cast<int64>(Depth[AttachmentForm]) + 1) * (CellWidth + PlacementHorizontalGap)
                : static_cast<int64>(Attachment->NodePosX)
                    + (Attachment->NodeWidth > 0 ? static_cast<int64>(Attachment->NodeWidth) : PlacementWidth) + PlacementHorizontalGap
                    - static_cast<int64>(Depth[AttachmentForm]) * (CellWidth + PlacementHorizontalGap);
            const int64 BaseY = static_cast<int64>(Attachment->NodePosY)
                + (Attachment->NodeHeight > 0 ? static_cast<int64>(Attachment->NodeHeight) : PlacementHeight) + PlacementVerticalGap;
            // Reserve every input in the lane before starting the next exec band.
            TMap<int32, int32> PureCounts;
            for (int32 I : Order)
                if (InNodes[I].ProbeExecInputs.IsEmpty() && InNodes[I].ProbeExecOutputs.IsEmpty()) ++PureCounts.FindOrAdd(Lane[I]);
            // Keep nested side lanes beside their parent, before unrelated siblings.
            // Numeric creation order alone interleaves nested branches and crossings.
            TArray<int32> LaneOrder, PendingLanes;
            PendingLanes.Add(0);
            while (!PendingLanes.IsEmpty())
            {
                const int32 L = PendingLanes.Last();
                PendingLanes.RemoveAt(PendingLanes.Num() - 1);
                LaneOrder.Add(L);
                for (int32 Child = NextLane - 1; Child > 0; --Child)
                    if (LaneParent[Child] == L) PendingLanes.Add(Child);
            }
            TArray<int64> LaneY; LaneY.Init(BaseY, NextLane);
            int64 BandY = BaseY;
            for (int32 L : LaneOrder)
            {
                LaneY[L] = BandY;
                BandY += (static_cast<int64>(PureCounts.FindRef(L)) + 1) * CellStride;
            }
            TMap<int32, int32> PureSlots;
            TMap<int32, int32> DataColumnSlots;
            TMap<int32, int64> ExecRight;
            TArray<int64> X, Y; X.Init(0, InNodes.Num()); Y.Init(0, InNodes.Num());
            for (int32 I : Order)
            {
                X[I] = BaseX + static_cast<int64>(Depth[I]) * (CellWidth + PlacementHorizontalGap);
                for (const FNewLink& Link : *InLinks)
                    if (Link.ToFormIndex == I && IsForward(Link))
                        X[I] = FMath::Max(X[I], X[Link.FromFormIndex] + CellWidth + PlacementHorizontalGap);
                Y[I] = LaneY[Lane[I]];
                if (InNodes[I].ProbeExecInputs.IsEmpty() && InNodes[I].ProbeExecOutputs.IsEmpty())
                    Y[I] += bDataAttachment
                        ? static_cast<int64>(DataColumnSlots.FindOrAdd(Depth[I])++) * CellStride
                        : static_cast<int64>(++PureSlots.FindOrAdd(Lane[I])) * CellStride;
                else
                {
                    if (const int64* PreviousRight = ExecRight.Find(Lane[I])) X[I] = FMath::Max(X[I], *PreviousRight + PlacementHorizontalGap);
                    ExecRight.Add(Lane[I], X[I] + CellWidth);
                }
            }
            // Fit the block between all fixed execution attachments/rejoins when
            // there is room. If fixed endpoints leave too little room, minimize
            // backward connections rather than moving or duplicating those nodes.
            struct FFixedExecBoundary { int64 IdealShift; bool bIncoming; };
            TArray<FFixedExecBoundary> Boundaries;
            for (const FNewLink& Link : *InLinks)
            {
                const bool bIncoming = Link.FromFormIndex == INDEX_NONE && IsNew(Link.ToFormIndex)
                    && InNodes[Link.ToFormIndex].ProbeExecInputs.Contains(Link.ToPin);
                const bool bOutgoing = IsNew(Link.FromFormIndex) && Link.ToFormIndex == INDEX_NONE
                    && InNodes[Link.FromFormIndex].ProbeExecOutputs.Contains(Link.FromPin);
                if (!bIncoming && !bOutgoing) continue;
                const FString& FixedKey = bIncoming ? Link.FromNodeKey : Link.ToNodeKey;
                for (const UEdGraphNode* Node : InGraph->Nodes)
                    if (Node && TurboMCPNodeId::MakeForNode(Node, true) == FixedKey)
                    {
                        const int64 IdealShift = bIncoming
                            ? static_cast<int64>(Node->NodePosX) + (Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth)
                                + PlacementHorizontalGap - X[Link.ToFormIndex]
                            : static_cast<int64>(Node->NodePosX) - PlacementHorizontalGap - X[Link.FromFormIndex] - CellWidth;
                        Boundaries.Add({IdealShift, bIncoming});
                        break;
                    }
            }
            // Search around each fixed connection instead of pushing the block down
            // indefinitely. Existing nodes and comment rectangles remain obstacles.
            struct FLocalAnchor { int64 ShiftX; int64 ShiftY; };
            TArray<FLocalAnchor> LocalAnchors;
            for (const FNewLink& Link : *InLinks)
            {
                const bool bIncoming = Link.FromFormIndex == INDEX_NONE && IsNew(Link.ToFormIndex);
                const bool bOutgoing = IsNew(Link.FromFormIndex) && Link.ToFormIndex == INDEX_NONE;
                if (!bIncoming && !bOutgoing) continue;
                const int32 Form = bIncoming ? Link.ToFormIndex : Link.FromFormIndex;
                const FString& Key = bIncoming ? Link.FromNodeKey : Link.ToNodeKey;
                for (const UEdGraphNode* Node : InGraph->Nodes)
                    if (Node && TurboMCPNodeId::MakeForNode(Node, true) == Key)
                    {
                        const int64 Width = Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth;
                        const int64 IdealX = bIncoming
                            ? int64(Node->NodePosX) + Width + PlacementHorizontalGap - X[Form]
                            : int64(Node->NodePosX) - PlacementHorizontalGap - CellWidth - X[Form];
                        LocalAnchors.Add({IdealX, int64(Node->NodePosY) - Y[Form]});
                        break;
                    }
            }
            bool bFoundLocal = false;
            double BestLocalScore = TNumericLimits<double>::Max();
            int64 ShiftY = 0;
            int64 LocalShiftX = 0;
            // Four footprint-sized rows on either side bound the search to the
            // attachment's neighbourhood. Failure asks for space, not a remote placement.
            if (!bPlaceOutside)
            for (const FLocalAnchor& Anchor : LocalAnchors)
                for (int32 Column = -1; Column <= 1; ++Column)
                    for (int32 Row = -4; Row <= 4; ++Row)
                    {
                        const int64 DX = Anchor.ShiftX + int64(Column) * (CellWidth + PlacementHorizontalGap);
                        const int64 DY = Anchor.ShiftY + int64(Row) * CellStride;
                        bool bClear = true;
                        for (int32 I : Order)
                        {
                            if (!FitsCoordinate(X[I] + DX) || !FitsCoordinate(X[I] + DX + CellWidth)
                                || !FitsCoordinate(Y[I] + DY) || !FitsCoordinate(Y[I] + DY + CellHeight))
                            { bClear = false; break; }
                            for (const UEdGraphNode* Node : InGraph->Nodes)
                            {
                                if (!Node) continue;
                                const int64 W = Node->NodeWidth > 0 ? Node->NodeWidth : PlacementWidth;
                                const int64 H = Node->NodeHeight > 0 ? Node->NodeHeight : PlacementHeight;
                                if (X[I] + DX < int64(Node->NodePosX) + W + PlacementHorizontalGap
                                    && X[I] + DX + CellWidth + PlacementHorizontalGap > Node->NodePosX
                                    && Y[I] + DY < int64(Node->NodePosY) + H + PlacementVerticalGap
                                    && Y[I] + DY + CellHeight + PlacementVerticalGap > Node->NodePosY)
                                { bClear = false; break; }
                            }
                            if (bClear && !ClearsReserved(X[I] + DX, Y[I] + DY, CellWidth, CellHeight)) bClear = false;
                            if (!bClear) break;
                        }
                        if (!bClear) continue;
                        double Score = 0.0;
                        for (const FLocalAnchor& Endpoint : LocalAnchors)
                            Score += FMath::Abs(double(DX - Endpoint.ShiftX)) + 2.0 * FMath::Abs(double(DY - Endpoint.ShiftY));
                        for (const FFixedExecBoundary& Boundary : Boundaries)
                            if (Boundary.bIncoming ? DX < Boundary.IdealShift - PlacementHorizontalGap
                                : DX > Boundary.IdealShift + PlacementHorizontalGap)
                                Score += 4.0 * FMath::Abs(double(DX - Boundary.IdealShift));
                        if (!bFoundLocal || Score < BestLocalScore)
                        { bFoundLocal = true; BestLocalScore = Score; LocalShiftX = DX; ShiftY = DY; }
                    }
            if (bPlaceOutside)
            {
                // Translate the dependency layout as a unit beyond all existing
                // rectangles, including comments. Never move the fixed graph.
                int64 MinX=MAX_int64, MinY=MAX_int64;
                for (int32 I : Order) { MinX=FMath::Min(MinX,X[I]); MinY=FMath::Min(MinY,Y[I]); }
                LocalShiftX=Right+PlacementHorizontalGap-MinX;
                ShiftY=Top-MinY;
                bFoundLocal=true;
            }
            if (!bFoundLocal)
            {
                OutRefusal = TEXT("placement: no collision-free local space was found near the fixed connections. Free space around the target flow or append a smaller connected block; existing nodes were not moved.");
                return false;
            }
            for (int32 I : Order) X[I] += LocalShiftX;
            for (int32 I = 0; I < InNodes.Num(); ++I)
                if (IsNew(I))
                {
                    const int64 FinalY = Y[I] + ShiftY;
                    if (!FitsCoordinate(X[I]) || !FitsCoordinate(X[I] + CellWidth)
                        || !FitsCoordinate(FinalY) || !FitsCoordinate(FinalY + CellHeight))
                    { OutPositions.Reset(); OutRefusal = TEXT("attachment lane exceeds graph coordinate range"); return false; }
                    FPlannedPosition Position;
                    Position.FormIndex = I; Position.X = static_cast<int32>(X[I]); Position.Y = static_cast<int32>(FinalY);
                    Position.bEstimatedSize = InNodes[I].MeasuredWidth <= 0 || InNodes[I].MeasuredHeight <= 0; OutPositions.Add(Position);
                }
            return true;
        }
    }

    if (Reserved)
    {
        OutRefusal = TEXT("per-anchor placement: this component has no supported execution-source or pure-data-consumer anchor. No outside fallback was applied.");
        return false;
    }

	int64 ColumnX = 0;
	int64 ColumnTop = 0;
	if (bHasExistingBounds)
	{
		if (!AddWide(Right, PlacementHorizontalGap, ColumnX))
		{
			OutRefusal = TEXT("placement: the column start cannot be represented in 64 bits");
			return false;
		}
		ColumnTop = Top;
	}

	if (!FitsCoordinate(ColumnX))
	{
		OutRefusal = FString::Printf(
			TEXT("placement: the column would start at x=%lld, outside the 32-bit node coordinate range"),
			ColumnX);
		return false;
	}

	// THE POSITION AND THE ESTIMATED FOOTPRINT MUST BOTH FIT. A top-left inside int32 does not mean the
	// reserved rectangle does, and a downstream int32 edge addition would otherwise overflow.
	const int64 ColumnRight = ColumnX + CellWidth;
	if (!FitsCoordinate(ColumnRight))
	{
		OutRefusal = FString::Printf(
			TEXT("placement: the column at x=%lld would place the reserved %lld-wide footprint past the ")
			TEXT("32-bit node coordinate range"),
			ColumnX, CellWidth);
		return false;
	}

	int64 Offset = 0;
	for (int32 Index = 0; Index < InNodes.Num(); ++Index)
	{
		const FNewNode& Form = InNodes[Index];
		if (Form.bLinksToExisting)
		{
			continue;
		}

		int64 Y = 0;
		if (!AddWide(ColumnTop, Offset, Y))
		{
			OutPositions.Reset();
			OutRefusal = FString::Printf(
				TEXT("placement: form at line %d cannot be placed because the vertical offset is too large"),
				Form.Line);
			return false;
		}
		if (!FitsCoordinate(Y))
		{
			OutPositions.Reset();
			OutRefusal = FString::Printf(
				TEXT("placement: the form at line %d would start at y=%lld, outside the 32-bit node ")
				TEXT("coordinate range"),
				Form.Line, Y);
			return false;
		}

		const int64 FootprintBottom = Y + CellHeight;
		if (!FitsCoordinate(FootprintBottom))
		{
			OutPositions.Reset();
			OutRefusal = FString::Printf(
				TEXT("placement: the form at line %d at y=%lld would place the reserved %lld-high ")
				TEXT("footprint past the 32-bit node coordinate range"),
				Form.Line, Y, CellHeight);
			return false;
		}

		FPlannedPosition Position;
		Position.FormIndex = Index;
		Position.X = static_cast<int32>(ColumnX);
		Position.Y = static_cast<int32>(Y);
		Position.bEstimatedSize = Form.MeasuredWidth <= 0 || Form.MeasuredHeight <= 0;
		OutPositions.Add(Position);

		// The count is bounded by the text size, so this addition cannot leave int64.
		Offset += CellStride;
	}

	return true;
}
