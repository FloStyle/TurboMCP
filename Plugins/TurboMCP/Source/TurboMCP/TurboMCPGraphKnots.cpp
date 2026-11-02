// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphKnots.h"

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

namespace
{
	/**
	 * THE CONSTANTS ARE CHOICES, AND SAYING SO IS THE POINT.
	 *
	 * The engine has no column, no neighbour gap and no knot clearance, so this capability invents
	 * all three. Each is named and justified rather than buried, because a number nobody can point
	 * at is a number nobody can argue with.
	 *
	 * `ColumnBand`: two nodes share a COLUMN when their `NodePosX` differ by no more than this.
	 */
	constexpr double ColumnBand = 64.0;

	/**
	 * `NeighbourGap`: two columns are NEIGHBOURING when the horizontal gap between their rectangles
	 * is no more than this.
	 *
	 * DERIVED FROM THE FIXTURE rather than copied: in `KnotProbe` the two calls are 1200 apart and
	 * the three probes are 300 apart, so 400 treats the probes as one column and the calls as two
	 * columns that are NOT neighbours. A wire over 300 units is worth a knot; a wire over 1200 is
	 * worth one. A wire between nodes 300 apart is not, because the knot footprint is a large
	 * fraction of the distance and the route had no room to bend badly.
	 */
	constexpr double NeighbourGap = 400.0;

	/** `StackClearance`: how far an existing knot is inflated when testing free space (`C5`). */
	constexpr double StackClearance = 32.0;

	/**
	 * `MinimumNodeExtent`: the size a node is GIVEN when the engine has measured none.
	 *
	 * THIS CONSTANT IS THE WHOLE ANSWER TO OPEN-1, AND IT WAS MEASURED RATHER THAN ASSUMED.
	 *
	 * `NodeWidth` and `NodeHeight` are plain `int32` fields on `UEdGraphNode`
	 * (`EdGraphNode.h:309,313`) and NOTHING in the engine computes them: the only writes in the entire
	 * engine tree are in `UEdGraphNode_Documentation::ResizeNode` and `::SetBounds`
	 * (`Runtime/Engine/Private/EdGraph/EdGraphNode_Documentation.cpp:69,74`). For every ordinary node
	 * the value is written by the SLATE WIDGET when it draws. MEASURED, from the acceptance
	 * commandlet's own diagnostic, in a headless run:
	 *
	 *     node K2Node_CallFunction_0 at (300,0) size (0,0) rect x 300..300 y 0..0
	 *
	 * for EVERY node, including nodes loaded from a previously saved package. So the spec's OPEN-1
	 * conclusion - that `LookTurboMCPLayoutReport`'s wire lengths prove the sizes are real - WAS WRONG. Those
	 * wire lengths come from `PinRowSpacing`, a plugin constant, not from a measured height.
	 *
	 * REFUSING IS NOT THE RIGHT ANSWER, and this is where the spec's advice is worth departing from
	 * with a reason. A zero-size rectangle is a POINT, and a point is a perfectly good obstacle: it
	 * answers "does this wire pass through where this node is", which is the question T-LAY-10 asks.
	 * A node whose size has never been measured is given this small square instead, centred on its
	 * position, so every predicate has an AREA to work with. The tool is then CONSERVATIVE rather than
	 * exact: it under-reports a crossing when a wire clips the corner of a node that is really much
	 * wider.
	 *
	 * WHAT THIS COSTS, STATED PLAINLY: on a graph whose nodes have never been drawn, this capability
	 * judges crossings against small squares rather than the real outlines. When measured node sizes are available they improve the estimate;
	 * straight-segment crossing checks still do not model rendered wire splines. Every report
	 * says which of the two it used, so a caller is never left guessing.
	 */
	constexpr double MinimumNodeExtent = 48.0;

	/**
	 * `FallbackKnotExtent`: the knot footprint used when no knot has ever been measured.
	 *
	 * Same reasoning as `MinimumNodeExtent`. A knot made by `NewObject` has size zero because Slate
	 * has never drawn it, and REFUSING THE WHOLE CAPABILITY FOR THAT REASON WOULD MAKE THE TOOL USELESS
	 * ON EVERY GRAPH IT EXISTS FOR.
	 */
	constexpr double FallbackKnotExtent = 32.0;

	/** How many crossing wires the preview lists before it caps (`P6`). */
	constexpr int32 MaxListedWires = 40;

	/** How many crossed node keys are named per wire before that too caps. */
	constexpr int32 MaxCrossedPerWire = 8;

	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	/** True when a node carries a wire rather than doing work. */
	bool IsKnot(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Knot>(InNode) != nullptr;
	}

	/**
	 * A node's rectangle in graph coordinates.
	 *
	 * IT DELEGATES, AND THAT IS REQUIRED RATHER THAN TIDY. `FTurboMCPLayoutReport::RectOf` is the
	 * only rectangle convention in this project, and `LookTurboMCPLayoutReport` already publishes numbers
	 * built from it. Two tools disagreeing about where a node IS would be an invisible
	 * inconsistency, so the arithmetic is not repeated here.
	 */
	FTurboMCPLayoutReport::FRect RectOf(const UEdGraphNode* InNode)
	{
		return FTurboMCPLayoutReport::RectOf(InNode);
	}

