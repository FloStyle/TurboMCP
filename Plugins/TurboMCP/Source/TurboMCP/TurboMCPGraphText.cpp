// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphText.h"
#include "TurboMCPGraphTextLex.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "K2Node_SwitchEnum.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPGraphReachability.h"
#include "TurboMCPNodeId.h"

namespace
{
	/** How many pins of one node are printed before the list is capped. */
	constexpr int32 MaxPinsPrinted = 60;

	/** How deep inline value expressions nest before being cut off. */
	constexpr int32 MaxInlineDepth = 12;
	constexpr int32 MaxRerouteHops = 64;

	/** True when the node is a plain reroute that the format hides (`2.1.7`). */
	bool IsKnot(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->IsA<UK2Node_Knot>();
	}

	/** True when the node is a comment box, which is not part of the flow. */
	bool IsComment(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->IsA<UEdGraphNode_Comment>();
	}


	FString EndpointLabel(const UEdGraphPin* InPin)
	{
		return TurboMCPNodeId::MakeForNode(InPin->GetOwningNodeUnchecked(), true)
			+ TEXT(":") + TurboMCPGraphTextLex::QuoteLiteral(InPin->PinName.ToString());
	}

	bool IsWire(const UEdGraphPin* InOutput, const UEdGraphPin* InInput)
	{
		const UEdGraphNode* Source = InOutput != nullptr ? InOutput->GetOwningNodeUnchecked() : nullptr;
		const UEdGraphNode* Target = InInput != nullptr ? InInput->GetOwningNodeUnchecked() : nullptr;
		return Source != nullptr && Target != nullptr && Source->GetGraph() == Target->GetGraph()
			&& InOutput->Direction == EGPD_Output && InInput->Direction == EGPD_Input
			&& InOutput->LinkedTo.Contains(InInput) && InInput->LinkedTo.Contains(InOutput)
			&& (InOutput->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				== (InInput->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec);
	}

	void AppendExecWireObservation(FString& Out, const FString& Indent,
		const UEdGraphPin* Output, const UEdGraphPin* Input)
	{
		const bool bFanOut = Output->LinkedTo.Num() > 1;
		if (bFanOut || IsKnot(Output->GetOwningNodeUnchecked()) || IsKnot(Input->GetOwningNodeUnchecked()))
		{
			Out += TEXT("\n") + Indent;
			Out += bFanOut ? TEXT("; execution fan-out (inspection only) ") : TEXT("; execution reroute wire ");
			Out += EndpointLabel(Output) + TEXT(" -> ") + EndpointLabel(Input);
		}
	}

	/** The real upstream node and the exact output pin its consumer reads. */
	struct FValueSourceEndpoint
	{
		const UEdGraphNode* Node = nullptr;
		const UEdGraphPin* Pin = nullptr;
		TArray<const UEdGraphPin*> Reroutes;
	};

	/** Follow one upstream value source, preserving the nearest-to-farthest reroute path. */
	FValueSourceEndpoint ResolveUpstreamValueSource(const UEdGraphPin* InConsumerPin)
	{
		FValueSourceEndpoint Source;
		const UEdGraphPin* Consumer = InConsumerPin;
		while (Consumer != nullptr && Consumer->LinkedTo.Num() == 1)
		{
			const UEdGraphPin* Output = Consumer->LinkedTo[0];
			if (!IsWire(Output, Consumer))
			{
				break;
			}
			const UEdGraphNode* Owner = Output->GetOwningNodeUnchecked();
			const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Owner);
			if (Knot == nullptr)
			{
				Source.Node = Owner;
				Source.Pin = Output;
				break;
			}
			if (Knot->Pins.Num() != 2 || Knot->GetOutputPin() != Output
				|| Source.Reroutes.Num() >= MaxRerouteHops || Source.Reroutes.Contains(Output))
			{
				break;
			}
			Source.Reroutes.Add(Output);
			Consumer = Knot->GetInputPin();
			if (Consumer == nullptr || Consumer->Direction != EGPD_Input
				|| (Consumer->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
					!= (Output->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
			{
				break;
			}
		}
		return Source;
	}

	FString RerouteLabels(const FValueSourceEndpoint& InSource)
	{
		TArray<FString> Labels;
		for (const UEdGraphPin* Output : InSource.Reroutes)
		{
			Labels.Add(EndpointLabel(Output));
		}
		return FString::Join(Labels, TEXT(", "));
	}

	FString DisplayLabel(const UEdGraphPin& InPin)
	{
		FText Display = InPin.PinFriendlyName;
		if (const UK2Node_SwitchEnum* Switch = Cast<UK2Node_SwitchEnum>(InPin.GetOwningNodeUnchecked()))
		{
			if (const UEnum* Enum = Switch->GetEnum(); Enum != nullptr && InPin.Direction == EGPD_Output)
			{
				const int32 Index = Enum->GetIndexByName(InPin.PinName);
				if (Index != INDEX_NONE)
				{
					Display = Enum->GetDisplayNameTextByIndex(Index);
				}
			}
		}
		const FString Label = Display.ToString();
		return Label.IsEmpty() || Label == InPin.PinName.ToString()
			? FString() : TEXT(" display=") + TurboMCPGraphTextLex::QuoteLiteral(Label);
	}

	FString DescribeCreateDelegate(const UK2Node_CreateDelegate& InDelegate)
	{
		const FName Function = InDelegate.GetFunctionName();
		const UEdGraphPin* Object = InDelegate.GetObjectInPin();
		FString Target = TEXT("unresolved object");
		if (Object != nullptr && Object->LinkedTo.IsEmpty())
		{
			Target = Object->DefaultObject != nullptr ? Object->DefaultObject->GetPathName() : TEXT("self");
		}
		else if (Object != nullptr)
		{
			const FValueSourceEndpoint Source = ResolveUpstreamValueSource(Object);
			if (Source.Pin != nullptr)
			{
				Target = Source.Node->IsA<UK2Node_Self>() ? TEXT("self") : EndpointLabel(Source.Pin);
			}
		}
		return Function.IsNone() ? TEXT("unbound") : Function.ToString() + TEXT(" on ") + Target;
	}

	FString DescribeDelegate(const UEdGraphNode* InNode)
	{
		if (const UK2Node_CreateDelegate* Create = Cast<UK2Node_CreateDelegate>(InNode))
		{
			return TEXT("create delegate; configured delegate function: ") + DescribeCreateDelegate(*Create) + TEXT(" (stored binding)");
		}
		const UK2Node_BaseMCDelegate* Delegate = Cast<UK2Node_BaseMCDelegate>(InNode);
		if (Delegate == nullptr)
		{
			return FString();
		}

		FString Description = TurboMCPNodeId::GetNodeTitle(InNode);
		Description.ReplaceInline(TEXT("\r"), TEXT(" "));
		Description.ReplaceInline(TEXT("\n"), TEXT(" "));
		Description += TEXT(" ; delegate ") + Delegate->GetPropertyName().ToString();
		if (const UEdGraphPin* Pin = Delegate->GetDelegatePin(); Pin != nullptr && Pin->Direction == EGPD_Input)
		{
			const FValueSourceEndpoint Source = ResolveUpstreamValueSource(Pin);
			if (const UK2Node_CreateDelegate* Create = Cast<UK2Node_CreateDelegate>(Source.Node))
			{
				Description += TEXT(" ; stored binding: ") + DescribeCreateDelegate(*Create);
			}
			else if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Source.Node);
				Event != nullptr && Source.Pin == Event->GetDelegatePin())
			{
				Description += TEXT(" ; bound event: ") + Event->GetFunctionName().ToString()
					+ TEXT(" (") + EndpointLabel(Source.Pin) + TEXT(")");
			}
			else if (Source.Pin != nullptr)
			{
				Description += TEXT(" ; binding source: ") + EndpointLabel(Source.Pin);
			}
			else
			{
				Description += Pin->LinkedTo.IsEmpty() ? TEXT(" ; no delegate source") : TEXT(" ; unresolved delegate source");
			}
		}
		return Description;
	}

	/** Identify a data source by both node and output pin, including function inputs. */
	FString SourcePinLink(const FValueSourceEndpoint& InSource)
	{
		if (InSource.Pin == nullptr)
		{
			return FString();
		}
		return FString::Printf(TEXT("(-> %s:%s)"),
			*TurboMCPNodeId::MakeForNode(InSource.Node), *InSource.Pin->PinName.ToString());
	}

	/** A bounded inline expression must retain a stable node reference when expansion stops. */
	FString InlineStopReference(const UEdGraphNode* InNode, const TCHAR* InReason)
	{
		// Keep the actionable link as a complete S-expression. A semicolon comment
		// consumes the rest of its line, so a closing ')' after it would be hidden
		// and make the containing node unreadable.
		return FString::Printf(TEXT("(-> %s)\n        ; inline expansion stopped: %s\n"),
			*TurboMCPNodeId::MakeForNode(InNode), InReason);
	}

	bool IsPureValueNode(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return false;
			}
		}
		return true;
	}

	/** Detect a value cycle without using the call stack. */
	bool HasValueCycle(const UEdGraphNode* InStart)
	{
		if (InStart == nullptr)
		{
			return false;
		}

		TArray<const UEdGraphNode*> Pending;
		TSet<const UEdGraphNode*> Visited;
		Pending.Add(InStart);
		Visited.Add(InStart);
		while (Pending.Num() > 0)
		{
			const UEdGraphNode* Current = Pending.Pop();
			for (const UEdGraphPin* Pin : Current->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					const UEdGraphNode* Target = Linked != nullptr
						? Linked->GetOwningNode() : nullptr;
					if (Target == InStart)
					{
						return true;
					}
					if (IsPureValueNode(Target) && !Visited.Contains(Target))
					{
						Visited.Add(Target);
						Pending.Add(Target);
					}
				}
			}
		}
		return false;
	}
}

