// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPBlueprintRead.h"
#include "TurboMCPDescribe.h"

#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_Knot.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPNodeId.h"

namespace
{
    FString CallerRoots(UEdGraphNode* Start)
    {
        TArray<UEdGraphNode*> Pending; Pending.Add(Start);
        TSet<UEdGraphNode*> Seen;
        TArray<FString> Roots;
        for (int32 Index = 0; Index < Pending.Num(); ++Index)
        {
            UEdGraphNode* Node = Pending[Index];
            if (!Node || Seen.Contains(Node)) continue;
            Seen.Add(Node);
            if (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>())
                Roots.AddUnique(TurboMCPNodeId::MakeForNode(Node, true));
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                // Walk exec predecessors; for a pure call first follow its value consumers.
                const UK2Node* K2 = Cast<UK2Node>(Node);
                const bool bPure = K2 && K2->IsNodePure();
                if ((Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                    || (bPure && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec))
                    for (UEdGraphPin* Other : Pin->LinkedTo) if (Other) Pending.Add(Other->GetOwningNode());
            }
        }
        Roots.Sort();
        return Roots.IsEmpty() ? TEXT("no static entry path found") : FString::Join(Roots, TEXT(", "));
    }

	/** How many graph names are listed before the manifest caps. */
	constexpr int32 MaxManifestNames = 60;

	/** How many variables are printed before the list caps. */
	constexpr int32 MaxVariables = 120;

	/** How many jobs one section may take before the section says it stopped. */
	constexpr int32 MaxSectionWork = 400;

	// `DescribeVariableFlags` and `DescribeFunctionParameters` USED TO LIVE HERE, in this
	// anonymous namespace. They moved to `TurboMCPDescribe` when the JSON writer became a
	// second rendering of the same facts: a flag list and a parameter list are exactly
	// what would drift, and two spellings of one fact is the disagreement
	// `04_FORMATS.md` warns about even when both are right.
	using TurboMCPDescribe::VariableFlags;
	using TurboMCPDescribe::FunctionParameters;

	/** The asset path of a Blueprint, so every section can name it the same way. */
	FString BlueprintPath(const UBlueprint* InBlueprint)
	{
		if (InBlueprint == nullptr)
		{
			return FString();
		}
		if (const UPackage* Package = InBlueprint->GetOutermost())
		{
			return FString::Printf(TEXT("%s.%s"), *Package->GetName(), *InBlueprint->GetName());
		}
		return InBlueprint->GetPathName();
	}
}

bool FTurboMCPBlueprintRead::ParseDetail(const FString& InName, EDetail& OutDetail)
{
	const FString Name = InName.TrimStartAndEnd().ToLower();
	if (Name.IsEmpty() || Name == TEXT("manifest"))
	{
		// Manifest is the default: `03_TOOLS.md` lists it first and calls the tool
		// the main Read tool, so the cheapest useful answer is the one a caller gets
		// without thinking about it.
		OutDetail = EDetail::Manifest;
		return true;
	}
	if (Name == TEXT("signatures")) { OutDetail = EDetail::Signatures; return true; }
	if (Name == TEXT("bodies"))     { OutDetail = EDetail::Bodies;     return true; }
	if (Name == TEXT("full"))       { OutDetail = EDetail::Full;       return true; }
	return false;
}

FString FTurboMCPBlueprintRead::DetailNames()
{
	return TEXT("manifest, signatures, bodies, full");
}

bool FTurboMCPBlueprintRead::PrintsBodies(EDetail InDetail)
{
	return InDetail == EDetail::Bodies || InDetail == EDetail::Full;
}

bool FTurboMCPBlueprintRead::PrintsRelations(EDetail InDetail)
{
	return InDetail != EDetail::Manifest;
}

void FTurboMCPBlueprintRead::CollectGraphs(const UBlueprint* InBlueprint, TArray<UEdGraph*>& OutGraphs)
{
	OutGraphs.Reset();
	if (InBlueprint == nullptr)
	{
		return;
	}

	TArray<UEdGraph*> AllGraphs;
	InBlueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* EventGraph : InBlueprint->EventGraphs)
	{
		if (EventGraph)
		{
			AllGraphs.Add(EventGraph);
			EventGraph->GetAllChildrenGraphs(AllGraphs);
		}
	}
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph) OutGraphs.AddUnique(Graph);
	}

	// A stable order, so two reads of an unchanged Blueprint give the same text.
	// The engine's own array order is not guaranteed and a reader that reorders
	// between calls is hard to compare against.
	OutGraphs.Sort([](const UEdGraph& A, const UEdGraph& B)
	{
		return A.GetName() == B.GetName()
			? A.GetPathName() < B.GetPathName() : A.GetName() < B.GetName();
	});
}

