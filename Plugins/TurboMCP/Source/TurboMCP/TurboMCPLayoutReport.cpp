// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPLayoutReport.h"
#include "K2Node_ExecutionSequence.h"
#include "TurboMCPGraphTidy.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Knot.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "UObject/Class.h"

#include "TurboMCPGraphReachability.h"
#include "TurboMCPNodeId.h"

namespace
{
	/**
	 * How far apart two pin centres are placed when their positions are computed.
	 *
	 * The editor's own row spacing is not exposed, so a constant is used. Any
	 * constant gives a consistent comparison, which is what a layout measure needs.
	 */
	constexpr double PinRowSpacing = 24.0;

	/** How many nodes overlap findings are printed for before the list caps. */
	constexpr int32 MaxOverlapFindings = 20;

	/** How many strays are printed before the list caps. */
	constexpr int32 MaxStrayFindings = 12;

	/** How far from the origin a node counts as "at the origin". */
	constexpr double OriginBand = 64.0;

	/** How far a link has to reach for a node at the origin not to count as stray. */
	constexpr double StrayLinkDistance = 1200.0;

	/** True when the two rectangles share more than a single point. */
	bool RectsOverlap(const FTurboMCPLayoutReport::FRect& A,
		const FTurboMCPLayoutReport::FRect& B)
	{
		// Touching edges are not an overlap: two nodes side by side share a border
		// and look fine.
		return A.MinX < B.MaxX && B.MinX < A.MaxX
			&& A.MinY < B.MaxY && B.MinY < A.MaxY;
	}

	/** True for a node the layout checks should ignore. */
	bool IsIgnored(const UEdGraphNode* InNode)
	{
		// A comment box is a region, not a node in the flow, and a knot is a
		// reroute that the format treats as invisible.
		return InNode == nullptr
			|| InNode->IsA<UEdGraphNode_Comment>()
			|| InNode->IsA<UK2Node_Knot>();
	}

	/** The signature of a node's logic, for the "repeats logic" check. */
	FString LogicSignature(const UEdGraphNode* InNode)
	{
		FString Signature = InNode->GetClass()->GetName();

		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode))
		{
			Signature += TEXT("::");
			Signature += Call->GetFunctionName().ToString();
		}
		else if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(InNode))
		{
			Signature += TEXT("::");
			Signature += Variable->GetVarName().ToString();
		}

		return Signature;
	}
}

FTurboMCPLayoutReport::FRect FTurboMCPLayoutReport::RectOf(const UEdGraphNode* InNode)
{
	FRect Rect;
	if (InNode == nullptr)
	{
		return Rect;
	}

	Rect.MinX = InNode->NodePosX;
	Rect.MinY = InNode->NodePosY;
	const FVector2D Size=FTurboMCPGraphTidy::AlignmentNodeSize(InNode);
	Rect.MaxX = InNode->NodePosX + Size.X;
	Rect.MaxY = InNode->NodePosY + Size.Y;
	return Rect;
}

FVector2D FTurboMCPLayoutReport::PinPosition(const UEdGraphPin* InPin)
{
	if (InPin == nullptr)
	{
		return FVector2D::ZeroVector;
	}

	const UEdGraphNode* Node = InPin->GetOwningNodeUnchecked();
	if (Node == nullptr)
	{
		return FVector2D::ZeroVector;
	}

	// Inputs come down the left edge, outputs down the right, each in its own
	// order, which is how the editor lays a node out.
	const bool bIsInput = InPin->Direction == EGPD_Input;

	int32 Index = 0;
	for (const UEdGraphPin* Pin : Node->Pins)
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

	const double EdgeX = bIsInput
		? static_cast<double>(Node->NodePosX)
		: static_cast<double>(Node->NodePosX + Node->NodeWidth);

	// The first row starts below the node's title, which is not measured here, so a
	// fixed offset is used. It cancels out when two wires are compared.
	const double EdgeY = static_cast<double>(Node->NodePosY)
		+ PinRowSpacing * (Index + 1);

	return FVector2D(EdgeX, EdgeY);
}

