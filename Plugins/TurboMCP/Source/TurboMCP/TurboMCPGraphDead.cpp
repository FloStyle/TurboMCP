// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphDead.h"

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_Tunnel.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphReachability.h"
#include "TurboMCPNodeId.h"

namespace
{
	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	/**
	 * True when this node is an ENTRY and must never be removed.
	 *
	 * `03_TOOLS.md` names events, custom events and function entries. `K2Node_Tunnel` is added
	 * because it is what a function or macro entry and exit actually are in a compiled graph, and
	 * removing one would leave a graph that cannot be entered at all. `K2Node_Knot` is NOT an entry
	 * but is skipped as a node in its own right, because a knot is a piece of wire.
	 */
	bool IsEntryNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Event>(InNode) != nullptr
			|| Cast<UK2Node_CustomEvent>(InNode) != nullptr
			|| Cast<UK2Node_FunctionEntry>(InNode) != nullptr
			|| Cast<UK2Node_Tunnel>(InNode) != nullptr;
	}

	/** True when a node is a knot, which carries a wire rather than doing work. */
	bool IsKnot(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Knot>(InNode) != nullptr;
	}

	/** The variable a node writes, or an empty name. */
	FName WriteOf(const UEdGraphNode* InNode)
	{
		if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(InNode))
		{
			return Set->GetVarName();
		}
		return NAME_None;
	}

	/** The variable a node reads, or an empty name. */
	FName ReadOf(const UEdGraphNode* InNode)
	{
		if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(InNode))
		{
			return Get->GetVarName();
		}
		return NAME_None;
	}

	/** The function a node calls, or an empty name. */
	FName CallOf(const UEdGraphNode* InNode)
	{
		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode))
		{
			return Call->GetFunctionName();
		}
		return NAME_None;
	}

	/**
	 * Groups the nodes that are not reachable, by which ones are wired to each other.
	 *
	 * A COMMENT NODE IS NEVER A GROUP, AND ITS INCLUDE AT THE TOP OF THIS FILE USED TO BE UNUSED - WHICH WAS
	 * THE CLUE. A comment is a DOCUMENTATION BOX: it has NO PINS AT ALL, so a walk that starts from entry
	 * nodes and follows wires can NEVER reach one. Every comment in every graph therefore looked exactly
	 * like an unreachable orphan, and since it also writes no variable and holds no entry, it satisfied all
	 * three conditions of the removal test.
	 *
	 * MEASURED, which is how this was found: `BP_NodeTest:EventGraph` has 13 nodes and the tool reported
	 * `1 unreachable group(s)` whose only member was `EdGraphNode_Comment`, as `WOULD REMOVE`. The author's
	 * COMMENT BOXES were queued for deletion as dead code. `E-GRAPH-DEAD` is documented in
	 * `Spec/03_TOOLS.md` as removing dead code, and a comment is not code - it is the one part of a graph
	 * that exists purely to be read by a person.
	 *
	 * THE GUARD IS HERE RATHER THAN IN `MarkReachable` DELIBERATELY. Marking comments reachable would also
	 * put them in the live set that other answers are computed from, and a comment has no pins to
	 * contribute to those answers either way. Excluding them at the point where a group is FORMED is the
	 * smallest change that cannot make another answer disagree.
	 */
	void CollectUnreachableGroups(UEdGraph* InGraph, const TSet<const UEdGraphNode*>& InReachable,
		TArray<TArray<UEdGraphNode*>>& OutGroups)
	{
		TSet<const UEdGraphNode*> Seen;
		for (UEdGraphNode* Seed : InGraph->Nodes)
		{
			if (Seed == nullptr || InReachable.Contains(Seed) || Seen.Contains(Seed))
			{
				continue;
			}

			// A comment is not dead code, so it neither forms a group nor seeds one.
			if (Seed->IsA<UEdGraphNode_Comment>())
			{
				Seen.Add(Seed);
				continue;
			}

			TArray<UEdGraphNode*> Group;
			TArray<UEdGraphNode*> Stack;
			Stack.Add(Seed);
			Seen.Add(Seed);

			while (Stack.Num() > 0)
			{
				UEdGraphNode* Node = Stack.Pop();
				Group.Add(Node);
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin == nullptr)
					{
						continue;
					}
					for (UEdGraphPin* Linked : Pin->LinkedTo)
					{
						if (Linked == nullptr)
						{
							continue;
						}
						UEdGraphNode* Next = Linked->GetOwningNode();
						if (Next != nullptr && !InReachable.Contains(Next)
							&& !Seen.Contains(Next))
						{
							Seen.Add(Next);
							Stack.Add(Next);
						}
					}
				}
			}

			OutGroups.Add(MoveTemp(Group));
		}
	}

	/** Every variable any REACHABLE node in the whole Blueprint reads. */
	void CollectLiveReads(UBlueprint* InBlueprint, const TSet<const UEdGraphNode*>& InReachableThisGraph,
		UEdGraph* InThisGraph, TSet<FName>& OutReads)
	{
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : InBlueprint->UbergraphPages)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->MacroGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->EventGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}

		for (UEdGraph* Graph : Graphs)
		{
			TSet<const UEdGraphNode*> Reachable;
			if (Graph == InThisGraph)
			{
				// The graph being examined already has its own answer, so it is reused rather than
				// recomputed; recomputing would be the same walk twice and could disagree.
				Reachable = InReachableThisGraph;
			}
			else
			{
				TurboMCPGraphReachability::MarkReachable(Graph, Reachable);
			}

			for (const UEdGraphNode* Node : Reachable)
			{
				// A read OR a write counts: another graph writing a variable is also a use of it,
				// because the value this group would have written is what that graph would see.
				const FName Read = ReadOf(Node);
				if (!Read.IsNone()) { OutReads.Add(Read); }
				const FName Write = WriteOf(Node);
				if (!Write.IsNone()) { OutReads.Add(Write); }
			}
		}
	}

	/** Every function name called by live code anywhere in the Blueprint. */
	void CollectLiveCalls(UBlueprint* InBlueprint, const TSet<const UEdGraphNode*>& InReachableThisGraph,
		UEdGraph* InThisGraph, TSet<FName>& OutCalls)
	{
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : InBlueprint->UbergraphPages)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->MacroGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}
		for (UEdGraph* Graph : InBlueprint->EventGraphs)
		{
			if (Graph != nullptr) { Graphs.Add(Graph); }
		}

		for (UEdGraph* Graph : Graphs)
		{
			TSet<const UEdGraphNode*> Reachable;
			if (Graph == InThisGraph)
			{
				Reachable = InReachableThisGraph;
			}
			else
			{
				TurboMCPGraphReachability::MarkReachable(Graph, Reachable);
			}

			for (const UEdGraphNode* Node : Reachable)
			{
				const FName Call = CallOf(Node);
				if (!Call.IsNone())
				{
					OutCalls.Add(Call);
				}
			}
		}
	}
}

