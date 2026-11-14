// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Math/Vector2D.h"

class UEdGraph;
class UEdGraphPin;
class UEdGraphNode;
class UEdGraphNode_Comment;
class UK2Node_Knot;
class UK2Node_VariableGet;
class UBlueprint;

/**
 * Plan, preview and apply graph layout without compiling or saving the asset.
 * Preview and edit share the planner; Apply also checks post-write geometry and
 * restores rejected changes. A preview is not a guarantee against a later refusal.
 * Rows mode and scoped layouts preserve comment corners. Whole-graph blocks
 * mode plans connected nodes together and fits comments to their original
 * members, retaining titles and nesting. Empty comments remain fixed notes.
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

		/** Compact execution/input band identity; fixed or unassigned nodes use INDEX_NONE. */
        int32 BlockIndex = INDEX_NONE;
		bool bIsComment = false;
		bool bIsKnot = false;
		bool bIsPure = false;
		bool bIsGetter = false;

		/** True when no entry point can reach it. Excluded from the layout, never moved. */
		bool bUnreachable = false;

	};

    /** A semantic execution block; spatial rows may contain several blocks. */
    struct FBlock
    {
        FString Kind;
        FString Handoff;
        int32 ParentIndex = INDEX_NONE;
        int32 LoopContext = INDEX_NONE;
    };

	/** One comment box, with its nesting parent resolved GEOMETRICALLY. */
	struct FBox
	{
		FString Key;
		FString Text;
		UEdGraphNode_Comment* Comment = nullptr;
        bool bGenerated = false;

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

		/** Planned rectangle. Whole-graph blocks mode may move the corner; rows keeps it. */
        double PlannedX = 0.0;
        double PlannedY = 0.0;
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

        /** Stock loop Break/DoOnce Reset control feedback: preserved, not a forward layout dependency. */
        bool bControlFeedback = false;

		/** Index into `FPlan::Nodes`, so a planned position can be looked up. */
		int32 SourceNodeIndex = INDEX_NONE;
		int32 TargetNodeIndex = INDEX_NONE;
	};

	/** A variable getter or self reference copied for one consumer, with what is needed to undo it. */
	struct FGetterCopy
	{
		/** Made by `Apply`, and null until then: `Plan` records WHERE a copy would go, not the copy. */
		UEdGraphNode* Copy = nullptr;

		/** The original getter this was copied from. */
		UEdGraphNode* Original = nullptr;

		/** The consumer's input pin that was moved off the original and onto the copy. */
		UEdGraphPin* ConsumerPin = nullptr;

		/** The original's value pin, kept so a rollback can link the consumer back. */
		UEdGraphPin* OriginalValuePin = nullptr;

		/** Where the copy goes, decided in `Plan` so the Preview can describe it without creating it. */
		double PlannedX = 0.0;
		double PlannedY = 0.0;
        /** Measured reader outline and output-pin offset, independent of the source entry. */
        double Width = 0.0;
        double Height = 0.0;
        FVector2D OutputOffset = FVector2D::ZeroVector;

	};

	/** A knot placed before the measure, with what is needed to undo it. */
	struct FKnotAdded
	{
		UK2Node_Knot* Knot = nullptr;

		/** The two pins the wire joined BEFORE the knot was inserted. */
		UEdGraphPin* UpstreamPin = nullptr;
		UEdGraphPin* DownstreamPin = nullptr;
	};

    /** A planned replacement for one existing wire. No live knot is created by planning. */
    struct FWireRoute
    {
        int32 WireIndex = INDEX_NONE;
        /** Top-left positions; measured input/output offsets define each segment. */
        TArray<FVector2D> KnotPositions;
        /** Filled only in the local post-edit plan used for actual geometry checks. */
        TArray<UK2Node_Knot*> AppliedKnots;
        FVector2D KnotSize = FVector2D::ZeroVector;
        FVector2D InputOffset = FVector2D::ZeroVector;
        FVector2D OutputOffset = FVector2D::ZeroVector;
        /** Human-readable rule or an explicit reason the wire was not routed. */
        FString Reason;
        bool bSkipped = false;
    };

	/** Everything the Preview and the Edit both need, computed once in `Plan`. */
	struct FPlan
	{
        /** Explicit row breaks after these execution node indices. */
        TSet<int32> WrapAfterNodes;
		FString Asset;
		FString GraphName;
		bool bForce = false;
        /** Explicit compact control-flow placement; omitted mode preserves row behavior. */
        bool bCompactBlocks = false;
        int32 BlockCount = 0;
        TArray<FBlock> Blocks;
        /** Primary incoming source for a compact merge; other exits route back to this row. */
        TMap<int32,int32> MergePredecessors;

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
        /** Explicit orthogonal route candidates shared by preview and apply. */
        TArray<FWireRoute> WireRoutes;

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
		bool bInMergeGetters, const TArray<FString>& InNodeIds, FPlan& OutPlan, FString& OutError, const FString& MainPath = FString(), double InHorizontalGap = 120.0, double InVerticalGap = 100.0, double InLanePitch = 280.0, const FString& WrapAfter = FString(), const FString& LayoutMode = TEXT("rows"), const FString& RouteWires = TEXT("off"), double ChannelWidth = 60.0, bool bCreateBoxes = false);

    /** Acquire native node/pin geometry when a graphical editor is available. */
    static bool PrepareAlignment(UBlueprint* Blueprint, UEdGraph* Graph, FString& Error);
    static FVector2D AlignmentPinPosition(const UEdGraphPin* Pin);
    static FVector2D AlignmentNodeSize(const UEdGraphNode* Node);

	/** What the pass would do, and both numbers, without touching the graph. */
	static FString RenderPreview(const FPlan& InPlan, int32 GeometryOffset = 0, int32 GeometryLimit = 0, int32 PlanOffset = 0, int32 PlanLimit = 0);

    /** Refuse the first moved non-comment pair with intersecting planned rectangles. Unmoved overlaps are preserved. Does not mutate the plan or graph. */
    static bool ValidatePlannedNodeOverlaps(const FPlan& InPlan, FString& OutError);

    /** Build bounded orthogonal route candidates from measured knot geometry.
     * This only fills WireRoutes; it does not change live links or authorize an edit.
     * Preview/apply integration must consume exactly these positions before enabling it.
     */
    /** Measure a transient knot, restore source geometry, then plan routes. No live edits. */
    static bool PrepareOrthogonalRoutes(FPlan& Plan, double Clearance, FString& Error);

    static bool PlanOrthogonalRoutes(FPlan& Plan, const FVector2D& KnotSize,
        const FVector2D& InputOffset, const FVector2D& OutputOffset, double Clearance, FString& Error);

	/** Moves the nodes, fits the boxes, and rolls back what it created when the result is not better. */
	static FString Apply(const FPlan& InPlan);
};
