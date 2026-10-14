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
			OutReachable.Add(Node);
			Stack.Add(Node);
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
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				const UEdGraphNode* Next = Linked != nullptr ? Linked->GetOwningNode() : nullptr;
				if (Next != nullptr && !OutReachable.Contains(Next))
				{
					OutReachable.Add(Next);
					Stack.Add(Next);
				}
			}
		}
	}
}