bool FTurboMCPGraphDead::Plan(const FString& InAsset, const FString& InGraphName,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is not a Blueprint, so it has no dead nodes."), *Resolved.ObjectPath);
		return false;
	}

	// The graph, by name or the event graph by default.
	TArray<UEdGraph*> Graphs;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	for (UEdGraph* Graph : Blueprint->EventGraphs)
	{
		if (Graph != nullptr) { Graphs.Add(Graph); }
	}
	if (Graphs.Num() == 0)
	{
		OutError = TEXT("the Blueprint has no graphs.");
		return false;
	}

	UEdGraph* Graph = nullptr;
	if (OutPlan.GraphName.IsEmpty())
	{
		for (UEdGraph* Candidate : Graphs)
		{
			if (NameMatches(Candidate->GetFName(), TEXT("EventGraph")))
			{
				Graph = Candidate;
				break;
			}
		}
		if (Graph == nullptr)
		{
			Graph = Graphs[0];
		}
	}
	else
	{
		for (UEdGraph* Candidate : Graphs)
		{
			if (NameMatches(Candidate->GetFName(), OutPlan.GraphName))
			{
				Graph = Candidate;
				break;
			}
		}
		if (Graph == nullptr)
		{
			TArray<FString> Names;
			for (UEdGraph* Candidate : Graphs) { Names.Add(Candidate->GetName()); }
			Names.Sort();
			OutError = FString::Printf(
				TEXT("'%s' is not a graph of this Blueprint. Its graphs are: %s"),
				*OutPlan.GraphName, *FString::Join(Names, TEXT(", ")));
			return false;
		}
	}

	OutPlan.GraphName = Graph->GetName();
	OutPlan.Asset = Resolved.ObjectPath;
	OutPlan.NodeCountBefore = Graph->Nodes.Num();

	TSet<const UEdGraphNode*> Reachable;
	TurboMCPGraphReachability::MarkReachable(Graph, Reachable);

	TSet<FName> LiveReads;
	CollectLiveReads(Blueprint, Reachable, Graph, LiveReads);
	TSet<FName> LiveCalls;
	CollectLiveCalls(Blueprint, Reachable, Graph, LiveCalls);

	TArray<TArray<UEdGraphNode*>> Groups;
	CollectUnreachableGroups(Graph, Reachable, Groups);

	for (const TArray<UEdGraphNode*>& GroupNodes : Groups)
	{
		FGroup Group;

		TSet<FName> GroupWrites;
		TSet<FName> GroupCalls;
		TSet<FString> ClassNames;

		for (UEdGraphNode* Node : GroupNodes)
		{
			Group.Keys.Add(KeyOf(Node));
			ClassNames.Add(Node->GetClass()->GetName());

			if (IsEntryNode(Node))
			{
				// NEVER A CANDIDATE. Named separately from the reachability test, because an entry
				// node in an unreachable group is still an entry: a custom event nothing calls yet
				// is a hook the user made on purpose.
				Group.bHoldsEntry = true;
			}

			const FName Write = WriteOf(Node);
			if (!Write.IsNone())
			{
				GroupWrites.Add(Write);
				Group.Writes.Add(Write.ToString());
			}
			const FName Call = CallOf(Node);
			if (!Call.IsNone())
			{
				GroupCalls.Add(Call);
				Group.Calls.Add(Call.ToString());
			}
		}

		Group.Keys.Sort();
		Group.Classes = ClassNames.Array();
		Group.Classes.Sort();
		Group.Writes.Sort();
		Group.Calls.Sort();

		// `bUnreachable` is true by construction: these groups were collected BECAUSE nothing
		// reached them. It is stored rather than assumed so the report can say it.
		Group.bUnreachable = true;

		// Whose writes are never read, checked against the WHOLE Blueprint's live reads.
		Group.bWritesAreUnread = true;
		for (const FName& Write : GroupWrites)
		{
			if (LiveReads.Contains(Write))
			{
				Group.bWritesAreUnread = false;
				break;
			}
		}

		// Does it repeat live logic? A group that calls something live code also calls, or touches
		// a variable live code touches, is very likely a copy of it — which is what a reader needs
		// to know before agreeing to delete it.
		for (const FName& Call : GroupCalls)
		{
			if (LiveCalls.Contains(Call))
			{
				Group.bRepeatsLiveLogic = true;
				Group.RepeatedItems.Add(FString::Printf(TEXT("calls %s"), *Call.ToString()));
			}
		}
		for (const FName& Write : GroupWrites)
		{
			if (LiveReads.Contains(Write))
			{
				Group.bRepeatsLiveLogic = true;
				Group.RepeatedItems.Add(
					FString::Printf(TEXT("touches %s, which live code uses"), *Write.ToString()));
			}
		}

		Group.Summary = FString::Printf(TEXT("%d node(s): %s"),
			Group.Keys.Num(), *FString::Join(Group.Classes, TEXT(", ")));

		const int32 Index = OutPlan.Groups.Add(MoveTemp(Group));
		const FGroup& Added = OutPlan.Groups[Index];

		// A CANDIDATE ONLY WHEN ALL THREE HOLD. Any one of them failing is a reason to keep, and the
		// reasons are printed rather than left implicit.
		if (!Added.bHoldsEntry && Added.bWritesAreUnread && Added.bUnreachable)
		{
			OutPlan.CandidateIndices.Add(Index);
		}
	}

	return true;
}

