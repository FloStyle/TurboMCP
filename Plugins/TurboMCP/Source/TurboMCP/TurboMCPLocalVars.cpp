// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPLocalVars.h"

#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
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

    // Purity alone permits random values, time and mutable reads. Only these stock
    // native arithmetic functions are currently reviewed for capture eligibility.
    bool IsReviewedCalculation(const UEdGraphNode* Node)
    {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
        if (!Function || Function->GetOuterUClass() != UKismetMathLibrary::StaticClass()
            || !Function->HasAllFunctionFlags(FUNC_Native | FUNC_Static | FUNC_BlueprintPure)) return false;
        const FName Name = Function->GetFName();
        static const TSet<FName> Reviewed{
            TEXT("Add_IntInt"), TEXT("Subtract_IntInt"), TEXT("Multiply_IntInt"),
            TEXT("Add_Int64Int64"), TEXT("Subtract_Int64Int64"), TEXT("Multiply_Int64Int64"),
            TEXT("Add_DoubleDouble"), TEXT("Subtract_DoubleDouble"), TEXT("Multiply_DoubleDouble")
        };
        return Reviewed.Contains(Name);
    }

	/** Trace input ancestry. The visited set contains only the active recursion path.
	 * Passing this check does not prove that pure functions are deterministic.
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
				OutWhy = TEXT("one of its inputs contains a missing pin link");
				return false;
			}
			UEdGraphNode* Source = Linked->GetOwningNode();
			if (Source == nullptr)
			{
				OutWhy = TEXT("one of its inputs is a broken link");
				return false;
			}

			if (InEntry != nullptr && Source == InEntry)
			{
				// Reference parameters may be changed by other execution paths.
                if (Linked->PinType.bIsReference)
                {
                    OutWhy = TEXT("it depends on a reference parameter that can change during the call");
                    return false;
                }
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
// A variable getter is pure, but its value can change during execution.
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

            if (!Cast<UK2Node_Knot>(Source) && !IsReviewedCalculation(Source))
            {
                OutWhy = TEXT("an upstream pure calculation has not been reviewed as deterministic");
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
					InOutVisited.Remove(Source);
					return false;
				}
			}
			InOutVisited.Remove(Source);
		}

		return true;
	}

    bool EntryInputsAreStable(UEdGraph* Graph, const UK2Node_FunctionEntry* Entry)
    {
        if (!Entry) return false;
        TSet<FName> Parameters;
        TArray<UEdGraphPin*> Pending;
        for (UEdGraphPin* Pin : Entry->Pins)
            if (Pin && Pin->Direction == EGPD_Output && IsDataPin(Pin))
            {
                Parameters.Add(Pin->PinName);
                Pending.Add(Pin);
            }
        // By-value parameters can still be assigned inside a function or passed to
        // a reference input. Reject both: an entry-time capture would observe an older value.
        for (UEdGraphNode* Node : Graph->Nodes)
            if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
                if (Parameters.Contains(Set->GetVarName())) return false;
        TSet<UEdGraphPin*> Visited;
        while (!Pending.IsEmpty())
        {
            UEdGraphPin* Output = Pending.Pop(EAllowShrinking::No);
            if (Visited.Contains(Output)) continue;
            Visited.Add(Output);
            for (UEdGraphPin* Consumer : Output->LinkedTo)
            {
                if (!Consumer || Consumer->PinType.bIsReference) return false;
                if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Consumer->GetOwningNode()))
                    for (UEdGraphPin* Pin : Knot->Pins)
                        if (Pin && Pin->Direction == EGPD_Output) Pending.Add(Pin);
            }
        }
        return true;
    }

    bool StockLoopValue(const UEdGraphNode* Node, const UEdGraphPin* Output)
    {
        const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node);
        const UEdGraph* Definition = Macro ? Macro->GetMacroGraph() : nullptr;
        if (!Definition || Definition->GetOutermost()->GetName() != TEXT("/Engine/EditorBlueprintResources/StandardMacros")) return false;
        const FString Name = Definition->GetName();
        return Output && ((Output->PinName == TEXT("Index") && (Name == TEXT("ForLoop") || Name == TEXT("ForLoopWithBreak")))
            || (Output->PinName == TEXT("Array Index")
                && (Name == TEXT("ForEachLoop") || Name == TEXT("ForEachLoopWithBreak"))));
    }

    // Conservative execution proof: removing this output must make every read
    // unreachable from entry, and Completed must never reach a captured read.
    bool BodyCaptureIsSafe(UEdGraph* Graph, const UK2Node_FunctionEntry* Entry,
        UEdGraphNode* Loop, UEdGraphPin* Value, FString& Why)
    {
        UEdGraphPin* Body = Loop->FindPin(TEXT("LoopBody"), EGPD_Output);
        UEdGraphPin* Completed = Loop->FindPin(TEXT("Completed"), EGPD_Output);
        if (!Entry || !Body || Body->LinkedTo.Num() != 1 || !Completed)
        { Why = TEXT("loop capture requires one connected body successor and a Completed output"); return false; }
        auto Reach = [&](UEdGraphNode* Start, const UEdGraphPin* Omit, const UEdGraphPin* Only)
        {
            TSet<UEdGraphNode*> Seen; TArray<UEdGraphNode*> Pending{Start};
            while (!Pending.IsEmpty())
            {
                UEdGraphNode* Current = Pending.Pop(EAllowShrinking::No);
                if (!Current || Seen.Contains(Current)) continue;
                Seen.Add(Current);
                for (UEdGraphPin* Pin : Current->Pins)
                    if (Pin && Pin != Omit && (!Only || Current != Start || Pin == Only)
                        && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                        for (UEdGraphPin* Link : Pin->LinkedTo)
                            if (Link)
                            {
                                const UK2Node_MacroInstance* Macro=Cast<UK2Node_MacroInstance>(Link->GetOwningNode());
                                const UEdGraph* Definition=Macro?Macro->GetMacroGraph():nullptr;
                                const bool Feedback=Definition && Definition->GetOutermost()->GetName()==TEXT("/Engine/EditorBlueprintResources/StandardMacros")
                                    && ((Link->PinName==TEXT("Break") && (Definition->GetName()==TEXT("ForLoopWithBreak") || Definition->GetName()==TEXT("ForEachLoopWithBreak")))
                                        || (Link->PinName==TEXT("Reset") && Definition->GetName()==TEXT("DoOnce")));
                                if (!Feedback) Pending.Add(Link->GetOwningNode());
                            }
            }
            return Seen;
        };
        const TSet<UEdGraphNode*> Outside = Reach(const_cast<UK2Node_FunctionEntry*>(Entry), Body, nullptr);
        const TSet<UEdGraphNode*> Inside = Reach(Loop, nullptr, Body);
        const TSet<UEdGraphNode*> After = Reach(Loop, nullptr, Completed);
        TArray<UEdGraphPin*> Pending{Value}; TSet<UEdGraphPin*> Seen;
        int32 Reads = 0;
        while (!Pending.IsEmpty())
        {
            UEdGraphPin* Output = Pending.Pop(EAllowShrinking::No);
            if (!Output || Seen.Contains(Output)) continue;
            Seen.Add(Output);
            for (UEdGraphPin* Input : Output->LinkedTo)
            {
                UEdGraphNode* Consumer = Input ? Input->GetOwningNode() : nullptr;
                UK2Node* K2 = Cast<UK2Node>(Consumer);
                if (!K2 || Consumer->GetGraph() != Graph || Input->PinType.bIsReference)
                { Why = TEXT("a captured value has an invalid or reference consumer"); return false; }
                if (K2->IsNodePure())
                {
                    for (UEdGraphPin* Pin : K2->Pins) if (Pin && Pin->Direction == EGPD_Output && IsDataPin(Pin)) Pending.Add(Pin);
                }
                else
                {
                    ++Reads;
                    if (!Inside.Contains(Consumer) || Outside.Contains(Consumer) || After.Contains(Consumer))
                    { Why = TEXT("a transitive execution consumer is not dominated by LoopBody or can read after Completed"); return false; }
                }
            }
        }
        if (!Reads) { Why = TEXT("no execution consumer establishes the value lifetime"); return false; }
        // Unknown macros and latent calls may retain/reenter the body. Nested
        // stock loops remain synchronous and are covered by the reachability proof.
        for (UEdGraphNode* Node : Inside)
        {
            if (Node == Loop || Outside.Contains(Node) || After.Contains(Node)) continue;
            if (!Node->IsA<UK2Node_CallFunction>() && !Node->IsA<UK2Node_VariableSet>() && !Node->IsA<UK2Node_IfThenElse>()
                && !Node->IsA<UK2Node_ExecutionSequence>() && !Node->IsA<UK2Node_Knot>() && !Node->IsA<UK2Node_MacroInstance>() && !Node->IsA<UK2Node_FunctionResult>())
            { Why=TEXT("the loop body contains an unreviewed execution lifetime"); return false; }
            if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
                if (!Call->GetTargetFunction() || Call->GetTargetFunction()->HasMetaData(TEXT("Latent")))
                { Why = TEXT("the loop body contains a latent or unresolved call"); return false; }
            if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
            {
                const UEdGraph* Definition = Macro->GetMacroGraph();
                const FString Name = Definition ? Definition->GetName() : FString();
                if (!Definition || Definition->GetOutermost()->GetName() != TEXT("/Engine/EditorBlueprintResources/StandardMacros")
                    || (Name != TEXT("ForLoop") && Name != TEXT("ForLoopWithBreak") && Name != TEXT("ForEachLoop") && Name != TEXT("ForEachLoopWithBreak")))
                { Why = TEXT("the loop body contains an unreviewed macro lifetime"); return false; }
            }
        }
        return true;
    }

    bool IsArrayCount(const UEdGraphNode* Node)
    {
        const UK2Node_CallFunction* Call=Cast<UK2Node_CallFunction>(Node);
        const UFunction* Function=Call?Call->GetTargetFunction():nullptr;
        return Function && Function->GetOuterUClass()==UKismetArrayLibrary::StaticClass()
            && Function->GetFName()==TEXT("Array_Length") && Function->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintPure);
    }

    // Num reads only container length (stock GenericArray_Length). Capture only a
    // by-value function input whose direct aliases are read exclusively by Num,
    // reroutes and reviewed stock foreach macros. Unknown/mutable aliases refuse.
    bool StableInputArrayCount(UEdGraph* Graph,const UK2Node_FunctionEntry* Entry,UEdGraphNode* Node,FString& Why)
    {
        UEdGraphPin* Input=Node->FindPin(TEXT("TargetArray"),EGPD_Input);
        if (!Entry || !Input || Input->LinkedTo.Num()!=1 || !Input->LinkedTo[0])
        { Why=TEXT("array count requires one direct function-input array source"); return false; }
        UEdGraphPin* Source=Input->LinkedTo[0]; FName Parameter;
        auto IsParameterGetter=[&](const UK2Node_VariableGet* Getter,FName Name)
        { return Getter && Getter->VariableReference.IsLocalScope() && Getter->VariableReference.GetMemberScopeName()==Graph->GetName() && Getter->GetVarName()==Name; };
        if (Source->GetOwningNode()==Entry) Parameter=Source->PinName;
        else if (const UK2Node_VariableGet* Getter=Cast<UK2Node_VariableGet>(Source->GetOwningNode()))
        { if (IsParameterGetter(Getter,Getter->GetVarName())) Parameter=Getter->GetVarName(); }
        UEdGraphPin* ParameterPin=Parameter.IsNone()?nullptr:Entry->FindPin(Parameter,EGPD_Output);
        if (!ParameterPin || !ParameterPin->PinType.IsArray() || ParameterPin->PinType.bIsReference)
        { Why=TEXT("array count requires a by-value function input; members, locals and reference arrays are excluded"); return false; }
        TArray<UEdGraphPin*> Pending{ParameterPin}; TSet<UEdGraphPin*> Seen;
        for (UEdGraphNode* Other:Graph->Nodes)
        {
            if (const UK2Node_VariableSet* Setter=Cast<UK2Node_VariableSet>(Other))
                if (Setter->GetVarName()==Parameter) { Why=TEXT("the array input can be assigned during the call"); return false; }
            if (const UK2Node_VariableGet* Getter=Cast<UK2Node_VariableGet>(Other))
                if (IsParameterGetter(Getter,Parameter))
                    for (UEdGraphPin* Pin:Getter->Pins) if (Pin && Pin->Direction==EGPD_Output && IsDataPin(Pin)) Pending.Add(Pin);
        }
        while (!Pending.IsEmpty())
        {
            UEdGraphPin* Output=Pending.Pop(EAllowShrinking::No); if (!Output || Seen.Contains(Output)) continue; Seen.Add(Output);
            for (UEdGraphPin* Consumer:Output->LinkedTo)
            {
                UEdGraphNode* Owner=Consumer?Consumer->GetOwningNode():nullptr;
                if (!Owner || Owner->GetGraph()!=Graph) { Why=TEXT("the array has a missing or cross-graph alias"); return false; }
                if (UK2Node_Knot* Knot=Cast<UK2Node_Knot>(Owner))
                { for (UEdGraphPin* Pin:Knot->Pins) if (Pin && Pin->Direction==EGPD_Output) Pending.Add(Pin); continue; }
                if (IsArrayCount(Owner) && Consumer->PinName==TEXT("TargetArray")) continue;
                const UK2Node_MacroInstance* Macro=Cast<UK2Node_MacroInstance>(Owner);
                const UEdGraph* Definition=Macro?Macro->GetMacroGraph():nullptr;
                if (Definition && Definition->GetOutermost()->GetName()==TEXT("/Engine/EditorBlueprintResources/StandardMacros")
                    && (Definition->GetName()==TEXT("ForEachLoop") || Definition->GetName()==TEXT("ForEachLoopWithBreak")) && Consumer->PinName==TEXT("Array")) continue;
                Why=TEXT("the array input has an unreviewed or mutable alias consumer; only stock length and foreach reads are supported"); return false;
            }
        }
        return true;
    }

    /** Each output is a separate value; two singly used outputs are not a repeated value. */
    TArray<UEdGraphPin*> RepeatedOutputs(UEdGraphNode* Node)
    {
        TArray<UEdGraphPin*> Result;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output || !IsDataPin(Pin)) continue;
            TSet<UEdGraphPin*> Consumers;
            for (UEdGraphPin* Consumer : Pin->LinkedTo)
            {
                if (Consumer && Consumer->GetOwningNode()) Consumers.Add(Consumer);
            }
            if (Consumers.Num() >= 2) Result.Add(Pin);
        }
        return Result;
    }

}

