// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/Vector2D.h"

class UEdGraph;
class UEdGraphPin;
class UEdGraphNode;
class UEdGraphNode_Comment;
class UK2Node_Knot;
class UK2Node_VariableGet;
class UBlueprint;

/**
 * `P/E-GRAPH-TIDY`: lay out one graph, inside the comment boxes the user made.
 *
 * FILE SHAPE. This is a plain `class` with `static` members rather than a `UCLASS`, exactly like
 * `FTurboMCPComment` and `FTurboMCPGraphDead`. There is no engine reason to register state, and the
 * tools in `TurboMCPGraphTidyTools.cpp` call `Plan` then either `RenderPreview` or `Apply`.
 *
 * THE PREVIEW AND THE EDIT SHARE ONE CODE PATH UP TO THE WRITE (`D-02`). Every refusal is decided in
 * `Plan`, so a Preview cannot offer a plan the Edit then declines - a disagreement this project has
 * been caught by twice.
 *
 * THE ORDER OF OPERATIONS IS THE PART MOST LIKELY TO BE WRONG. Read `Plan`'s body comment before
 * changing anything in it: the pass is INSIDE-OUT, the innermost boxes are arranged and fitted first,
 * and an outer box then treats each child box as ONE BLOCK. Fitting an outer box before its children
 * were arranged would compute its rectangle from stale child sizes, and the second pass would then have
 * to move the outer box - which the spec forbids.
 *
 * NEVER MOVES A USER'S BOX AS A WHOLE. A box's TOP-LEFT CORNER is preserved and only `NodeWidth` and
 * `NodeHeight` change, which is what `P/E-COMMENT`'s `fit` already does. That is the checkable form of
 * the rule, and the suite asserts it.
 */
class FTurboMCPGraphTidy
{
public:
	/**
	 * Reused rather than redefined, so tidy and `P/E-COMMENT` cannot disagree about what a box holds.
	 *
	 * `FTurboMCPComment::MembersOf` is the ONE copy of the geometric membership rule in this plugin,
	 * promoted out of an anonymous namespace for exactly this reason, and the constants below are the
	 * same ones `P/E-COMMENT` fits with. A second copy of a geometric rule agrees with the first until
	 * one of them is changed.
	 */
	static constexpr double FitPadding = 40.0;
	static constexpr double TitleBarHeight = 42.0;

	/** Larger than `FitPadding`, so a fitted box's edge does not sit on a neighbouring node. */
	static constexpr double HorizontalNodeGap = 60.0;

	/** Larger than `TitleBarHeight`, so a stacked row does not fall under the box above its title. */
	static constexpr double VerticalNodeGap = 80.0;

	/** The clearance between two sibling boxes, set equal to `FitPadding` so a box's margin is not doubled. */
	static constexpr double BoxGap = 40.0;

	/** One node, read once, with the position tidy plans for it kept as VALUES. */
	struct FNode
	{
		/** Stable identity, independent of array order. */
		FString Key;

		/** The live node, so `Apply` writes to the object `Plan` read. */
		UEdGraphNode* Node = nullptr;

		FString ClassName;

		/** The position the graph is in now, and the rectangle read from it. */
		double X = 0.0;
		double Y = 0.0;
		double Width = 0.0;
		double Height = 0.0;

		/** The rectangle tidy PLANS. Equals the current one until a pass writes to it. */
		double PlannedX = 0.0;
		double PlannedY = 0.0;

		/** The length of the longest chain of links leading to this node. See `ColumnOf`. */
		int32 Column = 0;

		/** Its place in `UEdGraph::Nodes`, the final tie-break so a re-run changes nothing. */
		int32 GraphOrder = 0;

		bool bIsComment = false;
		bool bIsKnot = false;
		bool bIsPure = false;
		bool bIsGetter = false;

		/** True when no entry point can reach it. Excluded from the layout, never moved. */
		bool bUnreachable = false;

	};

	/** One comment box, with its nesting parent resolved GEOMETRICALLY. */
	struct FBox
	{
		FString Key;
		FString Text;
		UEdGraphNode_Comment* Comment = nullptr;

		/** The TIGHTEST box around this one, or `INDEX_NONE`. Resolved geometrically. */
		int32 ParentIndex = INDEX_NONE;

		/** Directly nested boxes, by index into `FPlan::Boxes`. */
		TArray<int32> ChildBoxIndices;

		/** Depth in the resolved parent chain. 0 for an outermost box. */
		int32 Depth = 0;

		/** Keys of the NON-COMMENT nodes geometrically inside, from `MembersOf`. */
		TArray<FString> MemberKeys;

		/** The rectangle used for membership and for fitting. */
		double X = 0.0;
		double Y = 0.0;
		double Width = 0.0;
		double Height = 0.0;

