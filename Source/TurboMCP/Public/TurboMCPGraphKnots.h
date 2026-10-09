// Copyright FloStyle. All Rights Reserved.
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
 * Reroute knots in front of `P-GRAPH-KNOTS` and `E-GRAPH-KNOTS`.
 *
 * THE CONTRACT IS NARROWER THAN THE PROSE, AND THAT IS THE DESIGN CONSTRAINT. `06_ACCEPTANCE.md`
 * states T-LAY-10 as:
 *
 *     Preview lists only wires that cross nodes; after Edit those wires are straight
 *
 * `03_TOOLS.md` adds qualifiers, and each one is a REFUSAL or a PLACEMENT RULE rather than a wish:
 * a knot is placed only where the route through it is ONE RUN PLUS ONE BEND, wires between
 * NEIGHBOURING COLUMNS are skipped, the position is LEVEL WITH THE SOURCE OR THE TARGET PIN, it must
 * be in FREE SPACE, two knots are never stacked, and one pin feeding several distant nodes SHARES a
 * single knot.
 *
 * THE RULES, AND THE CONSTANTS THEY USE. Every constant below is a CHOICE made by this capability,
 * because the engine and the plugin have no such notion, and each is named so it can be argued with.
 *
 *  - C1 A wire CROSSES a node when the closed segment between its two computed pin positions meets
 *    the OPEN rectangle of that node. Open means a wire touching an edge exactly does NOT count,
 *    for the same reason two nodes sharing a border are not overlapping.
 *  - C2 The unit reported and fixed is the WIRE, once, naming every node it crosses. Reporting once
 *    per crossing would list one wire three times and make the before and after counts incomparable.
 *  - C3 A knot sits at one of exactly TWO positions: level with the target pin at
 *    `(TargetX, SourceY)`, or level with the source pin at `(SourceX, TargetY)`. Anywhere else makes
 *    a route with two bends. NOTE that this clause does NOT mean "fewer bends than before": before
 *    the knot the wire has ZERO bends, and the clause exists to stop the tool introducing a THIRD
 *    segment, not to minimise a number that starts at zero.
 *  - C4 A node COLUMN is its `NodePosX`, two nodes share a column within `ColumnBand`, and two
 *    columns are NEIGHBOURING when the horizontal gap between their rectangles is within
 *    `NeighbourGap`. A wire between neighbouring columns is refused, because a knot's own footprint
 *    is a large fraction of such a distance and the route had no room to bend badly.
 *  - C5 FREE SPACE: the candidate position plus the knot's own footprint must not meet any other
 *    node's rectangle, any comment box, or any existing knot's footprint inflated by
 *    `StackClearance`. Using the FOOTPRINT rather than the centre is what makes "never stack two
 *    knots" a consequence of the same test instead of a second rule.
 *  - C6 Wires are grouped by their SOURCE PIN, compared by pin identity and not by owning node,
 *    because two output pins of one node are two sources. A group of one gets its own knot; a group
 *    of several gets ONE knot at the candidate acceptable to every member. A group with no such
 *    candidate is refused sharing and falls back to one knot per wire, and the refusal is REPORTED.
 *  - C7 At most ONE knot per wire. A second knot would add a second bend, so a wire that still
 *    crosses a node after its single knot is REPORTED as not fixed rather than fixed twice. This
 *    also stops an implementation looping until clean on a graph where the route cannot avoid the
 *    obstacle.
 *
 * IT MEASURES AND ROLLS BACK. `08_ROADMAP.md` names this capability as one of the three that do.
 * `Plan` counts the crossings; `Apply` performs the pass, RE-MEASURES the same way, and when the
 * result is not better - `after >= before`, because leaving the count unchanged has added knots for
 * nothing - it undoes EVERYTHING, then re-measures to CONFIRM the undo restored the original count.
 * A rollback whose result is not checked is a rollback nobody knows works.
 *
 * WHY THAT RISK IS ACCEPTABLE: `UK2Node_Knot::IsCompilerRelevant` returns false and `ExpandNode`
 * collapses the knot, so a knot CANNOT change what a Blueprint does. A failed rollback therefore
 * leaves a cosmetic mess and not a behavioural one.
 *
 * `P/E-` shares ONE code path up to the write (`D-02`), and EVERY refusal is decided in `Plan` so
 * the Preview states it rather than the Edit surprising a caller who had already agreed.
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