bool FTurboMCPLocalVars::Plan(const FString& InAsset, const FString& InGraphName,
	FPlan& OutPlan, FString& OutError, const FString& RequestedNames)
{
	OutError.Empty();
	OutPlan = FPlan();
    OutPlan.Names = RequestedNames;
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

	// Report repeated outputs, including excluded producers; only reviewed lifetimes can be promoted.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr || Cast<UEdGraphNode_Comment>(Node) != nullptr)
		{
			continue;
		}
		const UK2Node* K2 = Cast<UK2Node>(Node);
		if (K2 == nullptr)
		{
            // A non-K2 node has no reviewed value-lifetime contract.
			continue;
		}
		// A variable GETTER is not a computed value; it is already a single read.
		if (Cast<UK2Node_VariableGet>(Node) != nullptr
			|| Cast<UK2Node_Knot>(Node) != nullptr
			|| Cast<UK2Node_FunctionEntry>(Node) != nullptr)
		{
			continue;
		}

        for (UEdGraphPin* Output : RepeatedOutputs(Node))
        {
            TSet<UEdGraphPin*> Consumers;
            for (UEdGraphPin* Consumer : Output->LinkedTo)
            {
                if (Consumer && Consumer->GetOwningNode()) Consumers.Add(Consumer);
            }

            FCandidate Candidate;
            Candidate.Key = KeyOf(Node);
            Candidate.NodeGuid = Node->NodeGuid;
            Candidate.OutputPinName = Output->PinName;
            Candidate.bRequiresExplicitSelection = !K2->IsNodePure();
            Candidate.AnchorGuid = Entry ? Entry->NodeGuid : FGuid();
            Candidate.AnchorPinName = UEdGraphSchema_K2::PN_Then;
            Candidate.Title = K2->GetNodeTitle(ENodeTitleType::ListView).ToString();
            Candidate.ClassName = Node->GetClass()->GetName();
            Candidate.ConsumerCount = Consumers.Num();
            Candidate.X = Node->NodePosX;
            Candidate.Y = Node->NodePosY;
            Candidate.TypeText = Output->PinType.PinCategory.ToString();
            Candidate.ProposedName = FString::Printf(TEXT("Local_%s_%s"),
                *Node->NodeGuid.ToString(EGuidFormats::Digits), *Output->PinName.ToString());

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

            if (bSafe && !IsReviewedCalculation(Node))
            {
                bSafe = false;
                Why = K2->IsNodePure()?TEXT("this pure calculation has not been reviewed as deterministic"):TEXT("this producer output has no reviewed capture timing; keep its original reads");
            }

            if (bSafe && !EntryInputsAreStable(Graph, Entry))
            {
                bSafe = false;
                Why = TEXT("function inputs can be assigned or passed by reference; entry-time capture is not supported");
            }

            if (!OutPlan.bSupportsLocals)
            {
                bSafe = false;
                Why = OutPlan.NoLocalsReason;
            }

            if (IsArrayCount(Node))
            {
                Candidate.bEntryArrayCount=true; Candidate.bRequiresExplicitSelection=true;
                bSafe=OutPlan.bSupportsLocals && StableInputArrayCount(Graph,Entry,Node,Why);
            }
            if (StockLoopValue(Node, Output))
            {
                Candidate.bBodyScoped = true;
                Candidate.AnchorGuid = Node->NodeGuid;
                Candidate.AnchorPinName = TEXT("LoopBody");
                bSafe = OutPlan.bSupportsLocals && BodyCaptureIsSafe(Graph, Entry, Node, Output, Why);
            }
            if (bSafe && (Output->PinType.IsContainer() || Output->PinType.bIsReference
                || (Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Int && Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Int64
                    && Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Real)))
            { bSafe=false; Why=TEXT("only scalar integer and real value captures are supported"); }
            if (Cast<UK2Node_MacroInstance>(Node) && Output->PinName==TEXT("Array Element"))
            { bSafe=false; Why=TEXT("array element capture is excluded because the underlying array can change within the body; only stable loop indexes are reviewed"); }
            Candidate.bSafe = bSafe;
            Candidate.UnsafeReason = bSafe ? FString() : Why;

            OutPlan.Candidates.Add(MoveTemp(Candidate));
        }
	}


    // Names use exact node-and-output identities from the preview. Validate the
    // complete map before mutation; never silently substitute a requested name.
    TSharedPtr<FJsonObject> NameMap;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RequestedNames), NameMap) || !NameMap.IsValid())
    {
        OutError = TEXT("Names must be a JSON object mapping exact output IDs from the preview to local variable names. Use {} for automatic names.");
        return false;
    }
    OutPlan.bExplicitSelection=!NameMap->Values.IsEmpty();
    for (FCandidate& Candidate : OutPlan.Candidates)
        Candidate.bSelected = NameMap->Values.IsEmpty() && !Candidate.bRequiresExplicitSelection;
    for (const auto& Pair : NameMap->Values)
    {
        FCandidate* Match = nullptr;
        for (FCandidate& Candidate : OutPlan.Candidates)
        {
            const FString Identity = TurboMCPNodeId::Make(OutPlan.Asset, OutPlan.GraphName, Candidate.NodeGuid, true)
                + TEXT(":") + Candidate.OutputPinName.ToString();
            const FString FullIdentity=TurboMCPNodeId::Make(OutPlan.Asset,OutPlan.GraphName,Candidate.NodeGuid,true,32)+TEXT(":")+Candidate.OutputPinName.ToString();
            if (FString(*Pair.Key) == Identity || FString(*Pair.Key) == FullIdentity)
            {
                if (Match) { OutError=TEXT("Names contains an ambiguous output ID; no value was selected. Use an unambiguous node identity."); return false; }
                Match=&Candidate;
            }
        }
        FString Name;
        if (!Match || !Pair.Value.IsValid() || !Pair.Value->TryGetString(Name))
        {
            OutError = TEXT("Names contains an unknown output ID or a non-string name. Copy an exact output ID from PreviewTurboMCPLocalVars.");
            return false;
        }
        bool Valid = !Name.IsEmpty() && (FChar::IsAlpha(Name[0]) || Name[0] == TEXT('_'));
        for (TCHAR Character : Name) Valid &= FChar::IsAlnum(Character) || Character == TEXT('_');
        if (!Valid || Name.Len() > 64 || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Each local name must contain 1 to 64 letters, digits or underscores, start with a letter or underscore, and must not be None.");
            return false;
        }
        Match->ProposedName = Name;
        Match->bSelected = true;
    }
    TSet<FName> UsedNames;
    for (const FCandidate& Candidate : OutPlan.Candidates)
    {
        if (!Candidate.bSelected) continue;
        const FName Name(*Candidate.ProposedName);
        bool Collision = UsedNames.Contains(Name);
        if (Entry) for (const UEdGraphPin* Pin : Entry->Pins) Collision |= Pin && Pin->PinName == Name;
        if (Entry) for (const FBPVariableDescription& Local : Entry->LocalVariables) Collision |= Local.VarName == Name;
        Collision |= FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, Candidate.ProposedName) != Name;
        if (Collision)
        {
            OutError = TEXT("Local name is already in use: ") + Candidate.ProposedName + TEXT(". Choose distinct names that do not match existing variables or function parameters.");
            return false;
        }
        UsedNames.Add(Name);
    }
    return true;
}

