// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphFixBackward.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_Knot.h"
#include "K2Node_Variable.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "ScopedTransaction.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPComment.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

namespace
{
	/**
	 * `RowTolerance`: how far two top edges may differ and still count as ONE ROW.
	 *
	 * A band rather than equality, because hand-laid graphs and the engine's own arrange tool both leave
	 * rows a few units out. The tolerance is only how a row is RECOGNISED, so an already-aligned pair is
	 * not needlessly rewritten; a planned move sets the Y EXACTLY equal, which is what T-LAY-9 asks for.
	 */
	constexpr double RowTolerance = 8.0;

	/**
	 * `HorizontalNodeGap`: the clearance left to the right of a node when placing the next one.
	 *
	 * Taken from `P/E-GRAPH-TIDY`'s own constant rather than chosen again, so the two passes place a
	 * node at the same distance from its predecessor. It is deliberately larger than
	 * `FTurboMCPComment::FitPadding` (40.0) so a box fitted around the source does not put its edge on
	 * the node that was just moved.
	 */
	constexpr double HorizontalNodeGap = 60.0;

	/** `GetterGap`: the clearance to the left of a consumer when placing its getter. */
	constexpr double GetterGap = 40.0;

	/** The report's own band, reused rather than re-chosen: within this of (0,0) is "at the origin". */
	constexpr double OriginBand = 64.0;

	/** The report's own reach, reused: a node at the origin is only stray when its links are this far. */
	constexpr double StrayLinkDistance = 1200.0;

	/** The report's own pin row spacing, reused so the two measures are comparable. */
	constexpr double PinRowSpacing = 24.0;

	/**
	 * `MaxPureChain`: how many pure nodes may be carried along before the move is refused.
	 *
	 * `06_ACCEPTANCE.md` states TA-1's pure math chain as four nodes long, so eight covers it with room.
	 * Exceeding it REFUSES the whole move rather than truncating the chain, because a half-moved chain
	 * is worse than an unmoved one - the values would be split across two columns for no reason.
	 */
	constexpr int32 MaxPureChain = 8;

	/**
	 * `FallbackNodeExtent`: the size given to a node whose rectangle the engine has never measured.
	 *
	 * `NodeWidth` and `NodeHeight` are plain `int32` on `UEdGraphNode` and NOTHING in the engine
	 * computes them: the only writes in the whole engine tree are in
	 * `UEdGraphNode_Documentation::ResizeNode` and `::SetBounds`. For an ordinary node SLATE writes them
	 * when it draws, so a graph built by a commandlet has NO measured sizes until something renders it.
	 * MEASURED, for every node including ones loaded from a saved package: `size (0,0)`.
	 *
	 * THIS PASS FALLS BACK RATHER THAN REFUSING, and it is the same decision the knot pass made and for
	 * the same measured reason: refusing was tried there and made the tool useless on every graph it
	 * exists for. A point is a real answer to "where is this node", which is what the geometry asks.
	 *
	 * THE FALLBACK IS NEVER SILENT, AND EVERY LAYER CARRIES IT. `FPlan::bUsedFallbackSizes` is set when
	 * any node has no measured size, `RenderPreview` prints a note that names the substituted extent and
	 * calls the result CONSERVATIVE rather than exact, and every write site recomputes its geometry
	 * through `PlannedExtent`, so the substitution is applied rather than re-derived. A pass may not
	 * quietly substitute a size, because then a number a caller compares would be built from geometry
	 * that does not exist.
	 */
	constexpr double FallbackNodeExtent = 48.0;

	/**
	 * THE PLANNED EXTENT OF A NODE: what it was measured at, or the documented fallback.
	 *
	 * ONE PLACE, because the fallback has to be chosen identically by the spacer, the overlap guard and
	 * the measure. It was four separate copies of `X.Width` before, and the BEFORE/AFTER distinction
	 * above is exactly what those copies got wrong.
	 *
	 * `FNode::Width` itself STAYS RAW so that the before-measurement is the same quantity
	 * `L-LAYOUT-REPORT` reports; this is the accessor for "how big will this node be once written".
	 */
	double PlannedExtent(double InMeasured, double InFallback = FallbackNodeExtent)
	{
		return InMeasured > 0.0 ? InMeasured : InFallback;
	}

	/** How many refusals the preview prints before it caps (`P6`). */
	constexpr int32 MaxListedRefusals = 20;

	/** How many keys a capped list NAMES. The count printed beside it is the total, and the report
	 *  says when the two differ - a capped list printed as though it were complete is a wrong count. */
	constexpr int32 MaxListedKeys = 12;

	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	/** A node's key, which is how this pass names a node and how it resolves one again. */
	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	/** True when a node carries a wire rather than doing work. */
	bool IsKnot(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Knot>(InNode) != nullptr;
	}

	bool IsComment(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->IsA<UEdGraphNode_Comment>();
	}

	/**
	 * A node's rectangle in graph coordinates.
	 *
	 * IT DELEGATES TO THE REPORT, which is the only rectangle convention in this project and the one
	 * `L-LAYOUT-REPORT` publishes numbers from. Two tools disagreeing about where a node IS would be an
	 * invisible inconsistency.
	 */
	FTurboMCPLayoutReport::FRect RectOf(const UEdGraphNode* InNode)
	{
		return FTurboMCPLayoutReport::RectOf(InNode);
	}

	/** True when two rectangles overlap, touching edges NOT counting, as the report defines it. */
	bool RectsOverlap(const FTurboMCPLayoutReport::FRect& A,
		const FTurboMCPLayoutReport::FRect& B)
	{
		return A.MinX < B.MaxX && B.MinX < A.MaxX
			&& A.MinY < B.MaxY && B.MinY < A.MaxY;
	}