	/** A node's position on the sheet, which is what this capability calls its COLUMN (`C4`). */
	double ColumnOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? static_cast<double>(InNode->NodePosX) : 0.0;
	}

	/**
	 * Walks through knots to the node a wire really comes from or goes to.
	 *
	 * A knot is a piece of wire, so a wire whose endpoint is a knot must be attributed to the node
	 * the knot leads to. Without this an existing knot would HIDE a crossing from the predicate,
	 * which is the opposite of what a reroute is for.
	 *
	 * The depth bound is because a malformed graph can point a knot at itself.
	 */
	UEdGraphNode* ResolveThroughKnots(const UEdGraphNode* InNode, int32 InDepth = 0)
	{
		if (InNode == nullptr || InDepth > 64 || !IsKnot(InNode))
		{
			return const_cast<UEdGraphNode*>(InNode);
		}

		// A knot passes through on its single output pin.
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
			{
				if (const UEdGraphNode* Next = Pin->LinkedTo[0]->GetOwningNode())
				{
					return ResolveThroughKnots(Next, InDepth + 1);
				}
			}
		}
		return const_cast<UEdGraphNode*>(InNode);
	}

	/**
	 * True when the segment S->T passes through the OPEN rectangle R.
	 *
	 * WHY THIS IS WRITTEN BY HAND INSTEAD OF CALLING AN ENGINE HELPER. The spec left
	 * `FMath::LineBoxIntersection`'s exact overload unread, and this project has had five guessed
	 * API names in a row turn out wrong. The slab method below needs no engine call at all and is
	 * exact for an AXIS-ALIGNED rectangle, which every node rectangle is.
	 *
	 * HOW IT WORKS. The segment is `P(t) = S + t * D` for `t` in `[0, 1]`. For each axis the set of
	 * `t` whose point lies between the rectangle's two bounds is an interval. The two intervals are
	 * intersected; if the result is non-empty the segment is inside the box.
	 *
	 * `bExclusive` IS WHAT MAKES "OPEN" MEAN OPEN. With it, the two candidate intervals are shrunk
	 * by an epsilon so a wire touching an edge exactly does NOT count, which is what `C1` requires
	 * and for the same reason `RectsOverlap` treats touching edges as not an overlap: two nodes side
	 * by side share a border.
	 */
	bool SegmentCrossesRect(const FVector2D& InStart, const FVector2D& InEnd,
		const FTurboMCPLayoutReport::FRect& InRect)
	{
		if (InRect.Width() <= 0.0 || InRect.Height() <= 0.0)
		{
			// A zero-size node has no interior, so nothing can cross it. This is reachable: the
			// engine stores what Slate measured, and a node never rendered may have no measured
			// size. Treating it as an obstacle would make the tool fire on every wire near it.
			return false;
		}

		const double Dx = InEnd.X - InStart.X;
		const double Dy = InEnd.Y - InStart.Y;

		// The parameter interval, shrunk so an exact edge touch does not count.
		constexpr double Edge = 1.0e-6;
		double TMin = 0.0 + Edge;
		double TMax = 1.0 - Edge;
		if (TMin > TMax)
		{
			return false;
		}

		// The X slab and the Y slab, intersected.
		const double XLo = FMath::Min(InRect.MinX, InRect.MaxX) + Edge;
		const double XHi = FMath::Max(InRect.MinX, InRect.MaxX) - Edge;
		const double YLo = FMath::Min(InRect.MinY, InRect.MaxY) + Edge;
		const double YHi = FMath::Max(InRect.MinY, InRect.MaxY) - Edge;

		if (XLo > XHi || YLo > YHi)
		{
			return false;
		}

		auto ClipSlab = [&TMin, &TMax](double InStartAxis, double InDelta,
			double InLo, double InHi) -> bool
		{
			if (FMath::IsNearlyZero(InDelta))
			{
				// The segment is parallel to this slab, so it either lies within the bounds for its
				// whole length or misses entirely.
				return InStartAxis > InLo && InStartAxis < InHi;
			}
			double T0 = (InLo - InStartAxis) / InDelta;
			double T1 = (InHi - InStartAxis) / InDelta;
			if (T0 > T1)
			{
				Swap(T0, T1);
			}
			TMin = FMath::Max(TMin, T0);
			TMax = FMath::Min(TMax, T1);
			return TMin <= TMax;
		};

		if (!ClipSlab(InStart.X, Dx, XLo, XHi))
		{
			return false;
		}
		if (!ClipSlab(InStart.Y, Dy, YLo, YHi))
		{
			return false;
		}
		return TMin <= TMax;
	}

	/** One node's geometry, gathered once so the predicates do not re-read the graph. */
	struct FObstacle
	{
		UEdGraphNode* Node = nullptr;
		FString Key;
		FTurboMCPLayoutReport::FRect Rect;
	};

	/**
	 * Every FLOW node, which is everything that is neither a knot nor a comment box.
	 *
	 * A NODE WITH NO MEASURED SIZE IS GIVEN ONE, centred on its position, so the crossing predicate
	 * has an area to test instead of a point that `SegmentCrossesRect` would reject outright. See
	 * `MinimumNodeExtent` for why that is the honest answer rather than abandoning the capability.
	 */
	void GatherObstacles(UEdGraph* InGraph, TArray<FObstacle>& OutObstacles)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || IsKnot(Node) || Node->IsA<UEdGraphNode_Comment>())
			{
				continue;
			}
			FObstacle Obstacle;
			Obstacle.Node = Node;
			Obstacle.Key = KeyOf(Node);
			Obstacle.Rect = RectOf(Node);

			if (Obstacle.Rect.Width() <= 0.0 || Obstacle.Rect.Height() <= 0.0)
			{
				// Centred on the node's position, so the smaller square sits where the node is rather
				// than growing to one side of it.
				const double CentreX = Obstacle.Rect.MinX;
				const double CentreY = Obstacle.Rect.MinY;
				Obstacle.Rect.MinX = CentreX - MinimumNodeExtent * 0.5;
				Obstacle.Rect.MaxX = CentreX + MinimumNodeExtent * 0.5;
				Obstacle.Rect.MinY = CentreY - MinimumNodeExtent * 0.5;
				Obstacle.Rect.MaxY = CentreY + MinimumNodeExtent * 0.5;
			}
			OutObstacles.Add(MoveTemp(Obstacle));
		}
	}

	/** Every comment box, which takes part in `C5` but is not a wire endpoint. */
	void GatherCommentRects(UEdGraph* InGraph, TArray<FTurboMCPLayoutReport::FRect>& OutRects)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				OutRects.Add(RectOf(Comment));
			}
		}
	}

	/** Every existing knot's rectangle, inflated by `StackClearance`. */
	void GatherKnotRects(UEdGraph* InGraph, TArray<FTurboMCPLayoutReport::FRect>& OutRects)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (!IsKnot(Node))
			{
				continue;
			}
			FTurboMCPLayoutReport::FRect Rect = RectOf(Node);

			// A KNOT'S FOOTPRINT IS MEASURED, NOT ASSUMED. `UK2Node_Knot`'s own
			// `AllocateDefaultPins` does not set `NodeWidth`/`NodeHeight`, and Slate supplies them
			// at render time. Measured: `LookTurboMCPLayoutReport` on the acceptance asset reports a longest
			// single wire of 400 for a wire between nodes at x=400 and x=1600, which is only
			// possible if `NodeWidth` is non-zero. So in practice the size is real - but a knot
			// whose rectangle is degenerate is inflated to a small square rather than being treated
			// as a point, because a point cannot reserve the space the knot will occupy.
			const double Minimum = 16.0;
			if (Rect.Width() < Minimum)
			{
				Rect.MaxX = Rect.MinX + Minimum;
			}
			if (Rect.Height() < Minimum)
			{
				Rect.MaxY = Rect.MinY + Minimum;
			}

			Rect.MinX -= StackClearance;
			Rect.MinY -= StackClearance;
			Rect.MaxX += StackClearance;
			Rect.MaxY += StackClearance;
			OutRects.Add(Rect);
		}
	}

	/** True when two rectangles overlap, touching edges NOT counting. */
	bool RectsOverlap(const FTurboMCPLayoutReport::FRect& A,
		const FTurboMCPLayoutReport::FRect& B)
	{
		return A.MinX < B.MaxX && B.MinX < A.MaxX
			&& A.MinY < B.MaxY && B.MinY < A.MaxY;
	}

	/** The knot's own footprint, measured from a throwaway knot rather than guessed. */
	struct FFootprint
	{
		double Width = 0.0;
		double Height = 0.0;

		/** True when no size could be measured and the named fallback was used instead. */
		bool bWasFallback = false;

		bool IsUsable() const { return Width > 0.0 && Height > 0.0; }
	};

	/** Reuse stored knot dimensions when available; otherwise disclose a fixed estimate. */
	FFootprint MeasureKnotFootprint(UEdGraph* InGraph)
	{
		FFootprint Out;
		if (InGraph == nullptr)
		{
			return Out;
		}

		// 1. AN EXISTING KNOT IS THE BEST SOURCE, because its size is what the editor measured.
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (!IsKnot(Node))
			{
				continue;
			}
			if (Node->NodeWidth > 0 && Node->NodeHeight > 0)
			{
				Out.Width = static_cast<double>(Node->NodeWidth);
				Out.Height = static_cast<double>(Node->NodeHeight);
				return Out;
			}
		}

        // A newly allocated knot has no measured size. Creating and destroying a
        // probe under the live graph can dirty it, so use the disclosed estimate.
        Out.Width = FallbackKnotExtent;
        Out.Height = FallbackKnotExtent;
        Out.bWasFallback = true;

		return Out;
	}

	/** One candidate knot position, and why it was refused when it was. */
	struct FCandidate
	{
		double X = 0.0;
		double Y = 0.0;
		bool bFree = false;
		FString Reason;
	};

	/** True when the candidate footprint meets nothing it must avoid (`C5`). */
	FString FreeSpaceReason(const FCandidate& InCandidate, const FFootprint& InFootprint,
		const TArray<FObstacle>& InObstacles, const TArray<FTurboMCPLayoutReport::FRect>& InComments,
		const TArray<FTurboMCPLayoutReport::FRect>& InKnots,
		const UEdGraphNode* InSource, const UEdGraphNode* InTarget)
	{
		FTurboMCPLayoutReport::FRect Footprint;
		Footprint.MinX = InCandidate.X;
		Footprint.MinY = InCandidate.Y;
		Footprint.MaxX = InCandidate.X + InFootprint.Width;
		Footprint.MaxY = InCandidate.Y + InFootprint.Height;

		for (const FObstacle& Obstacle : InObstacles)
		{
			if (Obstacle.Node == InSource || Obstacle.Node == InTarget)
			{
				continue;
			}
			if (RectsOverlap(Footprint, Obstacle.Rect))
			{
				return FString::Printf(TEXT("a node (%s) is already there"), *Obstacle.Key);
			}
		}
		for (const FTurboMCPLayoutReport::FRect& Comment : InComments)
		{
			if (RectsOverlap(Footprint, Comment))
			{
				return TEXT("a comment box is already there");
			}
		}
		for (const FTurboMCPLayoutReport::FRect& Knot : InKnots)
		{
			if (RectsOverlap(Footprint, Knot))
			{
				return TEXT("another knot is too close, and two knots are never stacked");
			}
		}
		return FString();
	}

	/** The two positions `C3` allows, in a fixed order so a report is stable. */
	void CandidatesFor(const FVector2D& InSource, const FVector2D& InTarget,
		FCandidate& OutLevelWithTarget, FCandidate& OutLevelWithSource)
	{
		// Level with the TARGET pin: same X as the target, same row as the source. The route is one
		// horizontal run then one vertical run.
		OutLevelWithTarget.X = InTarget.X;
		OutLevelWithTarget.Y = InSource.Y;

		// Level with the SOURCE pin: same X as the source, same row as the target.
		OutLevelWithSource.X = InSource.X;
		OutLevelWithSource.Y = InTarget.Y;
	}

	/** True when the two columns are neighbours, which is what `C4` refuses. */
	bool ColumnsAreNeighbours(const UEdGraphNode* InSource, const UEdGraphNode* InTarget)
	{
		const FTurboMCPLayoutReport::FRect A = RectOf(InSource);
		const FTurboMCPLayoutReport::FRect B = RectOf(InTarget);

		// A COLUMN IS A NODE'S `NodePosX`, and the gap is measured between the rectangles rather
		// than between the origins, because a wide node and a narrow one at the same origin do not
		// sit the same distance from their neighbour.
		const double Gap = FMath::Max(B.MinX - A.MaxX, A.MinX - B.MaxX);
		return Gap <= NeighbourGap;
	}

	/** Everything one crossing wire needs, resolved from the live graph. */
	struct FFoundWire
	{
		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		UEdGraphNode* SourceNode = nullptr;
		UEdGraphNode* TargetNode = nullptr;
		TArray<FString> CrossedIds;
		bool bAlreadyKnotted = false;
	};

	/**
	 * The pin a wire logically starts or ends at, walking through any knots in between.
	 *
	 * WHY THIS IS NEEDED FOR THE MEASUREMENT TO MEAN ANYTHING. After a knot is placed, the original
	 * wire becomes TWO wires, `source -> knot` and `knot -> target`. Measuring either stub on its own
	 * is measuring a segment that no longer reaches across the graph, so the count would APPEAR to
	 * improve for a reason that has nothing to do with the crossing being fixed - and worse, the
	 * before and after numbers would not be comparable at all, which is exactly what the spec's
	 * "counts wires crossing nodes before and after" requires them to be.
	 *
	 * Measured consequence of NOT doing this: a pass that placed one knot reported crossings going
	 * from 7 to 7 and rolled itself back, even though the crossing it fixed was gone. The count was
	 * right and the ROUTE IT MEASURED was wrong.
	 *
	 * The logical pin of a stub is found by following the wire THROUGH the knot to its far side,
	 * which is the same resolution `ResolveThroughKnots` does for the owning node.
	 */
	UEdGraphPin* ResolveThroughKnotsToPin(UEdGraphPin* InPin, int32 InDepth = 0)
	{
		if (InPin == nullptr || InDepth > 64)
		{
			return InPin;
		}
		UEdGraphNode* Owner = InPin->GetOwningNode();
		if (Owner == nullptr || !IsKnot(Owner))
		{
			return InPin;
		}

		// An INPUT pin on a knot means the wire came from the source side, so the logical pin is the
		// input pin's single link. An OUTPUT pin means the wire is leaving toward the target, so the
		// logical pin is the output's single link.
		const bool bIsInput = InPin->Direction == EGPD_Input;
		for (UEdGraphPin* FarSide : InPin->LinkedTo)
		{
			if (FarSide == nullptr)
			{
				continue;
			}
			// The far side must be on the other side of the knot from where the wire arrived.
			if (bIsInput && FarSide->Direction == EGPD_Output)
			{
				return ResolveThroughKnotsToPin(FarSide, InDepth + 1);
			}
			if (!bIsInput && FarSide->Direction == EGPD_Input)
			{
				return ResolveThroughKnotsToPin(FarSide, InDepth + 1);
			}
		}
		return InPin;
	}

	/**
	 * Every wire in the graph that crosses a node, found by `C1` and reported once each by `C2`.
	 *
	 * THE WIRE IS THE UNIT. Iterating output pins and following `LinkedTo` visits each wire from its
	 * source end exactly once, which is what makes "listed once however many nodes it crosses" a
	 * property of the walk rather than a de-duplication step afterwards.
	 *
	 * A WIRE THAT ALREADY TOUCHES A KNOT IS MEASURED BY ITS LOGICAL ROUTE, not by the stub the loop
	 * happens to be holding. See `ResolveThroughKnotsToPin`: without that, placing a knot SPLITS one
	 * crossing wire into two uncrossing stubs and the before/after counts stop being comparable.
	 */
	void FindCrossingWires(UEdGraph* InGraph, const TArray<FObstacle>& InObstacles,
		TArray<FFoundWire>& OutWires, int32& OutCrossings, int32& OutKnotCount)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node != nullptr && IsKnot(Node))
			{
				++OutKnotCount;
			}
		}

		// A LOGICAL ROUTE IS COUNTED ONCE, HOWEVER MANY PHYSICAL STUBS IT PASSES THROUGH. After a knot
		// is placed, one route is `source -> knot -> target`, which the walk below reaches TWICE - once
		// from the source output and once from the knot output. Without this set the same crossing
		// would be counted twice AFTER the pass and once BEFORE it, which would make the two numbers
		// incomparable and could make a good pass look like a bad one.
		TSet<TPair<UEdGraphPin*, UEdGraphPin*>> Seen;

		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output)
				{
					continue;
				}

				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked == nullptr)
					{
						continue;
					}

					UEdGraphNode* SourceNode = ResolveThroughKnots(Pin->GetOwningNode());
					UEdGraphNode* TargetNode = ResolveThroughKnots(Linked->GetOwningNode());
					if (SourceNode == nullptr || TargetNode == nullptr || SourceNode == TargetNode)
					{
						// A wire a node runs to itself is not a routing problem.
						continue;
					}

					// THE LOGICAL ENDS OF THE ROUTE, which is what has to be measured.
					UEdGraphPin* LogicalSource = ResolveThroughKnotsToPin(Pin);
					UEdGraphPin* LogicalTarget = ResolveThroughKnotsToPin(Linked);
					if (LogicalSource == nullptr || LogicalTarget == nullptr
						|| LogicalSource == LogicalTarget)
					{
						continue;
					}

					const TPair<UEdGraphPin*, UEdGraphPin*> Route(LogicalSource, LogicalTarget);
					if (Seen.Contains(Route))
					{
						continue;
					}
					Seen.Add(Route);

					const FVector2D Source = FTurboMCPLayoutReport::PinPosition(LogicalSource);
					const FVector2D Target = FTurboMCPLayoutReport::PinPosition(LogicalTarget);

					TArray<FString> Crossed;
					for (const FObstacle& Obstacle : InObstacles)
					{
						if (Obstacle.Node == SourceNode || Obstacle.Node == TargetNode)
						{
							continue;
						}
						if (SegmentCrossesRect(Source, Target, Obstacle.Rect))
						{
							Crossed.Add(Obstacle.Key);
						}
					}

					if (Crossed.Num() == 0)
					{
						continue;
					}

					Crossed.Sort();

					FFoundWire Found;
					Found.SourcePin = LogicalSource;
					Found.TargetPin = LogicalTarget;
					Found.SourceNode = SourceNode;
					Found.TargetNode = TargetNode;
					Found.CrossedIds = MoveTemp(Crossed);
					// ALREADY KNOTTED IS READ FROM THE IMMEDIATE PINS, NOT THE LOGICAL ONES. The
					// logical pins are the far ends of the route, so testing them would report a
					// freshly placed knot as pre-existing and the pass would never place a second one.
					Found.bAlreadyKnotted = IsKnot(Pin->GetOwningNode())
						|| IsKnot(Linked->GetOwningNode());
					OutCrossings += Found.CrossedIds.Num();
					OutWires.Add(MoveTemp(Found));
				}
			}
		}
	}

	/** A wire's pins and crossings turned into the reportable form, before a position is decided. */
	FTurboMCPGraphKnots::FWire DescribeWire(const FFoundWire& InFound)
	{
		FTurboMCPGraphKnots::FWire Wire;
		Wire.SourceId = KeyOf(InFound.SourceNode);
		Wire.TargetId = KeyOf(InFound.TargetNode);
		Wire.SourcePin = InFound.SourcePin != nullptr ? InFound.SourcePin->PinName.ToString()
			: FString();
		Wire.TargetPin = InFound.TargetPin != nullptr ? InFound.TargetPin->PinName.ToString()
			: FString();
		Wire.CrossedIds = InFound.CrossedIds;
		Wire.bAlreadyKnotted = InFound.bAlreadyKnotted;
		return Wire;
	}

	/**
	 * A refused wire, with its reason.
	 *
	 * A REFUSAL ALWAYS CARRIES A REASON. `P7` wants a result to lead somewhere, and a wire that is
	 * skipped without a reason is the one shape a caller cannot act on.
	 */
	FTurboMCPGraphKnots::FWire DescribeRefusal(const FFoundWire& InFound,
		const FString& InReason)
	{
		FTurboMCPGraphKnots::FWire Wire = DescribeWire(InFound);
		Wire.bRefused = true;
		Wire.RefusalReason = InReason;
		return Wire;
	}

	/** The resolved request, ready to be planned against. */
	struct FResolved
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString ObjectPath;
	};

	/** Resolves the asset and the graph, or says why not. Shared by `Plan` and `Apply`. */
	bool ResolveGraph(const FString& InAsset, const FString& InGraphName,
		FResolved& Out, FString& OutError)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
		if (!ResolveError.IsEmpty())
		{
			OutError = ResolveError;
			return false;
		}

		Out.ObjectPath = Resolved.ObjectPath;
		Out.Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
		if (Out.Blueprint == nullptr)
		{
			OutError = FString::Printf(
				TEXT("%s is not a Blueprint, so it has no graph to route."), *Resolved.ObjectPath);
			return false;
		}

        int32 Matches = 0;
        UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Out.Blueprint, InGraphName, Matches);
        if (InGraphName.TrimStartAndEnd().IsEmpty() || !Graph || Matches != 1)
        {
            OutError = TEXT("Specify one exact existing graph name; empty, missing or ambiguous graphs are refused.");
            return false;
        }
        Out.Graph = Graph;
		return true;
	}

	/** Finds a live pin on a live graph, by owning node key and pin name. */
	UEdGraphPin* FindPinByKey(UEdGraph* InGraph, const FString& InNodeKey,
		const FString& InPinName)
	{
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || !KeyOf(Node).Equals(InNodeKey, ESearchCase::IgnoreCase))
			{
				continue;
			}
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin != nullptr && Pin->PinName.ToString().Equals(InPinName,
						ESearchCase::IgnoreCase))
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}
}