FString FTurboMCPLocalVars::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: values to promote in %s : %s"),
		*InPlan.Asset, *InPlan.GraphName);
	Out += FString::Printf(TEXT("\n%d node(s), %d repeatedly consumed output value(s)"),
		InPlan.NodeCountBefore, InPlan.Candidates.Num());

	if (!InPlan.bSupportsLocals && !InPlan.NoLocalsReason.IsEmpty())
	{
		Out += FString::Printf(TEXT("\n%s"), *InPlan.NoLocalsReason);
	}

	if (InPlan.Candidates.Num() == 0)
	{
        Out += TEXT("\nNo pure output is consumed more than once. This tool only promotes repeated values; no changes are proposed. A local may still help explain a value, but that case is outside this tool's scope.");
		return Out;
	}

	int32 Safe = 0;
	for (const FCandidate& Candidate : InPlan.Candidates)
	{
		if (Candidate.bSafe) { ++Safe; }

		Out += FString::Printf(TEXT("\n\n%s  %s  (%s)"),
			Candidate.bSafe ? TEXT("CANDIDATE") : TEXT("EXCLUDED"),
			*Candidate.Title, *Candidate.ClassName);
		Out += FString::Printf(TEXT("\n    %s.%s, connected to %d input pin(s)"),
			*Candidate.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens), *Candidate.OutputPinName.ToString(), Candidate.ConsumerCount);

		for (const FString& Feed : Candidate.FeedsFrom)
		{
			Out += FString::Printf(TEXT("\n    %s"), *Feed);
		}

		if (Candidate.bSafe)
		{
			Out += FString::Printf(
				TEXT("\n    assignment and value-lifetime checks passed. Proposed local name: '%s'; ")
                TEXT("edit revalidates the assignment anchor and consumer lifetime, then compiles; unsupported lifetimes are refused."),
				*Candidate.ProposedName);
		}
		else
		{
			Out += FString::Printf(TEXT("\n    NOT SAFE: %s"), *Candidate.UnsafeReason);
		}
	}

	Out += FString::Printf(
		TEXT("\n\n%d of %d candidates passed input tracing, not behavioral verification. ")
		TEXT("EditTurboMCPLocalVars requires a clean compile baseline and one connected function-entry successor; any selected excluded candidate refuses the whole edit. Preview made no graph changes."),
		Safe, InPlan.Candidates.Num());

	if (Safe == 0)
	{
		Out += TEXT("\nNOTHING WOULD BE PROMOTED.");
	}

    for (const FCandidate& Candidate : InPlan.Candidates)
    {
        Out += FString::Printf(TEXT("\nassignment: %s:%s; selected=%s; scope=%s"),
            *Candidate.AnchorGuid.ToString(EGuidFormats::Digits), *Candidate.AnchorPinName.ToString(),
            Candidate.bSelected ? TEXT("true") : TEXT("false"), Candidate.bBodyScoped ? TEXT("each LoopBody execution") : TEXT("function entry"));
        Out += TEXT("\noutput ID: ") + TurboMCPNodeId::Make(InPlan.Asset, InPlan.GraphName, Candidate.NodeGuid, true, 32)
            + TEXT(":") + Candidate.OutputPinName.ToString() + TEXT("; local name: ") + Candidate.ProposedName;
    }
    Out += TEXT("\nA nonempty Names map selects only its exact outputs; input-array counts and loop values require explicit selection. For meaningful names, pass Names as a JSON object from exact output IDs above to local names. Use the same Names in the edit. No changes have been made.");
	return Out;
}