bool FTurboMCPGraphText::ParseDetail(const FString& InName, EDetail& OutDetail)
{
	const FString Name = InName.TrimStartAndEnd().ToLower();
	if (Name.IsEmpty() || Name == TEXT("bodies"))
	{
		// Bodies is the default, because it is the level a reader usually wants:
		// the shape of the graph, without the editing noise.
		OutDetail = EDetail::Bodies;
		return true;
	}
	if (Name == TEXT("manifest"))   { OutDetail = EDetail::Manifest;   return true; }
	if (Name == TEXT("signatures")) { OutDetail = EDetail::Signatures; return true; }
	if (Name == TEXT("full"))       { OutDetail = EDetail::Full;       return true; }
	return false;
}

FString FTurboMCPGraphText::DetailNames()
{
	return TEXT("manifest, signatures, bodies, full");
}

bool FTurboMCPGraphText::IsValueNode(const UEdGraphNode* InNode)
{
	if (InNode == nullptr)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : InNode->Pins)
	{
		if (Pin != nullptr && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return false;
		}
	}
	return true;
}

bool FTurboMCPGraphText::IsImplicitPin(const UEdGraphPin* InPin)
{
	if (InPin == nullptr)
	{
		return false;
	}

	// These pins are implicit only while they retain their automatic default. A
	// connection makes the source explicit and must be printed; otherwise a
	// connected Target/self pin hides the getter that supplies it.
	if (InPin->LinkedTo.Num() > 0)
	{
		return false;
	}

	const FName PinName = InPin->PinName;
	if (PinName == UEdGraphSchema_K2::PN_Self)
	{
		return true;
	}
	if (PinName == TEXT("WorldContextObject") || PinName == TEXT("WorldContext"))
	{
		return true;
	}
	return false;
}

UEdGraphPin* FTurboMCPGraphText::FindExecOut(const UEdGraphNode* InNode)
{
	if (InNode == nullptr)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : InNode->Pins)
	{
		if (Pin != nullptr
			&& Pin->Direction == EGPD_Output
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			&& Pin->LinkedTo.Num() > 0)
		{
			return Pin;
		}
	}
	return nullptr;
}

UEdGraphNode* FTurboMCPGraphText::LinkedNode(const UEdGraphPin* InPin)
{
	if (InPin == nullptr || InPin->LinkedTo.Num() == 0)
	{
		return nullptr;
	}
	return InPin->LinkedTo[0] != nullptr ? InPin->LinkedTo[0]->GetOwningNode() : nullptr;
}

