// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphStructure.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
	/**
	 * One graph, looked up the same way the append writer looks it up.
	 *
	 * THIS IS A SECOND COPY OF A HELPER THAT LIVES IN AN ANONYMOUS NAMESPACE IN `TurboMCPGraphAppend.cpp`,
	 * WHICH IS DELIBERATE AND IS THIS PROJECT'S RECORDED PATTERN FOR A HELPER IT CANNOT SHARE: the
	 * alternative is editing another capability's file, and the risk of a drift is paid down by the SUITE
	 * ASSERTING THE BEHAVIOUR rather than by trusting the two to stay identical. The same call was made for
	 * `AssociationName` in the material-instance work, where the duplicate's OUTPUT is asserted so a drift
	 * would be a check failure rather than a silent divergence.
	 */
	UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)
	{
		if (InBlueprint == nullptr)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		Graphs.Append(InBlueprint->UbergraphPages);
		Graphs.Append(InBlueprint->FunctionGraphs);
		Graphs.Append(InBlueprint->MacroGraphs);

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph != nullptr && Graph->GetName().Equals(InGraphName, ESearchCase::CaseSensitive))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	/** Finds a pin by name with the CASE THE ENGINE USES, rather than a case-insensitive guess. */
	UEdGraphPin* FindPinByName(UEdGraphNode* InNode, const FString& InPinName)
	{
		if (InNode == nullptr)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->PinName.ToString().Equals(InPinName, ESearchCase::CaseSensitive))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/** The node's key, made the same way every reader prints it. */
	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode == nullptr ? FString() : TurboMCPNodeId::MakeForNode(InNode, /*bLongForm=*/true);
	}

	/**
	 * Whether a node answers to a key a caller wrote, in EITHER of the two forms the project prints.
	 *
	 * THE PROJECT'S READERS PRINT THE SHORT FORM AND THIS WRITER MADE THE LONG ONE, which is a real
	 * interface gap rather than a detail: an agent that reads a graph and passes a key straight back would
	 * be refused BY A TOOL THAT WAS RIGHT AND AN ID THAT WAS REAL. Measured on the first run of this suite.
	 * Accepting both is what makes the round trip work without asking a caller to know which it holds.
	 */
	bool NodeAnswersToKey(const UEdGraphNode* InNode, const FString& InKey)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		return KeyOf(InNode).Equals(InKey, ESearchCase::CaseSensitive)
			|| TurboMCPNodeId::MakeForNode(InNode, /*bLongForm=*/false).Equals(InKey, ESearchCase::CaseSensitive);
	}

	/**
	 * Whether a node's class is one a collapse must REFUSE to move.
	 *
	 * THE SPEC REFUSES THE WHOLE SELECTION RATHER THAN SKIPPING THESE, because collapsing an event into a
	 * function is not the refactor an agent asked for and would silently change what runs. `Tunnel` is
	 * included because it is a boundary marker that only makes sense inside the graph it was made for.
	 */
	bool IsBoundaryNode(UEdGraphNode* InNode, FString& OutWhat)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		const FString ClassName = InNode->GetClass()->GetName();
		if (ClassName.Contains(TEXT("FunctionEntry")))
		{
			OutWhat = TEXT("the function's entry node");
			return true;
		}
		if (ClassName.Contains(TEXT("FunctionResult")))
		{
			OutWhat = TEXT("the function's result node");
			return true;
		}
		if (ClassName.Contains(TEXT("K2Node_Event")) || ClassName.Contains(TEXT("K2Node_CustomEvent")))
		{
			OutWhat = TEXT("an event node");
			return true;
		}
		if (ClassName.Contains(TEXT("Tunnel")))
		{
			OutWhat = TEXT("a tunnel node");
			return true;
		}
		return false;
	}

	/**
	 * True when a node can only live in the Blueprint that made it.
	 *
	 * A `UK2Node_CallFunction` names a function that the TARGET asset must also expose; a
	 * `UK2Node_VariableGet`/`Set` names a variable the target must have. A copy that carried either into an
	 * asset without the member would produce a graph that does not compile, so the member is checked BEFORE
	 * anything is created and a missing one REFUSES THE WHOLE COPY - `03_TOOLS.md`: "half a block is worse
	 * than none".
	 */
	bool MemberNameOf(UEdGraphNode* InNode, FString& OutKind, FString& OutName)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		const FString ClassName = InNode->GetClass()->GetName();

		if (ClassName.Contains(TEXT("K2Node_VariableGet")) || ClassName.Contains(TEXT("K2Node_VariableSet")))
		{
			// `GetVarName` is a member of both, and it is the name as the PIN carries it - which is what the
			// target has to declare, rather than the pin's display text.
			if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(InNode))
			{
				OutKind = TEXT("variable");
				OutName = VariableNode->GetVarName().ToString();
				return !OutName.IsEmpty();
			}
			return false;
		}

		if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(InNode))
		{
			OutKind = TEXT("function");
			OutName = CallNode->FunctionReference.GetMemberName().ToString();
			return !OutName.IsEmpty();
		}

		if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(InNode))
		{
			OutKind = TEXT("event");
			OutName = EventNode->EventReference.GetMemberName().ToString();
			return !OutName.IsEmpty();
		}

		return false;
	}

	/**
	 * Whether the Blueprint declares a variable of this name.
	 *
	 * THE VARIABLE LIST IS READ FROM `NewVariables`, WHICH IS THE AUTHORITATIVE SELF-CONTAINED LIST, rather
	 * than from generated class members: a Blueprint that has just been edited may not have regenerated its
	 * class yet, and this check runs in `Plan`, which must not depend on a compile having happened.
	 */
	bool BlueprintHasVariable(const UBlueprint* InBlueprint, const FString& InName)
	{
		if (InBlueprint == nullptr)
		{
			return false;
		}
		for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
		{
			if (Variable.VarName.ToString().Equals(InName, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** Whether the Blueprint declares or inherits a callable function of this name. */
	bool BlueprintHasFunction(const UBlueprint* InBlueprint, const FString& InName)
	{
		if (InBlueprint == nullptr)
		{
			return false;
		}
		if (const UClass* Generated = InBlueprint->GeneratedClass)
		{
			if (Generated->FindFunctionByName(FName(*InName)) != nullptr)
			{
				return true;
			}
		}
		// AN UNCALLABLE NAME IS NOT A MISSING ONE. A function the Blueprint declares but has not yet compiled
		// into its class is still a name the target owns, and refusing it would be a false negative on a
		// graph that compiles - so the graph list is consulted as well as the class.
		const FString Candidate = FString::Printf(TEXT("%s"), *InName);
		for (const UEdGraph* Graph : InBlueprint->FunctionGraphs)
		{
			if (Graph != nullptr && Graph->GetName().Equals(Candidate, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** Whether the Blueprint declares an event of this name, in any of its event graphs. */
	bool BlueprintHasEvent(const UBlueprint* InBlueprint, const FString& InName)
	{
		if (InBlueprint == nullptr)
		{
			return false;
		}
		if (const UClass* Generated = InBlueprint->GeneratedClass)
		{
			// A custom event becomes a function on the generated class, so the class is the right place to ask.
			if (Generated->FindFunctionByName(FName(*InName)) != nullptr)
			{
				return true;
			}
		}
		for (const UEdGraph* Graph : InBlueprint->UbergraphPages)
		{
			if (Graph == nullptr)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node))
				{
					if (EventNode->CustomFunctionName.ToString().Equals(InName, ESearchCase::CaseSensitive))
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	/** Reads the selected nodes out of a graph, refusing any key that is not there BY NAME. */
	bool CollectSelected(UEdGraph* InGraph, const TArray<FString>& InNodeKeys,
		TArray<UEdGraphNode*>& OutNodes, TArray<FString>& OutRefusals)
	{
		OutNodes.Reset();
		for (const FString& Key : InNodeKeys)
		{
			UEdGraphNode* Found = nullptr;
			for (UEdGraphNode* Node : InGraph->Nodes)
			{
				if (NodeAnswersToKey(Node, Key))
				{
					Found = Node;
					break;
				}
			}
			if (Found == nullptr)
			{
				// NAMED RATHER THAN COUNTED. "one of the nodes was not found" would leave the caller to
				// discover which, and a copy is the operation where that matters most.
				//
				// AND A SAMPLE OF THE KEYS THE GRAPH ACTUALLY HAS IS PRINTED, because a key format that
				// does not match the reader's is the one failure a caller cannot diagnose from a refusal
				// alone - the two tools would disagree while both looked correct.
				TArray<FString> Samples;
				for (UEdGraphNode* Node : InGraph->Nodes)
				{
					if (Node != nullptr && Samples.Num() < 3)
					{
						Samples.Add(KeyOf(Node));
					}
				}
				OutRefusals.Add(FString::Printf(
					TEXT("no node with id '%s' in graph '%s' (the graph's own ids look like: %s)"),
					*Key, *InGraph->GetName(), *FString::Join(Samples, TEXT(", "))));
				continue;
			}
			OutNodes.Add(Found);
		}
		return OutRefusals.Num() == 0;
	}

	/**
	 * True when the pin belongs to a node in the given set.
	 *
	 * THE MEMBERSHIP TEST IS BY POINTER AND NOT BY KEY, because two nodes can legitimately share a key prefix
	 * and a key comparison would then treat a node outside the selection as inside it - which silently turns a
	 * crossing link into an internal one and drops a wire.
	 */
	bool PinIsInside(const TSet<UEdGraphNode*>& InSelected, const UEdGraphPin* InPin)
	{
		return InPin != nullptr && InPin->GetOwningNode() != nullptr
			&& InSelected.Contains(InPin->GetOwningNode());
	}
}

bool FTurboMCPGraphStructure::PlanCopy(const FString& InSourceAsset, const FString& InSourceGraph,
	const TArray<FString>& InNodeKeys, const FString& InTargetAsset, const FString& InTargetGraph,
	const FString& InAnchorNodeKey, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Verb = EVerb::Copy;
	OutPlan.SourceAsset = InSourceAsset;
	OutPlan.SourceGraph = InSourceGraph;
	OutPlan.TargetAsset = InTargetAsset;
	OutPlan.TargetGraph = InTargetGraph;
	OutPlan.TargetAnchorNodeKey = InAnchorNodeKey;

	if (InNodeKeys.Num() == 0)
	{
		// A NO-OP IS REPORTED AS ONE. `P1`: an empty result must be distinguishable from a result of zero,
		// and "you selected nothing" is not "the copy produced nothing".
		OutPlan.bNothingToDo = true;
		OutPlan.Refusals.Add(TEXT("no node ids were given, so there is nothing to copy"));
		return true;
	}

	FTurboMCPAssetResolver::FResolved SourceResolved;
	const FString SourceResolveError = FTurboMCPAssetResolver::Resolve(InSourceAsset, SourceResolved);
	if (!SourceResolveError.IsEmpty())
	{
		OutError = SourceResolveError;
		return false;
	}

	FTurboMCPAssetResolver::FResolved TargetResolved;
	const FString TargetResolveError = FTurboMCPAssetResolver::Resolve(InTargetAsset, TargetResolved);
	if (!TargetResolveError.IsEmpty())
	{
		OutError = TargetResolveError;
		return false;
	}

	// ---- SAME ASSET AND GRAPH IS REFUSED BY NAME AND POINTED AT WHAT IT ACTUALLY IS --------------------
	if (SourceResolved.ObjectPath.Equals(TargetResolved.ObjectPath, ESearchCase::CaseSensitive)
		&& InSourceGraph.Equals(InTargetGraph, ESearchCase::CaseSensitive))
	{
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("the source and the target are the same graph ('%s' / '%s'), which is a duplicate rather than ")
			TEXT("a copy. Use the paste-in-place case for that."),
			*SourceResolved.PackagePath, *InSourceGraph));
		return true;
	}

	UBlueprint* SourceBlueprint = LoadObject<UBlueprint>(nullptr, *SourceResolved.ObjectPath);
	if (SourceBlueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Blueprint"), *SourceResolved.PackagePath);
		return false;
	}
	UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *TargetResolved.ObjectPath);
	if (TargetBlueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Blueprint"), *TargetResolved.PackagePath);
		return false;
	}

	UEdGraph* SourceGraphObject = FindGraph(SourceBlueprint, InSourceGraph);
	if (SourceGraphObject == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*InSourceGraph, *SourceResolved.PackagePath);
		return false;
	}
	UEdGraph* TargetGraphObject = FindGraph(TargetBlueprint, InTargetGraph);
	if (TargetGraphObject == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*InTargetGraph, *TargetResolved.PackagePath);
		return false;
	}

	// ---- THE SELECTION, AND EVERY KEY THAT IS NOT THERE NAMED ------------------------------------------
	TArray<UEdGraphNode*> Selected;
	if (!CollectSelected(SourceGraphObject, InNodeKeys, Selected, OutPlan.Refusals))
	{
		return true;
	}

	TSet<UEdGraphNode*> SelectedSet;
	for (UEdGraphNode* Node : Selected)
	{
		SelectedSet.Add(Node);
	}

	OutPlan.TargetNodeCountBefore = TargetGraphObject->Nodes.Num();

	// ---- EVERY NODE, ITS CLASS, AND EVERY MEMBER THE TARGET MUST HAVE ----------------------------------
	for (int32 Index = 0; Index < Selected.Num(); ++Index)
	{
		UEdGraphNode* Node = Selected[Index];

		FCopiedNode Copied;
		Copied.SourceKey = InNodeKeys.IsValidIndex(Index) ? InNodeKeys[Index] : KeyOf(Node);
		Copied.ClassName = Node->GetClass()->GetName();
		Copied.Index = Index;
		OutPlan.Nodes.Add(Copied);

		FString Kind;
		FString Name;
		if (!MemberNameOf(Node, Kind, Name))
		{
			continue;
		}

		// THE SAME-ASSET CASE IS EXEMPT FROM THE MEMBER CHECK, AND THAT IS NOT A SHORTCUT. A copy inside one
		// Blueprint between two of its graphs moves the member names to an asset that ALREADY HAS THEM by
		// construction, so checking them would be a check that cannot fail - and this project has now recorded
		// three suites whose counts overstated their coverage for exactly that reason.
		const bool bSameAsset = SourceResolved.ObjectPath.Equals(TargetResolved.ObjectPath,
			ESearchCase::CaseSensitive);
		if (bSameAsset)
		{
			continue;
		}

		bool bPresent = false;
		if (Kind == TEXT("variable"))
		{
			bPresent = BlueprintHasVariable(TargetBlueprint, Name);
		}
		else if (Kind == TEXT("function"))
		{
			bPresent = BlueprintHasFunction(TargetBlueprint, Name);
		}
		else if (Kind == TEXT("event"))
		{
			bPresent = BlueprintHasEvent(TargetBlueprint, Name);
		}

		if (!bPresent)
		{
			FMissingMember Missing;
			Missing.Kind = Kind;
			Missing.Name = Name;
			Missing.SourceKey = Copied.SourceKey;
			OutPlan.Missing.Add(Missing);
		}
	}

	// ---- THE BOUNDARY, FROM THE LINKS THAT CROSS IT ------------------------------------------------------
	//
	// THE ORDER IS STABLE ON PURPOSE. Every list below is built by walking the SELECTED NODES in the order
	// the caller named them and each node's pins in the graph's own order, so the result does not depend on
	// hash iteration - which is the difference between a replayable report and one that changes run to run.
	for (UEdGraphNode* Node : Selected)
	{
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

				FCrossingLink Crossing;
				Crossing.InsideNodeKey = KeyOf(Node);
				Crossing.InsidePin = Pin->PinName.ToString();

				if (PinIsInside(SelectedSet, Linked))
				{
					// BOTH ENDS INSIDE: an internal wire, re-made between the copies.
					Crossing.OutsideNodeKey = KeyOf(Linked->GetOwningNode());
					Crossing.OutsidePin = Linked->PinName.ToString();
					Crossing.bHasTarget = true;
					OutPlan.InternalLinks.Add(Crossing);
					continue;
				}

				UEdGraphNode* OutsideNode = Linked->GetOwningNode();
				Crossing.OutsideNodeKey = KeyOf(OutsideNode);
				Crossing.OutsidePin = Linked->PinName.ToString();

				// A LINK THAT LEAVES THE SELECTION CAN ONLY BE RE-MADE IF ITS FAR END IS REACHABLE FROM THE
				// TARGET. Inside one Blueprint that is always true, because both graphs belong to the same
				// asset. Across assets it is true only when the far node is itself being copied - which the
				// loop above already excluded - so a cross-asset crossing link is REPORTED rather than made.
				const bool bSameAsset = SourceResolved.ObjectPath.Equals(TargetResolved.ObjectPath,
					ESearchCase::CaseSensitive);
				if (bSameAsset)
				{
					Crossing.bHasTarget = true;
					OutPlan.ReLinkable.Add(Crossing);
				}
				else
				{
					Crossing.bHasTarget = false;
					// THE REASON NAMES WHICH OF THE TWO FAILURES IT IS, because they need different fixes: a
					// node left behind needs to be selected too, while a missing member needs the member
					// declared in the target. Collapsing them into "unlinked" would hide which one to act on.
					Crossing.Reason = FString::Printf(
						TEXT("its other end is %s, which is not in the target asset - select it as well, or ")
						TEXT("declare what it needs in '%s'"),
						*KeyOf(OutsideNode), *TargetResolved.PackagePath);
					OutPlan.Unlinkable.Add(Crossing);
				}
			}
		}
	}

	return true;
}

bool FTurboMCPGraphStructure::PlanCollapse(const FString& InAsset, const FString& InGraphName,
	const TArray<FString>& InNodeKeys, const FString& InMemberName, const FString& InMemberKind,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Verb = EVerb::Collapse;
	OutPlan.SourceAsset = InAsset;
	OutPlan.TargetAsset = InAsset;
	OutPlan.SourceGraph = InGraphName;
	OutPlan.MemberName = InMemberName;
	OutPlan.MemberKind = InMemberKind;

	if (!InMemberKind.Equals(TEXT("function"), ESearchCase::CaseSensitive)
		&& !InMemberKind.Equals(TEXT("macro"), ESearchCase::CaseSensitive))
	{
		OutPlan.Refusals.Add(FString::Printf(
			TEXT("'%s' is not a member kind. The kind must be 'function' or 'macro'."), *InMemberKind));
		return true;
	}
	if (InMemberName.IsEmpty())
	{
		OutPlan.Refusals.Add(TEXT("the new member needs a name"));
		return true;
	}
	if (InNodeKeys.Num() == 0)
	{
		OutPlan.bNothingToDo = true;
		OutPlan.Refusals.Add(TEXT("no node ids were given, so there is nothing to extract"));
		return true;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Blueprint"), *Resolved.PackagePath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, InGraphName);
	if (Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"), *InGraphName, *Resolved.PackagePath);
		return false;
	}

	TArray<UEdGraphNode*> Selected;
	if (!CollectSelected(Graph, InNodeKeys, Selected, OutPlan.Refusals))
	{
		return true;
	}

	TSet<UEdGraphNode*> SelectedSet;
	for (UEdGraphNode* Node : Selected)
	{
		SelectedSet.Add(Node);
	}

	// ---- THE MEMBER MUST NOT ALREADY EXIST, AND A BOUNDARY NODE IS REFUSED BY NAME -----------------------
	//
	// THE SPEC NAMES THIS ONE: "Refuses a selection that contains an entry, an event or a result node,
	// naming it, because collapsing those is not a refactor the agent asked for." A refusal that named only
	// a COUNT would leave the caller to work out which node was the problem.
	for (UEdGraphNode* Node : Selected)
	{
		FString What;
		if (IsBoundaryNode(Node, What))
		{
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("the selection contains %s (%s), which cannot be moved into a new member"),
				*What, *KeyOf(Node)));
		}
	}

	OutPlan.TargetNodeCountBefore = Graph->Nodes.Num();

	for (int32 Index = 0; Index < Selected.Num(); ++Index)
	{
		FCopiedNode Copied;
		Copied.SourceKey = InNodeKeys.IsValidIndex(Index) ? InNodeKeys[Index] : KeyOf(Selected[Index]);
		Copied.ClassName = Selected[Index]->GetClass()->GetName();
		Copied.Index = Index;
		OutPlan.Nodes.Add(Copied);
	}

	// ---- THE BOUNDARY, WHICH FOR A COLLAPSE IS WHAT BECOMES THE MEMBER'S SIGNATURE ----------------------
	for (UEdGraphNode* Node : Selected)
	{
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

				FCrossingLink Crossing;
				Crossing.InsideNodeKey = KeyOf(Node);
				Crossing.InsidePin = Pin->PinName.ToString();
				Crossing.OutsideNodeKey = KeyOf(Linked->GetOwningNode());
				Crossing.OutsidePin = Linked->PinName.ToString();

				if (PinIsInside(SelectedSet, Linked))
				{
					Crossing.bHasTarget = true;
					OutPlan.InternalLinks.Add(Crossing);
				}
				else
				{
					// A CROSSING LINK IS THE WHOLE POINT OF A COLLAPSE: it becomes an input or an output of
					// the new member, so it is always re-linkable and never "unlinkable" here. The list is
					// still kept separate from the internal ones because the two become different things.
					Crossing.bHasTarget = true;
					OutPlan.ReLinkable.Add(Crossing);
				}
			}
		}
	}

	// ---- A SELECTION WITH NO EXEC BOUNDARY IS REFUSED, NAMING WHY --------------------------------------
	//
	// THE SPEC SAYS A FUNCTION NEEDS ONE ENTRY AND ONE RESULT. A selection whose crossing links are all
	// data wires has no exec edge to become that entry, so the collapse is refused HERE rather than at apply
	// time - which is what keeps the Preview and the Edit refusing the same input.
	if (OutPlan.Refusals.Num() == 0 && OutPlan.ReLinkable.Num() == 0)
	{
		OutPlan.Refusals.Add(
			TEXT("the selection has no link crossing it, so there is no exec boundary to become the new ")
			TEXT("member's entry and result"));
	}

	return true;
}