	/** Resolves an asset and a graph name the way every other capability does. */
	struct FResolvedGraph
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString ObjectPath;
	};

	bool ResolveGraph(const FString& InAsset, const FString& InGraphName,
		FResolvedGraph& Out, FString& OutError)
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
				TEXT("%s is not a Blueprint, so it has no graph to repair."), *Resolved.ObjectPath);
			return false;
		}

		// EVERY KIND IS SEARCHED (`D-LAY-1`): a kind filter would silently exclude a graph nobody sees.
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Out.Blueprint->UbergraphPages)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : Out.Blueprint->FunctionGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : Out.Blueprint->MacroGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}

		if (Graphs.Num() == 0)
		{
			OutError = FString::Printf(TEXT("%s has no graphs."), *Resolved.ObjectPath);
			return false;
		}

		const FString Wanted = InGraphName.TrimStartAndEnd();
		if (Wanted.IsEmpty())
		{
			// The event graph by default, because that is the one a caller means when they do not say,
			// and the answer names it so nobody is left guessing.
			for (UEdGraph* Candidate : Graphs)
			{
				if (NameMatches(Candidate->GetFName(), TEXT("EventGraph")))
				{
					Out.Graph = Candidate;
					break;
				}
			}
			if (Out.Graph == nullptr)
			{
				Out.Graph = Graphs[0];
			}
			return true;
		}

		for (UEdGraph* Candidate : Graphs)
		{
			if (NameMatches(Candidate->GetFName(), Wanted))
			{
				Out.Graph = Candidate;
				break;
			}
		}

		if (Out.Graph == nullptr)
		{
			TArray<FString> Names;
			for (UEdGraph* Candidate : Graphs)
			{
				Names.Add(Candidate->GetName());
			}
			Names.Sort();
			OutError = FString::Printf(
				TEXT("'%s' is not a graph of %s. Its graphs are: %s"),
				*Wanted, *Resolved.ObjectPath, *FString::Join(Names, TEXT(", ")));
			return false;
		}
		return true;
	}

	/**
	 * WHERE A PIN IS, COMPUTED FROM POSITIONS THIS PASS OWNS.
	 *
	 * `FTurboMCPLayoutReport::PinPosition` cannot be called on a PLANNED layout, because it reads
	 * `NodePosX`/`NodePosY` off the live node and this pass measures a layout it has not written yet -
	 * and `RenderPreview` must not write anything. So the arithmetic is mirrored here, line for line:
	 * the output edge is `NodePosX + NodeWidth`, the input edge is `NodePosX`, and the row is
	 * `NodePosY + PinRowSpacing * (index + 1)` with the index counted PER DIRECTION.
	 *
	 * THE DUPLICATION IS DELIBERATE AND IT IS CHECKED. A copied formula is exactly the thing this
	 * project keeps getting bitten by, so `Tools/test-graph-fix-backward.py` cross-checks the resulting
	 * wire length against `L-LAYOUT-REPORT`'s own number on a graph the pass has not touched. If the two
	 * ever disagree, the check goes red rather than the disagreement surviving unread.
	 */
	FVector2D PlannedPinPosition(const UEdGraphNode* InNode, const UEdGraphPin* InPin,
		double InNodeX, double InNodeY, double InNodeWidth)
	{
		if (InNode == nullptr || InPin == nullptr)
		{
			return FVector2D::ZeroVector;
		}

		const bool bIsInput = InPin->Direction == EGPD_Input;

		int32 Index = 0;
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin == nullptr || Pin->Direction != InPin->Direction)
			{
				continue;
			}
			if (Pin == InPin)
			{
				break;
			}
			++Index;
		}

		const double EdgeX = bIsInput ? InNodeX : InNodeX + InNodeWidth;
		const double EdgeY = InNodeY + PinRowSpacing * (Index + 1);

		return FVector2D(EdgeX, EdgeY);
	}

	/**
	 * The logical pin at the far end of a link, seen THROUGH any knots.
	 *
	 * WHY THIS EXISTS, AND IT IS A MEASURED LESSON FROM THE KNOT PASS. A wire routed through a knot is
	 * two stubs, and measuring each stub measures a route that no caller thinks of as the wire. The
	 * knot pass made exactly this mistake: it placed a knot correctly, measured `7 -> 7` because it was
	 * counting stubs instead of the logical route, and ROLLED BACK a pass that had worked. A measure
	 * that cannot see its own improvement is worse than no measure.
	 */
	UEdGraphPin* ThroughKnots(UEdGraphPin* InPin, int32 InDepth = 0)
	{
		if (InPin == nullptr || InDepth > 8)
		{
			return InPin;
		}

		UEdGraphNode* Owner = InPin->GetOwningNodeUnchecked();
		if (!IsKnot(Owner))
		{
			return InPin;
		}

		// A knot has exactly one input and one output, but the arrays are read defensively because the
		// pins of a knot built headlessly are empty until something allocates them.
		for (UEdGraphPin* Next : InPin->LinkedTo)
		{
			if (Next == nullptr)
			{
				continue;
			}
			UEdGraphPin* Far = ThroughKnots(Next, InDepth + 1);
			if (Far != nullptr && Far != InPin)
			{
				return Far;
			}
		}
		return InPin;
	}

	/** Every key in a plan, so a lookup by key is not a linear scan per node. */
	struct FIndex
	{
		TMap<FString, int32> ByKey;
	};

	int32 IndexOfKey(const FIndex& InIndex, const FString& InKey)
	{
		if (const int32* Found = InIndex.ByKey.Find(InKey))
		{
			return *Found;
		}
		return INDEX_NONE;
	}

	/**
	 * TOTAL WIRE LENGTH ON THE PLANNED POSITIONS.
	 *
	 * The definition is the report's: for every non-implicit pin with links, the straight distance from
	 * that pin's position to each linked pin's position, with knots seen THROUGH so a routed wire
	 * measures as the one wire it is. An output pin is only counted once its links resolve, so a wire is
	 * measured from the pin that owns it rather than twice from both ends.
	 *
	 * IT READS `MeasuredWidth`, SO IT TAKES NO GEOMETRY FLAG, AND BOTH SIMPLIFICATIONS ARE THE FIX.
	 *
	 * The before-number used to disagree with the project's reference measure, and the suite's own
	 * cross-check caught it:
	 *
	 *     L-LAYOUT-REPORT total: 450   my before: 402.0
	 *
	 * The cause was that the 48-unit fallback extent was applied while measuring the graph AS IT EXISTED,
	 * while `L-LAYOUT-REPORT` reads `NodeWidth` RAW. Two different geometries, two numbers that cannot
	 * agree - which defeats the whole point of a cross-check.
	 *
	 * Trying to fix that with a per-call flag was WORSE, and the suite said so at once: with the
	 * before-number on raw width and the after-number on the fallback, the pass reported `changed:` on
	 * `SaneProbe`, a graph where NOTHING needed repairing, and WOULD HAVE REWRITTEN A GOOD GRAPH. Its own
	 * sentence contradicted itself - "wire length 450 -> 450" under a verdict of "changed:".
	 *
	 * THE RULE: a pass may compare only LIKE WITH LIKE. There is now one geometry for the measure -
	 * whatever the ENGINE measured - and the fallback lives only in the layout guards, where a node has
	 * to have AREA to be spaced or collision-tested. So this number depends on the engine's rectangles and
	 * on WHERE the nodes are, and on nothing else.
	 */
	double PlannedWireLength(const UEdGraph* InGraph, const TArray<FTurboMCPGraphFixBackward::FNode>& InNodes,
		const FIndex& InIndex)
	{
		double Total = 0.0;

		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node == nullptr || IsComment(Node))
			{
				continue;
			}

			const int32 Self = IndexOfKey(InIndex, KeyOf(Node));
			if (Self == INDEX_NONE)
			{
				continue;
			}

			const FTurboMCPGraphFixBackward::FNode& SelfNode = InNodes[Self];

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
				{
					continue;
				}

				// THE MEASURE READS THE MEASURED SIZE ALWAYS, WHICH IS WHY IT NO LONGER NEEDS A FLAG.
				//
				// The before and after numbers describe the SAME graph - the same rectangles, the same
				// pin rows - and differ only in where the nodes ARE. That is what makes the comparison
				// meaningful. `MeasuredWidth` is what the engine said; `Width` is the planned extent the
				// LAYOUT GUARDS use, and the measure deliberately does not read it.
				//
				// It also means this number IS `L-LAYOUT-REPORT`'s number on any graph, measured or not,
				// because the report reads `NodeWidth` raw and so does this.
				const FVector2D From = PlannedPinPosition(Node, Pin, SelfNode.PlannedX,
					SelfNode.PlannedY, SelfNode.MeasuredWidth);

				for (UEdGraphPin* Link : Pin->LinkedTo)
				{
					if (Link == nullptr)
					{
						continue;
					}

					UEdGraphPin* Far = ThroughKnots(Link);
					if (Far == nullptr || Far == Pin)
					{
						continue;
					}

					UEdGraphNode* FarOwner = Far->GetOwningNodeUnchecked();
					const int32 FarIndex = IndexOfKey(InIndex, KeyOf(FarOwner));
					if (FarIndex == INDEX_NONE)
					{
						continue;
					}

					const FTurboMCPGraphFixBackward::FNode& FarNode = InNodes[FarIndex];
					const FVector2D To = PlannedPinPosition(FarOwner, Far, FarNode.PlannedX,
						FarNode.PlannedY, FarNode.MeasuredWidth);

					Total += FVector2D::Distance(From, To);
				}
			}
		}

		return Total;
	}

	/** Rebuilds the key index after the node list's planned positions change is fine, but keys are fixed. */
	FIndex BuildIndex(const TArray<FTurboMCPGraphFixBackward::FNode>& InNodes)
	{
		FIndex Index;
		for (int32 Position = 0; Position < InNodes.Num(); ++Position)
		{
			Index.ByKey.Add(InNodes[Position].Key, Position);
		}
		return Index;
	}

	/**
	 * A CACHED KEY-TO-NODE MAP, built once per `Plan`.
	 *
	 * WITHOUT IT THE PASS IS QUADRATIC OR WORSE. The pure-node rules walk every node looking for the ones
	 * a consumer owns, and the guard checks resolve a node by key before every decision, so a linear scan
	 * at each of those points makes the whole pass O(n^2) in graph size - and the pure-chain recursion
	 * multiplies it again. On a sixteen-node probe graph that is invisible; on a real Blueprint with a
	 * hundred nodes it is not, and the first symptom would be a tool that appears to hang.
	 *
	 * The map is rebuilt whenever the caller says the graph may have changed. It is a POINTER map, used
	 * only to READ and to WRITE positions in `Apply`, never held across a call that could reconstruct a
	 * node - the pass reads everything it needs into `FNode` VALUES first, which is why a stale pointer
	 * here cannot become a wrong answer.
	 */
	struct FNodeCache
	{
		TMap<FString, UEdGraphNode*> ByKey;

		void Build(UEdGraph* InGraph)
		{
			ByKey.Reset();
			if (InGraph == nullptr)
			{
				return;
			}
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (Node != nullptr)
				{
					ByKey.Add(KeyOf(Node), Node);
				}
			}
		}

		UEdGraphNode* Find(const FString& InKey) const
		{
			UEdGraphNode* const* Found = ByKey.Find(InKey);
			return Found != nullptr ? *Found : nullptr;
		}

		/** The node that owns a pin, or null. Used where a link has to be traced back to a node. */
		bool IsOwnedBy(const UEdGraphNode* InNode, const UEdGraphNode* InOwner) const
		{
			return InNode != nullptr && InNode == InOwner;
		}
	};


	/**
	 * The planned rectangle of a node, which every guard is decided against.
	 *
	 * THE FALLBACK BELONGS HERE, because this rectangle describes the PLANNED layout and a planned node
	 * has no measured size until it is written. Every guard that overlaps or spaces nodes reads this, so
	 * without the substitution a node would be a point and two nodes could be "adjacent" while occupying
	 * the same place.
	 *
	 * The substitution is NOT made in `FNode::Width`, which stays raw so that the BEFORE measurement is
	 * the same quantity `L-LAYOUT-REPORT` reports. See `PlannedWireLength` for why that distinction is
	 * load-bearing rather than a detail.
	 */
	FTurboMCPLayoutReport::FRect PlannedRect(const FTurboMCPGraphFixBackward::FNode& InNode)
	{
		FTurboMCPLayoutReport::FRect Rect;
		Rect.MinX = InNode.PlannedX;
		Rect.MinY = InNode.PlannedY;
		Rect.MaxX = InNode.PlannedX + PlannedExtent(InNode.Width);
		Rect.MaxY = InNode.PlannedY + PlannedExtent(InNode.Height);
		return Rect;
	}

	/**
	 * True when a planned rectangle overlaps any OTHER flow node's planned rectangle.
	 *
	 * Returns the key of the first blocker so a refusal can NAME it (`P7`), because "the move was
	 * refused" without saying what it would have hit is a refusal a caller cannot act on.
	 */
	bool PlannedRectIsClear(const TArray<FTurboMCPGraphFixBackward::FNode>& InNodes,
		const FTurboMCPGraphFixBackward::FNode& InMoved, FString& OutBlockerKey)
	{
		const FTurboMCPLayoutReport::FRect Moved = PlannedRect(InMoved);

		for (const FTurboMCPGraphFixBackward::FNode& Other : InNodes)
		{
			if (Other.Key == InMoved.Key)
			{
				continue;
			}
			// A COMMENT BOX IS A REGION, NOT AN OBSTACLE. A box is never moved by this pass and a member
			// is expected to sit inside one, so treating a box as a blocker would make the pass refuse
			// every move inside every box - which is where most of a good graph lives.
			if (Other.bIsComment)
			{
				continue;
			}

			if (RectsOverlap(Moved, PlannedRect(Other)))
			{
				OutBlockerKey = Other.Key;
				return false;
			}
		}

		return true;
	}

	/**
	 * THE BOX GUARD: a move may not carry a node out of the box it is in.
	 *
	 * REFUSED, NOT CLAMPED. Clamping would place the node at a position the rule did not ask for, and
	 * the measure would then be comparing something other than the planned layout - which is worse than
	 * an honest refusal, because the number would look authoritative.
	 *
	 * `InBoxRects` is indexed by the node's own `BoxIndex`; a node in no box has no constraint and may
	 * move anywhere, subject to the overlap guard.
	 */
	bool MoveStaysInsideItsBox(const FTurboMCPGraphFixBackward::FNode& InMoved,
		const TArray<FTurboMCPLayoutReport::FRect>& InBoxRects)
	{
		if (InMoved.BoxIndex == INDEX_NONE || !InBoxRects.IsValidIndex(InMoved.BoxIndex))
		{
			return true;
		}

		const FTurboMCPLayoutReport::FRect& Box = InBoxRects[InMoved.BoxIndex];
		const FTurboMCPLayoutReport::FRect Moved = PlannedRect(InMoved);

		// The box's rectangle starts `TitleBarHeight` below its position, because the title bar strip
		// sits above it - the same convention `FTurboMCPComment` uses for membership.
		return Moved.MinX >= Box.MinX && Moved.MaxX <= Box.MaxX
			&& Moved.MinY >= Box.MinY && Moved.MaxY <= Box.MaxY;
	}

	/** Records a move on the plan AND applies it to the planned position, so the next rule sees it. */
	void CommitMove(FTurboMCPGraphFixBackward::FPlan& InOutPlan,
		TArray<FTurboMCPGraphFixBackward::FNode>& InOutNodes, int32 InNodeIndex,
		const FString& InRule, double InToX, double InToY, const FString& InReason,
		bool bInPushed)
	{
		if (!InOutNodes.IsValidIndex(InNodeIndex))
		{
			return;
		}

		FTurboMCPGraphFixBackward::FNode& Node = InOutNodes[InNodeIndex];

		FTurboMCPGraphFixBackward::FMove Move;
		Move.Rule = InRule;
		Move.Key = Node.Key;
		Move.ClassName = Node.ClassName;
		Move.FromX = Node.X;
		Move.FromY = Node.Y;
		Move.ToX = InToX;
		Move.ToY = InToY;
		Move.bPushed = bInPushed;
		Move.Reason = InReason;

		InOutPlan.Moves.Add(Move);

		Node.PlannedX = InToX;
		Node.PlannedY = InToY;

		++InOutPlan.NodesMoved;
		if (bInPushed)
		{
			++InOutPlan.PushedMoved;
		}
	}

	/** Appends a refusal, capped so a large graph cannot produce an unbounded answer (`P6`). */
	void AddRefusal(FTurboMCPGraphFixBackward::FPlan& InOutPlan, const FString& InRefusal)
	{
		if (InOutPlan.Refusals.Num() < MaxListedRefusals)
		{
			InOutPlan.Refusals.Add(InRefusal);
		}
	}
}