void FTurboMCPGraphText::CollectEntryPoints(const UEdGraph* InGraph,
	TArray<FEntryPoint>& OutEntries) const
{
	OutEntries.Reset();
	if (InGraph == nullptr)
	{
		return;
	}

	for (UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node == nullptr || IsComment(Node) || IsKnot(Node))
		{
			continue;
		}

		FEntryPoint Entry;

		if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
		{
			Entry.Node = Node;
			Entry.Kind = TEXT("event");
			Entry.Name = Custom->CustomFunctionName.ToString();
			Entry.bIsCustomEvent = true;
		}
		else if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
		{
			Entry.Node = Node;
			Entry.Kind = TEXT("event");
			// The function name is the stable identifier: the node title is
			// localised and carries words like `Event` that are not part of the
			// name an agent should write back.
			Entry.Name = Event->GetFunctionName().ToString();
			if (Entry.Name.IsEmpty())
			{
				Entry.Name = TurboMCPNodeId::GetNodeTitle(Node);
			}
		}
		else if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry.Node = Node;
			Entry.Kind = TEXT("fn");
			// A function entry's own name is the graph's name, which is how the
			// graph is addressed everywhere else.
			Entry.Name = FunctionEntry->GetGraph() != nullptr
				? FunctionEntry->GetGraph()->GetName()
				: TurboMCPNodeId::GetNodeTitle(Node);
		}
		else
		{
			continue;
		}

		OutEntries.Add(MoveTemp(Entry));
	}

	// A stable order, so two reads of an unchanged graph give the same text. The
	// engine's own node order is not guaranteed, and a text format whose lines
	// move around is hard to compare with the previous one.
	OutEntries.Sort([](const FEntryPoint& A, const FEntryPoint& B)
	{
		return A.Name < B.Name;
	});
}

FString FTurboMCPGraphText::RenderPinValue(const UEdGraphPin* InPin, EDetail InDetail,
	const FString& InAssetPath, int32 InInlineDepth,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TSet<const UEdGraphNode*>& InOutInlineNodes,
	TArray<FString>& OutNotes, EFormEvidence& OutEvidence) const
{
	OutEvidence = EFormEvidence::Complete;
	if (InPin == nullptr)
	{
		OutEvidence = EFormEvidence::None;
		return FString();
	}

	if (InPin->LinkedTo.Num() > 0)
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			TArray<FString> Sources;
			TSet<const UEdGraphPin*> Seen;
			for (const UEdGraphPin* SourcePin : InPin->LinkedTo)
			{
				if (!IsWire(SourcePin, InPin) || Seen.Contains(SourcePin))
				{
					OutEvidence = EFormEvidence::Partial;
					const FString Note = TEXT("; error: malformed execution input endpoint");
					OutNotes.Add(Note);
					return Note;
				}
				Seen.Add(SourcePin);
				Sources.Add(EndpointLabel(SourcePin));
			}
			if (Sources.Num() > 1)
			{
				Sources.Sort();
				return TEXT("; execution merge (inspection only) from ") + FString::Join(Sources, TEXT(", "));
			}
			return FString::Printf(TEXT("(-> %s)"),
				*TurboMCPNodeId::MakeForNode(InPin->LinkedTo[0]->GetOwningNodeUnchecked()));
		}

		// Unreal permits several scalar targets for certain impure calls. This
		// is inspection evidence, not single-value append syntax or an inline value.
		const UK2Node* Owner = Cast<UK2Node>(InPin->GetOwningNodeUnchecked());
		if (InPin->LinkedTo.Num() > 1 && GetDefault<UEdGraphSchema_K2>()->IsSelfPin(*InPin)
			&& Owner && Owner->AllowMultipleSelfs(false) && !InPin->PinType.IsContainer())
		{
			TSet<const UEdGraphPin*> Seen;
			TArray<FString> Targets;
			bool bValid = true;
			for (const UEdGraphPin* Target : InPin->LinkedTo)
			{
				if (!IsWire(Target, InPin) || Seen.Contains(Target) || Target->PinType.IsContainer())
				{ bValid = false; break; }
				Seen.Add(Target);
				Targets.Add(EndpointLabel(Target));
			}
			if (bValid)
			{
				OutEvidence = EFormEvidence::Partial;
				return TEXT("; multiple call targets (inspection only; connection order): ")
					+ FString::Join(Targets, TEXT(", "));
			}
		}

		if (InPin->LinkedTo.Num() != 1 || !IsWire(InPin->LinkedTo[0], InPin))
		{
			const FString Note = FString::Printf(
				TEXT("; error: value source for pin '%s' has a malformed link"),
				*InPin->PinName.ToString());
			OutNotes.Add(Note);
			OutEvidence = EFormEvidence::Partial;
			return Note;
		}

		const FValueSourceEndpoint Source = ResolveUpstreamValueSource(InPin);
		if (Source.Pin == nullptr)
		{
			const FString Note = FString::Printf(
				TEXT("; error: value source for pin '%s' has an unresolved knot chain starting at %s"),
				*InPin->PinName.ToString(), *EndpointLabel(InPin->LinkedTo[0]));
			OutNotes.Add(Note);
			OutEvidence = EFormEvidence::Partial;
			return Note;
		}

		// Keep exact machine endpoints and put human labels and routing evidence in comments.
		const FString Label = DisplayLabel(*Source.Pin);
		const FString Route = Source.Reroutes.IsEmpty() ? FString()
			: TEXT(" ; via knot ") + RerouteLabels(Source);
		const FString Link = SourcePinLink(Source) + (Label.IsEmpty() ? FString() : TEXT(" ;") + Label) + Route;

		const UEdGraphNode* RealTarget = Source.Node;

		// A self node is implied by the entry point, so it is not printed.
		if (RealTarget->IsA<UK2Node_Self>())
		{
			return TEXT("self") + Route;
		}

		if (IsValueNode(RealTarget))
		{
			if (InOutInlineNodes.Contains(RealTarget))
			{
				// The value graph is cyclic. Keep the node reference so the caller can
				// identify the offending node, but do not enter the cycle again.
				OutEvidence = EFormEvidence::Partial;
				return InlineStopReference(RealTarget, TEXT("value cycle"));
			}

			// A pure node used by more than one consumer is printed inline on its
			// first complete use and referred to by its EXISTING node ID and exact
			// output pin later. This format has no shared-definition token, so no
			// `$id` is emitted.
			int32 Consumers = 0;
			int32 ValueOutputs = 0;
			for (const UEdGraphPin* OutPin : RealTarget->Pins)
			{
				if (OutPin != nullptr && OutPin->Direction == EGPD_Output)
				{
					Consumers += OutPin->LinkedTo.Num();
					++ValueOutputs;
				}
			}

			const bool bShared = Consumers > 1 || Source.Reroutes.ContainsByPredicate(
				[](const UEdGraphPin* Pin) { return Pin->LinkedTo.Num() > 1; });
			if (bShared && InOutSharedValues.Contains(RealTarget))
			{
				return Link;
			}

			if (bShared && HasValueCycle(RealTarget))
			{
				OutEvidence = EFormEvidence::Partial;
				return InlineStopReference(RealTarget, TEXT("value cycle"));
			}

			if (InInlineDepth >= MaxInlineDepth)
			{
				// Depth is measured in inline value nodes, not in exec statements.
				// The declared limit is a second defence for a very deep acyclic graph.
				OutEvidence = EFormEvidence::Partial;
				return InlineStopReference(RealTarget, TEXT("depth limit"));
			}

			InOutInlineNodes.Add(RealTarget);
			EFormEvidence ChildEvidence = EFormEvidence::Complete;
			const FString Inline = RenderNode(RealTarget, InDetail, InAssetPath,
				InInlineDepth + 1, InOutPrinted, InOutSharedValues,
				InOutInlineNodes, OutNotes, ChildEvidence);
			InOutInlineNodes.Remove(RealTarget);

			const bool bNamedOutput = ValueOutputs > 1 || RealTarget->IsA<UK2Node_BreakStruct>();
			const FString OutputHint = bNamedOutput || !Label.IsEmpty() || !Route.IsEmpty()
				? TEXT(" ; reads output ") + EndpointLabel(Source.Pin) + Label + Route : FString();

			OutEvidence = ChildEvidence;
			if (ChildEvidence == EFormEvidence::Complete && !Inline.IsEmpty())
			{
				InOutPrinted.Add(RealTarget);
				if (bShared)
				{
					InOutSharedValues.Add(RealTarget);
				}
				return Inline + OutputHint;
			}
			if (!Inline.IsEmpty())
			{
				// A partial child form is returned so the caller sees the damage,
				// but it is NEVER marked as a reusable shared definition.
				return Inline + OutputHint;
			}

			OutEvidence = EFormEvidence::Partial;
			return InlineStopReference(RealTarget, TEXT("form not emitted"));
		}

		return Link;
	}

	// No link: the pin's own literal, which `2.3` says is written the usual way.
	if (!InPin->DefaultValue.IsEmpty())
	{
		const FString& Value = InPin->DefaultValue;
		const bool bLooksNumeric =
			Value.IsNumeric()
			|| Value == TEXT("true") || Value == TEXT("false");

		// A string value is quoted so it cannot be mistaken for a variable
		// reference, which is the engine writer's own rule.
		return bLooksNumeric ? Value : TurboMCPGraphTextLex::QuoteLiteral(Value);
	}

	if (InPin->DefaultObject != nullptr)
	{
		// `2.3`: object and class values by full object path.
		return FString::Printf(TEXT("\"%s\""), *InPin->DefaultObject->GetPathName());
	}

	if (!InPin->DefaultTextValue.IsEmpty())
	{
		// `2.3`: text values are marked as text, not string.
		return FString::Printf(TEXT("(text \"%s\")"), *InPin->DefaultTextValue.ToString());
	}

	return FString();
}

