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
#include "Kismet/KismetMathLibrary.h"
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
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
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
                Why = TEXT("this pure calculation has not been reviewed as deterministic");
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
		Out += FString::Printf(TEXT("\n    %s.%s, connected to %d input pin(s)"),
			*Candidate.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens), *Candidate.OutputPinName.ToString(), Candidate.ConsumerCount);

		for (const FString& Feed : Candidate.FeedsFrom)
		{
			Out += FString::Printf(TEXT("\n    %s"), *Feed);
		}

		if (Candidate.bSafe)
		{
			Out += FString::Printf(
				TEXT("\n    reviewed arithmetic and input-ancestry checks passed. Proposed local name: '%s'; ")
                TEXT("promotion remains unavailable until assignment and execution placement are implemented."),
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
    // Do not restore the former getter-only rewrite: it never assigned the calculated
    // value and destroyed the original calculation. The replacement needs explicit
    // assignment, execution placement and verified failure handling.
    if (!InPlan.Candidates.IsEmpty())
    {
        return TEXT("unchanged: local-variable promotion is unavailable; calculated-value assignment and execution placement are not implemented; no graph changes were made; use the Unreal editor to create and assign the local manually; compile=not_run; save=not_requested");
    }
    return TEXT("unchanged: no value was promoted; the preview plan contains no repeatedly consumed output values; no graph changes were made; compile=not_run; save=not_requested");
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
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
#endif
