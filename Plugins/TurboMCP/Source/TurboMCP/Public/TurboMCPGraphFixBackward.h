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

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/** Plans bounded position repairs without changing graph logic.
 * R1 repairs supported backward execution patterns and carries up to eight
 * exclusive pure producers atomically. R2 repositions eligible getters; R3
 * brings linked origin strays closer to their neighbours. Comment bounds stay
 * fixed. Planning requires measured node bounds and unique node identifiers.
 * The improvement metric is estimated straight-segment wire length, not a
 * rendered-spline or whole-graph readability guarantee. Force bypasses only
 * that improvement requirement. Apply records changed nodes for editor undo.
 */
class FTurboMCPGraphFixBackward
{
public:
	/** One flow node, read once. The pass works on these VALUES, never on live pointers. */
	struct FNode
	{
		/** `TurboMCPNodeId::MakeKey(NodeGuid)`, six lowercase hex characters. */
		FString Key;

		FString ClassName;

		/** The position and size AS READ, which the report's measure would use. */
		double X = 0.0;
		double Y = 0.0;

		/**
		 * The size this pass USES, which is the measured rectangle or the documented fallback.
		 *
		 * THIS IS THE PLANNED EXTENT. The spacer, the overlap guard and the collision checks read it, and
		 * it is deliberately NOT what the before-measurement reads: see `MeasuredWidth`.
		 */
		double Width = 0.0;
		double Height = 0.0;

        /** Stored dimensions retained for the legacy report-compatible wire metric;
         * these are not the transient widget measurements used for collision guards.
         */
		double MeasuredWidth = 0.0;
		double MeasuredHeight = 0.0;

		/** The position this pass would WRITE. Equal to `X`/`Y` when the node is not moved. */
		double PlannedX = 0.0;
		double PlannedY = 0.0;

		/** True when the size above is a NAMED FALLBACK and not a measured rectangle. */
		bool bSizeFallback = false;

		bool bIsComment = false;
		bool bIsKnot = false;
		bool bIsPure = false;
		bool bIsGetter = false;
		bool bUnreachable = false;

		/** The innermost box that contains this node, or `INDEX_NONE`. */
		int32 BoxIndex = INDEX_NONE;
	};

	/**
	 * One comment box, with its nesting parent resolved.
	 *
	 * THERE IS NO POSITION FIELD HERE, AND THAT IS THE POINT. The spec's hardest promise - a box is
	 * never moved - is a property of this type rather than a rule the code has to remember.
	 */
	struct FBox
	{
		FString Key;
		FString Text;
		int32 Depth = 0;

		/** The TIGHTEST box around this one, or `INDEX_NONE`. Resolved geometrically. */
		int32 ParentIndex = INDEX_NONE;

		/** Indices into `Nodes` of the non-comment nodes geometrically inside this box. */
		TArray<int32> MemberIndices;
	};

	/** One planned move, BY KEY rather than by pointer. */
	struct FMove
	{
		/** Which rule asked for it, as a stable identifier: `R1`, `R1-push`, `R1-pure`, `R2`, `R3`. */
		FString Rule;

		FString Key;
		FString ClassName;

		double FromX = 0.0;
		double FromY = 0.0;
		double ToX = 0.0;
		double ToY = 0.0;

		/** True when this move exists only to clear the way for an earlier one. */
		bool bPushed = false;

		/** In words, what asked for it. */
		FString Reason;
	};

	/** The parsed and checked request, with the whole pass computed against VALUES. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;
		bool bForce = false;

		TArray<FNode> Nodes;
		TArray<FBox> Boxes;
		TArray<FMove> Moves;

		/** Why a particular node was not moved, so the report can say rather than omit. */
		TArray<FString> Refusals;

		/** `backward-data` wires no rule covers, left alone on purpose. CAPPED, so it can be shorter
		 *  than the count below and the report has to say so. */
		TArray<FString> UncoveredDataWires;

		/** How many such wires were found, INCLUDING the ones the cap kept out of the list above.
		 *  `UncoveredDataWires.Num()` is the listed count, which is a different number. */
		int32 UncoveredDataWiresTotal = 0;

		/** Nodes no entry point can reach. Left alone; `P/E-GRAPH-DEAD` owns them. */
		TArray<FString> UnreachableKeys;

		/** The measurement, taken on the planned positions BEFORE anything is written. */
		double WireLengthBefore = 0.0;
		double WireLengthAfter = 0.0;

		int32 NodesMoved = 0;
		int32 PushedMoved = 0;
		int32 PureMoved = 0;
		int32 GettersMoved = 0;
		int32 StraysMoved = 0;
		int32 BackwardExecFound = 0;
		int32 BackwardGettersFound = 0;
		int32 StraysFound = 0;

		/** The longest push chain, so the cascade bound is reported rather than trusted. */
		int32 MaxPushDepth = 0;

		/** True when some node's size had to be replaced by the named fallback. */
		bool bUsedFallbackSizes = false;

		/**
		 * The roll-back decision, computed in `Plan` so the PREVIEW can state it.
		 *
		 * `After < Before`, or `bForce`. `>=` would apply a pass that moved nodes for no gain.
		 */
		bool bWouldApply = false;

		/** Why it would not, when it would not. */
		FString RollbackReason;

		/** True when the plan is identical to the graph as read, so there is nothing to write. */
		bool bAlreadyCorrect = false;
	};

	/** Reads the graph, finds the bad spots, and computes the whole pass against values. */
	static bool Plan(const FString& InAsset, const FString& InGraphName, bool bInForce,
		FPlan& OutPlan, FString& OutError);

	/** Describes the pass, the numbers and every refusal. Writes nothing. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Writes the planned positions when the measure says the result is better. */
	static FString Apply(const FPlan& InPlan);
};