FString FTurboMCPGraphText::RenderNode(const UEdGraphNode* InNode, EDetail InDetail,
	const FString& InAssetPath, int32 InInlineDepth,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TSet<const UEdGraphNode*>& InOutInlineNodes,
	TArray<FString>& OutNotes, EFormEvidence& OutEvidence) const
{
	OutEvidence = EFormEvidence::Complete;
	if (InNode == nullptr)
	{
		OutEvidence = EFormEvidence::None;
		return FString();
	}

	// A function or event entry is not a statement in the body: it is the head of
	// the form, so it is never rendered as a node.
	if (InNode->IsA<UK2Node_FunctionEntry>())
	{
		OutEvidence = EFormEvidence::None;
		return FString();
	}

	const bool bFull = InDetail == EDetail::Full;
	const FString DelegateDescription = DescribeDelegate(InNode);
	const FString TypeId = TurboMCPNodeId::GetNodeTypeId(InNode);
	if (TypeId.IsEmpty())
	{
		OutEvidence = EFormEvidence::Partial;

		const FString Id = TurboMCPNodeId::MakeForNode(InNode, true);
		const bool bDelegate = !DelegateDescription.IsEmpty();
		FString Note = bDelegate
			? FString::Printf(TEXT("; read-only %s %s"), *Id, *DelegateDescription)
			: FString::Printf(TEXT("; read-only %s has no node type ID (%s); not a writable node form"),
				*Id, *InNode->GetClass()->GetName());
		const FString Guidance = bDelegate
			? TEXT("; delegate observations describe stored bindings; use ReadTurboMCPNodeContext to inspect exact pins and links")
			: TEXT("; Retain this node. Inspect its pins with ReadTurboMCPNodeContext and use PreviewTurboMCPGraphConnect for explicit links. Check compatibility and any replaced wires.");
		if (!OutNotes.Contains(Guidance))
		{
			OutNotes.Add(Guidance);
			Note += TEXT("\n") + Guidance;
		}
		if (bDelegate && !bFull)
		{
			for (const UEdGraphPin* Pin : InNode->Pins)
			{
				if (Pin != nullptr && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
					&& Pin->LinkedTo.Num() > 1)
				{
					EFormEvidence PinEvidence;
					Note += TEXT("\n") + RenderPinValue(Pin, InDetail, InAssetPath, InInlineDepth,
						InOutPrinted, InOutSharedValues, InOutInlineNodes, OutNotes, PinEvidence);
				}
			}
		}
		if (bFull)
		{
			int32 Shown = 0;
			for (const UEdGraphPin* Pin : InNode->Pins)
			{
				if (Pin == nullptr)
				{
					continue;
				}
				if (Shown++ == MaxPinsPrinted)
				{
					const FString Limit = FString::Printf(TEXT("; further pins omitted for %s; use ReadTurboMCPNodeContext"), *Id);
					OutNotes.AddUnique(Limit);
					Note += TEXT("\n") + Limit;
					break;
				}
				Note += FString::Printf(TEXT("\n; %s %s (%s)%s"),
					Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
					*TurboMCPGraphTextLex::QuoteLiteral(Pin->PinName.ToString()),
					*Pin->PinType.PinCategory.ToString(), *DisplayLabel(*Pin));
				if (Pin->DefaultObject != nullptr)
				{
					Note += TEXT(" object=") + TurboMCPGraphTextLex::QuoteLiteral(Pin->DefaultObject->GetPathName());
				}
				if (!Pin->DefaultValue.IsEmpty())
				{
					Note += TEXT(" value=") + TurboMCPGraphTextLex::QuoteLiteral(Pin->DefaultValue);
				}
				if (!Pin->DefaultTextValue.IsEmpty())
				{
					Note += TEXT(" text=") + TurboMCPGraphTextLex::QuoteLiteral(Pin->DefaultTextValue.ToString());
				}
				for (const UEdGraphPin* Peer : Pin->LinkedTo)
				{
					if (Peer != nullptr && Peer->GetOwningNodeUnchecked() != nullptr)
					{
						Note += TEXT(" -> ") + EndpointLabel(Peer);
					}
				}
			}
		}
		return Note;
	}

	FString Out = FString::Printf(TEXT("(%s"), *TypeId);

	// The node ID rides in a comment, which `2.1.4` allows at `bodies`.
	Out += FString::Printf(TEXT(" ; %s"), *TurboMCPNodeId::MakeForNode(InNode, bFull));

	// At full the C++ class and the pin indexes are added, because they are the
	// editing details this level exists for.
	if (bFull)
	{
		Out += FString::Printf(TEXT(" ; class %s"), *InNode->GetClass()->GetName());
	}

	if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode))
	{
		if (const UFunction* Target = Call->GetTargetFunction())
		{
			Out += FString::Printf(TEXT(" ; target %s"), *Target->GetPathName());
		}
		else if (!Call->FunctionReference.GetMemberName().IsNone())
		{
			Out += FString::Printf(TEXT(" ; target %s (unresolved)"),
				*Call->FunctionReference.GetMemberName().ToString());
		}
	}

	if (!DelegateDescription.IsEmpty())
	{
		Out += TEXT(" ; ") + DelegateDescription;
	}

	int32 Printed = 0;
	int32 InputIndex = 0;
	int32 OutputIndex = 0;

	for (const UEdGraphPin* Pin : InNode->Pins)
	{
		if (Pin == nullptr)
		{
			continue;
		}

		const bool bIsInput = Pin->Direction == EGPD_Input;
		// THE INDEX IS COUNTED PER DIRECTION, which is what the engine does: an input's index is its
		// place among the INPUTS, not its place in `Pins`. `Spec/03_TOOLS.md:172` lists "pin indexes"
		// among what the `full` level adds, and ADVANCING THE COUNTER FOR OUTPUTS WHILE PRINTING -1 FOR
		// THEM MAKES the index printed for an input already wrong once an output has been walked.
		const int32 Index = bIsInput ? InputIndex++ : OutputIndex++;

		if (!bIsInput)
		{
			// An output pin is not a value of this node: it is what consumers read
			// from it. It is printed only at full, where the pin indexes matter.
			if (bFull && Pin->LinkedTo.Num() > 0)
			{
				// THE INDEX AND THE LINK COUNT ARE BOTH PRINTED, because `03_TOOLS.md:172` asks for
				// the INDEX at `full` and the link count was all that was here. A caller reading an
				// out line needs to know WHICH output, not only how many wires leave it.
				Out += FString::Printf(TEXT("\n    ; out %d %s -> %d link(s)%s"),
					OutputIndex - 1, *Pin->PinName.ToString(), Pin->LinkedTo.Num(), *DisplayLabel(*Pin));
			}
			continue;
		}

        // Empty object inputs can be significant even at bodies detail. Report
        // the stored state without guessing whether a function accepts null.
        const bool EmptyObject = Pin->LinkedTo.IsEmpty() && Pin->DefaultObject == nullptr
            && Pin->DefaultValue.IsEmpty() && Pin->DefaultTextValue.IsEmpty()
            && !Pin->PinType.IsContainer() && !Pin->ParentPin && Pin->SubPins.IsEmpty()
            && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
                || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface);
        if (EmptyObject && !IsImplicitPin(Pin))
        {
            const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode);
            const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
            const FString Name = Pin->PinName.ToString();
            const bool ContextDefault = Function &&
                (Function->GetMetaData(TEXT("DefaultToSelf")) == Name || Function->GetMetaData(TEXT("WorldContext")) == Name);
            Out += FString::Printf(TEXT("\n    ; object input %s: "), *Name);
            Out += ContextDefault
                ? TEXT("unconnected; function metadata supplies self/context (runtime validity not checked)")
                : TEXT("unconnected with no stored default; inspect nullability before use (no self/context fallback identified)");
        }

		if (IsImplicitPin(Pin))
		{
			continue;
		}

		if (Printed >= MaxPinsPrinted)
		{
			Out += FString::Printf(TEXT("\n    ; %d more pin(s) not shown"),
				InNode->Pins.Num() - Printed);
			OutEvidence = EFormEvidence::Partial;
			break;
		}

		// `2.1.5`: use the pin's real name, not its display name.
		FString PinLabel = Pin->PinName.ToString();
		EFormEvidence PinEvidence = EFormEvidence::Complete;
		FString PinValue = RenderPinValue(Pin, InDetail, InAssetPath,
			InInlineDepth, InOutPrinted, InOutSharedValues, InOutInlineNodes, OutNotes,
			PinEvidence);
		if (PinEvidence != EFormEvidence::Complete)
		{
			OutEvidence = EFormEvidence::Partial;
		}

		if (PinValue.IsEmpty() && Pin->LinkedTo.Num() == 0)
		{
			// An unlinked pin at its default is not printed (`2.1.8`), but at full
			// it is named so the agent knows the pin is at its default rather than
			// absent.
			if (bFull)
			{
				Out += FString::Printf(TEXT("\n    ; %s left at default"), *PinLabel);
			}
			++Printed;
			continue;
		}

		if (PinLabel.Contains(TEXT(" ")))
		{
			// The engine writer quotes a pin name holding spaces, and so does this
			// so the text reads back.
			PinLabel = FString::Printf(TEXT("\"%s\""), *PinLabel);
		}

		const FString IndexHint = bFull
			? FString::Printf(TEXT(" ; in %d"), Index)
			: FString();

		Out += FString::Printf(TEXT("\n    %s %s%s"), *PinLabel, *PinValue, *IndexHint);
		++Printed;
	}

	// A fresh line keeps the delimiter outside any trailing semicolon comment.
	Out += TEXT("\n)");
	return Out;
}