bool FTurboMCPGraphKnots::Plan(const FString& InAsset, const FString& InGraphName,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
    {
        OutError = TEXT("Reroute planning requires an idle editor outside PIE and active transactions.");
        return false;
    }

	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}

	FResolved Resolved;
	if (!ResolveGraph(OutPlan.Asset, InGraphName, Resolved, OutError))
	{
		return false;
	}

	UEdGraph* Graph = Resolved.Graph;
    TSet<FString> NodeKeys;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) continue;
        const FString Key = KeyOf(Node);
        if (!Node->NodeGuid.IsValid() || NodeKeys.Contains(Key))
        {
            OutError = TEXT("Graph contains invalid or ambiguous node identifiers; no routing was planned.");
            return false;
        }
        NodeKeys.Add(Key);
    }

	if (OutPlan.GraphName.IsEmpty())
	{
		// The answer says WHICH graph was used, so a caller who omitted the name is not guessing.
		OutPlan.GraphName = Graph->GetName();
	}

	OutPlan.NodeCountBefore = Graph->Nodes.Num();

	// THE FOOTPRINT IS MEASURED BEFORE ANYTHING IS PLANNED, because `C5` cannot be decided without
	// it. When nothing in the graph has ever been rendered the measurement falls back to a NAMED
	// constant and the report SAYS SO, rather than refusing: see `MinimumNodeExtent`, which explains
	// why the spec's advice to refuse was departed from, with the measurement that justifies it.
	const FFootprint Footprint = MeasureKnotFootprint(Graph);
	OutPlan.bFootprintUnknown = Footprint.bWasFallback;

	TArray<FObstacle> Obstacles;
	GatherObstacles(Graph, Obstacles);

	TArray<FTurboMCPLayoutReport::FRect> Comments;
	GatherCommentRects(Graph, Comments);

	TArray<FTurboMCPLayoutReport::FRect> KnotRects;
	GatherKnotRects(Graph, KnotRects);

	TArray<FFoundWire> Found;
	FindCrossingWires(Graph, Obstacles, Found, OutPlan.CrossingsBefore, OutPlan.KnotCountBefore);
	OutPlan.CrossingWiresBefore = Found.Num();

	// EVERY REFUSAL IS DECIDED HERE SO THE PREVIEW CAN STATE IT (`D-02`, and the reason `P/E-` share
	// one code path up to the write). A refusal moved into `Apply` would let the Preview offer a
	// plan the Edit then declines.
	//
	// `C6` SHARING IS DECIDED BY SOURCE PIN IDENTITY, and the groups are built first so a shared
	// position can be chosen from the candidates acceptable to EVERY member of the group.
	TMap<UEdGraphPin*, TArray<int32>> Groups;
	for (int32 Index = 0; Index < Found.Num(); ++Index)
	{
		const FFoundWire& Wire = Found[Index];
		if (Wire.bAlreadyKnotted)
		{
			// `C7`: a wire already carrying a knot is reported untouched, not refused as an error.
			OutPlan.Wires.Add(DescribeRefusal(Wire, TEXT(
				"this wire already passes through a knot, and a second knot would put a second bend "
				"on it. A knot is a piece of wire, so the wire is left as it is routed.")));
			continue;
		}
		if (ColumnsAreNeighbours(Wire.SourceNode, Wire.TargetNode))
		{
			OutPlan.Wires.Add(DescribeRefusal(Wire, FString::Printf(
				TEXT("its two nodes are in neighbouring columns (within %.0f units), where the knot ")
				TEXT("itself is a large part of the distance and the route had no room to bend badly."),
				NeighbourGap)));
			continue;
		}
		Groups.FindOrAdd(Wire.SourcePin).Add(Index);
	}

	// The shared position for each group, chosen once so every wire in the group agrees on it.
	TMap<UEdGraphPin*, FCandidate> SharedChoice;
	TMap<UEdGraphPin*, FString> SharedRefusal;
	for (const TPair<UEdGraphPin*, TArray<int32>>& Group : Groups)
	{
		if (Group.Value.Num() <= 1)
		{
			continue;
		}

		// A candidate is acceptable to the group when it is free for EVERY member.
		FCandidate Best;
		FString BestReason = FString(TEXT("no single position is free for every wire of this pin"));

		for (int32 Which = 0; Which < 2; ++Which)
		{
			FCandidate Trial;
			bool bUsable = true;
			FString TrialReason;

			for (const int32 Index : Group.Value)
			{
				const FFoundWire& Wire = Found[Index];
				const FVector2D Source = FTurboMCPLayoutReport::PinPosition(Wire.SourcePin);
				const FVector2D Target = FTurboMCPLayoutReport::PinPosition(Wire.TargetPin);

				FCandidate WithTarget;
				FCandidate WithSource;
				CandidatesFor(Source, Target, WithTarget, WithSource);
				const FCandidate& Candidate = (Which == 0) ? WithTarget : WithSource;

				const FString Reason = FreeSpaceReason(Candidate, Footprint, Obstacles, Comments,
					KnotRects, Wire.SourceNode, Wire.TargetNode);
				if (!Reason.IsEmpty())
				{
					bUsable = false;
					TrialReason = Reason;
					break;
				}
				Trial = Candidate;
			}

			if (bUsable)
			{
				Best = Trial;
				Best.bFree = true;
				BestReason.Empty();
				break;
			}
			if (TrialReason.Len() > 0)
			{
				BestReason = TrialReason;
			}
		}

		if (Best.bFree)
		{
			SharedChoice.Add(Group.Key, Best);
		}
		else
		{
			// SHARING IS REFUSED, AND THE REFUSAL IS REPORTED rather than silently falling back:
			// the Preview must say which wires were refused sharing and why.
			SharedRefusal.Add(Group.Key, BestReason);
		}
	}

	for (const TPair<UEdGraphPin*, TArray<int32>>& Group : Groups)
	{
		for (const int32 Index : Group.Value)
		{
			const FFoundWire& Wire = Found[Index];
			const FVector2D Source = FTurboMCPLayoutReport::PinPosition(Wire.SourcePin);
			const FVector2D Target = FTurboMCPLayoutReport::PinPosition(Wire.TargetPin);

			// A SHARED POSITION WINS WHEN ONE WAS AGREED, because `C6` asks for one knot per pin.
			if (const FCandidate* Shared = SharedChoice.Find(Group.Key))
			{
				FWire Described = DescribeWire(Wire);
				Described.bShared = true;
				Described.KnotX = Shared->X;
				Described.KnotY = Shared->Y;
				OutPlan.AcceptedIndices.Add(OutPlan.Wires.Num());
				OutPlan.Wires.Add(MoveTemp(Described));
				continue;
			}

			// Otherwise the two positions `C3` allows are tried in a fixed order, and the first free
			// one is taken. Both refused is a refusal WITH BOTH reasons, because a caller who is
			// told only one of them will try the other.
			FCandidate WithTarget;
			FCandidate WithSource;
			CandidatesFor(Source, Target, WithTarget, WithSource);

			const FString ReasonWithTarget = FreeSpaceReason(WithTarget, Footprint, Obstacles,
				Comments, KnotRects, Wire.SourceNode, Wire.TargetNode);
			const FString ReasonWithSource = FreeSpaceReason(WithSource, Footprint, Obstacles,
				Comments, KnotRects, Wire.SourceNode, Wire.TargetNode);

			const FString SharedWhy = SharedRefusal.Contains(Group.Key)
				? SharedRefusal[Group.Key] : FString();

			if (ReasonWithTarget.IsEmpty())
			{
				FWire Described = DescribeWire(Wire);
				Described.KnotX = WithTarget.X;
				Described.KnotY = WithTarget.Y;
				Described.bLevelWithTarget = true;
				OutPlan.AcceptedIndices.Add(OutPlan.Wires.Num());
				OutPlan.Wires.Add(MoveTemp(Described));
			}
			else if (ReasonWithSource.IsEmpty())
			{
				FWire Described = DescribeWire(Wire);
				Described.KnotX = WithSource.X;
				Described.KnotY = WithSource.Y;
				Described.bLevelWithTarget = false;
				OutPlan.AcceptedIndices.Add(OutPlan.Wires.Num());
				OutPlan.Wires.Add(MoveTemp(Described));
			}
			else
			{
				const FString Why = SharedWhy.IsEmpty()
					? FString::Printf(
						TEXT("no free space at either position: level with the target pin, %s; ")
						TEXT("level with the source pin, %s"),
						*ReasonWithTarget, *ReasonWithSource)
					: FString::Printf(
						TEXT("no free space at either position (level with the target pin: %s; level ")
						TEXT("with the source pin: %s), and sharing with the other wires of this pin ")
						TEXT("was refused too (%s)"),
						*ReasonWithTarget, *ReasonWithSource, *SharedWhy);
				OutPlan.Wires.Add(DescribeRefusal(Wire, Why));
			}
		}
	}

	return true;
}

