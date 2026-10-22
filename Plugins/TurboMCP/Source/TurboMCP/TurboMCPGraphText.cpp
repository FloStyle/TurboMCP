// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphText.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPGraphReachability.h"
#include "TurboMCPNodeId.h"

namespace
{
	/** How many pins of one node are printed before the list is capped. */
	constexpr int32 MaxPinsPrinted = 60;

	/** How deep inline value expressions nest before being cut off. */
	constexpr int32 MaxInlineDepth = 12;

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


	/** The real upstream node and the exact output pin its consumer reads. */
	struct FValueSourceEndpoint
	{
		const UEdGraphNode* Node = nullptr;
		UEdGraphPin* Pin = nullptr;
		bool bResolved = false;
	};

	/**
	 * Resolve an input pin to the real node/output pin behind any number of knots.
	 *
	 * This walks UPSTREAM: a consumer input links to the nearest knot's output,
	 * the knot's single input links to the next upstream pin, and so on. It is
	 * distinct from RenderChain, which follows execution edges DOWNSTREAM.
	 * Nulls, multiple links, non-output pins, cycles and depth >64 are unresolved.
	 */
	FValueSourceEndpoint ResolveUpstreamValueSource(const UEdGraphPin* InConsumerPin)
	{
		if (InConsumerPin == nullptr || InConsumerPin->LinkedTo.Num() != 1)
		{
			return FValueSourceEndpoint();
		}

		UEdGraphPin* Current = InConsumerPin->LinkedTo[0];
		const UEdGraphPin* Consumer = InConsumerPin;
		TSet<const UEdGraphPin*> SeenPins;
		TSet<const UEdGraphNode*> SeenKnots;

		for (int32 Depth = 0; Depth < 64; ++Depth)
		{
			if (Current == nullptr || SeenPins.Contains(Current) || !Current->LinkedTo.Contains(Consumer))
			{
				return FValueSourceEndpoint();
			}
			SeenPins.Add(Current);

			UEdGraphNode* Owner = Current->GetOwningNode();
			if (Owner == nullptr || Current->Direction != EGPD_Output)
			{
				return FValueSourceEndpoint();
			}

			if (!IsKnot(Owner))
			{
				FValueSourceEndpoint Endpoint;
				Endpoint.Node = Owner;
				Endpoint.Pin = Current;
				Endpoint.bResolved = true;
				return Endpoint;
			}

			if (SeenKnots.Contains(Owner))
			{
				return FValueSourceEndpoint();
			}
			SeenKnots.Add(Owner);

			UEdGraphPin* KnotInput = nullptr;
			for (UEdGraphPin* Pin : Owner->Pins)
			{
				if (Pin != nullptr && Pin->Direction == EGPD_Input)
				{
					if (KnotInput != nullptr)
					{
						// A knot with more than one input is malformed; refuse to guess.
						return FValueSourceEndpoint();
					}
					KnotInput = Pin;
				}
			}
			if (KnotInput == nullptr || KnotInput->LinkedTo.Num() != 1)
			{
				return FValueSourceEndpoint();
			}
			Consumer = KnotInput;
			Current = KnotInput->LinkedTo[0];
		}

		return FValueSourceEndpoint();
	}

