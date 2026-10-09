// Copyright FloStyle. All Rights Reserved.
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
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "UObject/UObjectGlobals.h"

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

	/**
	 * Follows a chain of knots to the real node behind them.
	 *
	 * `2.1.7` says a plain reroute is invisible at `bodies`, so a link reads as
	 * direct. At `full` the knots are named instead, because an agent breaking the
	 * link has to break it at the knot.
	 */
	UEdGraphNode* ResolveThroughKnots(const UEdGraphNode* InNode, int32 InDepth = 0)
	{
		if (InNode == nullptr || InDepth > 64)
		{
			return const_cast<UEdGraphNode*>(InNode);
		}
		if (!IsKnot(InNode))
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

	// The self pin and the world context pin are implicit when they carry their
	// automatic default, which is what `2.1.8` means by "not printed when they
	// are implicit".
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
	const FString& InAssetPath,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TArray<FString>& OutNotes) const
{
	if (InPin == nullptr)
	{
		return FString();
	}

	const bool bFull = InDetail == EDetail::Full;

	if (UEdGraphNode* Target = LinkedNode(InPin))
	{
		// A knot is transparent at bodies, so the link is followed to the real
		// node. At full the knot is named, because that is where it must be broken.
		if (IsKnot(Target) && bFull)
		{
			return FString::Printf(TEXT("(-> %s ; via knot)"),
				*TurboMCPNodeId::MakeForNode(Target));
		}

		UEdGraphNode* RealTarget = ResolveThroughKnots(Target);
		if (RealTarget == nullptr)
		{
			return FString();
		}

		// A self node is implied by the entry point, so it is not printed.
		if (RealTarget->IsA<UK2Node_Self>())
		{
			return TEXT("self");
		}

		if (IsValueNode(RealTarget))
		{
			// `2.1.6`: a pure node used by more than one consumer is defined once
			// and referred to by name. Used once it is written inline.
			int32 Consumers = 0;
			for (const UEdGraphPin* OutPin : RealTarget->Pins)
			{
				if (OutPin != nullptr && OutPin->Direction == EGPD_Output)
				{
					Consumers += OutPin->LinkedTo.Num();
				}
			}

			if (Consumers > 1)
			{
				InOutSharedValues.Add(RealTarget);
				// Named by node ID, which is stable, rather than by a made up
				// variable name that could collide.
				return FString::Printf(TEXT("$%s"),
					*TurboMCPNodeId::MakeForNode(RealTarget));
			}

			const FString Inline = RenderNode(RealTarget, InDetail, InAssetPath,
				InOutPrinted, InOutSharedValues, OutNotes);
			if (!Inline.IsEmpty())
			{
				InOutPrinted.Add(RealTarget);
				return Inline;
			}
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
	const FString& InAssetPath,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TSet<const UEdGraphNode*>& InOutSharedValues,
	TArray<FString>& OutNotes) const
{
	if (InNode == nullptr)
	{
		return FString();
	}

	// A function or event entry is not a statement in the body: it is the head of
	// the form, so it is never rendered as a node.
	if (InNode->IsA<UK2Node_FunctionEntry>())
	{
		return FString();
	}

	const FString TypeId = TurboMCPNodeId::GetNodeTypeId(InNode);
	if (TypeId.IsEmpty())
	{
		// `2.1.12`: a node that cannot be printed becomes a comment line in its
		// place, never a silent gap.
		const FString Note = FString::Printf(
			TEXT("; error: %s has no node type ID, so it cannot be written back. ")
			TEXT("Its class is %s."),
			*TurboMCPNodeId::MakeForNode(InNode), *InNode->GetClass()->GetName());
		OutNotes.Add(Note);
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
		// among what the `full` level adds, and the first version advanced the counter for OUTPUTS
		// too while printing -1 for them, so the index it did print for an input was already wrong
		// once an output had been walked.
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
			break;
		}

		// `2.1.5`: use the pin's real name, not its display name.
		FString PinLabel = Pin->PinName.ToString();
		FString PinValue = RenderPinValue(Pin, InDetail, InAssetPath,
			InOutPrinted, InOutSharedValues, OutNotes);

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
	FString Body;
	const FString Indent = FString::ChrN(InDepth * 2 + 2, TEXT(' '));

	const UEdGraphNode* Current = InFirst;
	int32 Guard = 0;

	while (Current != nullptr && Guard < 2000)
	{
		++Guard;

		if (IsComment(Current))
		{
			// A comment box is not part of the flow, so it is stepped over.
			UEdGraphPin* Skip = FindExecOut(Current);
			Current = Skip != nullptr ? LinkedNode(Skip) : nullptr;
			continue;
		}

		Current = ResolveThroughKnots(Current);

		if (InOutPrinted.Contains(Current))
		{
			// `2.1.3`: where two exec paths join, the join is printed once and the
			// other path jumps to it by name.
			Body += FString::Printf(TEXT("\n%s(goto %s)"),
				*Indent, *TurboMCPNodeId::MakeForNode(Current));
			break;
		}

		const FString Rendered = RenderNode(Current, InDetail, InAssetPath,
			InOutPrinted, InOutSharedValues, OutNotes);
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
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
				&& Pin->LinkedTo.Num() > 0)
			{
				ExecOuts.Add(Pin);
			}
		}

		if (ExecOuts.Num() == 0)
		{
			break;
		}

		if (ExecOuts.Num() == 1)
		{
			// The ordinary case: one output, so the chain simply continues in the
			// same block.
			Current = LinkedNode(ExecOuts[0]);
			continue;
		}

		// Several outputs: every arm becomes a named continuation. Printing the
		// first one inline would imply the others run after it rather than instead
		// of it, so they are all written the same way.
		for (const UEdGraphPin* Pin : ExecOuts)
		{
			Body += FString::Printf(TEXT("\n%s(:%s"), *Indent, *Pin->PinName.ToString());
			Body += RenderChain(LinkedNode(Pin), InDetail, InDepth + 1,
				InAssetPath, InOutPrinted, InOutSharedValues, OutNotes);
			Body += TEXT(")");
		}
		break;
	}

	if (Guard >= 2000)
	{
		// A cycle in exec links should not be possible, but a writer that loops
		// forever on a malformed graph is worse than one that says so.
		const FString Note = TEXT("; error: the exec chain did not end after 2000 nodes");
		OutNotes.Add(Note);
		Body += TEXT("\n") + Indent + Note;
	}

	return Body;
}