FString FTurboMCPGraphKnots::RenderPreview(const FPlan& InPlan)
{
	FString Out;
	Out += FString::Printf(TEXT("preview: knots: graph %s\n"), *InPlan.GraphName);
	Out += FString::Printf(
		TEXT("counts: %d graph array entries (includes comments and knots), %d knot(s) already (subset), %d wire(s) cross a node, ")
		TEXT("%d crossing(s) in total\n"),
		InPlan.NodeCountBefore, InPlan.KnotCountBefore, InPlan.CrossingWiresBefore,
		InPlan.CrossingsBefore);

	Out += TEXT("Geometry: crossings use estimated straight wire segments and node rectangles, not rendered wire curves. Missing node dimensions use fallback bounds. Inspect the visible graph before accepting the layout.\n");
	if (InPlan.bFootprintUnknown)
	{
		// This flag describes the reroute footprint, not whether every node was drawn.
		Out += FString::Printf(TEXT("Reroute size: no usable measured knot footprint was found; planning uses %.0f by %.0f graph units.\n"),
			FallbackKnotExtent, FallbackKnotExtent);
	}

	if (InPlan.Wires.Num() == 0)
	{
		// `P1`: never return nothing. "No crossings" is an answer and it says what was looked at.
		Out += TEXT("No crossings were found by this geometry estimate, so no reroute is proposed. A crossing means the ")
			TEXT("straight segment between its two pin positions meets a node's rectangle, ")
			TEXT("using the same pin-position estimate as LookTurboMCPLayoutReport. Rendered wires may differ.\n");
		return Out;
	}

	TSet<FString> ProposedKnots;
    for (int32 Index : InPlan.AcceptedIndices)
    {
        const FWire& Wire = InPlan.Wires[Index];
        ProposedKnots.Add(FString::Printf(TEXT("%s.%s@%.0f,%.0f"), *Wire.SourceId, *Wire.SourcePin, Wire.KnotX, Wire.KnotY));
    }
    Out += FString::Printf(TEXT("would place %d knot(s) for %d wire(s):\n"), ProposedKnots.Num(), InPlan.AcceptedIndices.Num());

	int32 Listed = 0;
	for (int32 Index = 0; Index < InPlan.Wires.Num(); ++Index)
	{
		if (Listed >= MaxListedWires)
		{
			Out += FString::Printf(TEXT("  ... and %d more wire(s), not listed.\n"),
				InPlan.Wires.Num() - Listed);
			break;
		}
		++Listed;

		const FWire& Wire = InPlan.Wires[Index];
		const bool bAccepted = InPlan.AcceptedIndices.Contains(Index);
		Out += FString::Printf(TEXT("\n  %s %s.%s -> %s.%s\n"),
			bAccepted ? TEXT("KNOT ") : TEXT("SKIP "),
			*Wire.SourceId, *Wire.SourcePin, *Wire.TargetId, *Wire.TargetPin);

		Out += FString::Printf(TEXT("      crosses %d node(s): %s\n"), Wire.CrossedIds.Num(),
			*FString::Join(Wire.CrossedIds, TEXT(", ")));

		if (bAccepted)
		{
			// WHICH PIN THE KNOT IS LEVEL WITH IS RECORDED IN THE PLAN, not worked out again here.
			// `C3` allows exactly two positions and the recogniser picked one, so it knows which.
			const FString Level = Wire.bShared
				? FString(TEXT("the target pin, and it is SHARED by every wire of this pin because one "))
					+ TEXT("pin feeding several distant nodes gets a single knot")
				: (Wire.bLevelWithTarget
					? FString(TEXT("the target pin, so the route is one horizontal run then one "))
						+ TEXT("vertical run")
					: FString(TEXT("the source pin, so the route is one vertical run then one "))
						+ TEXT("horizontal run"));

			Out += FString::Printf(TEXT("      knot at (%.0f, %.0f), level with %s\n"),
				Wire.KnotX, Wire.KnotY, *Level);
		}
		else
		{
			Out += FString::Printf(TEXT("      NOT PLACED: %s\n"), *Wire.RefusalReason);
		}
	}

	Out += FString::Printf(
		TEXT("\nthe edit would place %d knot(s), then re-measure the crossings and ROLL THE WHOLE ")
		TEXT("PASS BACK if the count is not lower than %d. Full restoration and runtime behavior are not verified.\n"),
		InPlan.AcceptedIndices.Num(), InPlan.CrossingsBefore);
	return Out;
}