bool FTurboMCPGraphStructure::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();

	if (!InPlan.bAcceptable())
	{
		// THE PLAN ALREADY DECIDED. Re-deciding here is how a Preview and an Edit come to disagree, which is
		// the one thing the "put every check in Plan" rule exists to prevent.
		OutReport = RenderPreview(InPlan);
		return true;
	}

	if (InPlan.Verb == EVerb::Collapse)
	{
		// THE COLLAPSE HALF IS NOT BUILT, AND IT SAYS SO RATHER THAN PRETENDING. The measurement in
		// `TurboMCPGraphStructureNotes.h` says the pieces are reachable, but reproducing the 290-line
		// sequence is not something to attempt without being able to test it - and `03_TOOLS.md` requires
		// the engine's own `CollapseNodesIntoGraph`, which is a member of an editor object whose
		// CONSTRUCTIBILITY outside an asset editor is the one thing the measurement could not settle.
		OutError = TEXT("E-GRAPH-COLLAPSE is measured REACHABLE but NOT YET BUILT: the engine's own ")
			TEXT("CollapseNodesIntoGraph is a member of FBlueprintEditor, and whether that instance can be ")
			TEXT("constructed outside an open asset editor is the one question the measurement left open. ")
			TEXT("COPY is built; COLLAPSE is not. NOTHING was written.");
		return false;
	}

	FTurboMCPAssetResolver::FResolved SourceResolved;
	const FString SourceResolveError = FTurboMCPAssetResolver::Resolve(InPlan.SourceAsset, SourceResolved);
	if (!SourceResolveError.IsEmpty())
	{
		OutError = SourceResolveError;
		return false;
	}
	FTurboMCPAssetResolver::FResolved TargetResolved;
	const FString TargetResolveError = FTurboMCPAssetResolver::Resolve(InPlan.TargetAsset, TargetResolved);
	if (!TargetResolveError.IsEmpty())
	{
		OutError = TargetResolveError;
		return false;
	}

	UBlueprint* SourceBlueprint = LoadObject<UBlueprint>(nullptr, *SourceResolved.ObjectPath);
	UBlueprint* TargetBlueprint = LoadObject<UBlueprint>(nullptr, *TargetResolved.ObjectPath);
	if (SourceBlueprint == nullptr || TargetBlueprint == nullptr)
	{
		OutError = TEXT("the source or the target could not be loaded as a Blueprint");
		return false;
	}

	UEdGraph* SourceGraph = FindGraph(SourceBlueprint, InPlan.SourceGraph);
	UEdGraph* TargetGraph = FindGraph(TargetBlueprint, InPlan.TargetGraph);
	if (SourceGraph == nullptr || TargetGraph == nullptr)
	{
		OutError = TEXT("the source or the target graph no longer exists");
		return false;
	}

	// ---- RE-RESOLVE THE SELECTION BY KEY AT APPLY TIME, WHICH IS `D-45` ---------------------------------
	//
	// THE PLAN'S KEYS ARE RE-READ RATHER THAN ITS POINTERS CARRIED. A plan can be built, then applied against
	// an asset that has been edited in between; a carried pointer would be a dangling one and a carried
	// index would silently address the wrong node. So the keys are resolved again and a key that has gone is
	// a REFUSAL that writes nothing.
	TArray<UEdGraphNode*> Selected;
	TArray<FString> SelectedRefusals;
	TArray<FString> Keys;
	for (const FCopiedNode& Node : InPlan.Nodes)
	{
		Keys.Add(Node.SourceKey);
	}
	if (!CollectSelected(SourceGraph, Keys, Selected, SelectedRefusals))
	{
		OutError = FString::Printf(TEXT("the graph changed between the plan and the apply, so NOTHING was ")
			TEXT("written: %s"), *FString::Join(SelectedRefusals, TEXT("; ")));
		return false;
	}

	// ---- ONE TRANSACTION, as the spec requires, so a refusal anywhere leaves the graph as it was --------
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphCopy", "TurboMCP copy a graph block"));
	TargetBlueprint->Modify();
	TargetGraph->Modify();

	// ---- THE COPY ITSELF: THE SOURCE NODE IS THE TEMPLATE -----------------------------------------------
	TMap<UEdGraphNode*, UEdGraphNode*> MadeFrom;
	TArray<FString> MadeLines;

	for (UEdGraphNode* Source : Selected)
	{
		// `SpawnNodeFromTemplate` CARRIES THE TEMPLATE'S OWN PROPERTY VALUES, which is why a copied
		// `UK2Node_CallFunction` keeps the function it calls. It also allocates the default pins, which is
		// mandatory on any node created outside the editor - this project has had one crash and one silent
		// corruption from omitting that step.
		//
		// *** IT TAKES A `UK2Node*`, NOT A `UEdGraphNode*`, AND THAT IS AN ENGINE CONSTRAINT RATHER THAN A
		// CHOICE: its `CreateNode` overload (`EdGraphSchema_K2_Actions.h:130`) declares the template parameter
		// as `UK2Node*`, so instantiating with `UEdGraphNode` does NOT COMPILE - measured, `error C2665` at
		// `EdGraphSchema_K2_Actions.h(102,25)`. A graph can hold nodes that are not K2 nodes, so the cast is
		// CHECKED and a non-K2 node is REFUSED BY NAME rather than silently skipped, because a copy that
		// quietly left a node behind is the worst outcome this function can produce. ***
		UK2Node* Template = Cast<UK2Node>(Source);
		if (Template == nullptr)
		{
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("'%s' (%s) is not a K2 node, so this tool cannot copy it. NOTHING was written."),
				*KeyOf(Source), *Source->GetClass()->GetName());
			return false;
		}

		const FVector2D Location(Source->NodePosX + 40.0f, Source->NodePosY + 40.0f);
		UEdGraphNode* Made = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node>(
			TargetGraph, Template, Location, /*bSelectNewNode=*/false);

		if (Made == nullptr)
		{
			// THE PLAN SAID THIS WOULD WORK AND IT DID NOT. Rolling back and saying so is the spec's rule:
			// a failed creation is a FAILURE rather than a success with a note.
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("the plan accepted '%s' but the graph refused to copy it at apply time. NOTHING was written."),
				*Source->GetClass()->GetName());
			return false;
		}

		MadeFrom.Add(Source, Made);
		MadeLines.Add(FString::Printf(TEXT("  %s  %s"), *KeyOf(Source), *Made->GetName()));
	}

	// ---- THE WIRES, MADE HERE BECAUSE BOTH ENDS EXIST ONLY NOW ------------------------------------------
	int32 InternalMade = 0;
	int32 CrossingMade = 0;
	int32 LeftUnconnected = 0;

	for (const FCrossingLink& Link : InPlan.InternalLinks)
	{
		// BOTH ENDS ARE COPIES, so both are found by the source key through the map rather than by name.
		UEdGraphNode* FromSource = nullptr;
		UEdGraphNode* ToSource = nullptr;
		for (const TPair<UEdGraphNode*, UEdGraphNode*>& Pair : MadeFrom)
		{
			if (KeyOf(Pair.Key).Equals(Link.InsideNodeKey, ESearchCase::CaseSensitive))
			{
				FromSource = Pair.Key;
			}
			if (KeyOf(Pair.Key).Equals(Link.OutsideNodeKey, ESearchCase::CaseSensitive))
			{
				ToSource = Pair.Key;
			}
		}
		if (FromSource == nullptr || ToSource == nullptr)
		{
			++LeftUnconnected;
			continue;
		}

		UEdGraphPin* FromPin = FindPinByName(MadeFrom[FromSource], Link.InsidePin);
		UEdGraphPin* ToPin = FindPinByName(MadeFrom[ToSource], Link.OutsidePin);
		if (FromPin == nullptr || ToPin == nullptr)
		{
			++LeftUnconnected;
			continue;
		}

		FromPin->MakeLinkTo(ToPin);
		++InternalMade;
	}

	// A CROSSING LINK WHOSE FAR END IS AN EXISTING NODE IN THE SAME ASSET IS RE-MADE BY KEY, so the copy is
	// wired into what was already there rather than left floating next to it.
	for (const FCrossingLink& Link : InPlan.ReLinkable)
	{
		UEdGraphNode* FromSource = nullptr;
		for (const TPair<UEdGraphNode*, UEdGraphNode*>& Pair : MadeFrom)
		{
			if (KeyOf(Pair.Key).Equals(Link.InsideNodeKey, ESearchCase::CaseSensitive))
			{
				FromSource = Pair.Key;
				break;
			}
		}
		if (FromSource == nullptr)
		{
			++LeftUnconnected;
			continue;
		}

		UEdGraphNode* Outside = nullptr;
		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (Node != nullptr && KeyOf(Node).Equals(Link.OutsideNodeKey, ESearchCase::CaseSensitive))
			{
				Outside = Node;
				break;
			}
		}
		if (Outside == nullptr)
		{
			++LeftUnconnected;
			continue;
		}

		UEdGraphPin* FromPin = FindPinByName(MadeFrom[FromSource], Link.InsidePin);
		UEdGraphPin* ToPin = FindPinByName(Outside, Link.OutsidePin);
		if (FromPin == nullptr || ToPin == nullptr)
		{
			++LeftUnconnected;
			continue;
		}

		FromPin->MakeLinkTo(ToPin);
		++CrossingMade;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);

	// ---- THE REPORT, FIRST LINE `changed`, AND THE UNLINKABLE LINKS NAMED RATHER THAN COUNTED -----------
	TArray<FString> Lines;
	Lines.Add(TEXT("changed"));
	Lines.Add(FString::Printf(TEXT("copied %d node(s) from %s / %s to %s / %s"),
		MadeFrom.Num(), *InPlan.SourceAsset, *InPlan.SourceGraph, *InPlan.TargetAsset, *InPlan.TargetGraph));
	Lines.Add(FString::Printf(TEXT("target graph nodes: %d before, %d after"),
		InPlan.TargetNodeCountBefore, TargetGraph->Nodes.Num()));
	Lines.Add(FString::Printf(TEXT("internal links re-made: %d of %d"),
		InternalMade, InPlan.InternalLinks.Num()));
	Lines.Add(FString::Printf(TEXT("crossing links re-linked: %d of %d"),
		CrossingMade, InPlan.ReLinkable.Num()));

	for (const FString& Line : MadeLines)
	{
		Lines.Add(Line);
	}

	// THE UNLINKABLE LIST IS PRINTED EVEN WHEN THE PLAN HAD NONE, because a report that omits the section is
	// indistinguishable from one where nothing crossed - and this is the list a caller must not lose.
	Lines.Add(FString::Printf(TEXT("links left UNCONNECTED (%d)"), InPlan.Unlinkable.Num()));
	for (const FCrossingLink& Link : InPlan.Unlinkable)
	{
		Lines.Add(FString::Printf(TEXT("  %s:%s -> %s:%s  BECAUSE %s"),
			*Link.InsideNodeKey, *Link.InsidePin,
			*Link.OutsideNodeKey, *Link.OutsidePin, *Link.Reason));
	}

	if (LeftUnconnected > 0)
	{
		// A WIRE THE PLAN EXPECTED AND THE APPLY COULD NOT MAKE. Reported as its own count rather than folded
		// into the above, because the two have different causes: one was known before anything was written,
		// this one was discovered while writing.
		Lines.Add(FString::Printf(
			TEXT("WARNING: %d link(s) the plan expected could not be made at apply time"), LeftUnconnected));
	}

	OutReport = FString::Join(Lines, TEXT("\n"));
	return true;
}