namespace
{
    UEdGraphNode* ExactNode(UEdGraph* Graph, const FGuid& Guid)
    {
        UEdGraphNode* Result = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->NodeGuid == Guid)
            {
                if (Result) return nullptr;
                Result = Node;
            }
        }
        return Result;
    }

    UEdGraphPin* ExactPin(UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction)
    {
        UEdGraphPin* Result = nullptr;
        if (!Node) return nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->PinName == Name && Pin->Direction == Direction)
            {
                if (Result) return nullptr;
                Result = Pin;
            }
        }
        return Result;
    }

    struct FLocalCapture
    {
        FGuid Source;
        FName Output;
        FBPVariableDescription Variable;
        TArray<TPair<FGuid, FName>> Consumers;
        FGuid Setter;
        FGuid Getter;
        FGuid Anchor;
        FName AnchorPin;
        FGuid Successor;
        FName SuccessorPin;
        int32 Depth = 0;
    };

    // Planning has already refused cycles and capped ancestry depth.
    int32 CalculationDepth(UEdGraphNode* Node, int32 Depth = 0)
    {
        if (Depth >= 64) return Depth;
        int32 Result = Depth;
        for (UEdGraphPin* Pin : Node->Pins)
            if (Pin && Pin->Direction == EGPD_Input && IsDataPin(Pin))
                for (UEdGraphPin* Source : Pin->LinkedTo)
                    if (Source && Source->GetOwningNode())
                        Result = FMath::Max(Result, CalculationDepth(Source->GetOwningNode(), Depth + 1));
        return Result;
    }

    // The failure injection is private to this translation unit and used only by the native regression.
    FString ApplyLocalPlan(const FTurboMCPLocalVars::FPlan& Requested, bool bFailAfterRewire = false)
    {
        auto Refuse = [](const FString& Why)
        {
            return TEXT("unchanged: ") + Why + TEXT("; no graph changes were made; compile=not_run; save=not_requested");
        };
        if (!IsInGameThread() || !GEditor || !GEditor->CanTransact()
            || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress() || IsEngineExitRequested())
            return Refuse(TEXT("local promotion requires an idle editor outside PIE, shutdown and active transactions"));
        FTurboMCPLocalVars::FPlan Plan;
        FString Error;
        if (!FTurboMCPLocalVars::Plan(Requested.Asset, Requested.GraphName, Plan, Error, Requested.Names)) return Refuse(Error);
        if (!Plan.bExplicitSelection) for (const auto& Candidate:Plan.Candidates) if (Candidate.bEntryArrayCount)
            return Refuse(TEXT("array counts require explicit output selection in Names; automatic entry-arithmetic behavior is unchanged"));
        if (Plan.Candidates.IsEmpty()) return Refuse(TEXT("no value was promoted; the preview has no repeatedly consumed output values"));
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Plan.Asset);
        if (!Blueprint || Blueprint->bBeingCompiled || Blueprint->Status != BS_UpToDate)
            return Refuse(TEXT("compile the Blueprint successfully without warnings before requesting promotion"));
        UEdGraph* Graph = nullptr;
        for (UEdGraph* Found : Blueprint->FunctionGraphs)
            if (Found && Found->GetName() == Plan.GraphName)
            {
                if (Graph) return Refuse(TEXT("the function graph name is ambiguous"));
                Graph = Found;
            }
        if (!Graph || !Graph->GetSchema()) return Refuse(TEXT("the function graph is unavailable"));
        TArray<UK2Node_FunctionEntry*> Entries;
        Graph->GetNodesOfClass(Entries);
        if (Entries.Num() != 1) return Refuse(TEXT("the function must have exactly one entry"));
        UK2Node_FunctionEntry* Entry = Entries[0];
        UEdGraphPin* EntryExec = ExactPin(Entry, UEdGraphSchema_K2::PN_Then, EGPD_Output);
        if (!EntryExec || EntryExec->LinkedTo.Num() != 1 || !EntryExec->LinkedTo[0])
            return Refuse(TEXT("the function entry must have exactly one connected execution successor"));
        TArray<FLocalCapture> Captures;
        for (const FTurboMCPLocalVars::FCandidate& Candidate : Plan.Candidates)
        {
            if (!Candidate.bSelected) continue;
            if (!Candidate.bSafe) return Refuse(Candidate.Key + TEXT(": ") + Candidate.UnsafeReason);
            UEdGraphNode* Source = ExactNode(Graph, Candidate.NodeGuid);
            UEdGraphPin* Output = ExactPin(Source, Candidate.OutputPinName, EGPD_Output);
            if (!Output || Output->PinType.IsContainer() || Output->PinType.bIsReference
                || (Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Int
                    && Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Int64
                    && Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Real))
                return Refuse(TEXT("the output must be one unambiguous scalar arithmetic value"));
            FLocalCapture Capture;
            Capture.Source = Candidate.NodeGuid;
            Capture.Output = Candidate.OutputPinName;
            Capture.Depth = Candidate.bBodyScoped ? 0 : CalculationDepth(Source);
            Capture.Anchor = Candidate.AnchorGuid; Capture.AnchorPin = Candidate.AnchorPinName;
            UEdGraphPin* Anchor = ExactPin(ExactNode(Graph, Capture.Anchor), Capture.AnchorPin, EGPD_Output);
            if (!Anchor || Anchor->LinkedTo.Num() != 1 || !Anchor->LinkedTo[0]) return Refuse(TEXT("assignment anchor requires exactly one connected execution successor"));
            Capture.Successor = Anchor->LinkedTo[0]->GetOwningNode()->NodeGuid;
            Capture.SuccessorPin = Anchor->LinkedTo[0]->PinName;
            Capture.Variable.VarName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, Candidate.ProposedName);
            Capture.Variable.VarGuid = FGuid::NewGuid();
            Capture.Variable.VarType = Output->PinType;
            Capture.Variable.VarType.bIsConst = false;
            Capture.Variable.VarType.bIsReference = false;
            Capture.Variable.PropertyFlags = CPF_BlueprintVisible;
            Capture.Variable.FriendlyName = Capture.Variable.VarName.ToString();
            Capture.Variable.Category = UEdGraphSchema_K2::VR_DefaultCategory;
            for (UEdGraphPin* Consumer : Output->LinkedTo)
            {
                if (!Consumer || Consumer->Direction != EGPD_Input || Consumer->GetOwningNode()->GetGraph() != Graph
                    || ExactPin(ExactNode(Graph, Consumer->GetOwningNode()->NodeGuid), Consumer->PinName, EGPD_Input) != Consumer
                    || Consumer->LinkedTo.Num() != 1 || Consumer->LinkedTo[0] != Output)
                    return Refuse(TEXT("an output has a broken, duplicate, ambiguous or cross-graph consumer link"));
                Capture.Consumers.Emplace(Consumer->GetOwningNode()->NodeGuid, Consumer->PinName);
            }
            Captures.Add(MoveTemp(Capture));
        }
        if (Captures.IsEmpty()) return Refuse(TEXT("no outputs selected; select exact output IDs using Names"));
        Captures.Sort([](const FLocalCapture& A, const FLocalCapture& B) { return A.Depth < B.Depth; });
        UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
        if (!Buffer) return Refuse(TEXT("editor undo is unavailable"));
        TUniquePtr<FScopedTransaction> Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "LocalPromotion", "TurboMCP promote calculated values"));
        const FGuid OwnedId = Transaction->IsOutstanding() && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty()
            ? Buffer->UndoBuffer.Last()->GetId() : FGuid();
        if (!OwnedId.IsValid()) { Transaction.Reset(); return Refuse(TEXT("could not acquire an owned undo record")); }
        auto Fail = [&](const FString& Why)
        {
            Transaction.Reset();
            const int32 Index = Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1;
            const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
            const bool bUndone = Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
            return TEXT("error: local promotion failed: ") + Why
                + (bUndone ? TEXT("; owned undo applied; restoration is unverified") : TEXT("; owned undo could not be applied"))
                + TEXT("; changes may remain; inspect the Blueprint before retrying; compilation may have run; save=not_requested");
        };
        Blueprint->Modify(); Graph->Modify();
        for (UEdGraphNode* Node : Graph->Nodes) if (Node) Node->Modify();
        // Scalar variable declarations use the same fields as AddLocalVariable, but one
        // structural update registers them together without nested editor transactions.
        for (const FLocalCapture& Capture : Captures) Entry->LocalVariables.Add(Capture.Variable);
        {
            // Source objects were captured above. Compiler caches and reinstanced
            // classes are derived state, not unrelated objects to replay on undo.
            TGuardValue<ITransaction*> SuppressCompilerUndo(GUndo,nullptr);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
        if (Blueprint->Status == BS_Error) return Fail(TEXT("registering the local variables failed compilation"));
        TMap<FString, UEdGraphPin*> Tails;
        for (const FLocalCapture& Capture : Captures)
        {
            const FString Key = Capture.Anchor.ToString() + Capture.AnchorPin.ToString();
            if (Tails.Contains(Key)) continue;
            UEdGraphPin* Anchor = ExactPin(ExactNode(Graph, Capture.Anchor), Capture.AnchorPin, EGPD_Output);
            UEdGraphPin* Successor = ExactPin(ExactNode(Graph, Capture.Successor), Capture.SuccessorPin, EGPD_Input);
            if (!Anchor || !Successor || !Anchor->LinkedTo.Contains(Successor)) return Fail(TEXT("assignment anchor changed during local registration"));
            Anchor->BreakLinkTo(Successor); Tails.Add(Key, Anchor);
        }
        for (FLocalCapture& Capture : Captures)
        {
            UEdGraphNode* Source = ExactNode(Graph, Capture.Source);
            UEdGraphPin* Output = ExactPin(Source, Capture.Output, EGPD_Output);
            if (!Output || Output->LinkedTo.Num() != Capture.Consumers.Num()) return Fail(TEXT("a calculated output or its consumer set changed during local registration"));
            UK2Node_VariableSet* Setter = NewObject<UK2Node_VariableSet>(Graph, NAME_None, RF_Transactional);
            UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
            for (UK2Node_Variable* Variable : { static_cast<UK2Node_Variable*>(Setter), static_cast<UK2Node_Variable*>(Getter) })
            {
                Variable->CreateNewGuid();
                Variable->VariableReference.SetLocalMember(Capture.Variable.VarName, Graph->GetName(), Capture.Variable.VarGuid);
                Graph->AddNode(Variable, false, false);
                Variable->AllocateDefaultPins();
            }
            Capture.Setter = Setter->NodeGuid; Capture.Getter = Getter->NodeGuid;
            // Preserve all existing coordinates; put the new nodes below the calculation.
            Setter->NodePosX = Source->NodePosX;
            Setter->NodePosY = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Source->NodePosY) + 240, MIN_int32, MAX_int32));
            Getter->NodePosX = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Source->NodePosX) + 240, MIN_int32, MAX_int32));
            Getter->NodePosY = Setter->NodePosY;
            UEdGraphPin* ValueIn = ExactPin(Setter, Capture.Variable.VarName, EGPD_Input);
            UEdGraphPin* ValueOut = Getter->GetValuePin();
            UEdGraphPin* ExecIn = ExactPin(Setter, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
            UEdGraphPin* ExecOut = ExactPin(Setter, UEdGraphSchema_K2::PN_Then, EGPD_Output);
            if (!ValueIn || !ValueOut || !ExecIn || !ExecOut) return Fail(TEXT("a local variable node has missing pins"));
            for (const TPair<FGuid, FName>& Reader : Capture.Consumers)
            {
                UEdGraphPin* Consumer = ExactPin(ExactNode(Graph, Reader.Key), Reader.Value, EGPD_Input);
                if (!Consumer || !Consumer->LinkedTo.Contains(Output)) return Fail(TEXT("a planned consumer link changed"));
                Output->BreakLinkTo(Consumer);
                if (!Graph->GetSchema()->TryCreateConnection(ValueOut, Consumer)) return Fail(TEXT("the schema refused a local getter connection"));
            }
            if (!Graph->GetSchema()->TryCreateConnection(Output, ValueIn)
                || !Graph->GetSchema()->TryCreateConnection(Tails[Capture.Anchor.ToString() + Capture.AnchorPin.ToString()], ExecIn)) return Fail(TEXT("the schema refused a local assignment connection"));
            Tails[Capture.Anchor.ToString() + Capture.AnchorPin.ToString()] = ExecOut;
        }
        for (const FLocalCapture& Capture : Captures)
        {
            UEdGraphPin* Tail = Tails[Capture.Anchor.ToString() + Capture.AnchorPin.ToString()];
            UEdGraphPin* Successor = ExactPin(ExactNode(Graph, Capture.Successor), Capture.SuccessorPin, EGPD_Input);
            if (!Tail->LinkedTo.Contains(Successor) && !Graph->GetSchema()->TryCreateConnection(Tail, Successor)) return Fail(TEXT("the schema refused the original execution continuation"));
        }
        if (bFailAfterRewire) return Fail(TEXT("injected post-rewire regression failure"));
        FCompilerResultsLog CompileLog;
        {
            TGuardValue<ITransaction*> SuppressCompilerUndo(GUndo,nullptr);
            FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);
        }
        if (CompileLog.NumErrors || CompileLog.NumWarnings || Blueprint->Status != BS_UpToDate)
            return Fail(TEXT("the promoted function did not compile without diagnostics"));
        // Compilation may reconstruct nodes. Read back by full identities, never old pins.
        Tails.Reset();
        for (const FLocalCapture& Capture : Captures)
        {
            const FString Key = Capture.Anchor.ToString() + Capture.AnchorPin.ToString();
            UEdGraphPin* Previous = Tails.Contains(Key) ? Tails[Key] : ExactPin(ExactNode(Graph, Capture.Anchor), Capture.AnchorPin, EGPD_Output);
            UEdGraphPin* Output = ExactPin(ExactNode(Graph, Capture.Source), Capture.Output, EGPD_Output);
            UEdGraphNode* Setter = ExactNode(Graph, Capture.Setter);
            UEdGraphPin* ValueIn = ExactPin(Setter, Capture.Variable.VarName, EGPD_Input);
            UEdGraphPin* ValueOut = ExactPin(ExactNode(Graph, Capture.Getter), Capture.Variable.VarName, EGPD_Output);
            UEdGraphPin* ExecIn = ExactPin(Setter, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
            if (!Output || !ValueIn || !ValueOut || !Previous || !ExecIn
                || !Output->LinkedTo.Contains(ValueIn) || !ValueIn->LinkedTo.Contains(Output)
                || !Previous->LinkedTo.Contains(ExecIn) || !ExecIn->LinkedTo.Contains(Previous))
                return Fail(TEXT("post-compile assignment verification failed"));
            for (const TPair<FGuid, FName>& Reader : Capture.Consumers)
            {
                UEdGraphPin* Consumer = ExactPin(ExactNode(Graph, Reader.Key), Reader.Value, EGPD_Input);
                if (!Consumer || !Consumer->LinkedTo.Contains(ValueOut) || !ValueOut->LinkedTo.Contains(Consumer))
                    return Fail(TEXT("post-compile consumer verification failed"));
            }
            Tails.Add(Key, ExactPin(Setter, UEdGraphSchema_K2::PN_Then, EGPD_Output));
        }
        for (const FLocalCapture& Capture : Captures)
        {
            UEdGraphPin* Tail = Tails[Capture.Anchor.ToString() + Capture.AnchorPin.ToString()];
            UEdGraphPin* Successor = ExactPin(ExactNode(Graph, Capture.Successor), Capture.SuccessorPin, EGPD_Input);
            if (!Tail || !Successor || !Tail->LinkedTo.Contains(Successor) || !Successor->LinkedTo.Contains(Tail)) return Fail(TEXT("post-compile execution continuation verification failed"));
        }
        Blueprint->MarkPackageDirty();
        Transaction.Reset();
        return FString::Printf(TEXT("changed: assigned %d selected value(s) at validated entry/body anchors and replaced their consumers with local getters; original calculations and existing node positions were preserved; compile=passed; save=not_requested"), Captures.Num());
    }
}