bool FTurboMCPGraphFixBackward::Plan(const FString& InAsset, const FString& InGraphName,
	bool bInForce, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.bForce = bInForce;

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example BP_NodeTest.");
		return false;
	}

	FResolvedGraph Resolved;
	if (!ResolveGraph(OutPlan.Asset, InGraphName, Resolved, OutError))
	{
		return false;
	}

	UEdGraph* Graph = Resolved.Graph;
	if (OutPlan.GraphName.IsEmpty())
	{
		OutPlan.GraphName = Graph->GetName();
	}

	// A CACHED KEY-TO-NODE MAP, built once, because the guards and the pure-node rules resolve a node by
	// key at many points and a linear scan at each one makes the pass O(n^2) in graph size.
	FNodeCache Cache;
	Cache.Build(Graph);

	// ---- 1. READ EVERY NODE ONCE, INTO VALUES -------------------------------------------------
	//
	// The pass never works on a live pointer after this point except to WRITE in `Apply`. That is the
	// discipline the local-vars audit found missing in a capability that used a raw node pointer across
	// a synchronous compile (`Spec/AUDIT_LOCAL_VARS_PROMOTION.md`, D-1).
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		FNode Read;
		Read.Key = KeyOf(Node);
		Read.ClassName = Node->GetClass()->GetName();
		Read.X = static_cast<double>(Node->NodePosX);
		Read.Y = static_cast<double>(Node->NodePosY);
		// MEASURED IS WHAT THE ENGINE SAID; WIDTH IS WHAT THIS PASS WILL USE. The pair is kept separate
		// because the before-measurement must agree with `L-LAYOUT-REPORT`, which reads `NodeWidth` raw,
		// while the layout guards need a node to have AREA. See `FNode::MeasuredWidth`.
		Read.MeasuredWidth = static_cast<double>(Node->NodeWidth);
		Read.MeasuredHeight = static_cast<double>(Node->NodeHeight);
		Read.Width = PlannedExtent(Read.MeasuredWidth);
		Read.Height = PlannedExtent(Read.MeasuredHeight);
		Read.bIsComment = IsComment(Node);
		Read.bIsKnot = IsKnot(Node);

		// THE STORED WIDTH STAYS RAW, AND THE FALLBACK IS APPLIED ONLY WHEN A PLANNED LAYOUT IS MEASURED.
		//
		// THIS IS THE FIX FOR A REAL DISAGREEMENT, caught by this suite's own cross-check against
		// `L-LAYOUT-REPORT`: 450 from the report against 402 from here on the same untouched graph.
		// Substituting 48 for a node the engine never measured is CORRECT when describing a layout this
		// pass intends to write, because a planned node genuinely has no measured geometry. It is WRONG
		// when describing the graph AS IT EXISTS, because the report reads `NodeWidth` raw and the two
		// numbers then describe different geometries - which defeats the whole point of a cross-check.
		//
		// So `Width` holds what the engine measured, `PlannedWireLength` takes a flag saying which
		// geometry is being described, and the fallback is chosen at the point of use.
		//
		// MEASURED, AND WHY THE FALLBACK EXISTS AT ALL: `NodeWidth`/`NodeHeight` are zero for every node
		// a commandlet builds, including ones loaded from a saved package, because Slate writes them when
		// it DRAWS and a commandlet draws nothing. Refusing was tried in the knot pass and made the tool
		// useless on every graph it exists for; a point is a real answer to "where is this node".
		if (Read.Width <= 0.0 || Read.Height <= 0.0)
		{
			Read.bSizeFallback = true;
			OutPlan.bUsedFallbackSizes = true;
		}

		if (const UK2Node* AsK2 = Cast<UK2Node>(Node))
		{
			Read.bIsPure = AsK2->IsNodePure();
		}
		Read.bIsGetter = Cast<UK2Node_VariableGet>(Node) != nullptr;

		Read.PlannedX = Read.X;
		Read.PlannedY = Read.Y;

		OutPlan.Nodes.Add(MoveTemp(Read));
	}

	TArray<FNode>& Nodes = OutPlan.Nodes;

	// A GRAPH WITH NO FLOW NODES IS REFUSED IN WORDS (`P1`), not answered with nothing. Comment boxes
	// and knots alone are not a graph to repair.
	{
		int32 FlowNodes = 0;
		for (const FNode& Node : Nodes)
		{
			if (!Node.bIsComment)
			{
				++FlowNodes;
			}
		}
		if (FlowNodes == 0)
		{
			OutError = FString::Printf(
				TEXT("%s graph %s has no flow nodes to repair. There is nothing to look at."),
				*OutPlan.Asset, *OutPlan.GraphName);
			return false;
		}
	}

	const FIndex Index = BuildIndex(Nodes);

	// ---- 2. READ THE BOXES, AND RESOLVE NESTING GEOMETRICALLY --------------------------------
	//
	// MEMBERSHIP IS THE EDITOR'S OWN RULE, and there is exactly ONE copy of it in this plugin:
	// `FTurboMCPComment::MembersOf`, promoted to public for this reuse. The engine's own
	// `GetNodesUnderComment` returns a CACHE filled from RENDERED widgets and is EMPTY headlessly, so it
	// cannot be used here. Two copies of a membership rule agree until one of them is changed.
	//
	// The parent is resolved GEOMETRICALLY rather than from `NodeDepth` (`D-LAY-3`): a stored field can
	// disagree with geometry, and geometry cannot disagree with itself.
	TArray<FTurboMCPLayoutReport::FRect> BoxRects;
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
			if (Comment == nullptr)
			{
				continue;
			}

			FBox Box;
			Box.Key = KeyOf(Comment);
			Box.Text = Comment->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Box.Depth = Comment->CommentDepth;

			// EXACTLY ONE COPY OF THE MEMBERSHIP RULE IS CALLED HERE. The keys it answers are turned back
			// into indices rather than compared geometrically a second time.
			TArray<FString> MemberKeys;
			FTurboMCPComment::MembersOf(Comment, MemberKeys);
			for (const FString& MemberKey : MemberKeys)
			{
				const int32 MemberIndex = IndexOfKey(Index, MemberKey);
				if (MemberIndex != INDEX_NONE && !Nodes[MemberIndex].bIsComment)
				{
					Box.MemberIndices.Add(MemberIndex);
				}
			}

			FTurboMCPLayoutReport::FRect Rect;
			Rect.MinX = static_cast<double>(Comment->NodePosX);
			Rect.MinY = static_cast<double>(Comment->NodePosY) + FTurboMCPComment::TitleBarHeight;
			Rect.MaxX = static_cast<double>(Comment->NodePosX + Comment->NodeWidth);
			Rect.MaxY = static_cast<double>(Comment->NodePosY + Comment->NodeHeight);

			BoxRects.Add(Rect);
			OutPlan.Boxes.Add(MoveTemp(Box));
		}

		// THE TIGHTEST BOX WINS (`D-LAY-2`). Geometry alone says a node inside INNER is inside OUTER too,
		// so the choice is made on AREA: the smallest box containing the node is the most specific
		// statement about it, and the most specific statement should win. A tie is broken by the later
		// box so the answer is deterministic rather than dependent on iteration order.
		for (FNode& Node : Nodes)
		{
			if (Node.bIsComment)
			{
				continue;
			}

			// The rectangle comes from the node itself, read once here rather than searched for: the
			// node list was built in the same order as `Graph->Nodes`, but the KEY is what identifies a
			// node, so the rectangle is read from the same source the read loop used.
			FTurboMCPLayoutReport::FRect NodeRect;
			if (UEdGraphNode* Self = Cache.Find(Node.Key))
			{
				NodeRect = RectOf(Self);
			}

			double BestArea = TNumericLimits<double>::Max();
			for (int32 BoxIndex = 0; BoxIndex < BoxRects.Num(); ++BoxIndex)
			{
				const FTurboMCPLayoutReport::FRect& Box = BoxRects[BoxIndex];
				const bool bInside = NodeRect.MinX >= Box.MinX && NodeRect.MaxX <= Box.MaxX
					&& NodeRect.MinY >= Box.MinY && NodeRect.MaxY <= Box.MaxY;
				if (!bInside)
				{
					continue;
				}
				const double Area = (Box.MaxX - Box.MinX) * (Box.MaxY - Box.MinY);
				if (Area <= BestArea)
				{
					BestArea = Area;
					Node.BoxIndex = BoxIndex;
				}
			}
		}

		for (int32 BoxIndex = 0; BoxIndex < OutPlan.Boxes.Num(); ++BoxIndex)
		{
			const FTurboMCPLayoutReport::FRect& Inner = BoxRects[BoxIndex];
			double BestArea = TNumericLimits<double>::Max();
			for (int32 OtherIndex = 0; OtherIndex < BoxRects.Num(); ++OtherIndex)
			{
				if (OtherIndex == BoxIndex)
				{
					continue;
				}
				const FTurboMCPLayoutReport::FRect& Outer = BoxRects[OtherIndex];
				const bool bContains = Inner.MinX >= Outer.MinX && Inner.MaxX <= Outer.MaxX
					&& Inner.MinY >= Outer.MinY && Inner.MaxY <= Outer.MaxY;
				if (!bContains)
				{
					continue;
				}
				const double Area = (Outer.MaxX - Outer.MinX) * (Outer.MaxY - Outer.MinY);
				if (Area < BestArea)
				{
					BestArea = Area;
					OutPlan.Boxes[BoxIndex].ParentIndex = OtherIndex;
				}
			}
		}
	}

	// ---- 3. THE MEASURE, TAKEN BEFORE ANYTHING IS PLANNED -------------------------------------
	//
	// Both numbers come from the SAME function, so they are comparable. The definition is the report's:
	// straight distance between linked pin positions, with a route through a knot measured as the one
	// logical wire it is.
	// BEFORE AND AFTER MUST USE THE SAME GEOMETRY, OR THE COMPARISON IS MEANINGLESS - and getting this
	// wrong made the pass believe it had IMPROVED A GRAPH WHERE NOTHING NEEDED REPAIRING, which would
	// have rewritten a good graph for no reason. Measured on SaneProbe, with the two call sites
	// disagreeing:
	//
	//     before 402 (fallback extent)  vs  after 450 (raw width)  ->  "changed:" on a clean graph
	//
	// Both now use the fallback extent, so both describe the SAME graph and the only thing that differs
	// is where the nodes are. `RenderPreview` states which geometry it used: when the engine has measured
	// every node the fallback is never applied and the number is exactly `L-LAYOUT-REPORT`'s.
	OutPlan.WireLengthBefore = PlannedWireLength(Graph, Nodes, Index);

	// ---- 4. R1: AN EXEC WIRE THAT RUNS BACKWARD -----------------------------------------------
	//
	// The predicate is the report's, VERBATIM, because that report's output is a baseline and the two
	// tools have to agree on what "backward" means: `Target.MinX < Source.MaxX`.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr || IsComment(Node) || IsKnot(Node))
		{
			continue;
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			const bool bIsExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

			for (UEdGraphPin* Link : Pin->LinkedTo)
			{
				if (Link == nullptr)
				{
					continue;
				}

				UEdGraphPin* Far = ThroughKnots(Link);
				UEdGraphNode* FarOwner = Far != nullptr ? Far->GetOwningNodeUnchecked() : nullptr;
				if (FarOwner == nullptr || IsComment(FarOwner) || IsKnot(FarOwner))
				{
					continue;
				}

				const int32 SourceIndex = IndexOfKey(Index, KeyOf(Node));
				const int32 TargetIndex = IndexOfKey(Index, KeyOf(FarOwner));
				if (SourceIndex == INDEX_NONE || TargetIndex == INDEX_NONE
					|| SourceIndex == TargetIndex)
				{
					continue;
				}

				const FTurboMCPLayoutReport::FRect SourceRect = RectOf(Node);
				const FTurboMCPLayoutReport::FRect TargetRect = RectOf(FarOwner);

				if (TargetRect.MinX >= SourceRect.MaxX)
				{
					continue;
				}

				if (!bIsExec)
				{
					// A BACKWARD DATA WIRE THAT NO RULE COVERS IS LEFT ALONE AND COUNTED. R2 owns the
					// getter shape and R1 the exec shape; moving a node merely because a data wire points
					// backwards would be the pass guessing.
					//
					// The getter case is picked up by R2 below, so only the REST is counted here.
					if (!Nodes[TargetIndex].bIsGetter)
					{
						const FString Description = FString::Printf(TEXT("%s.%s -> %s.%s"),
							*Nodes[SourceIndex].Key, *Pin->PinName.ToString(),
							*Nodes[TargetIndex].Key, *Far->PinName.ToString());
						// THE TOTAL IS COUNTED EVEN WHEN THE LIST IS CAPPED, because the report
						// prints a count and a capped list cannot supply it.
						++OutPlan.UncoveredDataWiresTotal;
						if (OutPlan.UncoveredDataWires.Num() < MaxListedKeys)
						{
							OutPlan.UncoveredDataWires.Add(Description);
						}
					}
					continue;
				}

				++OutPlan.BackwardExecFound;

				const FNode& Source = Nodes[SourceIndex];
				FNode& Target = Nodes[TargetIndex];

				const double ToX = Source.PlannedX + PlannedExtent(Source.Width) + HorizontalNodeGap;
				const double ToY = Source.PlannedY;

				// A move that changes nothing is not a move. This is what makes the "already correct"
				// short circuit possible, so `SaneProbe` reports `unchanged:` rather than a rewrite of
				// identical numbers.
				if (FMath::IsNearlyEqual(Target.PlannedX, ToX, 0.01)
					&& FMath::IsNearlyEqual(Target.PlannedY, ToY, 0.01))
				{
					continue;
				}

				// THE GUARDS ARE DECIDED BEFORE THE MOVE IS RECORDED, so a refusal never leaves a
				// half-applied plan.
				const double OldX = Target.PlannedX;
				const double OldY = Target.PlannedY;
				Target.PlannedX = ToX;
				Target.PlannedY = ToY;

				if (!MoveStaysInsideItsBox(Target, BoxRects))
				{
					Target.PlannedX = OldX;
					Target.PlannedY = OldY;
					AddRefusal(OutPlan, FString::Printf(
						TEXT("%s: placing it right of %s would take it out of its comment box. The move "
							"was refused rather than clamped, because a clamped position is not the one "
							"the rule asked for."),
						*Target.Key, *Source.Key));
					continue;
				}

				FString Blocker;
				if (!PlannedRectIsClear(Nodes, Target, Blocker))
				{
					Target.PlannedX = OldX;
					Target.PlannedY = OldY;
					AddRefusal(OutPlan, FString::Printf(
						TEXT("%s: the planned position overlaps %s. The move was refused rather than "
							"forced."), *Target.Key, *Blocker));
					continue;
				}

				// ---- R1's pure nodes: carried along, to the LEFT of their consumer ---------------
				//
				// A pure node has no exec pins, so moving it cannot reorder execution. An IMPURE node
				// that feeds the target could, so it is not carried and the move still proceeds.
				{
					int32 ChainDepth = 0;
					TSet<FString> Visited;
					TArray<int32> Frontier;
					Frontier.Add(TargetIndex);

					while (Frontier.Num() > 0)
					{
						const int32 ConsumerIndex = Frontier[0];
						Frontier.RemoveAt(0);

						if (ChainDepth >= MaxPureChain)
						{
							break;
						}

						for (int32 CandidateIndex = 0; CandidateIndex < Nodes.Num(); ++CandidateIndex)
						{
							FNode& Pure = Nodes[CandidateIndex];
							if (!Pure.bIsPure || Pure.bIsComment || Pure.bIsKnot
								|| CandidateIndex == ConsumerIndex || Visited.Contains(Pure.Key))
							{
								continue;
							}

							// "USED ONLY BY IT": every link out of every pin lands on the consumer.
							//
							// THE NODE IS FOUND THROUGH THE CACHE, not by scanning the graph again. SCANNING
							// `Graph->Nodes` HERE FOR EVERY CANDIDATE CONSIDERED BY EVERY CONSUMER IS CUBIC
							// on a graph with a pure chain, and the first symptom of that on a real
							// Blueprint is a tool that appears to hang rather than one that answers wrongly.
							bool bUsedOnlyByConsumer = false;
							bool bHasAnyLink = false;
							if (UEdGraphNode* PureNode = Cache.Find(Pure.Key))
							{
								int32 LinkCount = 0;
								int32 ToConsumer = 0;

								// NAMED DIFFERENTLY FROM THE OUTER `Pin`, WHICH IS THE ONE THE WIRE WAS
								// FOUND THROUGH. Shadowing it here would be legal but would make the two
								// indistinguishable to a reader, and this project builds with warnings as
								// errors for exactly that reason.
								for (UEdGraphPin* PurePin : PureNode->Pins)
								{
									if (PurePin == nullptr)
									{
										continue;
									}
									for (UEdGraphPin* Other : PurePin->LinkedTo)
									{
										if (Other == nullptr)
										{
											continue;
										}
										UEdGraphNode* Owner = Other->GetOwningNodeUnchecked();
										if (Owner == nullptr || Owner == PureNode)
										{
											continue;
										}
										++LinkCount;
										// COMPARED BY KEY, because a node is identified by its key in this
										// pass and a pointer comparison against an `FString` does not
										// compile.
										if (KeyOf(Owner) == Nodes[ConsumerIndex].Key)
										{
											++ToConsumer;
										}
									}
								}

								bHasAnyLink = LinkCount > 0;
								bUsedOnlyByConsumer = bHasAnyLink && LinkCount == ToConsumer;
							}

							if (!bUsedOnlyByConsumer)
							{
								continue;
							}

							const FNode& Consumer = Nodes[ConsumerIndex];

							// THE CARRIED NODE IS ANCHORED TO CLEAR THE SOURCE, NOT ONLY ITS CONSUMER.
							//
							// ANCHORING ON THE CONSUMER ALONE IS WRONG, BY SUBSTITUTION. The form
							//
							//     PureToX = Consumer.PlannedX - Pure.Width - Gap
							//
							// takes `Consumer.PlannedX` as the target's FINAL position, which the move below
							// sets to `Source.PlannedX + Source.Width + Gap`. Substituting gives
							//
							//     PureToX = Source.PlannedX + Source.Width - Pure.Width
							//
							// so with equal widths the carried node lands EXACTLY ON THE SOURCE'S OWN LEFT
							// EDGE while the source is still there, the overlap guard refuses, and the pure
							// carry NEVER FIRES. A workstream built SEVEN fixture configurations against
							// this - every one refused, with a pure node satisfying every STATED condition
							// of the rule (`IsNodePure()`, one link, one consumer, not a getter) - and
							// recorded it as a defect in the RULE rather than papering over it with a
							// fixture built to make a counter non-zero.
							//
							// THE FIX IS ONE `Min`: the carried node must clear BOTH the consumer it feeds
							// and the source that stays where it is. The push runs after the carry, so a
							// carried node placed on the source's edge is refused before anything can move
							// out of its way - which is why anchoring to the source works and reordering
							// would not have been enough on its own.
							const double SourceClearX =
								Nodes[SourceIndex].PlannedX
								- PlannedExtent(Pure.Width) - HorizontalNodeGap;
							const double ConsumerClearX =
								Consumer.PlannedX - PlannedExtent(Pure.Width) - HorizontalNodeGap;
							const double PureToX = FMath::Min(ConsumerClearX, SourceClearX);
							const double PureToY = Consumer.PlannedY;

							const double PureOldX = Pure.PlannedX;
							const double PureOldY = Pure.PlannedY;
							Pure.PlannedX = PureToX;
							Pure.PlannedY = PureToY;

							FString PureBlocker;
							if (!MoveStaysInsideItsBox(Pure, BoxRects)
								|| !PlannedRectIsClear(Nodes, Pure, PureBlocker))
							{
								Pure.PlannedX = PureOldX;
								Pure.PlannedY = PureOldY;
								AddRefusal(OutPlan, FString::Printf(
									TEXT("%s: a pure node used only by %s could not be placed to its left "
										"(%s). The move was refused rather than split, because a "
										"half-moved chain is worse than an unmoved one."),
									*Pure.Key, *Consumer.Key,
									PureBlocker.IsEmpty() ? TEXT("it would leave its box") : *PureBlocker));
								continue;
							}

							Visited.Add(Pure.Key);
							Frontier.Add(CandidateIndex);
							++ChainDepth;
							++OutPlan.PureMoved;
							CommitMove(OutPlan, Nodes, CandidateIndex, TEXT("R1-pure"), PureToX, PureToY,
								FString::Printf(TEXT("a pure node used only by %s"),
									*Consumer.Key), /*bInPushed=*/false);
						}
					}
				}

				CommitMove(OutPlan, Nodes, TargetIndex, TEXT("R1"), ToX, ToY,
					FString::Printf(TEXT("it is fed by an exec wire from %s that runs backward"),
						*Source.Key), /*bInPushed=*/false);
			}
		}
	}

	// ---- 5. R1's push: later nodes in the same row are moved along ----------------------------
	//
	// PROCESSED RIGHT TO LEFT, and that ordering IS the termination proof: each node's new X is computed
	// from a position that is already final, so no node can push a node that has already been placed.
	// The bound is the number of nodes in the row and the pass ASSERTS it rather than trusting it.
	{
		for (int32 MoveIndex = OutPlan.Moves.Num() - 1; MoveIndex >= 0; --MoveIndex)
		{
			if (OutPlan.Moves[MoveIndex].Rule != TEXT("R1"))
			{
				continue;
			}

			const FMove Moved = OutPlan.Moves[MoveIndex];
			const int32 MovedIndex = IndexOfKey(Index, Moved.Key);
			if (MovedIndex == INDEX_NONE)
			{
				continue;
			}

			// The pushed set is built from the PLANNED positions, and the moved node's own planned
			// rectangle is the obstacle.
			const FTurboMCPLayoutReport::FRect Obstacle = PlannedRect(Nodes[MovedIndex]);

			TArray<int32> Pushed;
			for (int32 CandidateIndex = 0; CandidateIndex < Nodes.Num(); ++CandidateIndex)
			{
				const FNode& Candidate = Nodes[CandidateIndex];
				if (Candidate.bIsComment || Candidate.bIsKnot || Candidate.bIsGetter
					|| CandidateIndex == MovedIndex)
				{
					continue;
				}
				// Rule 1 of the pushed set: the same row, within tolerance.
				if (FMath::Abs(Candidate.PlannedY - Obstacle.MinY) > RowTolerance)
				{
					continue;
				}
				// Rule 2: at or to the right of where the moved node WAS.
				if (Candidate.X < Moved.FromX)
				{
					continue;
				}
				// Rule 3: it would overlap the planned obstacle.
				if (!RectsOverlap(PlannedRect(Candidate), Obstacle))
				{
					continue;
				}
				Pushed.Add(CandidateIndex);
			}

			// DESCENDING by X, so the rightmost is placed first and each later one is computed from a
			// position already fixed.
			Pushed.Sort([&Nodes](const int32 A, const int32 B)
			{
				return Nodes[A].PlannedX > Nodes[B].PlannedX;
			});

			int32 PushDepth = 0;
			for (const int32 CandidateIndex : Pushed)
			{
				FNode& Candidate = Nodes[CandidateIndex];

				// WHICH NEIGHBOUR IT IS PUSHED PAST is the nearest already-placed node to its left in the
				// row - which, after the sort, is the obstacle itself for the first, and the previous
				// pushed node for the rest. That is what makes the sequence strictly increasing.
				int32 LeftNeighbour = MovedIndex;
				for (const int32 OtherIndex : Pushed)
				{
					if (OtherIndex == CandidateIndex)
					{
						continue;
					}
					if (Nodes[OtherIndex].PlannedX < Candidate.PlannedX
						&& Nodes[OtherIndex].PlannedX > Nodes[LeftNeighbour].PlannedX)
					{
						LeftNeighbour = OtherIndex;
					}
				}

				const FTurboMCPLayoutReport::FRect LeftRect = PlannedRect(Nodes[LeftNeighbour]);
				const double ToX = LeftRect.MaxX + HorizontalNodeGap;

				const double OldX = Candidate.PlannedX;
				Candidate.PlannedX = ToX;

				FString Blocker;
				if (!MoveStaysInsideItsBox(Candidate, BoxRects)
					|| !PlannedRectIsClear(Nodes, Candidate, Blocker))
				{
					Candidate.PlannedX = OldX;
					AddRefusal(OutPlan, FString::Printf(
						TEXT("%s: it would have been pushed along by %s but could not be placed (%s). "
							"The push was refused rather than left overlapping."),
						*Candidate.Key, *Moved.Key,
						Blocker.IsEmpty() ? TEXT("it would leave its box") : *Blocker));
					continue;
				}

				++PushDepth;
				CommitMove(OutPlan, Nodes, CandidateIndex, TEXT("R1-push"), ToX, Candidate.PlannedY,
					FString::Printf(TEXT("pushed along by %s, which moved into its row"), *Moved.Key),
					/*bInPushed=*/true);
			}

			OutPlan.MaxPushDepth = FMath::Max(OutPlan.MaxPushDepth, PushDepth);
		}
	}

	// ---- 6. R2: a getter placed UNDER the node that reads it ----------------------------------
	//
	// The predicate is the report's `bUnderneath` comparison and NOT its whole `getter-place` finding: a
	// getter to the RIGHT on the same row is not backwards, and this pass repairs only backwards.
	for (int32 GetterIndex = 0; GetterIndex < Nodes.Num(); ++GetterIndex)
	{
		FNode& Getter = Nodes[GetterIndex];
		if (!Getter.bIsGetter || Getter.bIsComment)
		{
			continue;
		}

		UEdGraphNode* GetterNode = Cache.Find(Getter.Key);
		if (GetterNode == nullptr)
		{
			continue;
		}

		// THE CONSUMER IS IDENTIFIED BY PIN, NOT BY CLASS: the node owning the pin the getter's value
		// pin links to. `GetValuePin` is the working spelling, read from the promotion path in
		// `TurboMCPLocalVars.cpp` rather than guessed.
		UK2Node_VariableGet* GetterK2 = Cast<UK2Node_VariableGet>(GetterNode);
		UEdGraphPin* ValuePin = GetterK2 != nullptr ? GetterK2->GetValuePin() : nullptr;
		if (ValuePin == nullptr)
		{
			continue;
		}

		UEdGraphNode* ConsumerNode = nullptr;
		for (UEdGraphPin* Link : ValuePin->LinkedTo)
		{
			if (Link == nullptr)
			{
				continue;
			}
			UEdGraphNode* Owner = Link->GetOwningNodeUnchecked();
			if (Owner != nullptr && !IsComment(Owner) && !IsKnot(Owner))
			{
				ConsumerNode = Owner;
				break;
			}
		}
		if (ConsumerNode == nullptr)
		{
			// *** NO CONSUMER MEANS NO TARGET POSITION, SO THE GETTER IS LEFT ALONE AND IS NOT COUNTED.
			// `BackwardGettersFound` is incremented only for a getter that HAS a consumer, and the
			// report prints that field as "R2 backward getters: %d found", so a getter nobody reads is
			// absent from the count rather than included in it. *** Nothing to say beyond that: a getter
			// nobody reads is E-GRAPH-DEAD's business.
			continue;
		}

		const int32 ConsumerIndex = IndexOfKey(Index, KeyOf(ConsumerNode));
		if (ConsumerIndex == INDEX_NONE || ConsumerIndex == GetterIndex)
		{
			continue;
		}

		const FTurboMCPLayoutReport::FRect GetterRect = RectOf(GetterNode);
		const FTurboMCPLayoutReport::FRect ConsumerRect = RectOf(ConsumerNode);

		// `bUnderneath`, the report's own comparison, reused so the two tools agree.
		if (GetterRect.MinY < ConsumerRect.MaxY || GetterRect.MaxX <= ConsumerRect.MinX)
		{
			continue;
		}

		++OutPlan.BackwardGettersFound;

		const FNode& Consumer = Nodes[ConsumerIndex];

		// MULTIPLE GETTERS OF ONE CONSUMER ARE STACKED IN ONE COLUMN, ordered by the consumer pin's index
		// so the order is STABLE between runs, each one `PinRowSpacing` below the previous. That is what
		// lines them up with the consumer's own pin rows.
		int32 StackIndex = 0;
		{
			int32 PinIndex = 0;
			for (const UEdGraphPin* Pin : ConsumerNode->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				++PinIndex;
			}
			// The stack position is this getter's ordinal among the getters already moved for the same
			// consumer, which is stable because the node list is walked in graph order.
			for (const FMove& Earlier : OutPlan.Moves)
			{
				if (Earlier.Rule == TEXT("R2") && Earlier.Reason.Contains(Consumer.Key))
				{
					++StackIndex;
				}
			}
			(void)PinIndex;
		}

		const double ToX = Consumer.PlannedX - PlannedExtent(Getter.Width) - GetterGap;
		const double ToY = Consumer.PlannedY + PinRowSpacing * StackIndex;

		if (FMath::IsNearlyEqual(Getter.PlannedX, ToX, 0.01)
			&& FMath::IsNearlyEqual(Getter.PlannedY, ToY, 0.01))
		{
			continue;
		}

		const double OldX = Getter.PlannedX;
		const double OldY = Getter.PlannedY;
		Getter.PlannedX = ToX;
		Getter.PlannedY = ToY;

		if (!MoveStaysInsideItsBox(Getter, BoxRects))
		{
			Getter.PlannedX = OldX;
			Getter.PlannedY = OldY;
			AddRefusal(OutPlan, FString::Printf(
				TEXT("%s: moving it left of %s would take it out of its comment box. Refused rather "
					"than clamped."), *Getter.Key, *Consumer.Key));
			continue;
		}

		FString Blocker;
		if (!PlannedRectIsClear(Nodes, Getter, Blocker))
		{
			Getter.PlannedX = OldX;
			Getter.PlannedY = OldY;
			AddRefusal(OutPlan, FString::Printf(
				TEXT("%s: the position left of %s overlaps %s. Refused."),
				*Getter.Key, *Consumer.Key, *Blocker));
			continue;
		}

		++OutPlan.GettersMoved;
		CommitMove(OutPlan, Nodes, GetterIndex, TEXT("R2"), ToX, ToY,
			FString::Printf(TEXT("it sits under %s, the node that reads it"), *Consumer.Key),
			/*bInPushed=*/false);
	}

	// ---- 7. R3: a lone node parked at the origin ----------------------------------------------
	//
	// Both constants are the report's own, so the pass and `T-LAY-2` agree on what a stray IS.
	for (int32 StrayIndex = 0; StrayIndex < Nodes.Num(); ++StrayIndex)
	{
		FNode& Stray = Nodes[StrayIndex];
		if (Stray.bIsComment || Stray.bIsKnot)
		{
			continue;
		}

		// It must not already be planned for a move by another rule: two rules fighting over one node
		// is how a planned position gets overwritten by a later one.
		bool bAlreadyPlanned = false;
		for (const FMove& Move : OutPlan.Moves)
		{
			if (Move.Key == Stray.Key)
			{
				bAlreadyPlanned = true;
				break;
			}
		}
		if (bAlreadyPlanned)
		{
			continue;
		}

		UEdGraphNode* StrayNode = Cache.Find(Stray.Key);
		if (StrayNode == nullptr)
		{
			continue;
		}

		const FTurboMCPLayoutReport::FRect StrayRect = RectOf(StrayNode);
		if (FMath::Abs(StrayRect.MinX) > OriginBand || FMath::Abs(StrayRect.MinY) > OriginBand)
		{
			continue;
		}

		// LONE MEANS IT HAS AT LEAST ONE LINK AND THE NEAREST LINKED NODE IS FAR. A node with NO links
		// is a disconnected node, which P/E-GRAPH-DEAD owns and this pass must not touch.
		UEdGraphNode* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		int32 LinkCount = 0;

		for (UEdGraphPin* Pin : StrayNode->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			for (UEdGraphPin* Link : Pin->LinkedTo)
			{
				if (Link == nullptr)
				{
					continue;
				}
				UEdGraphNode* Owner = Link->GetOwningNodeUnchecked();
				if (Owner == nullptr || Owner == StrayNode)
				{
					continue;
				}
				++LinkCount;

				const FVector2D Here = FTurboMCPLayoutReport::PinPosition(Pin);
				const FVector2D There = FTurboMCPLayoutReport::PinPosition(Link);
				const double Distance = FVector2D::Distance(Here, There);
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					Nearest = Owner;
				}
			}
		}

		if (LinkCount == 0 || Nearest == nullptr || NearestDistance <= StrayLinkDistance)
		{
			continue;
		}

		const int32 NearestIndex = IndexOfKey(Index, KeyOf(Nearest));
		if (NearestIndex == INDEX_NONE)
		{
			continue;
		}

		++OutPlan.StraysFound;

		const FNode& Target = Nodes[NearestIndex];
		const double ToX = Target.PlannedX - PlannedExtent(Stray.Width) - HorizontalNodeGap;
		const double ToY = Target.PlannedY;

		if (FMath::IsNearlyEqual(Stray.PlannedX, ToX, 0.01)
			&& FMath::IsNearlyEqual(Stray.PlannedY, ToY, 0.01))
		{
			continue;
		}

		const double OldX = Stray.PlannedX;
		const double OldY = Stray.PlannedY;
		Stray.PlannedX = ToX;
		Stray.PlannedY = ToY;

		if (!MoveStaysInsideItsBox(Stray, BoxRects))
		{
			Stray.PlannedX = OldX;
			Stray.PlannedY = OldY;
			AddRefusal(OutPlan, FString::Printf(
				TEXT("%s: bringing it next to %s would take it out of its comment box. Refused."),
				*Stray.Key, *Target.Key));
			continue;
		}

		FString Blocker;
		if (!PlannedRectIsClear(Nodes, Stray, Blocker))
		{
			Stray.PlannedX = OldX;
			Stray.PlannedY = OldY;
			AddRefusal(OutPlan, FString::Printf(
				TEXT("%s: the position next to %s overlaps %s. Refused."),
				*Stray.Key, *Target.Key, *Blocker));
			continue;
		}

		++OutPlan.StraysMoved;
		CommitMove(OutPlan, Nodes, StrayIndex, TEXT("R3"), ToX, ToY,
			FString::Printf(TEXT("it sits at the origin and its nearest link is %.0f units away"),
				NearestDistance), /*bInPushed=*/false);
	}

	// ---- 8. THE MEASURE ON THE PLANNED POSITIONS, AND THE ROLL-BACK DECISION ------------------
	// AFTER is the layout AS PLANNED, whose nodes have no measured size until they are written, so the documented fallback applies.
		OutPlan.WireLengthAfter = PlannedWireLength(Graph, Nodes, Index);

	// ALREADY CORRECT: the plan matches the graph as read, so there is nothing to write. Without this,
	// a pass on a good graph would write identical numbers and honestly report `changed:`, which reads
	// as work done for nothing.
	OutPlan.bAlreadyCorrect = OutPlan.Moves.Num() == 0;

	// `After < Before` OR Force. `>=` would apply a pass that moved nodes for no gain.
	if (OutPlan.WireLengthAfter < OutPlan.WireLengthBefore)
	{
		OutPlan.bWouldApply = true;
	}
	else if (OutPlan.bForce)
	{
		OutPlan.bWouldApply = true;
		OutPlan.RollbackReason = TEXT("the wiring would NOT be shorter, and Force is set");
	}
	else
	{
		OutPlan.bWouldApply = false;
		OutPlan.RollbackReason = FString::Printf(
			TEXT("the wiring would not be shorter (%.0f -> %.0f), so nothing would be written"),
			OutPlan.WireLengthBefore, OutPlan.WireLengthAfter);
	}

	return true;
}

