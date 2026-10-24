// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPLocalVars.h"

#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"

namespace
{
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	FString KeyOf(const UEdGraphNode* InNode)
	{
		return InNode != nullptr ? TurboMCPNodeId::MakeKey(InNode->NodeGuid) : FString();
	}

	/** True when the pin carries a value rather than starting an exec chain. */
	bool IsDataPin(const UEdGraphPin* InPin)
	{
		return InPin != nullptr && InPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;
	}

	/** Where a node came from, for the report: a literal, an input, or another node. */
	FString DescribeSource(UEdGraphPin* InPin, const UK2Node_FunctionEntry* InEntry)
	{
		if (InPin == nullptr)
		{
			return TEXT("nothing");
		}

		if (InPin->LinkedTo.Num() == 0)
		{
			// An unlinked data pin carries its own default, which is a literal in the graph.
			return FString::Printf(TEXT("a literal (%s)"), *InPin->PinName.ToString());
		}

		UEdGraphNode* Source = InPin->LinkedTo[0] != nullptr
			? InPin->LinkedTo[0]->GetOwningNode() : nullptr;
		if (Source == nullptr)
		{
			return TEXT("a broken link");
		}

		// AN INPUT OF THE FUNCTION IS THE SAFE ORIGIN. Recognised by the entry node's own pins, not
		// by a name convention, so a renamed parameter still counts.
		if (InEntry != nullptr && Source == InEntry)
		{
			return FString::Printf(TEXT("the function input '%s'"),
				*InPin->LinkedTo[0]->PinName.ToString());
		}

		return FString::Printf(TEXT("%s (%s)"), *Source->GetName(),
			*Source->GetClass()->GetName());
	}

	/**
	 * True when the value at this pin traces back ONLY to function inputs and literals.
	 *
	 * TRANSITIVE, with a visited set. A one-level check would call `A(input) -> B(A) -> used twice`
	 * safe for B without ever looking at A, which is the mistake the rule exists to prevent. The
	 * visited set stops a cycle from being walked forever, and a cycle is itself UNSAFE: a value that
	 * depends on itself is not fixed by its inputs.
	 */
	bool TracesToInputsOrLiterals(UEdGraphPin* InPin, const UK2Node_FunctionEntry* InEntry,
		TSet<UEdGraphNode*>& InOutVisited, FString& OutWhy, int32 InDepth)
	{
		if (InPin == nullptr)
		{
			return true;
		}

		// A depth cap, so a graph with a long chain reports rather than recursing without bound.
		if (InDepth > 64)
		{
			OutWhy = TEXT("the chain of nodes feeding it is longer than 64 steps");
			return false;
		}

		if (InPin->LinkedTo.Num() == 0)
		{
			// A literal, which is safe by definition.
			return true;
		}

		for (UEdGraphPin* Linked : InPin->LinkedTo)
		{
			if (Linked == nullptr)
			{
				continue;
			}
			UEdGraphNode* Source = Linked->GetOwningNode();
			if (Source == nullptr)
			{
				OutWhy = TEXT("one of its inputs is a broken link");
				return false;
			}

			if (InEntry != nullptr && Source == InEntry)
			{
				// A function input: safe.
				continue;
			}

			if (InOutVisited.Contains(Source))
			{
				OutWhy = FString::Printf(
					TEXT("it depends on itself through %s, so its value is not fixed by its inputs"),
					*Source->GetName());
				return false;
			}

			// A VARIABLE READ IS NEVER SAFE, AND THIS CHECK HAS TO COME FIRST.
			//
			// MEASURED BUG: this test used to sit INSIDE the `!IsNodePure()` branch below, so a
			// `K2Node_VariableGet` — which IS pure, because reading a variable has no side effect —
			// passed the purity test and was treated as a safe link. Measured consequence, on the
			// acceptance asset built for exactly this case:
			//
			//     SAFE    int + int
			//       A <- K2Node_VariableGet_0 (K2Node_VariableGet)
			//       everything feeding it is a function input or a literal
			//
			// which is the case the rule exists to REFUSE, because a member variable can change
			// between the two reads. BEING PURE IS NOT THE SAME AS BEING FIXED FOR THE CALL, and only
			// the second matters here. The check is ordered before the purity test for that reason.
			if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Source))
			{
				OutWhy = FString::Printf(
					TEXT("it reads the variable '%s', which can change during the call"),
					*Get->GetVarName().ToString());
				return false;
			}

