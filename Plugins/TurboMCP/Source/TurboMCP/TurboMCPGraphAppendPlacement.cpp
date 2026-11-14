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
#include "Math/NumericLimits.h"
#include "TurboMCPNodeId.h"

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