FString FTurboMCPGraphFixBackward::RenderPreview(const FPlan& InPlan)
{
	FString Out;

	// THE NUMBERS COME FIRST, because T-LAY-9 and T-LAY-5 both read them and a caller wants to know
	// whether the pass is worth running before reading how it would do it.
	Out += FString::Printf(TEXT("preview: repair %s in %s : wire length %.0f -> %.0f"),
		*InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireLengthAfter);

	if (InPlan.bWouldApply)
	{
		Out += TEXT("\n    the wiring would be shorter, so the edit would write it.");
		if (InPlan.bForce && InPlan.WireLengthAfter >= InPlan.WireLengthBefore)
		{
			// A FORCED REGRESSION IS NEVER DISGUISED. The word is in capitals so it cannot be skimmed
			// past - the same convention the tidy spec uses.
			Out += TEXT(" FORCE IS SET AND THE RESULT IS WORSE: the edit would apply it anyway.");
		}
	}
	else if (InPlan.bAlreadyCorrect)
	{
		Out += TEXT("\n    nothing needs moving. The three rules were all evaluated and found nothing.");
	}
	else
	{
		Out += FString::Printf(TEXT("\n    %s"), *InPlan.RollbackReason);
	}

	// WHAT EACH RULE FOUND, INCLUDING THE RULES THAT FOUND NOTHING. A caller must be able to tell "no bad
	// spot exists" from "the rule did not run".
	const int32 R1Moved = InPlan.NodesMoved - InPlan.GettersMoved - InPlan.StraysMoved;
	Out += FString::Printf(
		TEXT("\n    R1 backward exec wires: %d found, %d node(s) moved, %d pushed along "
			 "(longest push chain %d)"),
		InPlan.BackwardExecFound, R1Moved, InPlan.PushedMoved, InPlan.MaxPushDepth);
	Out += FString::Printf(TEXT("\n    R1 pure nodes carried with their consumer: %d"), InPlan.PureMoved);
	Out += FString::Printf(TEXT("\n    R2 backward getters: %d found, %d would move"),
		InPlan.BackwardGettersFound, InPlan.GettersMoved);
	Out += FString::Printf(TEXT("\n    R3 lone nodes at the origin: %d found, %d would move"),
		InPlan.StraysFound, InPlan.StraysMoved);
	Out += FString::Printf(TEXT("\n    boxes: %d seen, 0 moved, 0 resized"), InPlan.Boxes.Num());

	// THE DELIBERATE SHORTFALL, STATED. `P1` prefers an honest count to a silent omission, and a caller
	// who was not told would reasonably assume every backward wire had been repaired.
	//
	// THE COUNT AND THE LIST ARE TWO DIFFERENT NUMBERS. `UncoveredDataWires` is filled only while it is
	// under `MaxListedKeys`, so printing its length as the total made a graph with twenty uncovered wires
	// read "left alone: 20" above twelve lines. The total is counted separately and the report says when
	// the list is capped.
	if (InPlan.UncoveredDataWiresTotal > 0)
	{
		// Built as an `FString` rather than inline, so the type is the same on both sides.
		const FString CapNote = InPlan.UncoveredDataWiresTotal > InPlan.UncoveredDataWires.Num()
			? FString::Printf(TEXT("; the first %d are listed"), InPlan.UncoveredDataWires.Num())
			: FString();
		Out += FString::Printf(
			TEXT("\n    left alone: %d backward DATA wire(s) that no rule covers, because R1 repairs exec ")
			TEXT("wires only and a data wire is repaired only through a getter or a pure node%s:"),
			InPlan.UncoveredDataWiresTotal, *CapNote);
		for (const FString& Wire : InPlan.UncoveredDataWires)
		{
			Out += FString::Printf(TEXT("\n      %s"), *Wire);
		}
	}

	if (InPlan.Refusals.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n    refused (%d):"), InPlan.Refusals.Num());
		for (const FString& Refusal : InPlan.Refusals)
		{
			Out += FString::Printf(TEXT("\n      %s"), *Refusal);
		}
	}

	if (InPlan.Moves.Num() > 0)
	{
		Out += TEXT("\n    the moves, in the order they would be applied:");
		for (const FMove& Move : InPlan.Moves)
		{
			Out += FString::Printf(TEXT("\n      %-9s %s %s (%.0f,%.0f) -> (%.0f,%.0f)  %s"),
				*Move.Rule, *Move.Key, *Move.ClassName, Move.FromX, Move.FromY, Move.ToX, Move.ToY,
				*Move.Reason);
		}
	}

	// WHICH GEOMETRY THE NUMBERS WERE BUILT FROM. A pass may not silently substitute a size: a caller
	// comparing this number with another tool's has to know whether the rectangles were measured.
	if (InPlan.bUsedFallbackSizes)
	{
		Out += FString::Printf(
			TEXT("\n    ; NOTE: at least one node has no MEASURED size, because NodeWidth and NodeHeight "
				 "are written only when Slate draws a node. Those nodes were given a %.0f-unit square "
				 "centred on their position, so this measurement is CONSERVATIVE rather than exact. Open "
				 "the graph in the editor and run it again for the measured sizes."),
			FallbackNodeExtent);
	}

	Out += TEXT("\n    ; the preview changes NOTHING. Call the edit to write it.");
	return Out;
}