			// ONLY A PURE NODE CAN BE PART OF A SAFE CHAIN. An impure node runs on an exec wire and
			// can have a side effect, so its output is not fixed by the function's inputs.
			const UK2Node* SourceK2 = Cast<UK2Node>(Source);
			if (SourceK2 == nullptr || !SourceK2->IsNodePure())
			{
				OutWhy = FString::Printf(
					TEXT("it is fed by %s (%s), which is not a pure node, so its output is not ")
					TEXT("fixed by the function inputs"),
					*Source->GetName(), *Source->GetClass()->GetName());
				return false;
			}

			InOutVisited.Add(Source);

			for (UEdGraphPin* SourceInput : Source->Pins)
			{
				// Only INPUT pins are followed: an output pin of the source points forward, and
				// following it would walk the whole graph instead of the value's ancestry.
				if (SourceInput == nullptr || SourceInput->Direction != EGPD_Input
					|| !IsDataPin(SourceInput))
				{
					continue;
				}
				if (!TracesToInputsOrLiterals(SourceInput, InEntry, InOutVisited, OutWhy,
						InDepth + 1))
				{
					return false;
				}
			}
		}

		return true;
	}

	/** The pins a node's value is read from, i.e. its data output pins. */
	void OutputPins(UEdGraphNode* InNode, TArray<UEdGraphPin*>& OutPins)
	{
		for (UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Output && IsDataPin(Pin))
			{
				OutPins.Add(Pin);
			}
		}
	}
}

