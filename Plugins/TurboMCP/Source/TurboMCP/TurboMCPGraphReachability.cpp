// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphReachability.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Tunnel.h"

namespace
{
	bool IsEntryNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Event>(InNode) != nullptr
			|| Cast<UK2Node_CustomEvent>(InNode) != nullptr
			|| Cast<UK2Node_FunctionEntry>(InNode) != nullptr
			|| Cast<UK2Node_Tunnel>(InNode) != nullptr;
	}

	bool HasExecPin(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return true;
			}
		}
		return false;
	}

	bool IsPureValueNode(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && !HasExecPin(InNode);
	}

	void AddIfNew(const UEdGraphNode* InNode, TSet<const UEdGraphNode*>& OutReachable,
		TArray<const UEdGraphNode*>& InOutStack)
	{
		if (InNode != nullptr && !OutReachable.Contains(InNode))
		{
			OutReachable.Add(InNode);
			InOutStack.Add(InNode);
		}
	}
}

void TurboMCPGraphReachability::MarkReachable(const UEdGraph* InGraph,
	TSet<const UEdGraphNode*>& OutReachable)
{
	OutReachable.Reset();
	if (InGraph == nullptr)
	{
		return;
	}

	TArray<const UEdGraphNode*> Stack;
	for (const UEdGraphNode* Node : InGraph->Nodes)
	{
		if (IsEntryNode(Node))
		{
			AddIfNew(Node, OutReachable, Stack);
		}
	}

	while (Stack.Num() > 0)
	{
		const UEdGraphNode* Node = Stack.Pop();
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}

			const bool bIsExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			if (bIsExec && Pin->Direction == EGPD_Output)
			{
				// Execution flows forward from an entry. A data-only link must not
				// make an impure node appear live.
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					AddIfNew(Linked != nullptr ? Linked->GetOwningNode() : nullptr,
						OutReachable, Stack);
				}
				continue;
			}

			if (Pin->Direction != EGPD_Input)
			{
				continue;
			}

			// A live node reads its input values. Walk backwards only into pure
			// producers, so a disconnected impure body is not rescued by a data wire.
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				const UEdGraphNode* Source = Linked != nullptr ? Linked->GetOwningNode() : nullptr;
				if (IsPureValueNode(Source))
				{
					AddIfNew(Source, OutReachable, Stack);
				}
			}
		}
	}
}