FString FTurboMCPGraphDead::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: dead nodes in %s : %s"),
		*InPlan.Asset, *InPlan.GraphName);
	Out += FString::Printf(TEXT("\n%d node(s) in the graph, %d unreachable group(s)"),
		InPlan.NodeCountBefore, InPlan.Groups.Num());

	if (InPlan.Groups.Num() == 0)
	{
		// `P1`: never return nothing. "Nothing found" with the reason is the answer.
		Out += TEXT("\nNo removal candidates were found by these static checks. This does not prove runtime reachability.");
		return Out;
	}

	for (int32 Index = 0; Index < InPlan.Groups.Num(); ++Index)
	{
		const FGroup& Group = InPlan.Groups[Index];
		const bool bCandidate = InPlan.CandidateIndices.Contains(Index);

		Out += FString::Printf(TEXT("\n\n%s  %s"),
			bCandidate ? TEXT("WOULD REMOVE") : TEXT("would KEEP  "), *Group.Summary);
		Out += FString::Printf(TEXT("\n    nodes: %s"), *FString::Join(Group.Keys, TEXT(", ")));

		if (Group.bHoldsEntry)
		{
			Out += TEXT("\n    KEPT: it holds an event, a custom event or a function entry. An ")
				TEXT("entry is never removed, even when nothing reaches it — a hook the user made ")
				TEXT("on purpose looks the same as one nothing calls.");
		}
		if (!Group.bWritesAreUnread)
		{
			Out += FString::Printf(
				TEXT("\n    KEPT: it writes %s, which live code reads."),
				*FString::Join(Group.Writes, TEXT(", ")));
		}
		if (Group.Writes.Num() == 0 && Group.Calls.Num() == 0 && !Group.bHoldsEntry)
		{
			Out += TEXT("\n    Kept: this group has no recognized variable read or function call, so it does not meet the removal heuristic. External dependencies were not ruled out.");
		}

		if (Group.bRepeatsLiveLogic)
		{
			Out += FString::Printf(
				TEXT("\n    REPEATS LIVE LOGIC: %s"),
				*FString::Join(Group.RepeatedItems, TEXT("; ")));
		}
		else
		{
			Out += TEXT("\n    does not repeat live logic.");
		}
	}

	Out += FString::Printf(
		TEXT("\n\nthe edit would remove %d group(s) and leave %d, and would then report the node ")
		TEXT("counts before and after so the change can be checked."),
		InPlan.CandidateIndices.Num(), InPlan.Groups.Num() - InPlan.CandidateIndices.Num());

	if (InPlan.CandidateIndices.Num() == 0)
	{
		Out += TEXT("\nNOTHING WOULD BE REMOVED: every unreachable group is kept for one of the ")
			TEXT("reasons above.");
	}

	return Out;
}