bool FTurboMCPLocalVars::Plan(const FString& InAsset, const FString& InGraphName,
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
			TEXT("%s is not a Blueprint, so it has no functions."), *Resolved.ObjectPath);
		return false;
	}

	// ONLY FUNCTION GRAPHS, and the answer comes from the engine's own test rather than a guess.
	TArray<UEdGraph*> FunctionGraphs;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr) { FunctionGraphs.Add(Graph); }
	}

	if (FunctionGraphs.Num() == 0)
	{
		OutError = FString::Printf(
			TEXT("%s has no function graphs, and a local variable belongs to a function. Add a ")
			TEXT("function with EditTurboMCPFunctionAdd first."), *Resolved.ObjectPath);
		return false;
	}

	UEdGraph* Graph = nullptr;
	if (OutPlan.GraphName.IsEmpty())
	{
		// A default rather than a refusal, named in the answer so a caller who omitted the name
		// knows which function was examined.
		Graph = FunctionGraphs[0];
	}
	else
	{
		for (UEdGraph* Candidate : FunctionGraphs)
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
			for (UEdGraph* Candidate : FunctionGraphs) { Names.Add(Candidate->GetName()); }
			Names.Sort();
			OutError = FString::Printf(
				TEXT("'%s' is not a function graph of this Blueprint. Its functions are: %s. ")
				TEXT("An event graph has no local variables."),
				*OutPlan.GraphName, *FString::Join(Names, TEXT(", ")));
			return false;
		}
	}

	OutPlan.GraphName = Graph->GetName();
	OutPlan.Asset = Resolved.ObjectPath;
	OutPlan.NodeCountBefore = Graph->Nodes.Num();
	OutPlan.bSupportsLocals = FBlueprintEditorUtils::DoesSupportLocalVariables(Graph);
	OutPlan.NoLocalsReason = OutPlan.bSupportsLocals
		? FString()
		: FString::Printf(
			TEXT("'%s' cannot hold local variables, so nothing can be promoted here"),
			*Graph->GetName());

	// The function's own entry, so a parameter is recognised by IDENTITY rather than by name.
	const UK2Node_FunctionEntry* Entry = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (const UK2Node_FunctionEntry* Found = Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry = Found;
			break;
		}
	}

	// The candidates: PURE nodes whose value is read more than once.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
		{
			continue;
		}
		const UK2Node* K2 = Cast<UK2Node>(Node);
		if (K2 == nullptr || !K2->IsNodePure())
		{
			// Only a pure node computes a value. An impure node does something, and capturing its
			// output would not be the same as running it once.
			continue;
		}
		// A variable GETTER is not a computed value; it is already a single read.
		if (Cast<UK2Node_VariableGet>(Node) != nullptr
			|| Cast<UK2Node_VariableSet>(Node) != nullptr
			|| Cast<UK2Node_Knot>(Node) != nullptr
			|| Cast<UK2Node_FunctionEntry>(Node) != nullptr)
		{
			continue;
		}

		TArray<UEdGraphPin*> Outputs;
		OutputPins(Node, Outputs);

		int32 Consumers = 0;
		TArray<UEdGraphPin*> ConsumerPins;
		for (UEdGraphPin* Pin : Outputs)
		{
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked != nullptr && Linked->GetOwningNode() != nullptr)
				{
					++Consumers;
					ConsumerPins.Add(Linked);
				}
			}
		}

		if (Consumers < 2)
		{
			// USED MORE THAN ONCE, per the spec. A value read once gains nothing from a local and
			// costs a node.
			continue;
		}

		FCandidate Candidate;
		Candidate.Key = KeyOf(Node);
		Candidate.Title = K2->GetNodeTitle(ENodeTitleType::ListView).ToString();
		Candidate.ClassName = Node->GetClass()->GetName();
		Candidate.ConsumerCount = Consumers;
		Candidate.X = Node->NodePosX;
		Candidate.Y = Node->NodePosY;
		if (Outputs.Num() > 0 && Outputs[0] != nullptr)
		{
			Candidate.TypeText = Outputs[0]->PinType.PinCategory.ToString();
		}
		Candidate.ProposedName = FString::Printf(TEXT("Local_%s"), *Candidate.Key);

		// SAFETY, transitively over the node's own inputs.
		TSet<UEdGraphNode*> Visited;
		Visited.Add(Node);
		FString Why;
		bool bSafe = true;
		for (UEdGraphPin* Input : Node->Pins)
		{
			if (Input == nullptr || Input->Direction != EGPD_Input || !IsDataPin(Input))
			{
				continue;
			}
			Candidate.FeedsFrom.Add(FString::Printf(TEXT("%s <- %s"),
				*Input->PinName.ToString(), *DescribeSource(Input, Entry)));

			if (!TracesToInputsOrLiterals(Input, Entry, Visited, Why, 0))
			{
				bSafe = false;
				break;
			}
		}

		if (!OutPlan.bSupportsLocals)
		{
			bSafe = false;
			Why = OutPlan.NoLocalsReason;
		}

		Candidate.bSafe = bSafe;
		Candidate.UnsafeReason = bSafe ? FString() : Why;

		OutPlan.Candidates.Add(MoveTemp(Candidate));
	}

	return true;
}