bool FTurboMCPBlueprintRead::GraphMatchesFilter(const FString& InGraphName,
	const TArray<FString>& InFilter)
{
	if (InFilter.Num() == 0)
	{
		return true;
	}
	for (const FString& Item : InFilter)
	{
		// `03_TOOLS.md`: any graph whose name contains an item matches, and case
		// does not matter.
		if (InGraphName.Contains(Item, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FString FTurboMCPBlueprintRead::DescribeVariableType(const FEdGraphPinType& InType)
{
	// The engine's own type text, so a variable and a pin describe a type the same
	// way. Reimplementing this would be a second source of truth for one format.
	return UEdGraphSchema_K2::TypeToText(InType).ToString();
}

FString FTurboMCPBlueprintRead::RenderManifest(const UBlueprint* InBlueprint)
{
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	TArray<UEdGraph*> Graphs;
	CollectGraphs(InBlueprint, Graphs);

	FString Out = FString::Printf(TEXT("asset: %s"), *BlueprintPath(InBlueprint));
	Out += FString::Printf(TEXT("\n  class: %s"), *InBlueprint->GetClass()->GetName());

	const UClass* Parent = InBlueprint->ParentClass;
	Out += FString::Printf(TEXT("\n  parent: %s"), Parent != nullptr
		? *Parent->GetPathName() : TEXT("(none)"));

	if (InBlueprint->ImplementedInterfaces.Num() == 0)
	{
		Out += TEXT("\n  interfaces: none");
	}
	else
	{
		TArray<FString> Names;
		for (const FBPInterfaceDescription& Interface : InBlueprint->ImplementedInterfaces)
		{
			Names.Add(Interface.Interface != nullptr
				? Interface.Interface->GetPathName() : TEXT("(null)"));
		}
		Out += FString::Printf(TEXT("\n  interfaces: %s"), *FString::Join(Names, TEXT(", ")));
	}

	Out += FString::Printf(TEXT("\n  counts: %d graph(s), %d variable(s), %d component(s)"),
		Graphs.Num(), InBlueprint->NewVariables.Num(),
		InBlueprint->SimpleConstructionScript != nullptr
			? InBlueprint->SimpleConstructionScript->GetAllNodes().Num() : 0);

	// The names, because `03_TOOLS.md` asks for them at this level. Each list caps
	// and says how many were left, as `04_FORMATS.md` section 1 requires.
	Out += TEXT("\n  graphs:");
	if (Graphs.Num() == 0)
	{
		Out += TEXT(" none");
	}
	for (int32 Index = 0; Index < Graphs.Num() && Index < MaxManifestNames; ++Index)
	{
		UEdGraph* Graph = Graphs[Index];
		const TCHAR* Kind = InBlueprint->FunctionGraphs.Contains(Graph) ? TEXT("function")
			: InBlueprint->MacroGraphs.Contains(Graph) ? TEXT("macro")
			: (InBlueprint->UbergraphPages.Contains(Graph) || InBlueprint->EventGraphs.Contains(Graph))
				? TEXT("event graph") : TEXT("graph");
		Out += FString::Printf(TEXT("\n    %s [%s]"), *Graph->GetName(), Kind);
	}
	if (Graphs.Num() > MaxManifestNames)
	{
		Out += FString::Printf(TEXT("\n    (%d more, at detail signatures or above)"),
			Graphs.Num() - MaxManifestNames);
	}

	Out += TEXT("\n  variables:");
	if (InBlueprint->NewVariables.Num() == 0)
	{
		Out += TEXT(" none");
	}
	for (int32 Index = 0; Index < InBlueprint->NewVariables.Num() && Index < MaxManifestNames; ++Index)
	{
		Out += FString::Printf(TEXT("\n    %s"), *InBlueprint->NewVariables[Index].VarName.ToString());
	}
	if (InBlueprint->NewVariables.Num() > MaxManifestNames)
	{
		Out += FString::Printf(TEXT("\n    (%d more)"),
			InBlueprint->NewVariables.Num() - MaxManifestNames);
	}

	Out += TEXT("\n  components:");
	if (InBlueprint->SimpleConstructionScript == nullptr)
	{
		Out += TEXT(" none");
	}
	else
	{
		const TArray<USCS_Node*>& Nodes = InBlueprint->SimpleConstructionScript->GetAllNodes();
		if (Nodes.Num() == 0)
		{
			Out += TEXT(" none");
		}
		for (int32 Index = 0; Index < Nodes.Num() && Index < MaxManifestNames; ++Index)
		{
			const USCS_Node* Node = Nodes[Index];
			Out += FString::Printf(TEXT("\n    %s"),
				Node != nullptr ? *Node->GetVariableName().ToString() : TEXT("(null)"));
		}
		if (Nodes.Num() > MaxManifestNames)
		{
			Out += FString::Printf(TEXT("\n    (%d more)"), Nodes.Num() - MaxManifestNames);
		}
	}

	return Out;
}

FString FTurboMCPBlueprintRead::RenderVariables(const UBlueprint* InBlueprint)
{
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	FString Out = FString::Printf(TEXT("\nvariables: %d"), InBlueprint->NewVariables.Num());
	if (InBlueprint->NewVariables.Num() == 0)
	{
		Out += TEXT("\n  none");
		return Out;
	}

	int32 Shown = 0;
	for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
	{
		if (Shown >= MaxVariables)
		{
			Out += FString::Printf(TEXT("\n  (%d more variable(s) not shown)"),
				InBlueprint->NewVariables.Num() - Shown);
			break;
		}
		++Shown;

		// Type text from the engine, so it matches what a pin prints.
		Out += FString::Printf(TEXT("\n  %s : %s"),
			*Variable.VarName.ToString(), *DescribeVariableType(Variable.VarType));

		// The category is `Default` when unset, which is the same word the engine's
		// own graph writer uses for an uncategorised variable.
		const FString Category = Variable.Category.IsEmpty()
			? TEXT("Default") : Variable.Category.ToString();
		Out += FString::Printf(TEXT("\n      category: %s"), *Category);

		// An empty default is a fact worth printing: it distinguishes "at the
		// type's zero" from "has a value", which is what a caller deciding whether
		// to set it needs.
		Out += FString::Printf(TEXT("\n      default: %s"),
			Variable.DefaultValue.IsEmpty() ? TEXT("(type default)") : *Variable.DefaultValue);

		Out += FString::Printf(TEXT("\n      flags: %s"), *VariableFlags(Variable));

		if (!Variable.RepNotifyFunc.IsNone())
		{
			Out += FString::Printf(TEXT("\n      rep notify: %s"), *Variable.RepNotifyFunc.ToString());
		}
	}

	return Out;
}

void FTurboMCPBlueprintRead::CollectEventsAndFunctions(const UBlueprint* InBlueprint,
	TArray<FString>& OutEvents, TArray<FString>& OutFunctions,
	const TArray<UEdGraph*>* InGraphs)
{
	OutEvents.Reset();
	OutFunctions.Reset();

	if (InBlueprint == nullptr)
	{
		return;
	}

	TArray<UEdGraph*> Graphs;
	if (InGraphs) Graphs = *InGraphs;
	else CollectGraphs(InBlueprint, Graphs);

	// Signatures are read from the real `UFunction`, not from the node.
	//
	// `GetSignatureFunction` does not exist on `UK2Node_CustomEvent`, `UK2Node_Event` or
	// `UK2Node_FunctionEntry`. The function
	// a node stands for lives on a class: an event or a custom event on the Blueprint's
	// skeleton class, or the parent's for an inherited event, and a function-entry graph
	// on the Blueprint too. Reading the reflected `UFunction` also means the signature is
	// the real one rather than something rebuilt from pins.
	const UClass* SkeletonClass = InBlueprint->SkeletonGeneratedClass != nullptr
		? InBlueprint->SkeletonGeneratedClass : InBlueprint->GeneratedClass;

	// A real function graph is one the Blueprint holds in `FunctionGraphs`. It is NOT
	// enough to look for a `UK2Node_FunctionEntry` node: the event graph has one too, for
	// the ubergraph function the engine generates, and `ReceiveBeginPlay` is also the name
	// of a function graph. USING THE NODE ALONE LISTS EVERY EVENT as both an event and a
	// function, and then reports it as a function nothing calls.
	TSet<const UEdGraph*> RealFunctionGraphs;
	for (const TObjectPtr<UEdGraph>& Graph : InBlueprint->FunctionGraphs)
	{
		if (Graph != nullptr)
		{
			RealFunctionGraphs.Add(Graph);
		}
	}

	for (UEdGraph* Graph : Graphs)
	{
		// A function exists even when its entry node is missing or its body is empty.
		if (RealFunctionGraphs.Contains(Graph))
		{
			const UFunction* Function = SkeletonClass != nullptr
				? SkeletonClass->FindFunctionByName(Graph->GetFName()) : nullptr;
			OutFunctions.Add(FString::Printf(TEXT("%s%s"), *Graph->GetName(), Function ? *FunctionParameters(Function) : TEXT(" [signature unavailable]")));
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
			{
				// A custom event's function is on the Blueprint's own class.
				const FName Name = Custom->CustomFunctionName;
				const UFunction* Function = SkeletonClass != nullptr
					? SkeletonClass->FindFunctionByName(Name) : nullptr;
				const FString Label = Name.ToString();
				OutEvents.Add(FString::Printf(TEXT("%s%s [event in graph: %s]"),
					*Label, *FunctionParameters(Function), *Graph->GetName()));
			}
			else if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
			{
				// An inherited event's function is on the parent class, because the
				// Blueprint does not redeclare it.
				const FName Name = Event->GetFunctionName();
				const UFunction* Function = nullptr;
				if (const UClass* Parent = InBlueprint->ParentClass)
				{
					Function = Parent->FindFunctionByName(Name);
				}
				if (Function == nullptr && SkeletonClass != nullptr)
				{
					Function = SkeletonClass->FindFunctionByName(Name);
				}
				const FString Label = Name.ToString();
				OutEvents.Add(FString::Printf(TEXT("%s%s [event in graph: %s]"),
					*Label, *FunctionParameters(Function), *Graph->GetName()));
			}

		}
	}
}

FString FTurboMCPBlueprintRead::RenderSignatures(const UBlueprint* InBlueprint,
	const TArray<UEdGraph*>* InGraphs)
{
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	TArray<FString> Events;
	TArray<FString> Functions;
	CollectEventsAndFunctions(InBlueprint, Events, Functions, InGraphs);

	FString Out;

	auto PrintList = [&Out](const TCHAR* InLabel, const TArray<FString>& InItems)
	{
		Out += FString::Printf(TEXT("\n%s: %d"), InLabel, InItems.Num());
		if (InItems.Num() == 0)
		{
			Out += TEXT("\n  none");
			return;
		}
		int32 Shown = 0;
		for (const FString& Item : InItems)
		{
			if (Shown >= MaxSectionWork)
			{
				Out += FString::Printf(TEXT("\n  (%d more not shown)"), InItems.Num() - Shown);
				break;
			}
			++Shown;
			Out += FString::Printf(TEXT("\n  %s"), *Item);
		}
	};

	PrintList(TEXT("events"), Events);
	PrintList(TEXT("functions"), Functions);

	// Components, with their class, because a component's class is what decides
	// which nodes can be attached to it.
	Out += TEXT("\ncomponents:");
	if (InBlueprint->SimpleConstructionScript == nullptr)
	{
		Out += TEXT("\n  none");
	}
	else
	{
		const TArray<USCS_Node*>& Nodes = InBlueprint->SimpleConstructionScript->GetAllNodes();
		if (Nodes.Num() == 0)
		{
			Out += TEXT("\n  none");
		}
		for (const USCS_Node* Node : Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			const UClass* ComponentClass = Node->ComponentClass;
			Out += FString::Printf(TEXT("\n  %s : %s"),
				*Node->GetVariableName().ToString(),
				ComponentClass != nullptr ? *ComponentClass->GetName() : TEXT("(unknown class)"));
		}
	}

	return Out;
}

FString FTurboMCPBlueprintRead::RenderRelations(const UBlueprint* InBlueprint,
	const TArray<UEdGraph*>& InGraphs, int32 InTotalGraphCount,
	TArray<FString>& OutNotes, FUsageSummary& OutUsage)
{
	OutUsage = FUsageSummary();
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	// The relation layer is where bugs become visible without reading bodies, so it
	// is built from the structure rather than from any text.
	//
	// call graph:  which graph calls which function
	// variable map: for each variable, which graphs read it and which write it
	// dead code:   functions nothing calls, and events nothing binds
	//
	// Assets touched and disconnected groups are reported by R-REFS and by the
	// graph text writer respectively, so they are not repeated here.

	/** graph name -> the functions it calls */
	TMap<FString, TSet<FString>> CallsByGraph;
	TMap<FString, TArray<FString>> CallerSites;
    TArray<FString> DelegateSites;
	/** variable -> the graphs that write it */
	TMap<FString, TSet<FString>> WritersByVariable;
	/** variable -> the graphs that read it */
	TMap<FString, TSet<FString>> ReadersByVariable;
	/** a function of this Blueprint, by name, for the dead-code check */
	TSet<FString> OwnFunctionNames;
	/** event and custom event names bound in the graphs */
	TSet<FString> EventNames;
	/** every function called anywhere in this Blueprint */
	TSet<FString> CalledFunctions;

	// The Blueprint's own function graphs, so the dead-code check looks at
	// functions and not at the event graph. See the note in RenderSignatures.
	TSet<const UEdGraph*> RealFunctionGraphs;
	for (const TObjectPtr<UEdGraph>& Graph : InBlueprint->FunctionGraphs)
	{
		if (Graph != nullptr)
		{
			RealFunctionGraphs.Add(Graph);
		}
	}

	// A local or another class's same-named member is not a use of our
	// declaration. Explicit references to this Blueprint's class do count.
	const auto IsOwnMemberReference = [InBlueprint](const FMemberReference& Reference)
	{
		if (Reference.IsLocalScope()) return false;
		const UClass* OwnerClass = Reference.GetMemberParentClass();
		return Reference.IsSelfContext() || (OwnerClass && OwnerClass->ClassGeneratedBy == InBlueprint);
	};
	const auto SortedGraphNames = [](const TSet<FString>* Names)
	{
		if (!Names || Names->IsEmpty()) return FString(TEXT("nothing"));
		TArray<FString> Ordered = Names->Array();
		Ordered.Sort();
		return FString::Join(Ordered, TEXT(", "));
	};

	int32 NodesScanned = 0;

	for (UEdGraph* Graph : InGraphs)
	{
		const FString GraphName = Graph->GetName();

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			++NodesScanned;
            if (const UK2Node_BaseMCDelegate* Dispatcher = Cast<UK2Node_BaseMCDelegate>(Node))
            {
                const FString Member = Dispatcher->GetPropertyName().ToString();
                // Calls, binds and removals all reference the dispatcher, even
                // though they are not VariableGet nodes. Do not count another
                // object's same-named member as use of this Blueprint's member.
                if (IsOwnMemberReference(Dispatcher->DelegateReference) && !Dispatcher->GetPropertyName().IsNone())
                    ReadersByVariable.FindOrAdd(Member).Add(GraphName);
                DelegateSites.Add(FString::Printf(TEXT("dispatcher reference %s; operation=%s; self=%s; node=%s; runtime invocation not inferred"),
                    *Member, *Node->GetClass()->GetName(), Dispatcher->DelegateReference.IsSelfContext() ? TEXT("true") : TEXT("false"), *TurboMCPNodeId::MakeForNode(Node, true)));
            }

            if(const UK2Node_ComponentBoundEvent* ComponentBound=Cast<UK2Node_ComponentBoundEvent>(Node))
                DelegateSites.Add(FString::Printf(TEXT("component/widget %s.%s (owner class=%s) -> event %s; declared automatic binding, runtime invocation not inferred"),
                    *ComponentBound->ComponentPropertyName.ToString(),*ComponentBound->DelegatePropertyName.ToString(),
                    *GetPathNameSafe(ComponentBound->DelegateOwnerClass.Get()),*TurboMCPNodeId::MakeForNode(Node,true)));
            else if(const UK2Node_ActorBoundEvent* ActorBound=Cast<UK2Node_ActorBoundEvent>(Node))
                DelegateSites.Add(FString::Printf(TEXT("actor %s.%s (owner class=%s) -> event %s; declared automatic binding, runtime invocation not inferred"),
                    *GetPathNameSafe(ActorBound->EventOwner.Get()),*ActorBound->DelegatePropertyName.ToString(),
                    *GetPathNameSafe(ActorBound->DelegateOwnerClass.Get()),*TurboMCPNodeId::MakeForNode(Node,true)));
            FString DelegateTarget;
            bool OwnDelegateTarget=false;
            if(const UK2Node_CreateDelegate* Delegate=Cast<UK2Node_CreateDelegate>(Node))
            {
                UClass* Scope=Delegate->GetScopeClass();
                const FName Function=Delegate->GetFunctionName();
                if(!Function.IsNone())
                {
                    DelegateTarget=(Scope?Scope->GetPathName():TEXT("[unresolved object class]"))+TEXT(".")+Function.ToString();
                    OwnDelegateTarget=Scope && Scope->ClassGeneratedBy==InBlueprint;
                }
            }
            else if(const UK2Node_CustomEvent* Event=Cast<UK2Node_CustomEvent>(Node))
            { DelegateTarget=InBlueprint->GetPathName()+TEXT(".")+Event->CustomFunctionName.ToString(); OwnDelegateTarget=true; }
            if(!DelegateTarget.IsEmpty())
                for(const UEdGraphPin* P:Node->Pins) if(P && P->Direction==EGPD_Output && P->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate)
                {
                    TArray<const UEdGraphPin*> Pending;
                    for(const UEdGraphPin* Linked:P->LinkedTo) Pending.Add(Linked);
                    TSet<const UEdGraphPin*> Visited;
                    bool ThroughReroute=false;
                    for(int32 Cursor=0;Cursor<Pending.Num();++Cursor)
                    {
                        const UEdGraphPin* Consumer=Pending[Cursor];
                        if(!Consumer || !Consumer->GetOwningNodeUnchecked() || Visited.Contains(Consumer)) continue;
                        Visited.Add(Consumer);
                        if(const UK2Node_Knot* Knot=Cast<UK2Node_Knot>(Consumer->GetOwningNodeUnchecked()))
                        {
                            ThroughReroute=true;
                            for(const UEdGraphPin* K:Knot->Pins) if(K && K->Direction==EGPD_Output)
                                for(const UEdGraphPin* Next:K->LinkedTo) Pending.Add(Next);
                            continue;
                        }
                        DelegateSites.Add(FString::Printf(TEXT("%s bound via %s.%s -> %s.%s; binding entry=%s; runtime invocation not inferred"),
                            *DelegateTarget,*TurboMCPNodeId::MakeForNode(Node,true),*P->PinName.ToString(),
                            *TurboMCPNodeId::MakeForNode(Consumer->GetOwningNodeUnchecked(),true),*Consumer->PinName.ToString(),*CallerRoots(Consumer->GetOwningNodeUnchecked())));
                        if(OwnDelegateTarget)
                            if(const UK2Node_CreateDelegate* Delegate=Cast<UK2Node_CreateDelegate>(Node)) CalledFunctions.Add(Delegate->GetFunctionName().ToString());
                    }
                    if(ThroughReroute) DelegateSites.Add(TEXT("reroute traversal: ")+TurboMCPNodeId::MakeForNode(Node,true)+TEXT(".")+P->PinName.ToString()
                        +TEXT("; transparent knots followed with cycle detection; dynamic storage/dispatch is not inferred"));
                }



			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				const FString FunctionName = Call->GetFunctionName().ToString();
				if (!FunctionName.IsEmpty())
				{
					CallsByGraph.FindOrAdd(GraphName).Add(FunctionName);
					const UFunction* Target = Call->GetTargetFunction();
                    const UClass* Owner = Target ? Target->GetOuterUClass() : nullptr;
                    if (Call->FunctionReference.IsSelfContext() || (Owner && Owner->ClassGeneratedBy == InBlueprint)) CalledFunctions.Add(FunctionName);
                    const FString TargetName = Target ? Target->GetPathName() : FunctionName + TEXT(" [unresolved owner]");
                    CallerSites.FindOrAdd(TargetName).Add(FString::Printf(TEXT("graph=%s node=%s entry=%s"),
                        *GraphName, *TurboMCPNodeId::MakeForNode(Node, true), *CallerRoots(Node)));
				}
			}
			else if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
			{
				const FString VariableName = Set->GetVarName().ToString();
				if (IsOwnMemberReference(Set->VariableReference))
					WritersByVariable.FindOrAdd(VariableName).Add(GraphName);
			}
			else if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
			{
				const FString VariableName = Get->GetVarName().ToString();
				if (IsOwnMemberReference(Get->VariableReference))
					ReadersByVariable.FindOrAdd(VariableName).Add(GraphName);
			}
			else if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
			{
				EventNames.Add(Custom->CustomFunctionName.ToString());
			}
			else if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
			{
				EventNames.Add(Event->GetFunctionName().ToString());
			}
			else if (Node->IsA<UK2Node_FunctionEntry>())
			{
				// Only a real function graph contributes a function here. The event
				// graph also holds a function entry, for the ubergraph function the
				// engine generates, and counting it made every event appear as a
				// function that nothing calls.
				if (RealFunctionGraphs.Contains(Graph))
				{
					OwnFunctionNames.Add(GraphName);
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	if (const UWidgetBlueprint* WidgetBlueprint = InGraphs.Num() == InTotalGraphCount
		? Cast<UWidgetBlueprint>(InBlueprint) : nullptr)
	{
		for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
		{
			const FString Site = FString::Printf(TEXT("widget binding %s.%s"), *Binding.ObjectName, *Binding.PropertyName.ToString());
			FName BoundFunction = Binding.FunctionName;
			if (Binding.Kind == EBindingKind::Function && Binding.MemberGuid.IsValid())
			{
				// Match the engine's GUID-based rename resolution without compiling.
				BoundFunction = WidgetBlueprint->SkeletonGeneratedClass
					? UBlueprint::GetFieldNameFromClassByGuid<UFunction>(WidgetBlueprint->SkeletonGeneratedClass, Binding.MemberGuid) : NAME_None;
			}
			if (Binding.Kind == EBindingKind::Function && !BoundFunction.IsNone())
				CalledFunctions.Add(BoundFunction.ToString());
			if (Binding.Kind == EBindingKind::Property && Binding.SourcePath.IsEmpty() && !Binding.SourceProperty.IsNone())
				ReadersByVariable.FindOrAdd(Binding.SourceProperty.ToString()).Add(Site);
			for (const FEditorPropertyPathSegment& Segment : Binding.SourcePath.Segments)
			{
				const FFieldVariant Member = Segment.GetMember();
				if (const UFunction* Function = Cast<UFunction>(Member.ToUObject()))
				{
					const UClass* OwnerClass = Function->GetOuterUClass();
					if (OwnerClass && OwnerClass->ClassGeneratedBy == InBlueprint)
						CalledFunctions.Add(Function->GetName());
				}
				else if (const FProperty* Property = CastField<FProperty>(Member.ToField()))
				{
					const UClass* OwnerClass = Property->GetOwnerClass();
					if (OwnerClass && OwnerClass->ClassGeneratedBy == InBlueprint)
						ReadersByVariable.FindOrAdd(Property->GetName()).Add(Site);
				}
			}
			DelegateSites.Add(FString::Printf(TEXT("%s -> function=%s sourceProperty=%s sourcePath=%s; asset-wide designer declaration, validity/runtime invocation not inferred"),
				*Site, *BoundFunction.ToString(), *Binding.SourceProperty.ToString(), *Binding.SourcePath.GetDisplayText().ToString()));
		}
	}
#endif

	FString Out;

	// The filter narrows the graphs the relations were collected from, so the
	// relations have to say which graphs they cover. Without this, filtering to one
	// graph and seeing a variable listed as "read by: nothing" reads as "this
	// Blueprint never reads it", which is a different and wrong statement.
	if (InTotalGraphCount > InGraphs.Num())
	{
		Out += FString::Printf(
			TEXT("\n; NOTE: a graph filter is active, so these relations cover the %d ")
			TEXT("matching graph(s) out of %d. A variable or function may be used in a ")
			TEXT("graph the filter excluded. Drop the filter for the whole Blueprint."),
			InGraphs.Num(), InTotalGraphCount);
	}

	// Call graph. Printed per graph and sorted, so two reads of an unchanged
	// Blueprint give the same text.
	Out += FString::Printf(TEXT("\ncall graph: %d graph(s) that call something"), CallsByGraph.Num());
	if (CallsByGraph.Num() == 0)
	{
		Out += TEXT("\n  none");
	}
	else
	{
		TArray<FString> GraphNames;
		CallsByGraph.GetKeys(GraphNames);
		GraphNames.Sort();

		for (const FString& GraphName : GraphNames)
		{
			TArray<FString> Targets = CallsByGraph[GraphName].Array();
			Targets.Sort();
			Out += FString::Printf(TEXT("\n  %s -> %s"),
				*GraphName, *FString::Join(Targets, TEXT(", ")));
		}
	}

    Out += FString::Printf(TEXT("\ncallers (scanned scope: %d/%d graphs; static calls only):"), InGraphs.Num(), InTotalGraphCount);
    TArray<FString> Callees; CallerSites.GetKeys(Callees); Callees.Sort();
    if (Callees.IsEmpty()) Out += TEXT("\n  none found");
    for (const FString& Callee : Callees)
    {
        Out += TEXT("\n  ") + Callee;
        TArray<FString>& Sites = CallerSites[Callee]; Sites.Sort();
        for (const FString& Site : Sites) Out += TEXT("\n    called from: ") + Site;
    }

    DelegateSites.Sort();
    Out += FString::Printf(TEXT("\ndelegate binding sites (scanned scope: %d/%d graphs; graph references; designer bindings included only for whole-asset reads; not proof of runtime invocation):"), InGraphs.Num(), InTotalGraphCount);
    if(DelegateSites.IsEmpty()) Out += TEXT("\n  none found");
    for(const FString& Site:DelegateSites) Out += TEXT("\n  ")+Site;

	// Variable map, in the Blueprint's own variable order so it matches the
	// variables section above.
	Out += TEXT("\nvariable map (selected graph references; whole-asset reads also include unreferenced declarations):");
	if (InBlueprint->NewVariables.Num() == 0)
	{
		Out += TEXT("\n  none");
	}
	for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
	{
		const FString Name = Variable.VarName.ToString();

		const TSet<FString>* Writers = WritersByVariable.Find(Name);
		const TSet<FString>* Readers = ReadersByVariable.Find(Name);
		if (InGraphs.Num() < InTotalGraphCount && !Writers && !Readers) continue;
		if (InGraphs.Num() == InTotalGraphCount && (!Readers || Readers->Num() == 0))
			OutUsage.VariablesWithoutReadReferences.Add(Name);

		// A variable nothing touches is a fact worth printing, and it is the kind
		// of thing that is obvious here and invisible in a body.
		const FString WriteText = SortedGraphNames(Writers);
		const FString ReadText = SortedGraphNames(Readers);

		Out += FString::Printf(TEXT("\n  %s"), *Name);
		Out += FString::Printf(TEXT("\n      read by:  %s"), *ReadText);
		Out += FString::Printf(TEXT("\n      written by: %s"), *WriteText);

		if ((Writers == nullptr || Writers->Num() == 0)
			&& (Readers == nullptr || Readers->Num() == 0))
		{
			Out += TEXT("\n      ; no direct read/reference or write found in scanned graphs; external, native and dynamic uses are not excluded");
		}
		else if (Writers == nullptr || Writers->Num() == 0)
		{
			Out += TEXT("\n      ; no direct write found in scanned graphs; external/native writes and instance overrides may change the value");
		}
	}

	// Dead code. Only functions of this Blueprint are checked, and an event is
	// reported as unbound rather than dead, because an event is called by the
	// engine and not by a node.
	TArray<FString> ExternalEntries;
	TArray<FString> Uncalled;
	for (const FString& FunctionName : OwnFunctionNames)
	{
		const FName EntryName(*FunctionName);
		const UFunction* ParentFunction = InBlueprint->ParentClass
			? InBlueprint->ParentClass->FindFunctionByName(EntryName) : nullptr;
		bool bInterfaceEntry = false;
		for (const FBPInterfaceDescription& Interface : InBlueprint->ImplementedInterfaces)
		{
			if (Interface.Interface && Interface.Interface->FindFunctionByName(EntryName))
			{
				bInterfaceEntry = true;
				break;
			}
		}
		if (ParentFunction || bInterfaceEntry)
		{
			ExternalEntries.Add(FString::Printf(TEXT("%s (%s; possible external entry, runtime use not established)"), *FunctionName,
				bInterfaceEntry ? TEXT("interface contract") : TEXT("inherited function/override")));
		}
		else if (InGraphs.Num() == InTotalGraphCount && !CalledFunctions.Contains(FunctionName))
		{
			Uncalled.Add(FunctionName);
		}
	}
	Uncalled.Sort();
	OutUsage.FunctionsWithoutReferences = Uncalled;
	OutUsage.GraphsScanned = InGraphs.Num();
	OutUsage.VariablesScanned = InBlueprint->NewVariables.Num();
	OutUsage.FunctionsScanned = OwnFunctionNames.Num();
	// Lint must never interpret a filtered scan as complete asset-wide usage evidence.
	OutUsage.bCollected = InGraphs.Num() == InTotalGraphCount;

	Out += InGraphs.Num() < InTotalGraphCount
		? TEXT("\nfunctions without static references: not evaluated for a filtered read")
		: TEXT("\nfunctions with no observed static reference (not proof of runtime nonuse):");
	if (Uncalled.Num() == 0 && InGraphs.Num() == InTotalGraphCount)
	{
		Out += TEXT(" none");
	}
	for (const FString& FunctionName : Uncalled)
	{
		Out += FString::Printf(TEXT("\n  %s"), *FunctionName);
	}
	ExternalEntries.Sort();
	Out += FString::Printf(TEXT("\npossible external function entries: %d"), ExternalEntries.Num());
	for (const FString& Entry : ExternalEntries) Out += TEXT("\n  ") + Entry;

	Out += FString::Printf(TEXT("\nevent declarations (not proof of binding or invocation): %d"), EventNames.Num());
	if (EventNames.Num() == 0)
	{
		Out += TEXT(" none");
	}
	else
	{
		TArray<FString> Sorted = EventNames.Array();
		Sorted.Sort();
		Out += FString::Printf(TEXT("\n  %s"), *FString::Join(Sorted, TEXT(", ")));
	}

	if (NodesScanned == 0)
	{
		// `P1`: a section that examined nothing says so, rather than looking like a
		// Blueprint with no relations.
		Out += TEXT("\n  note: no nodes were scanned, so nothing here was checked.");
	}

	return Out;
}

FString FTurboMCPBlueprintRead::RenderBodies(const UBlueprint* InBlueprint,
	const TArray<UEdGraph*>& InGraphs, EDetail InDetail, TArray<FString>& OutNotes)
{
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	FString Out = FString::Printf(TEXT("\nbodies: %d graph(s)"), InGraphs.Num());

	if (InGraphs.Num() == 0)
	{
		// A filter that matched nothing is not an error, but it must say so and
		// name the real graphs (`P7`), rather than printing an empty section.
		Out += TEXT("\n  no graph matched the filter.");
		TArray<UEdGraph*> All;
		CollectGraphs(InBlueprint, All);
		TArray<FString> Names;
		for (const UEdGraph* Graph : All)
		{
			Names.Add(Graph->GetName());
		}
		Out += FString::Printf(TEXT("\n  the graphs are: %s"),
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
		return Out;
	}

	const FString AssetPath = BlueprintPath(InBlueprint);

	// The graph text writer is the same one R-GRAPH-LOAD uses, so the two tools
	// cannot print a graph differently.
	const FTurboMCPGraphText Writer;
	const FTurboMCPGraphText::EDetail GraphDetail = InDetail == EDetail::Full
		? FTurboMCPGraphText::EDetail::Full
		: FTurboMCPGraphText::EDetail::Bodies;

	for (UEdGraph* Graph : InGraphs)
	{
		Out += FString::Printf(TEXT("\n\n; ---- %s ----"), *Graph->GetName());
		Out += TEXT("\n")
			+ Writer.Render(Graph, GraphDetail, AssetPath, OutNotes);
	}

	return Out;
}

FTurboMCPBlueprintRead::FResult FTurboMCPBlueprintRead::Read(
	const UBlueprint* InBlueprint, const FRequest& InRequest)
{
	FResult Result;
	Result.DetailUsed = InRequest.Detail;

	if (InBlueprint == nullptr)
	{
		Result.Text = TEXT("error: no Blueprint to read.");
		return Result;
	}

	TArray<UEdGraph*> AllGraphs;
	CollectGraphs(InBlueprint, AllGraphs);

	// All graph-dependent sections use the same selected graph set.
	TArray<UEdGraph*> Graphs;
	for (UEdGraph* Graph : AllGraphs)
	{
		if (GraphMatchesFilter(Graph->GetName(), InRequest.GraphFilter))
		{
			Graphs.Add(Graph);
		}
	}

	// A filter that matched nothing is refused rather than returning an empty
	// read, because an empty answer looks like "this Blueprint has nothing" when
	// the truth is "your filter is wrong".
	if (InRequest.GraphFilter.Num() > 0 && Graphs.Num() == 0)
	{
		TArray<FString> Names;
		for (const UEdGraph* Graph : AllGraphs)
		{
			Names.Add(Graph->GetName());
		}
		Result.Text = FString::Printf(
			TEXT("error: no graph in %s matches '%s'. Its graphs are: %s"),
			*BlueprintPath(InBlueprint),
			*FString::Join(InRequest.GraphFilter, TEXT(", ")),
			Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
		return Result;
	}

	// Build the requested detail once. Budget refusal never substitutes a manifest.
	Result.Text = FString::Printf(TEXT("read: %s"), *BlueprintPath(InBlueprint));

	Result.Text += TEXT("\n") + RenderManifest(InBlueprint);

	if (InRequest.Detail != EDetail::Manifest)
	{
		Result.Text += TEXT("\n") + RenderVariables(InBlueprint);
		Result.Text += TEXT("\n") + RenderSignatures(InBlueprint, &Graphs);
		Result.Text += TEXT("\n") + RenderRelations(InBlueprint, Graphs, AllGraphs.Num(), Result.Notes, Result.Usage);
	}

	if (PrintsBodies(InRequest.Detail))
	{
		Result.Text += RenderBodies(InBlueprint, Graphs, InRequest.Detail, Result.Notes);
	}

	if (Result.Notes.Num() > 0)
	{
		Result.Text += FString::Printf(
			TEXT("\n\n; %d item(s) could not be printed in full:"), Result.Notes.Num());
		for (const FString& Note : Result.Notes)
		{
			Result.Text += FString::Printf(TEXT("\n%s"), *Note);
		}
	}

	// Refuse an oversized document without substituting a different detail level.
	// Count diagnostic notes too, so the required size describes the actual document.
	if (InRequest.TokenBudgetChars > 0 && Result.Text.Len() > InRequest.TokenBudgetChars)
	{
		const TCHAR* DetailName = InRequest.Detail == EDetail::Full ? TEXT("full")
			: InRequest.Detail == EDetail::Bodies ? TEXT("bodies")
			: InRequest.Detail == EDetail::Signatures ? TEXT("signatures") : TEXT("manifest");
		Result.Text = FString::Printf(
			TEXT("error: %s at requested detail '%s' requires %d characters; TokenBudget is %d. ")
			TEXT("The requested document was not returned or downgraded. Raise TokenBudget to the required size, ")
			TEXT("narrow GraphFilter, or use ReadTurboMCPGraphLoad for a paged graph read. ")
			TEXT("TokenBudget applies per asset and does not enforce a hard response limit."),
			*BlueprintPath(InBlueprint), DetailName, Result.Text.Len(), InRequest.TokenBudgetChars);
	}

	return Result;
}