double FTurboMCPLayoutReport::WireLength(const UEdGraphPin* InPin)
{
	if (InPin == nullptr || InPin->LinkedTo.Num() == 0)
	{
		return 0.0;
	}

	double Total = 0.0;
	const FVector2D Start = PinPosition(InPin);

	for (const UEdGraphPin* Other : InPin->LinkedTo)
	{
		if (Other == nullptr)
		{
			continue;
		}
		Total += FVector2D::Distance(Start, PinPosition(Other));
	}
	return Total;
}

FTurboMCPLayoutReport::FReport FTurboMCPLayoutReport::Measure(
	const UEdGraph* InGraph, const FString& InAssetPath, double AlignmentTolerance)
{
	FReport Report;
	if (InGraph == nullptr)
	{
		Report.Summary = TEXT("no graph was given");
		return Report;
	}

	// Every node, connected or not, and never twice: the graph's own array is walked
	// once and each node is added to this list once.
	TArray<UEdGraphNode*> FlowNodes;
	TArray<UEdGraphNode_Comment*> Boxes;

	bool bExtentSet = false;

	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			Boxes.Add(Comment);
			continue;
		}


		FlowNodes.Add(Node);
		++Report.NodeCount;

		const FRect Rect = RectOf(Node);
		if (!bExtentSet)
		{
			Report.Extent = Rect;
			bExtentSet = true;
		}
		else
		{
			Report.Extent.MinX = FMath::Min(Report.Extent.MinX, Rect.MinX);
			Report.Extent.MinY = FMath::Min(Report.Extent.MinY, Rect.MinY);
			Report.Extent.MaxX = FMath::Max(Report.Extent.MaxX, Rect.MaxX);
			Report.Extent.MaxY = FMath::Max(Report.Extent.MaxY, Rect.MaxY);
		}
	}

	Report.CommentBoxCount = Boxes.Num();

	// Nesting, computed from the rectangles, exactly as R-COMMENT-LIST does.
	for (const UEdGraphNode_Comment* Box : Boxes)
	{
		const FRect BoxRect = RectOf(Box);
		for (const UEdGraphNode_Comment* Other : Boxes)
		{
			if (Other == Box)
			{
				continue;
			}
			const FRect OtherRect = RectOf(Other);
			const bool bContains =
				OtherRect.MinX <= BoxRect.MinX && OtherRect.MinY <= BoxRect.MinY
				&& OtherRect.MaxX >= BoxRect.MaxX && OtherRect.MaxY >= BoxRect.MaxY;
			const bool bIdentical =
				OtherRect.MinX == BoxRect.MinX && OtherRect.MinY == BoxRect.MinY
				&& OtherRect.Width() == BoxRect.Width()
				&& OtherRect.Height() == BoxRect.Height();

			if (bContains && !bIdentical)
			{
				++Report.NestedBoxCount;
				break;
			}
		}
	}

	// The wires. Each link is counted once, from the output side, so an input that
	// is linked to an output is not measured twice.
	for (const UEdGraphNode* Node : FlowNodes)
	{
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			const bool bIsExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

			for (const UEdGraphPin* Other : Pin->LinkedTo)
			{
				if (Other == nullptr)
				{
					continue;
				}

				++Report.WireCount;
				const double Length = FVector2D::Distance(PinPosition(Pin), PinPosition(Other));
				Report.TotalWireLength += Length;
				Report.LongestWire = FMath::Max(Report.LongestWire, Length);

				// A wire is backward when its target node STARTS to the left of where
				// its source node ENDS. Comparing node positions alone would call a
				// node stacked under its predecessor fine, and the spec says that is
				// backward.
				const UEdGraphNode* Source = Pin->GetOwningNodeUnchecked();
				const UEdGraphNode* Target = Other->GetOwningNodeUnchecked();
				if (Source == nullptr || Target == nullptr || Source == Target)
				{
					continue;
				}

                bool IntentionalSequenceLane=false;
                if(bIsExec && Source->IsA<UK2Node_ExecutionSequence>())
                {
                    int32 OutputIndex=0;
                    for(const UEdGraphPin* Candidate:Source->Pins)
                        if(Candidate && Candidate->Direction==EGPD_Output && Candidate->PinType.PinCategory==TEXT("exec"))
                        {
                            if(Candidate==Pin) { IntentionalSequenceLane=OutputIndex>0 && FTurboMCPGraphTidy::AlignmentPinPosition(Other).Y>FTurboMCPGraphTidy::AlignmentPinPosition(Pin).Y; break; }
                            ++OutputIndex;
                        }
                }
                if (!IntentionalSequenceLane && FMath::Abs(FTurboMCPGraphTidy::AlignmentPinPosition(Pin).Y - FTurboMCPGraphTidy::AlignmentPinPosition(Other).Y) > AlignmentTolerance && Report.Findings.Num() < 128)
                {
                    FFinding Finding;
                    Finding.Kind = bIsExec ? TEXT("exec-not-straight") : TEXT("pin-not-level");
                    Finding.Detail = FString::Printf(TEXT("%s.%s -> %s.%s: vertical offset %+.1f graph units (target Y minus source Y; positive means target is lower; branch lanes may intentionally differ)"), *TurboMCPNodeId::MakeForNode(Source), *Pin->PinName.ToString(), *TurboMCPNodeId::MakeForNode(Target), *Other->PinName.ToString(), FTurboMCPGraphTidy::AlignmentPinPosition(Other).Y-FTurboMCPGraphTidy::AlignmentPinPosition(Pin).Y);
                    Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Source));
                    Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Target));
                    Report.Findings.Add(MoveTemp(Finding));
                }
				const FRect SourceRect = RectOf(Source);
				const FRect TargetRect = RectOf(Target);

				if (TargetRect.MinX < SourceRect.MaxX)
				{
					FFinding Finding;
					Finding.Kind = bIsExec ? TEXT("backward-exec") : TEXT("backward-data");
					Finding.Detail = FString::Printf(
						TEXT("%s -> %s : the target starts at x %.0f and the source ends at x %.0f, so the wire runs backwards"),
						*TurboMCPNodeId::MakeForNode(Source),
						*TurboMCPNodeId::MakeForNode(Target),
						TargetRect.MinX, SourceRect.MaxX);
					Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Source));
					Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Target));
					Report.Findings.Add(MoveTemp(Finding));
				}
			}
		}
	}

	Report.bHasNoWires = Report.WireCount == 0;

	// Overlapping nodes. Reported once per pair, in array order, so a pair is never
	// listed twice.
	int32 OverlapsFound = 0;
	for (int32 A = 0; A < FlowNodes.Num(); ++A)
	{
		for (int32 B = A + 1; B < FlowNodes.Num(); ++B)
		{
			const FRect RectA = RectOf(FlowNodes[A]);
			const FRect RectB = RectOf(FlowNodes[B]);
			if (!RectsOverlap(RectA, RectB))
			{
				continue;
			}

			++OverlapsFound;
			if (OverlapsFound > MaxOverlapFindings)
			{
				continue;
			}

			FFinding Finding;
			Finding.Kind = TEXT("overlap");
			Finding.Detail = FString::Printf(TEXT("%s overlaps %s"),
				*TurboMCPNodeId::MakeForNode(FlowNodes[A]),
				*TurboMCPNodeId::MakeForNode(FlowNodes[B]));
			Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(FlowNodes[A]));
			Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(FlowNodes[B]));
			Report.Findings.Add(MoveTemp(Finding));
		}
	}
	if (OverlapsFound > MaxOverlapFindings)
	{
		FFinding Finding;
		Finding.Kind = TEXT("overlap");
		Finding.Detail = FString::Printf(TEXT("(%d more overlapping pair(s) not listed)"),
			OverlapsFound - MaxOverlapFindings);
		Report.Findings.Add(MoveTemp(Finding));
	}

	struct FSegment { const UEdGraphPin* A; const UEdGraphPin* B; FVector2D P; FVector2D Q; };
	TArray<FSegment> Segments;
	for (const UEdGraphNode* N : FlowNodes)
		for (const UEdGraphPin* P : N->Pins)
			if (P && P->Direction == EGPD_Output)
				for (const UEdGraphPin* Q : P->LinkedTo)
					if (Q && Q->GetOwningNodeUnchecked())
					{
						if (Segments.Num() >= 2048) { Report.bCrossingsCapped = true; continue; }
						Segments.Add({P, Q, PinPosition(P), PinPosition(Q)});
					}
	auto Side = [](const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{ return (B.X-A.X)*(C.Y-A.Y) - (B.Y-A.Y)*(C.X-A.X); };
	for (int32 I = 0; I < Segments.Num(); ++I)
		for (int32 J = I+1; J < Segments.Num(); ++J)
		{
			const FSegment& A = Segments[I]; const FSegment& B = Segments[J];
			if (A.A == B.A || A.A == B.B || A.B == B.A || A.B == B.B) continue;
			if (Side(A.P,A.Q,B.P)*Side(A.P,A.Q,B.Q) < 0 && Side(B.P,B.Q,A.P)*Side(B.P,B.Q,A.Q) < 0)
				++Report.WireCrossings;
		}
	if (Report.WireCrossings || Report.bCrossingsCapped)
	{
		FFinding F; F.Kind = TEXT("wire-crossings");
		F.Detail = FString::Printf(TEXT("%d straight-segment crossings%s; approximate pin geometry, not rendered splines"),
			Report.WireCrossings, Report.bCrossingsCapped ? TEXT(" in first 2048 wires (capped)") : TEXT(""));
		Report.Findings.Add(MoveTemp(F));
	}

	// A getter placed to the right of, or under, its consumer. A getter takes no
	// input, so it can always sit to the left of what reads it, and a getter to the
	// right makes the reader follow a wire backwards.
	for (const UEdGraphNode* Node : FlowNodes)
	{
		const UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node);
		if (Getter == nullptr || !Getter->IsNodePure())
		{
			continue;
		}

        // Validated/exec-dependent getters must follow execution flow, not pure-input placement.
        bool bHasExecutionPin = false;
        for (const UEdGraphPin* GetterPin : Getter->Pins)
            if (GetterPin && GetterPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                bHasExecutionPin = true;
        if (bHasExecutionPin) continue;

		const FRect GetterRect = RectOf(Getter);

		for (const UEdGraphPin* Pin : Getter->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (const UEdGraphPin* Consumer : Pin->LinkedTo)
			{
				if (Consumer == nullptr)
				{
					continue;
				}
				const UEdGraphNode* ConsumerNode = Consumer->GetOwningNodeUnchecked();
				if (ConsumerNode == nullptr || ConsumerNode == Getter)
				{
					continue;
				}

				const FRect ConsumerRect = RectOf(ConsumerNode);

				const bool bToTheRight = GetterRect.MinX >= ConsumerRect.MaxX;
				const bool bUnderneath = GetterRect.MinY >= ConsumerRect.MaxY
					&& GetterRect.MaxX > ConsumerRect.MinX; // Below-left is an intentional forward-reading layout.

				if (bToTheRight || bUnderneath)
				{
					FFinding Finding;
					Finding.Kind = TEXT("getter-place");
					Finding.Detail = FString::Printf(
						TEXT("%s (%s) is %s its consumer %s"),
						*TurboMCPNodeId::MakeForNode(Getter),
						*Getter->GetVarName().ToString(),
						bToTheRight ? TEXT("to the right of") : TEXT("under"),
						*TurboMCPNodeId::MakeForNode(ConsumerNode));
					Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Getter));
					Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(ConsumerNode));
					Report.Findings.Add(MoveTemp(Finding));
				}
			}
		}
	}

	// Nodes sitting at the origin with nothing near them. A node at the origin is
	// only a finding when everything it links to is far away, because a small graph
	// legitimately lives near the origin.
	int32 StraysFound = 0;
	for (const UEdGraphNode* Node : FlowNodes)
	{
		const FRect Rect = RectOf(Node);
		const bool bAtOrigin = FMath::Abs(Rect.MinX) <= OriginBand
			&& FMath::Abs(Rect.MinY) <= OriginBand;
		if (!bAtOrigin)
		{
			continue;
		}

		double Nearest = TNumericLimits<double>::Max();
		bool bHasAnyLink = false;

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Other : Pin->LinkedTo)
			{
				if (Other == nullptr)
				{
					continue;
				}
				const UEdGraphNode* OtherNode = Other->GetOwningNodeUnchecked();
				if (OtherNode == nullptr || OtherNode == Node)
				{
					continue;
				}
				bHasAnyLink = true;
				Nearest = FMath::Min(Nearest,
					FVector2D::Distance(PinPosition(Pin), PinPosition(Other)));
			}
		}

		// A node with no links at all is reported by the disconnected check instead,
		// so this one is about a node that IS linked but parked far away.
		if (!bHasAnyLink || Nearest < StrayLinkDistance)
		{
			continue;
		}

		++StraysFound;
		if (StraysFound > MaxStrayFindings)
		{
			continue;
		}

		FFinding Finding;
		Finding.Kind = TEXT("stray");
		Finding.Detail = FString::Printf(
			TEXT("%s sits at the origin and its nearest link is %.0f away"),
			*TurboMCPNodeId::MakeForNode(Node), Nearest);
		Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Node));
		Report.Findings.Add(MoveTemp(Finding));
	}

	// Disconnected groups use the same execution/value liveness rule as the graph
	// reader and dead-node preview. Keeping one walk prevents those tools from
	// disagreeing about data-only links and pure value producers.
	TSet<const UEdGraphNode*> Reachable;
	TurboMCPGraphReachability::MarkReachable(InGraph, Reachable);

	// The logic the connected graph already contains, by signature.
	TSet<FString> ConnectedSignatures;
	for (const UEdGraphNode* Node : FlowNodes)
	{
		if (Reachable.Contains(Node))
		{
			ConnectedSignatures.Add(LogicSignature(Node));
		}
	}

	TArray<const UEdGraphNode*> Disconnected;
	for (const UEdGraphNode* Node : FlowNodes)
	{
		if (!Reachable.Contains(Node))
		{
			Disconnected.Add(Node);
		}
	}

	if (Disconnected.Num() > 0)
	{
		FFinding Finding;
		Finding.Kind = TEXT("disconnected");
		Finding.Detail = FString::Printf(
			TEXT("%d node(s) cannot be reached from any node without an incoming link:"),
			Disconnected.Num());

		// One line per node, with whether its logic already exists in the connected
		// graph. That is what turns a list of orphans into a decision.
		TSet<FString> ReportedSignatures;
		for (const UEdGraphNode* Node : Disconnected)
		{
			const FString Id = TurboMCPNodeId::MakeForNode(Node);
			const FString Signature = LogicSignature(Node);
			const bool bRepeats = ConnectedSignatures.Contains(Signature);
			const bool bAlreadyListed = ReportedSignatures.Contains(Signature);

			Finding.NodeIds.Add(Id);

			FString Line = FString::Printf(TEXT("\n      %s  %s"),
				*Id, *TurboMCPNodeId::GetNodeTitle(Node));
			if (bRepeats)
			{
				Line += TEXT("  [repeats logic that exists in the connected graph]");
			}
			else if (bAlreadyListed)
			{
				Line += TEXT("  [duplicates another disconnected node's logic]");
			}
			else
			{
				Line += TEXT("  [logic not present in the connected graph]");
			}
			Finding.Detail += Line;
			ReportedSignatures.Add(Signature);
		}

		Report.Findings.Add(MoveTemp(Finding));
	}

    // Review candidates only: an empty override/custom event can be intentional.
    for(const UEdGraphNode* Node:InGraph->Nodes) if(Node && Node->IsA<UK2Node_Event>())
    {
        bool HasBody=false;
        for(const UEdGraphPin* Pin:Node->Pins) if(Pin && Pin->Direction==EGPD_Output && Pin->PinType.PinCategory==TEXT("exec") && !Pin->LinkedTo.IsEmpty()) HasBody=true;
        if(!HasBody)
        {
            FFinding Finding; Finding.Kind=TEXT("empty-event-entry");
            Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(Node));
            Finding.Detail=TEXT("Event has no execution body; review candidate, not automatically safe to delete (an empty override can suppress parent behavior).");
            const FRect EventRect=RectOf(Node);
            for(const UEdGraphNode* Candidate:InGraph->Nodes) if(Candidate && Candidate->IsA<UEdGraphNode_Comment>())
            {
                const FRect BoxRect=RectOf(Candidate);
                if(EventRect.MinX>=BoxRect.MinX && EventRect.MinY>=BoxRect.MinY && EventRect.MaxX<=BoxRect.MaxX && EventRect.MaxY<=BoxRect.MaxY)
                    Finding.Detail+=TEXT(" Containing box: ")+TurboMCPNodeId::MakeForNode(Candidate)+TEXT(" (")+Candidate->NodeComment+TEXT(").");
            }
            Report.Findings.Add(MoveTemp(Finding));
        }
    }
    for(const UEdGraphNode* BoxNode:InGraph->Nodes) if(BoxNode && BoxNode->IsA<UEdGraphNode_Comment>())
    {
        const FRect Bounds=RectOf(BoxNode); double OccupiedArea=0; int32 Members=0;
        for(const UEdGraphNode* Node:InGraph->Nodes) if(Node && !Node->IsA<UEdGraphNode_Comment>())
        {
            const FRect R=RectOf(Node);
            if(R.MinX>=Bounds.MinX && R.MinY>=Bounds.MinY && R.MaxX<=Bounds.MaxX && R.MaxY<=Bounds.MaxY)
            { OccupiedArea+=(R.MaxX-R.MinX)*(R.MaxY-R.MinY); ++Members; }
        }
        const double BoxArea=(Bounds.MaxX-Bounds.MinX)*(Bounds.MaxY-Bounds.MinY);
        if(Members>0 && BoxArea>0 && OccupiedArea/BoxArea<0.20)
        {
            FFinding Finding; Finding.Kind=TEXT("sparse-comment-box"); Finding.NodeIds.Add(TurboMCPNodeId::MakeForNode(BoxNode));
            Finding.Detail=FString::Printf(TEXT("%d contained nodes occupy approximately %.0f%% of box area (summed node rectangles). Review whitespace; fit to explicit member IDs with the comment tool if unwanted."),Members,100.0*OccupiedArea/BoxArea);
            Report.Findings.Add(MoveTemp(Finding));
        }
    }


    // Each endpoint belongs to its smallest enclosing comment (nested boxes
    // therefore have a single, deterministic owner). Unboxed endpoints are explicit.
    TMap<const UEdGraphNode*, const UEdGraphNode*> BoxOwners;
    for (const UEdGraphNode* Node : InGraph->Nodes)
    {
        if (!Node || Node->IsA<UEdGraphNode_Comment>()) continue;
        const FRect R = RectOf(Node);
        const UEdGraphNode* Owner = nullptr;
        double OwnerArea = TNumericLimits<double>::Max();
        for (const UEdGraphNode* Box : InGraph->Nodes)
        {
            if (!Box || !Box->IsA<UEdGraphNode_Comment>()) continue;
            const FRect B = RectOf(Box);
            const double Area = (B.MaxX-B.MinX)*(B.MaxY-B.MinY);
            if (R.MinX >= B.MinX && R.MinY >= B.MinY && R.MaxX <= B.MaxX && R.MaxY <= B.MaxY
                && (Area < OwnerArea || (Area == OwnerArea && Owner && Box->GetName() < Owner->GetName())))
            { Owner = Box; OwnerArea = Area; }
        }
        BoxOwners.Add(Node, Owner);
    }
    struct FCrossBoxWire { double Length; FString Text; FString SourceId; FString TargetId; };
    TArray<FCrossBoxWire> CrossBoxWires;
    double CrossBoxTotal = 0.0;
    for (const UEdGraphNode* Node : InGraph->Nodes)
    {
        if (!Node || Node->IsA<UEdGraphNode_Comment>()) continue;
        const UEdGraphNode* SourceBox = BoxOwners.FindRef(Node);
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output) continue;
            TSet<const UEdGraphPin*> SeenTargets;
            for (const UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || SeenTargets.Contains(Other)) continue;
                SeenTargets.Add(Other);
                const UEdGraphNode* Target = Other->GetOwningNode();
                if (!Target || Target->GetGraph() != InGraph) continue;
                const UEdGraphNode* TargetBox = BoxOwners.FindRef(Target);
                if (SourceBox == TargetBox) continue;
                const double Length = FVector2D::Distance(PinPosition(Pin), PinPosition(Other));
                const FString SourceId = TurboMCPNodeId::MakeForNode(Node);
                const FString TargetId = TurboMCPNodeId::MakeForNode(Target);
                const FString SourceOwner = SourceBox ? TurboMCPNodeId::MakeForNode(SourceBox) : TEXT("unboxed");
                const FString TargetOwner = TargetBox ? TurboMCPNodeId::MakeForNode(TargetBox) : TEXT("unboxed");
                CrossBoxWires.Add({Length, FString::Printf(TEXT("%.1f units: %s.%s [%s] -> %s.%s [%s]"),
                    Length, *SourceId, *Pin->PinName.ToString(), *SourceOwner,
                    *TargetId, *Other->PinName.ToString(), *TargetOwner), SourceId, TargetId});
                CrossBoxTotal += Length;
            }
        }
    }
    CrossBoxWires.Sort([](const FCrossBoxWire& A, const FCrossBoxWire& B)
    { return A.Length == B.Length ? A.Text < B.Text : A.Length > B.Length; });
    FFinding CrossBoxFinding;
    CrossBoxFinding.Kind = TEXT("cross-box-wires");
    CrossBoxFinding.Detail = FString::Printf(TEXT("%d cross-box connections; total %.1f graph units (estimated straight-line pin distances). Smallest fully enclosing comment owns each endpoint; unboxed-to-unboxed excluded. Longest first:"), CrossBoxWires.Num(), CrossBoxTotal);
    for (const FCrossBoxWire& Wire : CrossBoxWires)
    {
        CrossBoxFinding.Detail += TEXT("\n      ") + Wire.Text;
        CrossBoxFinding.NodeIds.AddUnique(Wire.SourceId);
        CrossBoxFinding.NodeIds.AddUnique(Wire.TargetId);
    }
    Report.Findings.Add(MoveTemp(CrossBoxFinding));

	if (Report.bHasNoWires)
	{
		// The spec asks for this in words rather than as a zero.
		Report.Summary = TEXT("this graph has no wires at all, so there is no flow ");
		Report.Summary += TEXT("to measure. Every layout measure below is therefore ");
		Report.Summary += TEXT("about node placement only.");
	}

	return Report;
}