FString FTurboMCPGraphKnots::Apply(const FPlan& InPlan)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: rerouting requires an idle editor outside PIE and active transactions; unchanged");

	FResolved Resolved;
	FString Error;
	if (!ResolveGraph(InPlan.Asset, InPlan.GraphName, Resolved, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	UEdGraph* Graph = Resolved.Graph;
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema == nullptr)
	{
		return FString::Printf(
			TEXT("error: graph '%s' has no schema, so a wire cannot be reconnected safely."),
			*Graph->GetName());
	}

	if (InPlan.AcceptedIndices.Num() == 0)
	{
		return FString::Printf(
			TEXT("unchanged: %s has %d wire(s) crossing a node and no knot can be placed for any of ")
			TEXT("them, so nothing was added. PreviewTurboMCPGraphKnots says which reason applies to each."),
			*Graph->GetName(), InPlan.CrossingWiresBefore);
	}

	// THE PLAN IS RE-RESOLVED BY PIN IDENTITY BEFORE ANYTHING IS TOUCHED, because a plan is made
	// and applied in two calls and the graph can change between them. A wire whose pins no longer
	// connect is REPORTED and skipped, never silently dropped: a partly applied edit is what `P8`
	// forbids.
	struct FPending
	{
		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		FString SourceId;
		FString TargetId;
		FString SourcePinName;
		FString TargetPinName;
		double KnotX = 0.0;
		double KnotY = 0.0;
	};

	TArray<FPending> Pending;
	TArray<FString> Skipped;
	for (const int32 Index : InPlan.AcceptedIndices)
	{
		if (!InPlan.Wires.IsValidIndex(Index))
		{
			continue;
		}
		const FWire& Wire = InPlan.Wires[Index];

		UEdGraphPin* SourcePin = FindPinByKey(Graph, Wire.SourceId, Wire.SourcePin);
		UEdGraphPin* TargetPin = FindPinByKey(Graph, Wire.TargetId, Wire.TargetPin);
		if (SourcePin == nullptr || TargetPin == nullptr)
		{
			Skipped.Add(FString::Printf(TEXT("%s.%s -> %s.%s (a pin is gone)"),
				*Wire.SourceId, *Wire.SourcePin, *Wire.TargetId, *Wire.TargetPin));
			continue;
		}
		if (!SourcePin->LinkedTo.Contains(TargetPin))
		{
			// The wire no longer connects the two pins the plan recorded, so the knot would be
			// spliced into a wire that is not there.
			Skipped.Add(FString::Printf(TEXT("%s.%s -> %s.%s (no longer linked)"),
				*Wire.SourceId, *Wire.SourcePin, *Wire.TargetId, *Wire.TargetPin));
			continue;
		}

		FPending Item;
		Item.SourcePin = SourcePin;
		Item.TargetPin = TargetPin;
		Item.SourceId = Wire.SourceId;
		Item.TargetId = Wire.TargetId;
		Item.SourcePinName = Wire.SourcePin;
		Item.TargetPinName = Wire.TargetPin;
		Item.KnotX = Wire.KnotX;
		Item.KnotY = Wire.KnotY;
		Pending.Add(MoveTemp(Item));
	}

	if (Pending.Num() == 0)
	{
		return FString::Printf(
			TEXT("unchanged: the graph changed since the preview, and none of the %d planned wire(s) ")
			TEXT("still connects. Nothing was added. %s"),
			InPlan.AcceptedIndices.Num(),
			Skipped.Num() > 0 ? *FString::Printf(TEXT("Skipped: %s"),
				*FString::Join(Skipped, TEXT("; "))) : TEXT(""));
	}

	// THE PASS IS MADE IN ONE TRANSACTION, because `P4` says every Edit is one undo.
	const FScopedTransaction Transaction(
		FText::FromString(TEXT("TurboMCP: place reroute knots")));

	// Keep dirty state when restoration has not been independently verified.

	Resolved.Blueprint->Modify();
    Graph->Modify();
    for (const FPending& Item : Pending)
    {
        Item.SourcePin->GetOwningNode()->Modify();
        Item.TargetPin->GetOwningNode()->Modify();
    }
    TMap<FString, UK2Node_Knot*> SharedKnots;
    TArray<UK2Node_Knot*> CompletedKnots;
    TArray<UK2Node_Knot*> Created;
	TArray<FPending> Completed;
	TArray<FString> Failed;

	for (const FPending& Item : Pending)
	{
		Graph->Modify();

		// 1. THE KNOT IS CREATED, AND ITS PINS ARE ALLOCATED EXPLICITLY.
		//
		// THIS IS MANDATORY AND ITS ABSENCE CRASHES. `UK2Node_Knot::GetInputPin` is `return
		// Pins[0];` and `GetOutputPin` is `return Pins[1];`, BOTH UNGUARDED
		// (`K2Node_Knot.h:54-62`). `NewObject` does not allocate a node's pins and
		// `UEdGraph::AddNode` does not either: the editor allocates them when the node is
		// RECONSTRUCTED FOR DISPLAY, which never happens in a commandlet. READING THOSE PINS WITHOUT
		// ALLOCATING THEM FIRST IS `Array index out of bounds: 1 into an array of size 0`.
		const FString SharedKey = FString::Printf(TEXT("%s.%s@%.0f,%.0f"), *Item.SourceId, *Item.SourcePinName, Item.KnotX, Item.KnotY);
        UK2Node_Knot* Knot = SharedKnots.FindRef(SharedKey);
        const bool bNewKnot = Knot == nullptr;
        if (bNewKnot) Knot = NewObject<UK2Node_Knot>(Graph, NAME_None, RF_Transactional);
		if (Knot == nullptr)
		{
			Failed.Add(FString::Printf(TEXT("%s.%s (the knot could not be made)"),
				*Item.SourceId, *Item.SourcePinName));
			continue;
		}
		if (bNewKnot)
        {
        Knot->CreateNewGuid();
		Knot->NodePosX = static_cast<int32>(Item.KnotX);
		Knot->NodePosY = static_cast<int32>(Item.KnotY);
		Graph->AddNode(Knot, /*bUserAction=*/false, /*bSelectNewNode=*/false);
		Knot->AllocateDefaultPins();
        }

		UEdGraphPin* KnotIn = Knot->GetInputPin();
		UEdGraphPin* KnotOut = Knot->GetOutputPin();
		if (KnotIn == nullptr || KnotOut == nullptr)
		{
			// Allocated but still empty: the plan is abandoned for this wire rather than used.
			Knot->DestroyNode();
			Failed.Add(FString::Printf(TEXT("%s.%s (the knot produced no pins)"),
				*Item.SourceId, *Item.SourcePinName));
			continue;
		}

		// 2. THE ORIGINAL LINK IS BROKEN THROUGH THE SCHEMA, which is what the editor itself uses.
		// `BreakLinkTo` is lower level and skips the schema's notification work.
		Schema->BreakSinglePinLink(Item.SourcePin, Item.TargetPin);

		// 3. THE TWO NEW LINKS ARE MADE, AND EACH RETURN VALUE IS CHECKED. A failed connection
		// leaves a wire broken in two, which is WORSE than the crossing it was fixing, so a failure
		// here abandons this wire and the pass-level rollback below decides the rest.
		const bool bIn = !bNewKnot || Schema->TryCreateConnection(Item.SourcePin, KnotIn);
		const bool bOut = bIn && Schema->TryCreateConnection(KnotOut, Item.TargetPin);

		if (!bIn || !bOut)
		{
			// UNDO THIS WIRE IMMEDIATELY: put the original link back and drop the knot. Leaving it
			// half-connected is the one outcome that is worse than doing nothing.
			if (bIn && bNewKnot)
			{
				Schema->BreakSinglePinLink(Item.SourcePin, KnotIn);
			}
			if (bOut)
			{
				Schema->BreakSinglePinLink(KnotOut, Item.TargetPin);
			}
			if (bNewKnot) Knot->DestroyNode();
			Schema->TryCreateConnection(Item.SourcePin, Item.TargetPin);
			Failed.Add(FString::Printf(TEXT("%s.%s (the schema refused a connection)"),
				*Item.SourceId, *Item.SourcePinName));
			continue;
		}

		if (bNewKnot) { Created.Add(Knot); SharedKnots.Add(SharedKey, Knot); }
        CompletedKnots.Add(Knot);
		Completed.Add(Item);
	}

	if (Created.Num() == 0)
	{
		return FString::Printf(
			TEXT("partial: %d wire(s) were planned and none was successfully routed. ")
			TEXT("Restoration is not verified; inspect links and unsaved changes before saving.%s%s"),
			Pending.Num(),
			Failed.Num() > 0 ? *FString::Printf(TEXT(" Failed: %s"),
				*FString::Join(Failed, TEXT("; "))) : TEXT(""),
			Skipped.Num() > 0 ? *FString::Printf(TEXT(" Skipped: %s"),
				*FString::Join(Skipped, TEXT("; "))) : TEXT(""));
	}

	// 4. RE-MEASURE, WITH THE SAME CODE THAT MEASURED BEFORE.
	//
	// The measurement is taken with `FindCrossingWires` rather than a second implementation,
	// because two implementations of "how many crossings" would agree only until one was changed.
	int32 CrossingsAfter = 0;
	int32 KnotsAfter = 0;
	{
		TArray<FObstacle> ObstaclesAfter;
		GatherObstacles(Graph, ObstaclesAfter);
		TArray<FFoundWire> FoundAfter;
		FindCrossingWires(Graph, ObstaclesAfter, FoundAfter, CrossingsAfter, KnotsAfter);
	}

	Graph->MarkPackageDirty();

	bool bRolledBack = false;
	int32 CrossingsAfterRollback = CrossingsAfter;

	// 5. NOT BETTER MEANS ROLLED BACK. `>=` AND NOT `>`: a pass that leaves the count unchanged has
	// added knots for nothing, so it is undone.
	if (CrossingsAfter >= InPlan.CrossingsBefore)
	{
		bRolledBack = true;

		// THE ROLLBACK UNDOES THE WIRES IN REVERSE ORDER, using the very knot objects the pass
		// created. LOOKING A KNOT UP AGAIN BY ITS POSITION WOULD BE A GUESS: two wires can be planned
		// at the same coordinates when they share one, and a position is not an identity. Holding the
		// pointer is exact.
		// CompletedKnots tracks each routed wire; shared knots appear more than once.
		// Remove each knot once while restoring every original connection.
		TSet<UK2Node_Knot*> RemovedKnots;
        for (int32 Index = Completed.Num() - 1; Index >= 0; --Index)
		{
			const FPending& Item = Completed[Index];
			UK2Node_Knot* Knot = CompletedKnots[Index];

			if (Knot != nullptr && !RemovedKnots.Contains(Knot) && Knot->GetGraph() == Graph)
			{
				UEdGraphPin* KnotIn = Knot->GetInputPin();
				UEdGraphPin* KnotOut = Knot->GetOutputPin();
				if (KnotIn != nullptr)
				{
					Schema->BreakPinLinks(*KnotIn, /*bSendsNodeNotifcation=*/false);
				}
				if (KnotOut != nullptr)
				{
					Schema->BreakPinLinks(*KnotOut, /*bSendsNodeNotifcation=*/false);
				}
				RemovedKnots.Add(Knot);
                Knot->DestroyNode();
			}

			// The knot's links are gone, so the original wire can be put back. The pins are found
			// again because breaking a link can leave a pin pointer stale.
			UEdGraphPin* SourcePin = FindPinByKey(Graph, Item.SourceId, Item.SourcePinName);
			UEdGraphPin* TargetPin = FindPinByKey(Graph, Item.TargetId, Item.TargetPinName);
			if (SourcePin != nullptr && TargetPin != nullptr)
			{
				Schema->TryCreateConnection(SourcePin, TargetPin);
			}
		}

		// Recount crossings for diagnostics; this does not verify complete restoration.
		int32 ConfirmCrossings = 0;
		int32 ConfirmKnots = 0;
		{
			TArray<FObstacle> ObstaclesConfirm;
			GatherObstacles(Graph, ObstaclesConfirm);
			TArray<FFoundWire> FoundConfirm;
			FindCrossingWires(Graph, ObstaclesConfirm, FoundConfirm, ConfirmCrossings, ConfirmKnots);
		}
		CrossingsAfterRollback = ConfirmCrossings;

		FString Out = FString::Printf(
			TEXT("partial: rollback attempted. %d knot(s) were placed and the estimated crossings ")
			TEXT("went from %d to %d, so rollback was requested. Full graph restoration is not verified. ")
			TEXT("The crossings after the rollback are %d, %s the %d it started at.%s"),
			Created.Num(), InPlan.CrossingsBefore, CrossingsAfter,
			CrossingsAfterRollback,
			CrossingsAfterRollback == InPlan.CrossingsBefore ? TEXT("equal to") : TEXT("different from"),
			InPlan.CrossingsBefore,
			Failed.Num() > 0 ? *FString::Printf(TEXT(" Failed: %s"),
				*FString::Join(Failed, TEXT("; "))) : TEXT(""));

		// Crossing counts alone cannot establish restoration of links or callback effects.
		Graph->MarkPackageDirty();
		Out += TEXT(" Unsaved changes are retained. Inspect the graph and its links before saving; compilation and behavior were not checked.");

		return Out;
	}

	FString Out = FString::Printf(
		TEXT("changed: placed %d knot(s) at the positions the preview listed. The crossings went ")
		TEXT("from %d to %d, so the pass was kept. The graph has %d array entries (includes comments and knots), of which %d are knots; previously it ")
		TEXT("had %d and %d.\n"),
		Created.Num(), InPlan.CrossingsBefore, CrossingsAfter,
		Graph->Nodes.Num(), KnotsAfter, InPlan.NodeCountBefore, InPlan.KnotCountBefore);

	Out += TEXT("Estimated crossings decreased; some crossings may remain. Compilation, runtime behavior and rendered layout are not verified. Inspect the graph before compiling or saving.\n");
	if (Failed.Num() > 0 || Skipped.Num() > 0)
	{
		Out = TEXT("partial: some requested wires were not routed.\n") + Out;
	}

	if (Failed.Num() > 0)
	{
		Out += FString::Printf(TEXT("Failed: %s\n"), *FString::Join(Failed, TEXT("; ")));
	}
	if (Skipped.Num() > 0)
	{
		Out += FString::Printf(TEXT("Skipped, because the graph changed since the preview: %s\n"),
			*FString::Join(Skipped, TEXT("; ")));
	}
	return Out;
}
