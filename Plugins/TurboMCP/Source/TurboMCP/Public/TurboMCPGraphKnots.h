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

/**
 * Plan and insert reroute knots for estimated node crossings in a named Blueprint graph.
 * Preview does not edit the graph. Edit replans and uses an undo transaction.
 * Geometry estimates are not a guarantee about rendered splines or runtime behavior.
 * Non-improving edits attempt restoration, but the result must be inspected before saving.
 * No automatic compilation or saving is performed.
 */
class FTurboMCPGraphKnots
{
public:
	/** One wire that crosses at least one node, with the knot that would fix it. */
	struct FWire
	{
		/** The source pin's owning node, in node-key form. */
		FString SourceId;

		/** The target pin's owning node, in node-key form. */
		FString TargetId;

		/** The source pin's name, so a PIN is identified and not merely a node. */
		FString SourcePin;

		/** The target pin's name. */
		FString TargetPin;

		/** The node keys the wire crosses, sorted, so a report is stable. */
		TArray<FString> CrossedIds;

		/** True when the wire was refused before a position was computed. */
		bool bRefused = false;

		/** Why, when it was. Empty otherwise. */
		FString RefusalReason;

		/** The position the knot would take. Valid only when `bRefused` is false. */
		double KnotX = 0.0;
		double KnotY = 0.0;

		/** True when this wire shares its knot with another wire from the same source pin. */
		bool bShared = false;

		/**
		 * Which of the two allowed positions the knot takes (`C3`).
		 *
		 * RECORDED RATHER THAN DERIVED IN THE REPORT. The position is one of exactly two, and the
		 * recogniser computes both, so it knows which one it chose. Working it out again from the
		 * coordinates would be a second implementation of the same rule, and this project has been
		 * bitten by two places that agree until one is changed.
		 */
		bool bLevelWithTarget = false;

		/** True when the wire already carried a knot, so it is left alone (`C7`). */
		bool bAlreadyKnotted = false;
	};

	/** The parsed and checked request. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;

		/** Every wire that crosses a node, accepted or refused. */
		TArray<FWire> Wires;

		/** The indices into `Wires` that `Apply` would place a knot for. */
		TArray<int32> AcceptedIndices;

		int32 NodeCountBefore = 0;
		int32 KnotCountBefore = 0;

		/** The count of (wire, crossed node) pairs. What `Apply` re-measures. */
		int32 CrossingsBefore = 0;

		/** The count of wires that cross something. The number T-LAY-10 asks the Preview to list. */
		int32 CrossingWiresBefore = 0;

		/** True when the knot footprint could not be measured, so nothing may be placed. */
		bool bFootprintUnknown = false;
	};

	/** Reads the graph, finds the crossing wires, and decides where a knot would go for each. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		FPlan& OutPlan, FString& OutError);

	/** Lists only the wires that cross a node, with the position and the refusals. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Places the knots, re-measures, and rolls the whole pass back when it is not better. */
	static FString Apply(const FPlan& InPlan);
};