FString FTurboMCPGraphText::RenderEntryBody(const UEdGraph* InGraph,
	const FEntryPoint& InEntry, EDetail InDetail, const FString& InAssetPath,
	TSet<const UEdGraphNode*>& InOutPrinted,
	TArray<FString>& OutNotes) const
{
	TSet<const UEdGraphNode*> SharedValues;

	UEdGraphPin* FirstOut = FindExecOut(InEntry.Node);
	const UEdGraphNode* First = FirstOut != nullptr ? LinkedNode(FirstOut) : nullptr;

	FString Body = RenderChain(First, InDetail, /*InDepth=*/0, InAssetPath,
		InOutPrinted, SharedValues, OutNotes);

	if (Body.IsEmpty())
	{
		// `P1`: an entry point with no body says so rather than printing nothing.
		Body = TEXT("\n  ; this entry point has no exec chain and no body");
	}

	return Body;
}

FString FTurboMCPGraphText::Render(const UEdGraph* InGraph, EDetail InDetail,
	const FString& InAssetPath, TArray<FString>& OutNotes) const
{
	if (InGraph == nullptr)
	{
		return FString();
	}

	TArray<FEntryPoint> Entries;
	CollectEntryPoints(InGraph, Entries);

	const int32 NodeCount = InGraph->Nodes.Num();
	const FString GraphName = InGraph->GetName();

	FString Out = FString::Printf(TEXT("; graph %s.%s"),
		*InAssetPath, *GraphName);

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

	// Bodies and full. A comment box is printed as a note about the region it
	// covers, because it groups nodes an agent may want to know about, but it is
	// not part of the flow.
	TArray<FString> Comments;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (IsComment(Node))
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
			Printed, OutNotes);
		Out += Head + Body + TEXT(")");
	}

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
		if (!Printed.Contains(Node))
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