FString FTurboMCPGraphText::RenderChain(const UEdGraphNode* InFirst, EDetail InDetail,
	int32 InDepth, const FString& InAssetPath,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TArray<FString>& OutNotes) const
{
	if (InFirst != nullptr && InDepth >= 64)
	{
		const FString Note = TEXT("; error: execution branch depth limit reached; graph dump is incomplete");
		OutNotes.Add(Note);
		return TEXT("\n") + Note;
	}
	FString Body;
	const FString Indent = FString::ChrN(InDepth * 2 + 2, TEXT(' '));
	TSet<const UEdGraphNode*> InlineNodes;

	const UEdGraphNode* Current = InFirst;
	int32 Guard = 0;

	while (Current != nullptr)
	{
		if (Guard++ >= 2000)
		{
			const FString Note = TEXT("; error: execution chain exceeds 2000 nodes; graph dump is incomplete");
			OutNotes.Add(Note);
			Body += TEXT("\n") + Indent + Note;
			break;
		}

		if (IsComment(Current))
		{
			// A comment box is not part of the flow, so it is stepped over.
			UEdGraphPin* Skip = FindExecOut(Current);
			Current = Skip != nullptr ? LinkedNode(Skip) : nullptr;
			continue;
		}

		const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Current);
		if (InOutPrinted.Contains(Current))
		{
			// Shared reroutes are valid. Refer to the first occurrence of the
			// knot or node, including on a cycle, without expanding it again.
			const FString Id = TurboMCPNodeId::MakeForNode(Current, Knot != nullptr);
			Body += Knot != nullptr
				? FString::Printf(TEXT("\n%s; continue at execution reroute %s"), *Indent, *Id)
				: FString::Printf(TEXT("\n%s(goto %s)"), *Indent, *Id);
			break;
		}

		EFormEvidence NodeEvidence = EFormEvidence::Complete;
		FString Rendered;
		if (Knot != nullptr)
		{
			Rendered = FString::Printf(TEXT("; execution reroute %s"), *TurboMCPNodeId::MakeForNode(Knot, true));
			if (Knot->Pins.Num() != 2 || Knot->GetInputPin() == nullptr || Knot->GetOutputPin() == nullptr
				|| Knot->GetInputPin()->Direction != EGPD_Input || Knot->GetOutputPin()->Direction != EGPD_Output
				|| Knot->GetOutputPin()->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				const FString Note = Rendered + TEXT(" has malformed execution pins");
				OutNotes.Add(Note);
				Body += TEXT("\n") + Indent + Note;
				break;
			}
			Rendered += FString::Printf(TEXT(" %s -> %s"), *Knot->GetInputPin()->PinName.ToString(),
				*Knot->GetOutputPin()->PinName.ToString());
			if (Knot->GetOutputPin()->LinkedTo.IsEmpty())
			{
				Rendered += TEXT(" (output unconnected)");
			}
		}
		else
		{
			Rendered = RenderNode(Current, InDetail, InAssetPath, /*InInlineDepth=*/0,
				InOutPrinted, InOutSharedValues, InlineNodes, OutNotes, NodeEvidence);
		}
		InOutPrinted.Add(Current);

		if (!Rendered.IsEmpty())
		{
			// Re-indent every line of the node, so a nested continuation reads as
			// nested. ReplaceInline is used rather than Replace: FString has no
			// two-argument Replace in this engine version.
			FString Indented = Rendered;
			Indented.ReplaceInline(TEXT("\n"), *(FString(TEXT("\n")) + Indent));
			Body += FString(TEXT("\n")) + Indent + Indented;
		}

		// Every exec output is followed, not just the first. A branch has one per
		// arm, and following only the first loses the other arm entirely: an
		// if/else printed one arm and reported the other as unreachable.
		TArray<UEdGraphPin*> ExecOuts;
		for (UEdGraphPin* Pin : Current->Pins)
		{
			if (Pin != nullptr
				&& Pin->Direction == EGPD_Output
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				ExecOuts.Add(Pin);
			}
		}

		if (ExecOuts.Num() == 0)
		{
			break;
		}

		if (ExecOuts.Num() == 1 && ExecOuts[0]->LinkedTo.Num() == 1 && DisplayLabel(*ExecOuts[0]).IsEmpty())
		{
			// The ordinary case: one output, so the chain simply continues in the
			// same block.
			const UEdGraphPin* Endpoint = ExecOuts[0]->LinkedTo[0];
			if (!IsWire(ExecOuts[0], Endpoint))
			{
				const FString Note = TEXT("; error: malformed execution endpoint");
				OutNotes.Add(Note);
				Body += TEXT("\n") + Indent + Note;
				break;
			}
			AppendExecWireObservation(Body, Indent, ExecOuts[0], Endpoint);
			Current = Endpoint->GetOwningNodeUnchecked();
			continue;
		}

		// Every declared arm is named, including unconnected outputs. Printing the
		// first one inline would imply the others run after it rather than instead
		// of it, so they are all written the same way.
		for (const UEdGraphPin* Pin : ExecOuts)
		{
			const FString Display = DisplayLabel(*Pin);
			Body += FString::Printf(TEXT("\n%s(:%s"), *Indent, *Pin->PinName.ToString());
			if (!Display.IsEmpty())
			{
				Body += TEXT(" ;") + Display;
			}
			TSet<const UEdGraphPin*> Seen;
			for (const UEdGraphPin* Endpoint : Pin->LinkedTo)
			{
				if (!IsWire(Pin, Endpoint) || Seen.Contains(Endpoint))
				{
					const FString Note = TEXT("; error: malformed execution endpoint");
					OutNotes.Add(Note);
					Body += TEXT("\n") + Indent + Note;
					continue;
				}
				Seen.Add(Endpoint);
				AppendExecWireObservation(Body, Indent, Pin, Endpoint);
				Body += RenderChain(Endpoint->GetOwningNodeUnchecked(), InDetail, InDepth + 1,
					InAssetPath, InOutPrinted, InOutSharedValues, OutNotes);
			}
			Body += TEXT("\n") + Indent + TEXT(")");
		}
		break;
	}

	return Body;
}

