// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/Vector2D.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * The layout analysis behind `L-LAYOUT-REPORT`.
 *
 * Pure measurement: it reads a graph and computes facts about where its nodes and
 * wires are. It never moves anything, which is what `P3` requires of a Look tool.
 *
 * WHY PIN POSITIONS ARE COMPUTED RATHER THAN READ
 * -----------------------------------------------
 * `04_FORMATS.md` and `03_TOOLS.md` call total wire length "the main layout
 * measure", and a wire's length is the distance between the two pins it joins.
 * The engine has no pin-position API on `UEdGraphNode`: the only one is
 * `SGraphPanel::GetBoundsForNode`, which needs a live graph widget and therefore
 * cannot be used from a tool call.
 *
 * So pin positions are computed from the node's rectangle and its pin order, and
 * this is stated in the result rather than hidden. The measure is consistent and
 * comparable, which is what a layout measure is for: a tidy graph scores lower than
 * a tangled one of the same size. It is not pixel exact.
 */
class FTurboMCPLayoutReport
{
public:
	/** A node's rectangle in graph coordinates. */
	struct FRect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;

		double Width() const { return MaxX - MinX; }
		double Height() const { return MaxY - MinY; }
	};

	/** One thing worth telling the caller about. */
	struct FFinding
	{
		/** `backward-exec`, `backward-data`, `overlap`, `getter-place`, `stray`, `disconnected` */
		FString Kind;

		/** One line, ready to print. */
		FString Detail;

		/** The node IDs this finding is about. */
		TArray<FString> NodeIds;
	};

	/** Everything measured about one graph. */
	struct FReport
	{
		int32 NodeCount = 0;
		int32 WireCount = 0;
		int32 WireCrossings = 0;
		bool bCrossingsCapped = false;
		int32 CommentBoxCount = 0;
		int32 NestedBoxCount = 0;

		/** The graph's bounding rectangle, or a zero rectangle when there are no nodes. */
		FRect Extent;

		/** The sum of straight distances between linked pins. */
		double TotalWireLength = 0.0;

		/** The longest single wire, so a caller can see what dominates. */
		double LongestWire = 0.0;

		TArray<FFinding> Findings;

		/** True when the graph has no wires at all. */
		bool bHasNoWires = false;

		/** Said in words when there is nothing to report, per `P1`. */
		FString Summary;
	};

	/** Measures one graph. Never modifies it. */
	static FReport Measure(const UEdGraph* InGraph, const FString& InAssetPath, double AlignmentTolerance = 8.0);

	/** The rectangle a node occupies. */
	static FRect RectOf(const UEdGraphNode* InNode);

	/**
	 * The computed position of a pin, in graph coordinates.
	 *
	 * Inputs sit on the node's left edge and outputs on its right, spread down the
	 * node in pin order. This is a model of where the editor draws them, not a read
	 * of it; see the note at the top of this file.
	 */
	static FVector2D PinPosition(const UEdGraphPin* InPin);

	/** The distance between two linked pins. */
	static double WireLength(const UEdGraphPin* InPin);
};