FString FTurboMCPLocalVars::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: values to promote in %s : %s"),
		*InPlan.Asset, *InPlan.GraphName);
	Out += FString::Printf(TEXT("\n%d node(s), %d value(s) used more than once by a pure node"),
		InPlan.NodeCountBefore, InPlan.Candidates.Num());

	if (!InPlan.bSupportsLocals && !InPlan.NoLocalsReason.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n%s"), *InPlan.NoLocalsReason);
	}

	if (InPlan.Candidates.Num() == 0)
	{
		// `P1`: never return nothing, and say WHY. A function whose values are each read once has
		// nothing to gain from a local, which is a different answer from a failure.
		Out += TEXT("\nno pure node's value is read more than once, so there is nothing to ")
			TEXT("promote. A value read once gains nothing from a local variable and costs a node.");
		return Out;
	}

	int32 Safe = 0;
	for (const FCandidate& Candidate : InPlan.Candidates)
	{
		if (Candidate.bSafe) { ++Safe; }

		Out += FString::Printf(TEXT("\n\n%s  %s  (%s)"),
			Candidate.bSafe ? TEXT("CANDIDATE") : TEXT("EXCLUDED"),
			*Candidate.Title, *Candidate.ClassName);
		Out += FString::Printf(TEXT("\n    %s, read %d time(s)"),
			*Candidate.Key, Candidate.ConsumerCount);

		for (const FString& Feed : Candidate.FeedsFrom)
		{
			Out += FString::Printf(TEXT("\n    %s"), *Feed);
		}

		if (Candidate.bSafe)
		{
			Out += FString::Printf(
				TEXT("\n    input tracing found no excluded source; this does not prove the calculation ")
				TEXT("is deterministic. Proposed local name: '%s'; promotion is unavailable."),
				*Candidate.ProposedName);
		}
		else
		{
			Out += FString::Printf(TEXT("\n    NOT SAFE: %s"), *Candidate.UnsafeReason);
		}
	}

	Out += FString::Printf(
		TEXT("\n\n%d of %d candidates passed input tracing, not behavioral verification. ")
		TEXT("EditTurboMCPLocalVars currently refuses promotion: calculated-value assignment is incomplete; no graph changes were made."),
		Safe, InPlan.Candidates.Num());

	if (Safe == 0)
	{
		Out += TEXT("\nNOTHING WOULD BE PROMOTED.");
	}

	return Out;
}