FString FTurboMCPGraphDead::Apply(const FPlan& InPlan)
{
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(TEXT("error: %s is not a Blueprint."), *Resolved.ObjectPath);
	}

	// Keep apply's graph resolution identical to the reader and planning phase:
	// all graph arrays are eligible and names are case-insensitive.
	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, InPlan.GraphName);
	if (Graph == nullptr)
	{
		return FString::Printf(TEXT("error: graph '%s' is no longer in %s."),
			*InPlan.GraphName, *Resolved.ObjectPath);
	}

	if (InPlan.CandidateIndices.Num() == 0)
	{
		return FString::Printf(
			TEXT("unchanged: %s has %d unreachable group(s) and none meets the removal heuristic, so ")
			TEXT("nothing was deleted. PreviewTurboMCPGraphDead says which reason applies to each."),
			*InPlan.GraphName, InPlan.Groups.Num());
	}

	// THE NODES ARE RESOLVED BY KEY AGAIN rather than held from the plan, because a plan is made and
	// applied in two calls and the graph could have changed between them. A key that no longer
	// resolves is REPORTED, not silently skipped: quietly removing eight of ten nodes would be a
	// half-applied edit, which `P8` forbids.
	const int32 Before = Graph->Nodes.Num();
	int32 Removed = 0;
	int32 Skipped = 0;
	TArray<FString> Missing;
	TArray<FString> RemovedClasses;

	for (const int32 Index : InPlan.CandidateIndices)
	{
		if (!InPlan.Groups.IsValidIndex(Index))
		{
			continue;
		}
		for (const FString& Key : InPlan.Groups[Index].Keys)
		{
			UEdGraphNode* Found = nullptr;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node != nullptr && KeyOf(Node).Equals(Key, ESearchCase::IgnoreCase))
				{
					Found = Node;
					break;
				}
			}

			if (Found == nullptr)
			{
				++Skipped;
				Missing.Add(Key);
				continue;
			}

			// The entry rule is re-checked HERE, on the node that is about to go, rather than
			// trusted from the plan. It is the rule whose failure destroys work.
			if (IsEntryNode(Found))
			{
				++Skipped;
				Missing.Add(Key + TEXT(" (an entry node)"));
				continue;
			}

			RemovedClasses.Add(Found->GetClass()->GetName());
			Graph->Modify();
			// `FBlueprintEditorUtils::RemoveNode` is the editor's own removal: it breaks the links,
			// tells the Blueprint, and does not leave a dangling pin behind.
			FBlueprintEditorUtils::RemoveNode(Blueprint, Found, /*bDontRecompile=*/true);
			++Removed;
		}
	}

	if (Removed > 0)
	{
		Graph->MarkPackageDirty();
	}

	FString Out = FString::Printf(TEXT("changed: removed %d node(s) from %s : %s"),
		Removed, *Resolved.ObjectPath, *Graph->GetName());
	Out += FString::Printf(TEXT("\nnodes: %d -> %d"), Before, Graph->Nodes.Num());

	if (Skipped > 0)
	{
		Out = TEXT("partial: some selected nodes were not removed.\n") + Out;
		Out += FString::Printf(
			TEXT("\n%d node(s) were NOT removed because they no longer resolve or are entries: %s"),
			Skipped, *FString::Join(Missing, TEXT(", ")));
	}

	Out += TEXT("\nChanges are in memory. No tool-owned undo transaction, compilation or save was performed. Inspect the graph, compile it, then use EditTurboMCPSave if the result is correct. Runtime behavior is not verified.");

	if (Removed == 0)
	{
		Out = FString::Printf(
			TEXT("unchanged: nothing could be removed from %s : %s. %d node key(s) were missing or protected entries. ")
			TEXT("Read the graph again before retrying."),
			*Resolved.ObjectPath, *Graph->GetName(), Skipped);
	}

	return Out;
}
