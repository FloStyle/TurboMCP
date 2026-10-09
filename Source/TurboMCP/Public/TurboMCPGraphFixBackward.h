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
 * Repair the bad spots in a graph that is otherwise fine, in front of `P-GRAPH-FIX-BACKWARD` and
 * `E-GRAPH-FIX-BACKWARD`.
 *
 * THREE RULES, AND IT REPAIRS ONLY WHAT ONE OF THEM COVERS. `Spec/03_TOOLS.md` states the capability as
 * three sentences, and every clause in them is load-bearing:
 *
 *  - R1 AN EXEC WIRE THAT RUNS BACKWARD. The node it feeds is placed on the row of the node before it,
 *    to its right, together with any PURE nodes used only by it; later nodes in that row are pushed
 *    along. "Backward" is `RectOf(Target).MinX < RectOf(Source).MaxX`, reused VERBATIM from
 *    L-LAYOUT-REPORT (TurboMCPLayoutReport.cpp:281), because that report output is asserted as a
 *    baseline by T-LAY-1 and the two tools have to agree.
 *  - R2 A GETTER PLACED UNDER THE NODE THAT READS IT. It is moved to that node's own row, directly to
 *    its left. The predicate is the bUnderneath comparison of the report (:374) and NOT its whole
 *    `getter-place` finding: a getter to the RIGHT on the same row is not backwards.
 *  - R3 A LONE NODE PARKED AT THE ORIGIN. Within `OriginBand` of `(0,0)` on both axes, with at least one
 *    link and its NEAREST linked node farther than `StrayLinkDistance`, it is brought next to that
 *    neighbour. Both constants are the own constants of the report (:35, :38).
 *
 * A BACKWARD DATA WIRE THAT NO RULE COVERS IS LEFT ALONE AND COUNTED. R1 is about an EXEC wire; R2 and
 * R3 are about specific shapes. Moving a node merely because a data wire points backwards, with no
 * getter and no pure consumer involved, would be the pass guessing. The count is reported so the answer
 * never implies it fixed more than it did - an honest count beats a silent omission.
 *
 * NOTHING IS CREATED, NOTHING IS REMOVED, NO LINK IS MADE OR BROKEN. That is what makes the rollback a
 * PROPERTY rather than a code path: `Plan` carries the planned positions as VALUES, measures the wire
 * length ON those values, and `Apply` writes only when the result is strictly shorter. There is nothing
 * to undo, because nothing was done until the decision was made.
 *
 * AND A COMMENT BOX IS NEVER MOVED, RESIZED OR CREATED - A PROPERTY OF THE TYPE, NOT A PROMISE IN THE
 * CODE. `FBox` below carries a key, its text, its depth, its parent index and its member indices. It
 * carries NO X, NO Y, NO WIDTH AND NO HEIGHT. There is nothing to write a box position from, so a later
 * edit cannot break the promise by accident.
 *
 * IT MEASURES TOTAL WIRE LENGTH, WHICH IS WHAT `P/E-GRAPH-TIDY` IS JUDGED ON. `P/E-GRAPH-KNOTS` is
 * judged on WIRES CROSSING NODES instead. The two numbers are deliberately not comparable, and each
 * pass says which one it printed.
 *
 * `P/E-` SHARES ONE CODE PATH UP TO THE WRITE (`D-02`), and EVERY REFUSAL IS DECIDED IN `Plan` so the
 * Preview states it rather than the Edit surprising a caller who had already agreed.
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

		/**
		 * WHAT THE ENGINE MEASURED, AND NOTHING ELSE. Zero when Slate has never drawn the node.
		 *
		 * KEPT SEPARATE FROM `Width`/`Height`, AND THAT SEPARATION IS A MEASURED FIX - the same one
		 * `FTurboMCPGraphTidy` made independently, which is a good sign it is the right one.
		 *
		 * `L-LAYOUT-REPORT` reads `NodeWidth` RAW. This pass substitutes a documented 48-unit fallback
		 * for a node the engine never measured, because `NodeWidth` is written only when Slate DRAWS a
		 * node and a commandlet draws nothing. So if the fallback is stored in the field the MEASURE
		 * reads, the two tools describe DIFFERENT RECTANGLES and their numbers cannot agree.
		 *
		 * The same fault made THIS pass compare a fallback-measured before against a raw-measured after,
		 * so on a graph where nothing needed repairing it believed it had improved things, answered
		 * `changed:`, and would have REWRITTEN A GOOD GRAPH.
		 *
		 * THE RULE: a pass may compare only LIKE WITH LIKE. These two fields are what the graph IS;
		 * `Width` and `Height` are what this pass will MAKE OF IT.
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

		/** `backward-data` wires no rule covers, left alone on purpose. */
		TArray<FString> UncoveredDataWires;

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