		/** The size `Plan` computed for this box. Its CORNER is never changed. */
		double PlannedWidth = 0.0;
		double PlannedHeight = 0.0;

		/** True when this box is fitted because it was reached, false when left alone. */
		bool bWouldFit = false;
	};

	/** One wire, by the two pins it joins, so the measure can be computed on planned positions. */
	struct FWire
	{
		UEdGraphPin* OutputPin = nullptr;
		UEdGraphPin* InputPin = nullptr;

		/** Index into `FPlan::Nodes`, so a planned position can be looked up. */
		int32 SourceNodeIndex = INDEX_NONE;
		int32 TargetNodeIndex = INDEX_NONE;
	};

	/** A variable getter copied for one consumer, with what is needed to undo it. */
	struct FGetterCopy
	{
		/** Made by `Apply`, and null until then: `Plan` records WHERE a copy would go, not the copy. */
		UK2Node_VariableGet* Copy = nullptr;

		/** The original getter this was copied from. */
		UK2Node_VariableGet* Original = nullptr;

		/** The consumer's input pin that was moved off the original and onto the copy. */
		UEdGraphPin* ConsumerPin = nullptr;

		/** The original's value pin, kept so a rollback can link the consumer back. */
		UEdGraphPin* OriginalValuePin = nullptr;

		/** Where the copy goes, decided in `Plan` so the Preview can describe it without creating it. */
		double PlannedX = 0.0;
		double PlannedY = 0.0;
	};

	/** A knot placed before the measure, with what is needed to undo it. */
	struct FKnotAdded
	{
		UK2Node_Knot* Knot = nullptr;

		/** The two pins the wire joined BEFORE the knot was inserted. */
		UEdGraphPin* UpstreamPin = nullptr;
		UEdGraphPin* DownstreamPin = nullptr;
	};

	/** Everything the Preview and the Edit both need, computed once in `Plan`. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;
		bool bForce = false;

		/** Empty means the whole graph; otherwise only these nodes may move. */
		TSet<FString> SelectedNodeKeys;

		/** Every node outside a scoped selection is an anchor and remains fixed. */
		int32 AnchorCount = 0;

		TArray<FNode> Nodes;
		TArray<FBox> Boxes;
		TArray<FWire> Wires;

		/** The measure. `Before` is read from the graph, `After` from the PLANNED positions. */
		double WireLengthBefore = 0.0;
		double WireLengthAfter = 0.0;

		/** How many pins the before-measure summed, so a caller can see the number's basis. */
		int32 WireCount = 0;


		/** Nodes no entry point reaches. Reported, and left exactly where they are. */
		int32 UnreachableCount = 0;

		/** Nodes whose planned position differs from the current one. */
		int32 NodesToMove = 0;

		/** Stable keys of those planned moves, so a scoped preview can name its exact boundary. */
		TArray<FString> NodesToMoveKeys;

		/** Boxes whose planned size differs from the current one. */
		int32 BoxesToFit = 0;

		/** Getter copies and knots, created before the measure because they change the node set. */
		TArray<FGetterCopy> GetterCopies;
		TArray<FKnotAdded> KnotsAdded;

		/** False when the planned layout is not better and `bForce` is not set. */
		bool bWouldApply = false;

		/** Why it would not apply. Empty when it would. */
		FString RollbackReason;

		/** True when the pass had nothing to do, so the Edit must say `unchanged:`. */
		bool bAlreadyTidy = false;

		/** Getter merging was asked for. Recorded so the refusal can name it. */
		bool bMergeGettersRequested = false;
	};

	/**
	 * Reads the graph and works out the whole layout IN MEMORY. Writes nothing.
	 *
	 * `bInMergeGetters` is the spec's optional merging, and it is REFUSED BY NAME rather than silently
	 * ignored: the spec says merging is off by default, and a caller who turns it on is entitled to be
	 * told that this build does not do it rather than to receive a layout that quietly did not merge.
	 *
	 * Returns false with `OutError` set when the graph cannot be tidied. Every refusal opens with
	 * `error:` and names what stopped it.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, bool bInForce,
		bool bInMergeGetters, const TArray<FString>& InNodeIds, FPlan& OutPlan, FString& OutError, const FString& MainPath = FString());

    /** Acquire native node/pin geometry when a graphical editor is available. */
    static bool PrepareAlignment(UBlueprint* Blueprint, UEdGraph* Graph, FString& Error);
    static FVector2D AlignmentPinPosition(const UEdGraphPin* Pin);
    static FVector2D AlignmentNodeSize(const UEdGraphNode* Node);

	/** What the pass would do, and both numbers, without touching the graph. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Moves the nodes, fits the boxes, and rolls back what it created when the result is not better. */
	static FString Apply(const FPlan& InPlan);
};