FTurboMCPLayoutReport::FCrossBoxMeasure FTurboMCPLayoutReport::MeasureCrossBox(
    const UEdGraph* Graph, const TMap<const UEdGraphNode*, FRect>& RectOverrides)
{
    FCrossBoxMeasure Result;
    if (!Graph) return Result;
    auto Bounds = [&RectOverrides](const UEdGraphNode* Node)
    {
        const FRect* Override = RectOverrides.Find(Node);
        return Override ? *Override : RectOf(Node);
    };
    TMap<const UEdGraphNode*, const UEdGraphNode*> Owners;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || Node->IsA<UEdGraphNode_Comment>()) continue;
        const FRect R = Bounds(Node);
        const UEdGraphNode* Owner = nullptr;
        double OwnerArea = TNumericLimits<double>::Max();
        for (const UEdGraphNode* Box : Graph->Nodes)
        {
            if (!Box || !Box->IsA<UEdGraphNode_Comment>()) continue;
            const FRect B = Bounds(Box);
            const double Area = B.Width()*B.Height();
            if (R.MinX >= B.MinX && R.MinY >= B.MinY && R.MaxX <= B.MaxX && R.MaxY <= B.MaxY
                && (Area < OwnerArea || (Area == OwnerArea && Owner && Box->GetName() < Owner->GetName())))
            { Owner = Box; OwnerArea = Area; }
        }
        Owners.Add(Node, Owner);
    }
    auto Position = [&Bounds](const UEdGraphPin* Pin)
    {
        const UEdGraphNode* Node = Pin->GetOwningNode();
        const FRect R = Bounds(Node);
        return PinPosition(Pin) + FVector2D(R.MinX-Node->NodePosX, R.MinY-Node->NodePosY);
    };
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || Node->IsA<UEdGraphNode_Comment>()) continue;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output) continue;
            TSet<const UEdGraphPin*> Seen;
            for (const UEdGraphPin* Other : Pin->LinkedTo)
            {
                if (!Other || Seen.Contains(Other)) continue;
                Seen.Add(Other);
                const UEdGraphNode* Target = Other->GetOwningNode();
                if (!Target || Target->GetGraph() != Graph || Owners.FindRef(Node) == Owners.FindRef(Target)) continue;
                ++Result.Count;
                Result.Length += FVector2D::Distance(Position(Pin), Position(Other));
            }
        }
    }
    return Result;
}