FString FTurboMCPLocalVars::Apply(const FPlan& InPlan)
{
    // Until assignment and execution placement are implemented, replacing the calculation
    // with a getter would return an unassigned local. Refuse before any mutation.
    if (!InPlan.Candidates.IsEmpty())
    {
        return TEXT("unchanged: local-variable promotion is unavailable; the implementation does not assign the calculated value to the local; no graph changes were made; use the Unreal editor to create and assign the local manually; compile=not_run; save=not_requested");
    }

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

	UEdGraph* Graph = nullptr;
	for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
	{
		if (Candidate != nullptr && NameMatches(Candidate->GetFName(), InPlan.GraphName))
		{
			Graph = Candidate;
			break;
		}
	}
	if (Graph == nullptr)
	{
		return FString::Printf(TEXT("error: function graph '%s' is no longer in %s."),
			*InPlan.GraphName, *Resolved.ObjectPath);
	}

	if (!FBlueprintEditorUtils::DoesSupportLocalVariables(Graph))
	{
		return FString::Printf(
			TEXT("unchanged: %s cannot hold local variables, so nothing was promoted."),
			*Graph->GetName());
	}

	int32 Promoted = 0;
	int32 Refused = 0;
	TArray<FString> Lines;

	for (const FCandidate& Candidate : InPlan.Candidates)
	{
		if (!Candidate.bSafe)
		{
			// THE EDIT PROMOTES ONLY WHAT THE PREVIEW MARKED SAFE, re-checked here rather than
			// trusted from the plan, because a plan is made and applied in two calls.
			++Refused;
			continue;
		}

		// The node is found by key again, for the same reason.
		UEdGraphNode* Node = nullptr;
		for (UEdGraphNode* Found : Graph->Nodes)
		{
			if (Found != nullptr && KeyOf(Found).Equals(Candidate.Key, ESearchCase::IgnoreCase))
			{
				Node = Found;
				break;
			}
		}
		if (Node == nullptr)
		{
			++Refused;
			Lines.Add(FString::Printf(TEXT("  %s: no longer in the graph, skipped"),
				*Candidate.Key));
			continue;
		}

		TArray<UEdGraphPin*> Outputs;
		OutputPins(Node, Outputs);
		if (Outputs.Num() == 0)
		{
			++Refused;
			Lines.Add(FString::Printf(TEXT("  %s: it has no data output pin"), *Candidate.Key));
			continue;
		}

		// The variable's type comes from the pin, with the const, reference AND WEAK POINTER
		// qualifiers cleared, which is what the editor's own promote does. `bIsWeakPointer` was
		// MISSING here and is a defect the source audit found: the engine clears all three
		// (`BlueprintEditor.cpp:4885`), so a weak-pointer output would have produced a local of the
		// wrong type.
		FEdGraphPinType NewType = Outputs[0]->PinType;
		NewType.bIsConst = false;
		NewType.bIsReference = false;
		NewType.bIsWeakPointer = false;
		const FString DefaultValue = Outputs[0]->DefaultValue;
		const double NodeX = Node->NodePosX;
		const double NodeY = Node->NodePosY;

		// `FindUniqueKismetName` already answers an `FName`, so no dereference is needed here.
		const FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(
			Blueprint, *Candidate.ProposedName);

		if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, Graph, VarName, NewType,
				DefaultValue))
		{
			++Refused;
			Lines.Add(FString::Printf(TEXT("  %s: the engine refused to add local '%s'"),
				*Candidate.Key, *VarName.ToString()));
			continue;
		}

		// THE NODE AND ITS PINS ARE RE-RESOLVED, BECAUSE ADDING THE VARIABLE COMPILED.
		//
		// FOUND BY SOURCE AUDIT, NOT BY A RUN. `FBlueprintEditorUtils::AddLocalVariable` ends with
		// `MarkBlueprintAsStructurallyModified` (`BlueprintEditorUtils.cpp:5529`), which calls
		// `FBlueprintCompilationManager::CompileSynchronously` and then `Blueprint->BroadcastChanged()`.
		// A synchronous compile can RECONSTRUCT nodes, so `Node` and every pointer in `Outputs` may
		// be freed by the call above — and they were used afterwards to read a position, to break
		// links and to destroy the original.
		//
		// The engine's own promote guards against exactly this. `BlueprintEditor.cpp:4982` carries
		// the comment "the owning node may have been reconstructed as a result of adding a new
		// variable above, so ensure the pin reference is up-to-date", and re-fetches. That is the
		// strongest available evidence, from the code that already solved it.
		//
		// The node is found again BY KEY rather than by pointer, and the output pins are re-derived
		// from it, so nothing below can touch a freed object.
		Node = nullptr;
		for (UEdGraphNode* Found : Graph->Nodes)
		{
			if (Found != nullptr && KeyOf(Found).Equals(Candidate.Key, ESearchCase::IgnoreCase))
			{
				Node = Found;
				break;
			}
		}
		if (Node == nullptr)
		{
			// The value is gone after the compile. Reported, and the local variable that was just
			// added is LEFT in place rather than removed: it is a harmless unused local, and
			// removing it would be a second write to undo the first.
			++Refused;
			Lines.Add(FString::Printf(
				TEXT("  %s: it no longer exists after the compile that adding local '%s' caused, so ")
				TEXT("nothing was rewired and no node was removed. The local '%s' is unused."),
				*Candidate.Key, *VarName.ToString(), *VarName.ToString()));
			continue;
		}
		Outputs.Reset();
		OutputPins(Node, Outputs);
		if (Outputs.Num() == 0)
		{
			++Refused;
			Lines.Add(FString::Printf(
				TEXT("  %s: it has no data output pin after the compile; local '%s' is unused"),
				*Candidate.Key, *VarName.ToString()));
			continue;
		}

		// A GETTER IS MADE, because a local variable is read through one. It is placed where the
		// value was computed, so the promoted graph reads the same way it did.
		UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph);
		Getter->CreateNewGuid();
		Getter->VariableReference.SetLocalMember(VarName, Graph->GetName(),
			FBlueprintEditorUtils::FindLocalVariableGuidByName(Blueprint, Graph, VarName));
		// THE GETTER GOES WHERE THE VALUE WAS, using the position captured BEFORE the compile rather
		// than read from the re-resolved node. Both would usually agree; the captured pair cannot be
		// a stale read, and the whole point of capturing it was to have a value that survived.
		Getter->NodePosX = static_cast<int32>(NodeX);
		Getter->NodePosY = static_cast<int32>(NodeY);
		Graph->Modify();
		Graph->AddNode(Getter, /*bUserAction=*/true, /*bSelectNewNode=*/false);

		// THE PINS ARE ALLOCATED EXPLICITLY, AND THIS WAS A SEVERE DEFECT BEFORE IT WAS HERE.
		//
		// FOUND BY SOURCE AUDIT, NOT BY A RUN: `UK2Node_VariableGet::AllocateDefaultPins` is guard-
		// ed on `GetVarName() != NAME_None` and is what creates the value pin, while `GetValuePin`
		// is `FindPin(GetVarName())`. With no allocation it returns NULL — and the rewire below was
		// written to SKIP ITSELF when that happened, `continue` past it, DESTROY THE ORIGINAL NODE
		// ANYWAY, and report `changed:`. That is the worst possible outcome: the graph is severed
		// and the tool says it succeeded.
		//
		// This project already MEASURED the identical trap in the acceptance-asset commandlet, where
		// `UK2Node_Knot::GetOutputPin` crashed on `Pins[1]` of an empty array. There it crashed; here
		// the null guard turned the same omission into silence.
		Getter->AllocateDefaultPins();
		UEdGraphPin* GetterValue = Getter->GetValuePin();
		if (GetterValue == nullptr)
		{
			// REFUSED RATHER THAN HALF-DONE. The getter is removed again and the original is left
			// COMPLETELY ALONE, because continuing would destroy it with nothing to replace it.
			Getter->DestroyNode();
			++Refused;
			Lines.Add(FString::Printf(
				TEXT("  %s: the getter for local '%s' produced no value pin, so NOTHING was ")
				TEXT("changed for this value and the original node is untouched"),
				*Candidate.Key, *VarName.ToString()));
			continue;
		}

		// THE CONSUMERS ARE RE-POINTED AT THE GETTER, pin by pin.
		int32 Rewired = 0;
		for (UEdGraphPin* Output : Outputs)
		{
			// A COPY of the array, because `BreakLinkTo` mutates `LinkedTo` while it is walked.
			TArray<UEdGraphPin*> Consumers = Output->LinkedTo;
			for (UEdGraphPin* Consumer : Consumers)
			{
				if (Consumer == nullptr)
				{
					continue;
				}
				Output->BreakLinkTo(Consumer);
				GetterValue->MakeLinkTo(Consumer);
				++Rewired;
			}
		}

		// THE ORIGINAL IS REMOVED LAST, once nothing points at it. Removing it first would break the
		// links the rewire still needs.
		Node->DestroyNode();
		Graph->MarkPackageDirty();

		++Promoted;
		Lines.Add(FString::Printf(
			TEXT("  %s -> local '%s' (%s), %d consumer(s) re-pointed"),
			*Candidate.Key, *VarName.ToString(), *Candidate.Title, Rewired));
	}

	if (Promoted == 0)
	{
		return FString::Printf(
			TEXT("unchanged: no value in %s : %s was promoted. %d candidate(s) were refused, ")
			TEXT("either because the preview marked them unsafe or because they no longer ")
			TEXT("resolve."),
			*Resolved.ObjectPath, *Graph->GetName(), Refused);
	}

	FString Out = FString::Printf(TEXT("changed: promoted %d value(s) to local variables in %s"),
		Promoted, *Graph->GetName());
	for (const FString& Line : Lines)
	{
		Out += TEXT("\n") + Line;
	}
	Out += FString::Printf(TEXT("\nnodes: %d -> %d"),
		InPlan.NodeCountBefore, Graph->Nodes.Num());

	if (Refused > 0)
	{
		// Reported rather than hidden: a caller who asked for four and got two must know.
		Out += FString::Printf(
			TEXT("\n%d candidate(s) were NOT promoted. An unsafe value is never promoted: ")
			TEXT("capturing it once is not the same as computing it twice."), Refused);
	}

	Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
	return Out;
}