FString FTurboMCPLocalVars::Apply(const FPlan& InPlan)
{
    return ApplyLocalPlan(InPlan);
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tools/TurboMCPToolsets.h"
#include "Misc/ScopeExit.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPLocalAncestryTest,
    "TurboMCP.LocalVars.Ancestry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPLocalAncestryTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/LocalAnalysis_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("LocalFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPLocalTest"));
    if (!TestNotNull(TEXT("Blueprint fixture created"), Blueprint)) return false;
    ON_SCOPE_EXIT
    {
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false);
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, NAME_None, RF_Transient);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto MakeKnot = [&]()
    {
        UK2Node_Knot* Node = NewObject<UK2Node_Knot>(Graph, NAME_None, RF_Transient);
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        return Node;
    };
    UK2Node_Knot* Shared = MakeKnot();
    UK2Node_Knot* Left = MakeKnot();
    UK2Node_Knot* Right = MakeKnot();
    Shared->GetOutputPin()->MakeLinkTo(Left->GetInputPin());
    Shared->GetOutputPin()->MakeLinkTo(Right->GetInputPin());
    TSet<UEdGraphNode*> Path;
    FString Why;
    TestTrue(TEXT("First shared ancestor accepted"), TracesToInputsOrLiterals(Left->GetInputPin(), nullptr, Path, Why, 0));
    TestTrue(TEXT("Second shared ancestor is not a cycle"), TracesToInputsOrLiterals(Right->GetInputPin(), nullptr, Path, Why, 0));
    TestEqual(TEXT("Completed traversal leaves no active ancestors"), Path.Num(), 0);

    Left->GetOutputPin()->MakeLinkTo(Shared->GetInputPin());
    TestFalse(TEXT("Actual recursion cycle refused"), TracesToInputsOrLiterals(Right->GetInputPin(), nullptr, Path, Why, 0));
    TestTrue(TEXT("Cycle explanation present"), Why.Contains(TEXT("depends on itself")));
    TestEqual(TEXT("Failed traversal clears active ancestors"), Path.Num(), 0);
    Left->GetOutputPin()->BreakLinkTo(Shared->GetInputPin());
    Shared->GetInputPin()->LinkedTo.Add(nullptr);
    TestFalse(TEXT("Missing linked pin refused"), TracesToInputsOrLiterals(Right->GetInputPin(), nullptr, Path, Why, 0));
    TestTrue(TEXT("Missing link explanation present"), Why.Contains(TEXT("missing pin link")));
    Shared->GetInputPin()->LinkedTo.Remove(nullptr);

    auto MakeCall = [&](const TCHAR* Name)
    {
        UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transient);
        Call->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Name));
        Graph->AddNode(Call, false, false);
        Call->AllocateDefaultPins();
        return Call;
    };
    UK2Node_CallFunction* Add = MakeCall(TEXT("Add_IntInt"));
    UK2Node_CallFunction* Random = MakeCall(TEXT("RandomFloat"));
    TestTrue(TEXT("Reviewed native arithmetic eligible"), IsReviewedCalculation(Add));
    TestFalse(TEXT("Random pure function ineligible"), IsReviewedCalculation(Random));
    Right->GetInputPin()->BreakAllPinLinks();
    Random->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Right->GetInputPin());
    TestFalse(TEXT("Random ancestry refused"), TracesToInputsOrLiterals(Right->GetInputPin(), nullptr, Path, Why, 0));
    TestTrue(TEXT("Determinism reason reported"), Why.Contains(TEXT("deterministic")));
    Right->GetInputPin()->BreakAllPinLinks();
    UK2Node_FunctionEntry* Entry = NewObject<UK2Node_FunctionEntry>(Graph, NAME_None, RF_Transient);
    UEdGraphPin* Parameter = Entry->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, TEXT("Input"));
    Parameter->PinType.bIsReference = true;
    Parameter->MakeLinkTo(Right->GetInputPin());
    TestFalse(TEXT("Reference input ancestry refused"), TracesToInputsOrLiterals(Right->GetInputPin(), Entry, Path, Why, 0));
    TestTrue(TEXT("Reference parameter reason reported"), Why.Contains(TEXT("reference parameter")));
    Parameter->PinType.bIsReference = false;
    TestTrue(TEXT("Value input ancestry accepted"), TracesToInputsOrLiterals(Right->GetInputPin(), Entry, Path, Why, 0));

    UK2Node_CallFunction* Multi = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transient);
    UEdGraphPin* First = Multi->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, TEXT("First"));
    UEdGraphPin* Second = Multi->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Int, TEXT("Second"));
    First->MakeLinkTo(Add->FindPinChecked(TEXT("A")));
    Second->MakeLinkTo(Add->FindPinChecked(TEXT("B")));
    TestEqual(TEXT("Two different singly used outputs are not repeated values"), RepeatedOutputs(Multi).Num(), 0);
    First->LinkedTo.Add(Add->FindPinChecked(TEXT("A")));
    TestEqual(TEXT("Duplicate link entries do not create extra consumers"), RepeatedOutputs(Multi).Num(), 0);
    First->LinkedTo.RemoveAt(1);
    UEdGraphPin* OtherConsumer = Multi->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, TEXT("OtherConsumer"));
    First->MakeLinkTo(OtherConsumer);
    const TArray<UEdGraphPin*> Repeated = RepeatedOutputs(Multi);
    TestEqual(TEXT("Only the repeatedly used output is selected"), Repeated.Num(), 1);
    if (Repeated.Num() == 1) TestTrue(TEXT("Selected value retains its pin identity"), Repeated[0] == First);
    First->BreakAllPinLinks();
    Second->BreakAllPinLinks();

    FTurboMCPLocalVars::FPlan Reused;
    Reused.Candidates.AddDefaulted();
    Reused.NodeCountBefore = 99;
    Reused.bSupportsLocals = true;
    TestFalse(TEXT("Empty asset refused"), FTurboMCPLocalVars::Plan(TEXT(""), TEXT(""), Reused, Why));
    TestEqual(TEXT("Reused candidates reset"), Reused.Candidates.Num(), 0);
    TestEqual(TEXT("Reused node count reset"), Reused.NodeCountBefore, 0);
    TestFalse(TEXT("Reused support flag reset"), Reused.bSupportsLocals);
    Graph->MarkAsGarbage();
    return !HasAnyErrors();
}
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionResult.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPLocalPromotionTest,
    "TurboMCP.LocalVars.Promotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPLocalPromotionTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/LocalPromotion_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("LocalFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPLocalTest"));
    if (!TestNotNull(TEXT("Blueprint fixture created"), Blueprint)) return false;
    ON_SCOPE_EXIT
    {
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false);
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint, TEXT("ComputeTwice"));
    if (!TestNotNull(TEXT("Function created"), Graph)) return false;
    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("Unique entry"), Entries.Num(), 1)) return false;
    UK2Node_FunctionEntry* Entry = Entries[0];
    UK2Node_FunctionResult* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
    if (!TestNotNull(TEXT("Result created"), Result)) return false;
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
    UEdGraphPin* Number = Entry->CreateUserDefinedPin(TEXT("Number"), IntType, EGPD_Output, false);
    UEdGraphPin* Third = Result->CreateUserDefinedPin(TEXT("Third"), IntType, EGPD_Input, false);
    UEdGraphPin* Left = Result->CreateUserDefinedPin(TEXT("Left"), IntType, EGPD_Input, false);
    UEdGraphPin* Right = Result->CreateUserDefinedPin(TEXT("Right"), IntType, EGPD_Input, false);
    UK2Node_CallFunction* Add = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
    Add->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
    Add->CreateNewGuid(); Graph->AddNode(Add, false, false); Add->AllocateDefaultPins();
    Add->NodePosX = 100; Add->NodePosY = 400;
    Number->MakeLinkTo(Add->FindPinChecked(TEXT("A")));
    Add->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("3");
    Add->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Left);
    UK2Node_CallFunction* Dependent = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
    Dependent->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
    Dependent->CreateNewGuid(); Graph->AddNode(Dependent, false, false); Dependent->AllocateDefaultPins();
    Dependent->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("4");
    Add->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Dependent->FindPinChecked(TEXT("A")));
    Dependent->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Right);
    Dependent->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Third);
    Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    const FGuid AddId = Add->NodeGuid;
    const int32 OriginalCount = Graph->Nodes.Num();
    auto Compile = [&]()
    {
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
        return TestEqual(TEXT("No compile errors"), Log.NumErrors, 0)
            && TestEqual(TEXT("No compile warnings"), Log.NumWarnings, 0);
    };
    auto CheckValues = [&]()
    {
        UFunction* Function = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(TEXT("ComputeTwice")) : nullptr;
        if (!TestNotNull(TEXT("Compiled function exists"), Function)) return false;
        for (int32 InputValue : { 2, 7 })
        {
        FStructOnScope Args(Function);
        const FIntProperty* Input = FindFProperty<FIntProperty>(Function, TEXT("Number"));
        if (!TestNotNull(TEXT("Input property exists"), Input)) return false;
        Input->SetPropertyValue_InContainer(Args.GetStructMemory(), InputValue);
        Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function, Args.GetStructMemory());
        for (const TCHAR* Name : { TEXT("Left"), TEXT("Right"), TEXT("Third") })
        {
            const FIntProperty* Property = FindFProperty<FIntProperty>(Function, Name);
            if (!TestNotNull(TEXT("Result property exists"), Property)) return false;
            TestEqual(FString::Printf(TEXT("%s retains calculated value"), Name), Property->GetPropertyValue_InContainer(Args.GetStructMemory()), InputValue + (FString(Name) == TEXT("Left") ? 3 : 7));
        }
        }
        return !HasAnyErrors();
    };
    if (!Compile() || !CheckValues()) return false;
    FTurboMCPLocalVars::FPlan Plan;
    FString Error;
    if (!TestTrue(TEXT("Production plan"), FTurboMCPLocalVars::Plan(Blueprint->GetPathName(), Graph->GetName(), Plan, Error))) return false;
    TestEqual(TEXT("Two dependent repeated values"), Plan.Candidates.Num(), 2);
    FGuid OriginalDependentId;
    for (const auto& Candidate:Plan.Candidates) if (Candidate.NodeGuid!=AddId) OriginalDependentId=Candidate.NodeGuid;
    UEdGraphNode* CollisionNode=ExactNode(Graph,OriginalDependentId);
    if (!TestNotNull(TEXT("Collision test calculation"),CollisionNode)) return false;
    CollisionNode->NodeGuid.A=(AddId.A&0xFFFFFF00u)|((AddId.A+1u)&0xFFu);
    const FString AmbiguousId=TurboMCPNodeId::Make(Blueprint->GetPathName(),Graph->GetName(),AddId,true)+TEXT(":ReturnValue");
    FTurboMCPLocalVars::FPlan AmbiguousPlan;
    TestFalse(TEXT("Ambiguous explicit output ID refused"),FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),Graph->GetName(),AmbiguousPlan,Error,
        TEXT("{\"")+AmbiguousId+TEXT("\":\"Chosen\"}")));
    TestTrue(TEXT("Ambiguous selection names its refusal"),Error.Contains(TEXT("ambiguous output ID")));
    CollisionNode->NodeGuid=OriginalDependentId;

    TArray<UK2Node_FunctionEntry*> CurrentEntries;
    Graph->GetNodesOfClass(CurrentEntries);
    UEdGraphPin* LiveParameter = ExactPin(CurrentEntries[0], TEXT("Number"), EGPD_Output);
    if (!TestNotNull(TEXT("Live input resolved"), LiveParameter)) return false;
    LiveParameter->PinType.bIsReference = true;
    const FString ReferenceRefusal = FTurboMCPLocalVars::Apply(Plan);
    TestTrue(TEXT("Reference parameter edit refused before mutation"), ReferenceRefusal.StartsWith(TEXT("unchanged:"))
        && ReferenceRefusal.Contains(TEXT("reference parameter")));
    TestEqual(TEXT("Reference refusal preserves node count"), Graph->Nodes.Num(), OriginalCount);
    LiveParameter->PinType.bIsReference = false;
    UEdGraphPin* ArithmeticInput = ExactPin(ExactNode(Graph, AddId), TEXT("A"), EGPD_Input);
    ArithmeticInput->PinType.bIsReference = true;
    const FString MutableRefusal = FTurboMCPLocalVars::Apply(Plan);
    TestTrue(TEXT("By-value parameter passed by reference refused"), MutableRefusal.StartsWith(TEXT("unchanged:"))
        && MutableRefusal.Contains(TEXT("inputs can be assigned or passed by reference")));
    ArithmeticInput->PinType.bIsReference = false;
    UEdGraphPin* Start = ExactPin(CurrentEntries[0], UEdGraphSchema_K2::PN_Then, EGPD_Output);
    if (!TestTrue(TEXT("Fixture has one execution successor"), Start && Start->LinkedTo.Num() == 1)) return false;
    UEdGraphPin* Continuation = Start->LinkedTo[0];
    Start->BreakLinkTo(Continuation);
    const FString ExecutionRefusal = FTurboMCPLocalVars::Apply(Plan);
    TestTrue(TEXT("Disconnected entry refused"), ExecutionRefusal.StartsWith(TEXT("unchanged:"))
        && ExecutionRefusal.Contains(TEXT("exactly one connected execution successor")));
    TestEqual(TEXT("Entry refusal creates no locals"), CurrentEntries[0]->LocalVariables.Num(), 0);
    Start->MakeLinkTo(Continuation);
    const FString Failure = ApplyLocalPlan(Plan, true);
    AddInfo(Failure);
    TestTrue(TEXT("Post-rewire failure reports unverified restoration"), Failure.Contains(TEXT("owned undo applied; restoration is unverified")));
    TestFalse(TEXT("Owned transaction ended"), GEditor->IsTransactionActive());
    TestEqual(TEXT("Failure restores node count"), Graph->Nodes.Num(), OriginalCount);
    TArray<UK2Node_FunctionEntry*> RestoredEntries;
    Graph->GetNodesOfClass(RestoredEntries);
    if (!TestEqual(TEXT("Entry restored"), RestoredEntries.Num(), 1)) return false;
    TestEqual(TEXT("Failure restores local declarations"), RestoredEntries[0]->LocalVariables.Num(), 0);
    UEdGraphNode* RestoredAdd = ExactNode(Graph, AddId);
    if (!TestNotNull(TEXT("Calculation restored"), RestoredAdd)) return false;
    TestEqual(TEXT("Both consumers restored"), ExactPin(RestoredAdd, TEXT("ReturnValue"), EGPD_Output)->LinkedTo.Num(), 2);
    if (!Compile() || !CheckValues()) return false;
    const FString Applied = FTurboMCPLocalVars::Apply(Plan);
    AddInfo(Applied);
    TestTrue(TEXT("Promotion succeeds"), Applied.StartsWith(TEXT("changed:")) && Applied.Contains(TEXT("compile=passed")));
    TestFalse(TEXT("Success transaction ended"), GEditor->IsTransactionActive());
    TestEqual(TEXT("Only two setters and two getters added"), Graph->Nodes.Num(), OriginalCount + 4);
    UEdGraphNode* KeptAdd = ExactNode(Graph, AddId);
    if (!TestNotNull(TEXT("Original calculation retained"), KeptAdd)) return false;
    TestEqual(TEXT("Existing X retained"), KeptAdd->NodePosX, 100);
    TestEqual(TEXT("Existing Y retained"), KeptAdd->NodePosY, 400);
    CheckValues();
    const FString Again = FTurboMCPLocalVars::Apply(Plan);
    TestTrue(TEXT("Repeated request is a no-op"), Again.StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Repeated request adds no nodes"), Graph->Nodes.Num(), OriginalCount + 4);
    // Explicit selection must not attempt the other repeated output.
    FTurboMCPLocalVars::FPlan Selected;
    // A stale ID is refused rather than silently promoting another output.
    TestFalse(TEXT("Stale explicit output selection refused"), FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),Graph->GetName(),Selected,Error,
        TEXT("{\"missing:ReturnValue\":\"Chosen\"}")));

    UEdGraph* LoopGraph=UBlueprintEditorLibrary::AddFunctionGraph(Blueprint,TEXT("ComputeLoop"));
    TArray<UK2Node_FunctionEntry*> LoopEntries; LoopGraph->GetNodesOfClass(LoopEntries);
    if (!TestEqual(TEXT("Loop function entry"),LoopEntries.Num(),1)) return false;
    UK2Node_FunctionEntry* LoopEntry=LoopEntries[0];
    UK2Node_FunctionResult* LoopResult=FBlueprintEditorUtils::FindOrCreateFunctionResultNode(LoopEntry);
    LoopResult->CreateUserDefinedPin(TEXT("Total"),IntType,EGPD_Input,false);
    FBPVariableDescription Sum; Sum.VarName=TEXT("Sum"); Sum.VarGuid=FGuid::NewGuid(); Sum.VarType=IntType; Sum.PropertyFlags=CPF_BlueprintVisible;
    LoopEntry->LocalVariables.Add(Sum);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    UBlueprint* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (Macros == nullptr) { AddError(TEXT("Stock macro Blueprint is unavailable")); return false; }
    UEdGraph* Definition=nullptr;
    for (UEdGraph* Macro:Macros->MacroGraphs) if (Macro && Macro->GetName()==TEXT("ForLoop")) Definition=Macro;
    if (!TestNotNull(TEXT("Stock ForLoop definition"),Definition)) return false;
    UK2Node_MacroInstance* Loop=NewObject<UK2Node_MacroInstance>(LoopGraph,NAME_None,RF_Transactional);
    Loop->SetMacroGraph(Definition); LoopGraph->AddNode(Loop,false,false); Loop->CreateNewGuid(); Loop->AllocateDefaultPins();
    const FGuid LoopId=Loop->NodeGuid;
    if (!TestNotNull(TEXT("Stock first index pin"),Loop->FindPin(TEXT("FirstIndex"))) || !TestNotNull(TEXT("Stock last index pin"),Loop->FindPin(TEXT("LastIndex")))) return false;
    Loop->FindPinChecked(TEXT("FirstIndex"))->DefaultValue=TEXT("0");
    Loop->FindPinChecked(TEXT("LastIndex"))->DefaultValue=TEXT("3");
    auto Variable=[&](bool bSet)->UK2Node_Variable*
    {
        UK2Node_Variable* Node=bSet?static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableSet>(LoopGraph,NAME_None,RF_Transactional))
            :static_cast<UK2Node_Variable*>(NewObject<UK2Node_VariableGet>(LoopGraph,NAME_None,RF_Transactional));
        Node->VariableReference.SetLocalMember(Sum.VarName,LoopGraph->GetName(),Sum.VarGuid);
        Node->CreateNewGuid(); LoopGraph->AddNode(Node,false,false); Node->AllocateDefaultPins(); return Node;
    };
    UK2Node_Variable* Initialize=Variable(true); UK2Node_Variable* Accumulate=Variable(true);
    UK2Node_Variable* SumRead=Variable(false); UK2Node_Variable* FinalRead=Variable(false);
    auto Arithmetic=[&]()->UK2Node_CallFunction*
    {
        UK2Node_CallFunction* Node=NewObject<UK2Node_CallFunction>(LoopGraph,NAME_None,RF_Transactional);
        Node->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
        Node->CreateNewGuid(); LoopGraph->AddNode(Node,false,false); Node->AllocateDefaultPins(); return Node;
    };
    UK2Node_CallFunction* Twice=Arithmetic(); UK2Node_CallFunction* AddSum=Arithmetic();
    for (const auto& Pair:TArray<TPair<UEdGraphNode*,FName>>{{Loop,TEXT("FirstIndex")},{Loop,TEXT("LastIndex")},
        {Initialize,TEXT("Sum")},{Accumulate,TEXT("Sum")},{SumRead,TEXT("Sum")},{FinalRead,TEXT("Sum")}})
        if (!TestNotNull(TEXT("Loop fixture pin ")+Pair.Value.ToString(),Pair.Key->FindPin(Pair.Value))) return false;
    LoopEntry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    LoopEntry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Initialize->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Initialize->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Loop->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Loop->FindPinChecked(TEXT("LoopBody"))->MakeLinkTo(Accumulate->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Loop->FindPinChecked(TEXT("Completed"))->MakeLinkTo(LoopResult->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Loop->FindPinChecked(TEXT("Index"))->MakeLinkTo(Twice->FindPinChecked(TEXT("A")));
    Loop->FindPinChecked(TEXT("Index"))->MakeLinkTo(Twice->FindPinChecked(TEXT("B")));
    Twice->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(AddSum->FindPinChecked(TEXT("A")));
    SumRead->FindPinChecked(TEXT("Sum"))->MakeLinkTo(AddSum->FindPinChecked(TEXT("B")));
    AddSum->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Accumulate->FindPinChecked(TEXT("Sum")));
    FinalRead->FindPinChecked(TEXT("Sum"))->MakeLinkTo(LoopResult->FindPinChecked(TEXT("Total")));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (!Compile()) return false;
    auto CheckLoop=[&]()
    {
        UFunction* Function=Blueprint->GeneratedClass->FindFunctionByName(TEXT("ComputeLoop"));
        if (!TestNotNull(TEXT("Compiled loop function"),Function)) return false;
        FStructOnScope Args(Function); Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function,Args.GetStructMemory());
        const FIntProperty* Total=FindFProperty<FIntProperty>(Function,TEXT("Total"));
        return TestNotNull(TEXT("Loop result property"),Total) && TestEqual(TEXT("Loop capture observes each iteration 2*(0+1+2+3)"),Total->GetPropertyValue_InContainer(Args.GetStructMemory()),12);
    };
    if (!CheckLoop()) return false;
    FTurboMCPLocalVars::FPlan LoopPlan;
    const FString LoopOutput=TurboMCPNodeId::Make(Blueprint->GetPathName(),LoopGraph->GetName(),LoopId,true,32)+TEXT(":Index");
    const FString LoopNames=TEXT("{\"")+LoopOutput+TEXT("\":\"CurrentIndex\"}");
    if (!TestTrue(TEXT("Explicit loop output plan"),FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),LoopGraph->GetName(),LoopPlan,Error,LoopNames))) return false;
    int32 SelectedCount=0; for (const auto& Candidate:LoopPlan.Candidates) if (Candidate.bSelected) { ++SelectedCount; TestTrue(TEXT("Loop body candidate safe"),Candidate.bSafe); }
    TestEqual(TEXT("Only requested loop output selected"),SelectedCount,1);
    // A Completed read must invalidate this same selection before any mutation.
    UEdGraphPin* Index=ExactPin(ExactNode(LoopGraph,LoopId),TEXT("Index"),EGPD_Output);
    UEdGraphPin* Final=LoopResult->FindPinChecked(TEXT("Total"));
    UEdGraphPin* Old=Final->LinkedTo[0]; Old->BreakLinkTo(Final); Index->MakeLinkTo(Final);
    const int32 LoopCount=LoopGraph->Nodes.Num();
    const FString OutsideRefusal=FTurboMCPLocalVars::Apply(LoopPlan);
    TestTrue(TEXT("Completed consumer refused"),OutsideRefusal.StartsWith(TEXT("unchanged:")) && OutsideRefusal.Contains(TEXT("Completed")));
    TestEqual(TEXT("Lifetime refusal preserves graph"),LoopGraph->Nodes.Num(),LoopCount);
    Index->BreakLinkTo(Final); Old->MakeLinkTo(Final);
    auto SnapshotLoop=[&]()
    {
        TArray<FString> Rows;
        for (UEdGraphNode* Node:LoopGraph->Nodes) if (Node)
        {
            Rows.Add(Node->NodeGuid.ToString()+FString::Printf(TEXT("@%d,%d"),Node->NodePosX,Node->NodePosY));
            for (UEdGraphPin* Pin:Node->Pins) if (Pin)
            {
                FString Row=Node->NodeGuid.ToString()+TEXT(":")+Pin->PinName.ToString()+FString::FromInt(Pin->Direction)+TEXT("=")+Pin->DefaultValue;
                for (UEdGraphPin* Link:Pin->LinkedTo) Row+=TEXT("->")+Link->GetOwningNode()->NodeGuid.ToString()+TEXT(":")+Link->PinName.ToString();
                Rows.Add(Row);
            }
        }
        return Rows;
    };
    const TArray<FString> OriginalLoop=SnapshotLoop();
    const FString LoopApplied=FTurboMCPLocalVars::Apply(LoopPlan); AddInfo(LoopApplied);
    TestTrue(TEXT("Loop promotion compiles"),LoopApplied.StartsWith(TEXT("changed:")) && LoopApplied.Contains(TEXT("compile=passed")));
    CheckLoop();
    const FString PublicUndo=UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Blueprint->GetPathName());
    TestTrue(TEXT("Public undo admits the local promotion transaction"),PublicUndo.StartsWith(TEXT("preview:")));
    FString Prefix,Confirm;
    if (!PublicUndo.Split(TEXT("Confirm="),&Prefix,&Confirm)) return false;
    TestTrue(TEXT("Public loop promotion undo succeeds"),UTurboMCPEditToolset::EditTurboMCPAssetUndo(Blueprint->GetPathName(),Confirm).StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Loop promotion undo restores ordered nodes, positions, defaults and links"),SnapshotLoop()==OriginalLoop);
    TArray<UK2Node_FunctionEntry*> UndoneEntries; LoopGraph->GetNodesOfClass(UndoneEntries);
    TestEqual(TEXT("Loop promotion undo removes only the new local"),UndoneEntries[0]->LocalVariables.Num(),1);
    if (!Compile()) return false;
    CheckLoop();

    UEdGraph* CountGraph=UBlueprintEditorLibrary::AddFunctionGraph(Blueprint,TEXT("CountRepeated"));
    TArray<UK2Node_FunctionEntry*> CountEntries; CountGraph->GetNodesOfClass(CountEntries);
    if (!TestEqual(TEXT("Count function entry"),CountEntries.Num(),1)) return false;
    UK2Node_FunctionEntry* CountEntry=CountEntries[0];
    UK2Node_FunctionResult* CountResult=FBlueprintEditorUtils::FindOrCreateFunctionResultNode(CountEntry);
    FEdGraphPinType ArrayType=IntType; ArrayType.ContainerType=EPinContainerType::Array;
    CountEntry->CreateUserDefinedPin(TEXT("Items"),ArrayType,EGPD_Output,false);
    CountResult->CreateUserDefinedPin(TEXT("TotalCount"),IntType,EGPD_Input,false);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    auto* InputReader=NewObject<UK2Node_VariableGet>(CountGraph,NAME_None,RF_Transactional);
    InputReader->VariableReference.SetLocalMember(TEXT("Items"),CountGraph->GetName(),FGuid());
    InputReader->CreateNewGuid(); CountGraph->AddNode(InputReader,false,false); InputReader->AllocateDefaultPins();
    const FGuid InputReaderId=InputReader->NodeGuid;
    auto* Length=NewObject<UK2Node_CallArrayFunction>(CountGraph,NAME_None,RF_Transactional);
    Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Length")));
    Length->CreateNewGuid(); CountGraph->AddNode(Length,false,false); Length->AllocateDefaultPins();
    const FGuid LengthId=Length->NodeGuid;
    auto* DoubleCount=NewObject<UK2Node_CallFunction>(CountGraph,NAME_None,RF_Transactional);
    DoubleCount->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")));
    DoubleCount->CreateNewGuid(); CountGraph->AddNode(DoubleCount,false,false); DoubleCount->AllocateDefaultPins();
    if (!TestNotNull(TEXT("Count input reader"),InputReader->GetValuePin())
        || !TestTrue(TEXT("Array input connection"),CountGraph->GetSchema()->TryCreateConnection(InputReader->GetValuePin(),Length->FindPin(TEXT("TargetArray"))))) return false;
    Length->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(DoubleCount->FindPinChecked(TEXT("A")));
    Length->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(DoubleCount->FindPinChecked(TEXT("B")));
    DoubleCount->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(CountResult->FindPinChecked(TEXT("TotalCount")));
    if (!Compile()) return false;
    auto CheckCount=[&]()
    {
        UFunction* Function=Blueprint->GeneratedClass->FindFunctionByName(TEXT("CountRepeated"));
        if (!TestNotNull(TEXT("Compiled count function"),Function)) return false;
        const FArrayProperty* Items=FindFProperty<FArrayProperty>(Function,TEXT("Items"));
        const FIntProperty* Total=FindFProperty<FIntProperty>(Function,TEXT("TotalCount"));
        if (!TestNotNull(TEXT("Count array argument"),Items) || !TestNotNull(TEXT("Count result"),Total)) return false;
        for (int32 Num:{3,7})
        {
            FStructOnScope Args(Function); FScriptArrayHelper Values(Items,Items->ContainerPtrToValuePtr<void>(Args.GetStructMemory())); Values.Resize(Num);
            Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function,Args.GetStructMemory());
            TestEqual(TEXT("Count capture observes each call's by-value input"),Total->GetPropertyValue_InContainer(Args.GetStructMemory()),Num*2);
        }
        return !HasAnyErrors();
    };
    if (!CheckCount()) return false;
    const FString CountId=TurboMCPNodeId::Make(Blueprint->GetPathName(),CountGraph->GetName(),LengthId,true,32)+TEXT(":ReturnValue");
    const FString CountNames=TEXT("{\"")+CountId+TEXT("\":\"PointCount\"}");
    FTurboMCPLocalVars::FPlan CountPlan;
    if (!TestTrue(TEXT("Explicit stable array count plan"),FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),CountGraph->GetName(),CountPlan,Error,CountNames))) return false;
    if (!TestTrue(TEXT("By-value array count is eligible"),CountPlan.Candidates.Num()==1 && CountPlan.Candidates[0].bSafe && CountPlan.Candidates[0].bSelected)) return false;
    FTurboMCPLocalVars::FPlan AutomaticCount;
    FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),CountGraph->GetName(),AutomaticCount,Error);
    const int32 CountBefore=CountGraph->Nodes.Num();
    TestTrue(TEXT("Array count still requires explicit selection"),FTurboMCPLocalVars::Apply(AutomaticCount).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Default selection creates nothing"),CountGraph->Nodes.Num(),CountBefore);
    auto* Clear=NewObject<UK2Node_CallArrayFunction>(CountGraph,NAME_None,RF_Transactional);
    Clear->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Clear")));
    Clear->CreateNewGuid(); CountGraph->AddNode(Clear,false,false); Clear->AllocateDefaultPins();
    InputReader=Cast<UK2Node_VariableGet>(ExactNode(CountGraph,InputReaderId));
    if (!TestNotNull(TEXT("Live count reader"),InputReader)) return false;
    CountGraph->GetSchema()->TryCreateConnection(InputReader->GetValuePin(),Clear->FindPin(TEXT("TargetArray")));
    FTurboMCPLocalVars::FPlan MutableCount;
    TestTrue(TEXT("Mutable array count is reported"),FTurboMCPLocalVars::Plan(Blueprint->GetPathName(),CountGraph->GetName(),MutableCount,Error,CountNames));
    TestTrue(TEXT("Mutable array alias refuses capture"),MutableCount.Candidates.Num()==1 && !MutableCount.Candidates[0].bSafe && MutableCount.Candidates[0].UnsafeReason.Contains(TEXT("mutable")));
    Clear->DestroyNode();
    if (!Compile()) return false;
    const FString CountApplied=FTurboMCPLocalVars::Apply(CountPlan); AddInfo(CountApplied);
    TestTrue(TEXT("Explicit array count capture compiles"),CountApplied.StartsWith(TEXT("changed:")) && CountApplied.Contains(TEXT("compile=passed")));
    TestEqual(TEXT("Count capture adds one setter and getter"),CountGraph->Nodes.Num(),CountBefore+2);
    CheckCount();
    const FString CountUndo=UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Blueprint->GetPathName());
    if (!TestTrue(TEXT("Count capture admits public undo"),CountUndo.Split(TEXT("Confirm="),&Prefix,&Confirm))) return false;
    TestTrue(TEXT("Count capture public undo succeeds"),UTurboMCPEditToolset::EditTurboMCPAssetUndo(Blueprint->GetPathName(),Confirm).StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Count capture undo restores node count"),CountGraph->Nodes.Num(),CountBefore);
    TestEqual(TEXT("Count capture undo restores both original consumers"),ExactPin(ExactNode(CountGraph,LengthId),TEXT("ReturnValue"),EGPD_Output)->LinkedTo.Num(),2);
    if (!Compile()) return false;
    CheckCount();

    return !HasAnyErrors();
}

#endif