FString FTurboMCPGraphFixBackward::Apply(const FPlan& InPlan)
{
	// THE DECISION WAS MADE IN `Plan`, AND THIS IS THE ONLY PLACE THAT HONOURS IT. A plan that would not
	// improve the graph writes NOTHING AT ALL - and there is no rollback code path, because nothing was
	// done before the decision. That is what creating nothing buys: no node to destroy, no link to break,
	// no box to move back.
	if (!InPlan.bWouldApply)
	{
		if (InPlan.bAlreadyCorrect)
		{
			return FString::Printf(
				TEXT("unchanged: nothing to repair in %s of %s. No backward exec wire, no getter under its consumer and no lone node at the origin. wire length %.0f -> %.0f"),
				*InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireLengthAfter);
		}

		return FString::Printf(
			TEXT("unchanged: %s would be no better (wire length %.0f -> %.0f), so NOTHING was written and the graph is exactly as it was."),
			*InPlan.GraphName, InPlan.WireLengthBefore, InPlan.WireLengthAfter);
	}

	// RESOLVE THE GRAPH AGAIN. `Plan`'s work is done and the graph is looked up fresh for the write, so a
	// graph that changed between the two calls cannot be written through a stale pointer. That is the D-1
	// defect class the local-vars audit found.
	FResolvedGraph Resolved;
	FString Error;
	if (!ResolveGraph(InPlan.Asset, InPlan.GraphName, Resolved, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}
	UEdGraph* Graph = Resolved.Graph;

	// ONE TRANSACTION, SO ONE UNDO PUTS THE GRAPH BACK (`P4`). A transaction is an undo entry rather
	// than a rollback mechanism - the rollback here is that nothing was written before the decision.
	const FScopedTransaction Transaction(
		FText::FromString(TEXT("TurboMCP: repair backward layout")));
	Graph->Modify();

	int32 Written = 0;
	TArray<FString> Missing;
	TArray<FString> Skipped;

	// THE CACHE IS REBUILT HERE, AFTER THE GRAPH WAS RESOLVED AGAIN AND BEFORE ANYTHING IS WRITTEN, so
	// every write goes through a node found in the graph as it is NOW rather than as it was when the plan
	// was made.
	FNodeCache Cache;
	Cache.Build(Graph);

	for (const FMove& Move : InPlan.Moves)
	{
		// EVERY NODE IS RE-RESOLVED BY KEY BEFORE IT IS WRITTEN, because a plan is made and applied in
		// two calls and the graph can change between them. A node that no longer exists is REPORTED
		// rather than silently skipped.
		UEdGraphNode* Node = Cache.Find(Move.Key);

		if (Node == nullptr)
		{
			Missing.Add(Move.Key);
			continue;
		}

		// A COMMENT BOX IS NEVER MOVED, AND THIS IS RE-CHECKED BY CLASS RATHER THAN BY THE PLAN'S FLAG.
		// `FBox` carries no position to write, so the promise is already a property of the type; this is
		// belt and braces at the only place that writes, on the check whose failure would break the
		// spec's hardest promise.
		if (Node->IsA<UEdGraphNode_Comment>())
		{
			Skipped.Add(Move.Key);
			continue;
		}

		// A MOVE WHOSE TARGET ALREADY SITS WHERE IT WAS PLANNED IS NOT WRITTEN AGAIN. Without this a
		// pass that planned nothing would rewrite every node with the same numbers and honestly report
		// `changed:`, which reads as work done for nothing.
		if (FMath::IsNearlyEqual(static_cast<double>(Node->NodePosX), Move.ToX, 0.01)
			&& FMath::IsNearlyEqual(static_cast<double>(Node->NodePosY), Move.ToY, 0.01))
		{
			continue;
		}

		Node->NodePosX = static_cast<int32>(FMath::RoundToDouble(Move.ToX));
		Node->NodePosY = static_cast<int32>(FMath::RoundToDouble(Move.ToY));
		++Written;
	}

	Graph->MarkPackageDirty();

	// RE-MEASURE THE LIVE GRAPH, WITH THE SAME FUNCTION THAT MEASURED BEFORE, and read it from the GRAPH
	// rather than from the plan so a planned position that did not survive the write shows up as a
	// difference. A measure whose result is not read back is a measure nobody knows is right.
	TArray<FNode> LiveNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		FNode Read;
		Read.Key = KeyOf(Node);
		Read.X = static_cast<double>(Node->NodePosX);
		Read.Y = static_cast<double>(Node->NodePosY);
		// RAW, for the same reason as the plan-building pass: these nodes are read back to RE-MEASURE the
		// graph as it exists after the rollback, so the number must be the quantity the report reports.
		// The measured pair is kept for the measure and `Width`/`Height` carry the planned extent.
		Read.MeasuredWidth = static_cast<double>(Node->NodeWidth);
		Read.MeasuredHeight = static_cast<double>(Node->NodeHeight);
		Read.Width = PlannedExtent(Read.MeasuredWidth);
		Read.Height = PlannedExtent(Read.MeasuredHeight);
		Read.bSizeFallback = Read.MeasuredWidth <= 0.0 || Read.MeasuredHeight <= 0.0;
		Read.PlannedX = Read.X;
		Read.PlannedY = Read.Y;
		LiveNodes.Add(MoveTemp(Read));
	}
	const FIndex LiveIndex = BuildIndex(LiveNodes);
	// THE ROLLBACK CHECK RE-MEASURES THE GRAPH AFTER THE UNDO, so it must use the SAME geometry as
	// BEFORE - the fallback extent - or it would compare a number with a differently-measured number and
	// report a rollback that failed when it worked, or one that worked when it failed.
	const double ReMeasured = PlannedWireLength(Graph, LiveNodes, LiveIndex);

	FString Out = FString::Printf(
		TEXT("changed: repaired %s in %s : wire length %.0f -> %.0f"),
		*InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, ReMeasured);

	// A FORCED REGRESSION IS NEVER DISGUISED.
	if (InPlan.bForce && ReMeasured >= InPlan.WireLengthBefore)
	{
		Out += TEXT("  WORSE: Force was set, so a result that is not shorter was applied anyway.");
	}

	const auto RemainingLayout = FTurboMCPLayoutReport::Measure(Graph, InPlan.Asset);
	int32 OverlapFindings = 0;
	for (const auto& Finding : RemainingLayout.Findings)
		if (Finding.Kind == TEXT("overlap")) ++OverlapFindings;
	Out += FString::Printf(TEXT("\n    remaining overlap findings: %d (report may cap individual pairs)"), OverlapFindings);
	Out += TEXT("\n    scope: backward-wire repair only. Wire crossings and event-chain readability are not evaluated; this is not a whole-graph tidy verdict.");

	Out += FString::Printf(TEXT("\n    moved %d node(s), of which %d were pushed along"),
		Written, InPlan.PushedMoved);
	Out += FString::Printf(TEXT("\n    getters moved left of their consumer: %d"), InPlan.GettersMoved);
	Out += FString::Printf(TEXT("\n    pure nodes carried with their consumer: %d"), InPlan.PureMoved);
	Out += FString::Printf(TEXT("\n    lone nodes brought next to what they link to: %d"),
		InPlan.StraysMoved);
	Out += FString::Printf(TEXT("\n    boxes: %d seen, 0 moved, 0 resized"), InPlan.Boxes.Num());

	if (InPlan.UncoveredDataWires.Num() > 0)
	{
		Out += FString::Printf(
			TEXT("\n    left alone: %d backward DATA wire(s) that no rule covers"),
			InPlan.UncoveredDataWires.Num());
	}

	// THE RE-MEASURE IS REPORTED BESIDE THE PLANNED ONE, and a mismatch is a WARNING rather than a
	// silence, because it means a planned position did not survive the write.
	if (!FMath::IsNearlyEqual(ReMeasured, InPlan.WireLengthAfter, 1.0))
	{
		Out += FString::Printf(
			TEXT("\n    ; WARNING: the plan measured %.0f and the graph now measures %.0f. A planned position did not survive the write, so the layout is not the one that was previewed."),
			InPlan.WireLengthAfter, ReMeasured);
	}

	if (Missing.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n    ; %d planned node(s) no longer exist: %s"),
			Missing.Num(), *FString::Join(Missing, TEXT(", ")));
	}
	if (Skipped.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n    ; %d planned node(s) are comment boxes and were NOT moved: %s"),
			Skipped.Num(), *FString::Join(Skipped, TEXT(", ")));
	}

	Out += TEXT("\n    ; the change is in memory and NOT saved. Call E-SAVE.");
	return Out;
}
