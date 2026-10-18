// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphAppend.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Math/NumericLimits.h"

/**
 * Deterministic placement for the forms an append creates.
 *
 * The column is to the right of every existing stored/estimated bound, with the new forms in input order.
 * Stored non-positive sizes are replaced by an explicit 320x160 estimate, and the result is reported as
 * geometry rather than as a measured Slate footprint. Every edge, extent and addition is widened to int64
 * and checked before the int32 cast; a coordinate outside the node coordinate range refuses.
 */
namespace
{
	constexpr int64 PlacementWidth = 320;
	constexpr int64 PlacementHeight = 160;
	constexpr int64 PlacementHorizontalGap = 160;
	constexpr int64 PlacementVerticalGap = 64;
	constexpr int64 PlacementVerticalStride = PlacementHeight + PlacementVerticalGap;

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
	TArray<FPlannedPosition>& OutPositions, bool& bOutUsedEstimatedSize, FString& OutRefusal)
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
		// height is missing, so the column cannot be placed inside a known wide bound.
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
	// 320x160 rectangle does, and a downstream int32 edge addition would otherwise overflow.
	const int64 ColumnRight = ColumnX + PlacementWidth;
	if (!FitsCoordinate(ColumnRight))
	{
		OutRefusal = FString::Printf(
			TEXT("placement: the column at x=%lld would place the estimated %lld-wide footprint past the ")
			TEXT("32-bit node coordinate range"),
			ColumnX, PlacementWidth);
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

		const int64 FootprintBottom = Y + PlacementHeight;
		if (!FitsCoordinate(FootprintBottom))
		{
			OutPositions.Reset();
			OutRefusal = FString::Printf(
				TEXT("placement: the form at line %d at y=%lld would place the estimated %lld-high ")
				TEXT("footprint past the 32-bit node coordinate range"),
				Form.Line, Y, PlacementHeight);
			return false;
		}

		FPlannedPosition Position;
		Position.FormIndex = Index;
		Position.X = static_cast<int32>(ColumnX);
		Position.Y = static_cast<int32>(Y);
		Position.bEstimatedSize = true;
		OutPositions.Add(Position);

		// The count is bounded by the text size, so this addition cannot leave int64.
		Offset += PlacementVerticalStride;
	}

	return true;
}