	/** The supported link form for an existing pure value: `(-> node-id:output-pin)`. */
	FString SourcePinLink(const FValueSourceEndpoint& InSource)
	{
		if (!InSource.bResolved || InSource.Node == nullptr || InSource.Pin == nullptr)
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

	const bool bFull = InDetail == EDetail::Full;

	if (InPin->LinkedTo.Num() > 0)
	{
        // GL-15: execution inputs may merge multiple valid predecessors; data inputs may not.
        if (InPin->Direction == EGPD_Input && InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && InPin->LinkedTo.Num() > 1)
        {
            TArray<FString> Sources;
            for (const UEdGraphPin* SourcePin : InPin->LinkedTo)
            {
                if (!SourcePin || !SourcePin->GetOwningNode() || SourcePin->Direction != EGPD_Output
                    || SourcePin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || !SourcePin->LinkedTo.Contains(InPin))
                {
                    OutEvidence = EFormEvidence::Partial;
                    const FString Note = TEXT("; error: execution merge contains an invalid endpoint\n");
                    OutNotes.Add(Note);
                    return Note;
                }
                Sources.Add(FString::Printf(TEXT("%s.%s"), *TurboMCPNodeId::MakeForNode(SourcePin->GetOwningNode(), true), *SourcePin->PinName.ToString()));
            }
            Sources.Sort();
            const FString Note = FString::Printf(TEXT("; execution merge '%s': sources=[%s]; multiple execution sources are valid; merge round-trip is not represented by this value form\n"),
                *InPin->PinName.ToString(), *FString::Join(Sources, TEXT(", ")));
            OutNotes.Add(Note);
            OutEvidence = EFormEvidence::Partial;
            return Note;
        }
		// A nonempty link array is not proof of a usable source. A null first
		// element or a null owner must not fall through to the default-value path
		// with Complete evidence.
		if (InPin->LinkedTo.Num() != 1 || InPin->LinkedTo[0] == nullptr
			|| InPin->LinkedTo[0]->GetOwningNode() == nullptr)
		{
			const FString Note = FString::Printf(
				TEXT("; error: value source for pin '%s' has a malformed link\n"),
				*InPin->PinName.ToString());
			OutNotes.Add(Note);
			OutEvidence = EFormEvidence::Partial;
			return Note;
		}

		UEdGraphNode* Target = InPin->LinkedTo[0]->GetOwningNode();

		// A knot is transparent at bodies, so the link is followed to the real
		// node. At full the knot is named, because that is where it must be broken.
		if (IsKnot(Target) && bFull)
		{
			return FString::Printf(TEXT("(-> %s ; via knot)"),
				*TurboMCPNodeId::MakeForNode(Target));
		}

		// At bodies, resolve the exact upstream pin behind any knots. At full a
		// non-knot target is direct; the knot case returned above.
		FValueSourceEndpoint Source;
		if (bFull)
		{
			Source.Node = Target;
			Source.Pin = InPin->LinkedTo.Num() == 1 ? InPin->LinkedTo[0] : nullptr;
			Source.bResolved = Source.Pin != nullptr
				&& Source.Pin->GetOwningNode() == Target
				&& Source.Pin->Direction == EGPD_Output;
		}
		else
		{
			Source = ResolveUpstreamValueSource(InPin);
		}

		if (!Source.bResolved || Source.Node == nullptr || Source.Pin == nullptr)
		{
			const FString Note = FString::Printf(
				TEXT("; error: value source for pin '%s' has an unresolved knot chain\n"),
				*InPin->PinName.ToString());
			OutNotes.Add(Note);
			OutEvidence = EFormEvidence::Partial;
			return Note;
		}

		const UEdGraphNode* RealTarget = Source.Node;

		// A self node is implied by the entry point, so it is not printed.
		if (RealTarget->IsA<UK2Node_Self>())
		{
			return TEXT("self");
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
			for (const UEdGraphPin* OutPin : RealTarget->Pins)
			{
				if (OutPin != nullptr && OutPin->Direction == EGPD_Output)
				{
					Consumers += OutPin->LinkedTo.Num();
				}
			}

			if (Consumers > 1 && InOutSharedValues.Contains(RealTarget))
			{
				const FString Link = SourcePinLink(Source);
				if (Link.IsEmpty())
				{
					OutEvidence = EFormEvidence::Partial;
					return InlineStopReference(RealTarget, TEXT("unresolved source pin"));
				}
				return Link;
			}

			if (Consumers > 1 && HasValueCycle(RealTarget))
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

			OutEvidence = ChildEvidence;
			if (ChildEvidence == EFormEvidence::Complete && !Inline.IsEmpty())
			{
				InOutPrinted.Add(RealTarget);
				if (Consumers > 1)
				{
					InOutSharedValues.Add(RealTarget);
				}
				return Inline;
			}
			if (!Inline.IsEmpty())
			{
				// A partial child form is returned so the caller sees the damage,
				// but it is NEVER marked as a reusable shared definition.
				return Inline;
			}

			OutEvidence = EFormEvidence::Partial;
			return InlineStopReference(RealTarget, TEXT("form not emitted"));
		}

		// An exec or impure node is a separate statement, so the pin refers to it
		// by node ID. `2.1.4` requires the ID so the agent can act on it.
		return FString::Printf(TEXT("(-> %s)"), *TurboMCPNodeId::MakeForNode(RealTarget));
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
		return bLooksNumeric ? Value : FString::Printf(TEXT("\"%s\""), *Value);
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

	const FString TypeId = TurboMCPNodeId::GetNodeTypeId(InNode);
	if (TypeId.IsEmpty())
	{
		// `2.1.12`: a node that cannot be printed becomes a comment line in its
		// place, never a silent gap. It is PARTIAL evidence: text was emitted, but
		// no usable form was produced.
		FString Note = FString::Printf(
			TEXT("; error: %s has no node type ID, so it cannot be written back. ")
			TEXT("Its class is %s."),
			*TurboMCPNodeId::MakeForNode(InNode), *InNode->GetClass()->GetName());
        auto Safe=[](const FString& Value) { return Value.Replace(TEXT("\r"),TEXT("\\r")).Replace(TEXT("\n"),TEXT("\\n")); };
        for(const UEdGraphPin* Pin:InNode->Pins)
        {
            if(!Pin) continue;
            Note+=FString::Printf(TEXT("\n; read-only pin %s (%s, %s): value=%s object=%s text=%s"),
                *Safe(Pin->PinName.ToString()),Pin->Direction==EGPD_Input?TEXT("input"):TEXT("output"),
                *Pin->PinType.PinCategory.ToString(),*Safe(Pin->DefaultValue),
                Pin->DefaultObject?*Safe(Pin->DefaultObject->GetPathName()):TEXT("None"),*Safe(Pin->DefaultTextValue.ToString()));
            for(const UEdGraphPin* Peer:Pin->LinkedTo)
                if(Peer && Peer->GetOwningNodeUnchecked())
                    Note+=FString::Printf(TEXT("\n; linked to %s pin=%s"),*TurboMCPNodeId::MakeForNode(Peer->GetOwningNodeUnchecked(),true),*Safe(Peer->PinName.ToString()));
        }
		OutNotes.Add(Note);
		OutEvidence = EFormEvidence::Partial;
		return Note;
	}

	const bool bFull = InDetail == EDetail::Full;

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
				Out += FString::Printf(TEXT("\n    ; out %d %s -> %d link(s)"),
					OutputIndex - 1, *Pin->PinName.ToString(), Pin->LinkedTo.Num());
			}
			continue;
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

	Out += TEXT(")");
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

		if (InOutPrinted.Contains(Current))
		{
			// `2.1.3`: where two exec paths join, the join is printed once and the
			// other path jumps to it by name.
			if (IsKnot(Current))
			{
				const FString Note = TEXT("; error: execution reroute is revisited (cycle or shared path); continuation is incomplete");
				OutNotes.Add(Note);
				Body += TEXT("\n") + Indent + Note;
			}
			else
			{
				Body += FString::Printf(TEXT("\n%s(goto %s)"),
					*Indent, *TurboMCPNodeId::MakeForNode(Current));
			}
			break;
		}

		EFormEvidence NodeEvidence = EFormEvidence::Complete;
		const FString Rendered = IsKnot(Current) ? FString() : RenderNode(Current, InDetail, InAssetPath,
			/*InInlineDepth=*/0, InOutPrinted, InOutSharedValues,
			InlineNodes, OutNotes, NodeEvidence);
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

		if (ExecOuts.Num() == 1 && ExecOuts[0]->LinkedTo.Num() == 1)
		{
			// The ordinary case: one output, so the chain simply continues in the
			// same block.
			const UEdGraphPin* Endpoint = ExecOuts[0]->LinkedTo[0];
			if (Endpoint == nullptr || Endpoint->GetOwningNode() == nullptr
				|| Endpoint->Direction != EGPD_Input || Endpoint->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
				|| !Endpoint->LinkedTo.Contains(ExecOuts[0]))
			{
				const FString Note = TEXT("; error: malformed execution endpoint");
				OutNotes.Add(Note);
				Body += TEXT("\n") + Indent + Note;
				break;
			}
			Current = Endpoint->GetOwningNode();
			continue;
		}

		// Every declared arm is named, including unconnected outputs. Printing the
		// first one inline would imply the others run after it rather than instead
		// of it, so they are all written the same way.
		for (const UEdGraphPin* Pin : ExecOuts)
		{
			Body += FString::Printf(TEXT("\n%s(:%s"), *Indent, *Pin->PinName.ToString());
			if (Pin->LinkedTo.Num() > 1)
			{
				const FString Note = TEXT("; error: execution output has multiple endpoints; all are shown, but fan-out is not round-trip representable");
				OutNotes.Add(Note);
				Body += TEXT("\n") + Indent + Note;
			}
			for (const UEdGraphPin* Endpoint : Pin->LinkedTo)
			{
				if (Endpoint == nullptr || Endpoint->GetOwningNode() == nullptr
					|| Endpoint->Direction != EGPD_Input || Endpoint->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
					|| !Endpoint->LinkedTo.Contains(Pin))
				{
					const FString Note = TEXT("; error: malformed execution endpoint");
					OutNotes.Add(Note);
					Body += TEXT("\n") + Indent + Note;
					continue;
				}
				Body += RenderChain(Endpoint->GetOwningNode(), InDetail, InDepth + 1,
					InAssetPath, InOutPrinted, InOutSharedValues, OutNotes);
			}
			Body += TEXT(")");
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
			if (Pin->LinkedTo.Num() > 1)
			{
				const FString Note = TEXT("; error: entry execution output has multiple endpoints; all are shown, but fan-out is not round-trip representable");
				OutNotes.Add(Note);
				Body += TEXT("\n  ") + Note;
			}
			for (const UEdGraphPin* Endpoint : Pin->LinkedTo)
			{
				if (Endpoint == nullptr || Endpoint->GetOwningNode() == nullptr
					|| Endpoint->Direction != EGPD_Input || Endpoint->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
					|| !Endpoint->LinkedTo.Contains(Pin))
				{
					const FString Note = TEXT("; error: malformed entry execution endpoint");
					OutNotes.Add(Note);
					Body += TEXT("\n  ") + Note;
					continue;
				}
				Body += RenderChain(Endpoint->GetOwningNode(), InDetail, 0, InAssetPath,
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
	const FString& InAssetPath, TArray<FString>& OutNotes, const FString& EntryFilter) const
{
	if (InGraph == nullptr)
	{
		return FString();
	}

	TArray<FEntryPoint> Entries;
	CollectEntryPoints(InGraph, Entries);
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
    if(!Filter.IsEmpty()) Out+=TEXT("\n; error: filtered inspection view, not a complete graph for write-back\n; entry-name contains: ")+Filter;

	if (InDetail == EDetail::Manifest)
	{
		Out += FString::Printf(TEXT("\n; %d node(s), %d entry point(s)"),
			NodeCount, Entries.Num());
		for (const FEntryPoint& Entry : Entries)
		{
			Out += FString::Printf(TEXT("\n;   %s %s"), *Entry.Kind, *Entry.Name);
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
			Out += FString::Printf(TEXT("\n(%s %s%s)"),
				*Entry.Kind, *Entry.Name, *Signature);
		}
		return Out;
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
		const FString Head = FString::Printf(TEXT("\n(%s %s"),
			*Entry.Kind, *Entry.Name);
		const FString Body = RenderEntryBody(InGraph, Entry, InDetail, InAssetPath,
			Printed, SharedValues, OutNotes);
		Out += Head + Body + TEXT(")");
	}

    if(!Filter.IsEmpty()) return Out;

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
			Out += FString::Printf(TEXT("\n;   %s  %s"),
				*TurboMCPNodeId::MakeForNode(Node),
				*TurboMCPNodeId::GetNodeTitle(Node));
			++Shown;
		}
	}

	return Out;
}