FString FTurboMCPGraphText::RenderEntryBody(const UEdGraph* InGraph,
	const FEntryPoint& InEntry, EDetail InDetail, const FString& InAssetPath,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TArray<FString>& OutNotes) const
{
	FString Body;
	if (InEntry.Node != nullptr)
	{
		for (const UEdGraphPin* Pin : InEntry.Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output
				|| Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) { continue; }
			TSet<const UEdGraphPin*> Seen;
			for (const UEdGraphPin* Endpoint : Pin->LinkedTo)
			{
				if (!IsWire(Pin, Endpoint) || Seen.Contains(Endpoint))
				{
					const FString Note = TEXT("; error: malformed entry execution endpoint");
					OutNotes.Add(Note);
					Body += TEXT("\n  ") + Note;
					continue;
				}
				Seen.Add(Endpoint);
				AppendExecWireObservation(Body, TEXT("  "), Pin, Endpoint);
				Body += RenderChain(Endpoint->GetOwningNodeUnchecked(), InDetail, 0, InAssetPath,
					InOutPrinted, InOutSharedValues, OutNotes);
			}
		}
	}

	if (Body.IsEmpty())
	{
		// `P1`: an entry point with no body says so rather than printing nothing.
		Body = TEXT("\n  ; this entry point has no exec chain and no body");
	}

	return Body;
}