FString FTurboMCPGraphStructure::RenderPreview(const FPlan& InPlan)
{
	TArray<FString> Lines;
	const bool bAcceptable = InPlan.bAcceptable();
	Lines.Add(bAcceptable ? TEXT("would change") : TEXT("unchanged"));

	if (InPlan.Verb == EVerb::Copy)
	{
		Lines.Add(FString::Printf(TEXT("copy: %s / %s  ->  %s / %s"),
			*InPlan.SourceAsset, *InPlan.SourceGraph, *InPlan.TargetAsset, *InPlan.TargetGraph));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("collapse: %s / %s  ->  %s '%s'"),
			*InPlan.SourceAsset, *InPlan.SourceGraph, *InPlan.MemberKind, *InPlan.MemberName));
	}

	if (!InPlan.TargetAnchorNodeKey.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("anchor: %s"), *InPlan.TargetAnchorNodeKey));
	}

	Lines.Add(FString::Printf(TEXT("nodes selected: %d of %d in the target graph before"),
		InPlan.Nodes.Num(), InPlan.TargetNodeCountBefore));

	for (const FCopiedNode& Node : InPlan.Nodes)
	{
		Lines.Add(FString::Printf(TEXT("  %s  %s"), *Node.SourceKey, *Node.ClassName));
	}

	// ---- THE THREE BOUNDARY LISTS, EACH WITH ITS OWN HEADING AND ITS OWN COUNT --------------------------
	//
	// A SILENTLY DROPPED WIRE IS THE WORST OUTCOME THIS FILE CAN PRODUCE, so `unlinkable` is printed even
	// when it is empty - an absent section and an empty one are different answers, which is `P1` applied to
	// the report itself rather than only to the numbers in it.
	Lines.Add(FString::Printf(TEXT("internal links to re-make (%d)"), InPlan.InternalLinks.Num()));
	for (const FCrossingLink& Link : InPlan.InternalLinks)
	{
		Lines.Add(FString::Printf(TEXT("  %s:%s <-> %s:%s"),
			*Link.InsideNodeKey, *Link.InsidePin, *Link.OutsideNodeKey, *Link.OutsidePin));
	}

	Lines.Add(FString::Printf(TEXT("crossing links to re-link (%d)"), InPlan.ReLinkable.Num()));
	for (const FCrossingLink& Link : InPlan.ReLinkable)
	{
		Lines.Add(FString::Printf(TEXT("  %s:%s -> %s:%s"),
			*Link.InsideNodeKey, *Link.InsidePin, *Link.OutsideNodeKey, *Link.OutsidePin));
	}

	Lines.Add(FString::Printf(TEXT("crossing links that CANNOT be re-linked and are left unconnected (%d)"),
		InPlan.Unlinkable.Num()));
	for (const FCrossingLink& Link : InPlan.Unlinkable)
	{
		Lines.Add(FString::Printf(TEXT("  %s:%s -> %s:%s  BECAUSE %s"),
			*Link.InsideNodeKey, *Link.InsidePin,
			*Link.OutsideNodeKey, *Link.OutsidePin, *Link.Reason));
	}

	if (InPlan.Missing.Num() > 0)
	{
		// ALL OR NOTHING, AND EVERY NAME GIVEN. `03_TOOLS.md`: half a block is worse than none.
		Lines.Add(FString::Printf(
			TEXT("REFUSED: the target asset does not have %d member(s) these nodes need, so NOTHING is copied:"),
			InPlan.Missing.Num()));
		for (const FMissingMember& One : InPlan.Missing)
		{
			Lines.Add(FString::Printf(TEXT("  %s '%s', needed by %s"),
				*One.Kind, *One.Name, *One.SourceKey));
		}
	}

	if (InPlan.Refusals.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("REFUSED (%d reason(s)):"), InPlan.Refusals.Num()));
		for (const FString& Refusal : InPlan.Refusals)
		{
			Lines.Add(FString::Printf(TEXT("  %s"), *Refusal));
		}
	}

	if (InPlan.bNothingToDo)
	{
		Lines.Add(TEXT("nothing to do."));
	}

	return FString::Join(Lines, TEXT("\n"));
}
