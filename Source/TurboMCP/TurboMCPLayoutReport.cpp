// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPLayoutReport.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Knot.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "UObject/Class.h"

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
	Rect.MaxX = InNode->NodePosX + InNode->NodeWidth;
	Rect.MaxY = InNode->NodePosY + InNode->NodeHeight;
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
	const UEdGraph* InGraph, const FString& InAssetPath)
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

		if (IsIgnored(Node))
		{
			// A knot is still part of a wire, but not a node in the layout count.
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

	// A getter placed to the right of, or under, its consumer. A getter takes no
	// input, so it can always sit to the left of what reads it, and a getter to the
	// right makes the reader follow a wire backwards.
	for (const UEdGraphNode* Node : FlowNodes)
	{
		const UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node);
		if (Getter == nullptr)
		{
			continue;
		}

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
				const bool bUnderneath = GetterRect.MinY >= ConsumerRect.MaxY;

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

	// Disconnected groups, and whether each repeats logic already in the connected
	// graph. Reachability starts from every node with no incoming exec link, which is
	// what an entry point looks like, plus every node with no links at all.
	TSet<const UEdGraphNode*> Reachable;
	TArray<const UEdGraphNode*> Stack;

	for (const UEdGraphNode* Node : FlowNodes)
	{
		bool bHasIncoming = false;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
			{
				bHasIncoming = true;
				break;
			}
		}
		if (!bHasIncoming)
		{
			Stack.Add(Node);
		}
	}

	while (Stack.Num() > 0)
	{
		const UEdGraphNode* Node = Stack.Pop();
		if (Node == nullptr || Reachable.Contains(Node))
		{
			continue;
		}
		Reachable.Add(Node);

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Other : Pin->LinkedTo)
			{
				if (Other != nullptr)
				{
					Stack.Add(Other->GetOwningNodeUnchecked());
				}
			}
		}
	}

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

	if (Report.bHasNoWires)
	{
		// The spec asks for this in words rather than as a zero.
		Report.Summary = TEXT("this graph has no wires at all, so there is no flow ");
		Report.Summary += TEXT("to measure. Every layout measure below is therefore ");
		Report.Summary += TEXT("about node placement only.");
	}

	return Report;
}