FString FTurboMCPGraphText::Render(const UEdGraph* InGraph, EDetail InDetail,
	const FString& InAssetPath, TArray<FString>& OutNotes, const FString& EntryFilter,
	const UEdGraphNode* InEntry) const
{
	if (InGraph == nullptr)
	{
		return FString();
	}

	TArray<FEntryPoint> Entries;
	CollectEntryPoints(InGraph, Entries);
    const TArray<FEntryPoint> AllEntries = Entries;
	if (InEntry != nullptr)
	{
		Entries.RemoveAll([InEntry](const FEntryPoint& Entry) { return Entry.Node != InEntry; });
	}
    const FString Filter=EntryFilter.TrimStartAndEnd();
    if(!Filter.IsEmpty())
    {
        TArray<FString> Available;
        for(const FEntryPoint& Entry:Entries) Available.Add(Entry.Name);
        Entries.RemoveAll([&](const FEntryPoint& Entry){return !Entry.Name.Contains(Filter,ESearchCase::IgnoreCase);});
        if(Entries.IsEmpty())
        {
            const FString Error=TEXT("entry filter matched nothing; available entries: ")+FString::Join(Available,TEXT(", "));
            OutNotes.Add(Error);
            return TEXT("; error: ")+Error;
        }
    }


	const int32 NodeCount = InGraph->Nodes.Num();
	const FString GraphName = InGraph->GetName();

	FString Out = FString::Printf(TEXT("; graph %s.%s"),
		*InAssetPath, *GraphName);
	if (InEntry != nullptr)
	{
		Out += TEXT("\n; scope: one authored event entry (inspection only)");
	}
    if(!Filter.IsEmpty()) Out+=TEXT("\n; partial: filtered inspection view, not a complete graph for write-back\n; entry-name contains: ")+Filter;

	if (InDetail == EDetail::Manifest)
	{
		Out += FString::Printf(TEXT("\n; %d node(s), %d entry point(s)"),
			NodeCount, Entries.Num());
		for (const FEntryPoint& Entry : Entries)
		{
			Out += FString::Printf(TEXT("\n;   %s %s ; %s"), *Entry.Kind, *Entry.Name, *TurboMCPNodeId::MakeForNode(Entry.Node, true));
		}
		return Out;
	}

	// Entry points are printed before bodies, so a reader sees the shape first.
	if (InDetail == EDetail::Signatures)
	{
		Out += FString::Printf(TEXT("\n; %d node(s), %d entry point(s)"),
			NodeCount, Entries.Num());
		for (const FEntryPoint& Entry : Entries)
		{
			FString Signature;
			for (const UEdGraphPin* Pin : Entry.Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output
					|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					continue;
				}
				Signature += FString::Printf(TEXT(" %s"), *Pin->PinName.ToString());
			}
			Out += FString::Printf(TEXT("\n(%s %s%s) ; %s"),
				*Entry.Kind, *Entry.Name, *Signature, *TurboMCPNodeId::MakeForNode(Entry.Node, true));
		}
		return Out;
	}

    // Compute origins independently for each entry: a globally visited set would
    // hide the second origin precisely where execution chains merge. Follow only
    // explicit exec links, including knots; this is static reachability, not a
    // claim that runtime conditions take every branch.
    TMap<const UEdGraphNode*, TArray<FString>> ExecOrigins;
    TSet<const UEdGraphNode*> SelectedReachable;
    TSet<const UEdGraphNode*> SelectedEntries;
    for (const FEntryPoint& Entry : Entries) SelectedEntries.Add(Entry.Node);
    for (const FEntryPoint& Entry : AllEntries)
    {
        TSet<const UEdGraphNode*> Visited;
        TArray<const UEdGraphNode*> Pending;
        Pending.Add(Entry.Node);
        const FString Origin = Entry.Name + TEXT(" [") + TurboMCPNodeId::MakeForNode(Entry.Node) + TEXT("]");
        while (!Pending.IsEmpty())
        {
            const UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
            if (!Node || Node->GetGraph() != InGraph || Visited.Contains(Node)) continue;
            Visited.Add(Node);
            ExecOrigins.FindOrAdd(Node).Add(Origin);
            if (SelectedEntries.Contains(Entry.Node)) SelectedReachable.Add(Node);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
                for (const UEdGraphPin* Target : Pin->LinkedTo)
                {
                    if (IsWire(Pin, Target))
                        Pending.Add(Target->GetOwningNodeUnchecked());
                }
            }
        }
    }
    TArray<FString> SharedTailLines;
    for (const TPair<const UEdGraphNode*, TArray<FString>>& Pair : ExecOrigins)
    {
        if (Pair.Value.Num() < 2 || !SelectedReachable.Contains(Pair.Key) || IsKnot(Pair.Key)) continue;
        SharedTailLines.Add(FString::Printf(TEXT("\n; shared-tail %s: reachable via exec links from %s"),
            *TurboMCPNodeId::MakeForNode(Pair.Key), *FString::Join(Pair.Value, TEXT(", "))));
    }
    SharedTailLines.Sort();
    if (!SharedTailLines.IsEmpty())
    {
        Out += TEXT("\n; Shared execution tails (static graph links; includes other entry origins even in a filtered view):");
        for (const FString& Line : SharedTailLines) Out += Line;
    }

    // BL-12: declarations are stored on the function entry, not on variable-get nodes.
    for (const UEdGraphNode* Node : InGraph->Nodes)
    {
        const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
        if (!Entry) continue;
        for (const FBPVariableDescription& Variable : Entry->LocalVariables)
        {
            FString Default = Variable.DefaultValue;
            Default.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
            Default.ReplaceInline(TEXT("\r"), TEXT("\\r"));
            Default.ReplaceInline(TEXT("\n"), TEXT("\\n"));
            Out += FString::Printf(TEXT("\n; local %s type=%s stored-default=%s"),
                *Variable.VarName.ToString(), *Variable.VarType.PinCategory.ToString(),
                Default.IsEmpty() ? TEXT("<unset; runtime/type default not evaluated>") : *Default);
        }
    }

	// Bodies and full. A comment box is printed as a note about the region it
	// covers, because it groups nodes an agent may want to know about, but it is
	// not part of the flow.
	TArray<FString> Comments;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Filter.IsEmpty() && IsComment(Node))
		{
			Comments.Add(FString::Printf(TEXT("; comment: %s"),
				*TurboMCPNodeId::GetNodeTitle(Node)));
		}
	}
	for (const FString& Comment : Comments)
	{
		Out += TEXT("\n") + Comment;
	}

	if (Entries.Num() == 0)
	{
		// A graph with no entry point is not an error: a macro or a state graph
		// may have none. It still has an ID, so it says so.
		Out += FString::Printf(
			TEXT("\n; this graph has no event or function entry point. Its %d node(s) ")
			TEXT("are reachable through FindNodes."),
			NodeCount);
		return Out;
	}

	TSet<const UEdGraphNode*> Printed;

	// Shared represented values are tracked ACROSS entries, in the stable sorted
	// entry order below. A value first rendered completely in one entry is linked
	// by ID in later entries; a partial first form is never added here.
	TSet<const UEdGraphNode*> SharedValues;

	// The entry nodes are marked before any body is walked, because each of them
	// is printed as the head of its own form rather than as a node. Leaving them
	// out made every event appear twice: once as its form, and again in the
	// "not reachable from any entry point" list below.
	for (const FEntryPoint& Entry : Entries)
	{
		Printed.Add(Entry.Node);
	}

	for (const FEntryPoint& Entry : Entries)
	{
		// Entry points are printed in the order collected, which is sorted by
		// name so the text is stable between reads.
		const FString Head = FString::Printf(TEXT("\n(%s %s ; %s"),
			*Entry.Kind, *Entry.Name, *TurboMCPNodeId::MakeForNode(Entry.Node, true));
		const FString Body = RenderEntryBody(InGraph, Entry, InDetail, InAssetPath,
			Printed, SharedValues, OutNotes);
		Out += Head + Body + TEXT("\n)");
	}

    if (!Filter.IsEmpty() || InEntry != nullptr) return Out;

	// Use the same all-wire reachability walk as the dead-node preview. Exec
	// traversal alone would call a getter feeding a live target pin disconnected.
	TSet<const UEdGraphNode*> Reachable;
	TurboMCPGraphReachability::MarkReachable(InGraph, Reachable);

	// `2.1.11`: disconnected groups are printed after the entry point forms, in
	// their own section, each group once, with a count.
	TArray<const UEdGraphNode*> Disconnected;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (Node == nullptr || IsComment(Node) || IsKnot(Node)
			|| Node->IsA<UK2Node_FunctionEntry>())
		{
			continue;
		}
		if (!Reachable.Contains(Node))
		{
			Disconnected.Add(Node);
		}
	}

	if (Disconnected.Num() > 0)
	{
		Out += FString::Printf(
			TEXT("\n\n; %d node(s) are not reachable from any entry point:"),
			Disconnected.Num());

		int32 Shown = 0;
		for (const UEdGraphNode* Node : Disconnected)
		{
			if (Shown >= 40)
			{
				Out += FString::Printf(TEXT("\n;   (%d more)"), Disconnected.Num() - Shown);
				break;
			}
			const FString DelegateDescription = DescribeDelegate(Node);
			const FString Description = DelegateDescription.IsEmpty() ? TurboMCPNodeId::GetNodeTitle(Node) : DelegateDescription;
			Out += FString::Printf(TEXT("\n;   %s  %s"), *TurboMCPNodeId::MakeForNode(Node), *Description);
			++Shown;
		}
	}

	return Out;
}
