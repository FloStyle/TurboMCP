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
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPNodeId.h"

namespace
{
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

	auto Add = [&OutGraphs](const TArray<TObjectPtr<UEdGraph>>& InGraphs)
	{
		for (const TObjectPtr<UEdGraph>& Graph : InGraphs)
		{
			if (Graph != nullptr)
			{
				OutGraphs.Add(Graph);
			}
		}
	};

	// Every graph kind is collected, because a caller asking about a graph should
	// not have to know which array the Blueprint keeps it in.
	Add(InBlueprint->UbergraphPages);
	Add(InBlueprint->FunctionGraphs);
	Add(InBlueprint->MacroGraphs);
	Add(InBlueprint->EventGraphs);

	// A stable order, so two reads of an unchanged Blueprint give the same text.
	// The engine's own array order is not guaranteed and a reader that reorders
	// between calls is hard to compare against.
	OutGraphs.Sort([](const UEdGraph& A, const UEdGraph& B)
	{
		return A.GetName() < B.GetName();
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
		Out += FString::Printf(TEXT("\n    %s"), *Graphs[Index]->GetName());
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
	TArray<FString>& OutEvents, TArray<FString>& OutFunctions)
{
	OutEvents.Reset();
	OutFunctions.Reset();

	if (InBlueprint == nullptr)
	{
		return;
	}

	TArray<UEdGraph*> Graphs;
	CollectGraphs(InBlueprint, Graphs);

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
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
			{
				// A custom event's function is on the Blueprint's own class.
				const FName Name = Custom->CustomFunctionName;
				const UFunction* Function = SkeletonClass != nullptr
					? SkeletonClass->FindFunctionByName(Name) : nullptr;
				const FString Label = Name.ToString();
				OutEvents.Add(FString::Printf(TEXT("%s%s"),
					*Label, *FunctionParameters(Function)));
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
				OutEvents.Add(FString::Printf(TEXT("%s%s"),
					*Label, *FunctionParameters(Function)));
			}
			else if (Cast<UK2Node_FunctionEntry>(Node) != nullptr && RealFunctionGraphs.Contains(Graph))
			{
				// The graph is named for its function, so the graph name is the function
				// name. FindFunctionByName wants an FName, not a string.
				const FName FunctionName(*Graph->GetName());
				const UFunction* Function = SkeletonClass != nullptr
					? SkeletonClass->FindFunctionByName(FunctionName) : nullptr;
				// The name a caller writes back is the function's, which is the graph's.
				const FString Label = Graph->GetName();
				OutFunctions.Add(FString::Printf(TEXT("%s%s"),
					*Label, *FunctionParameters(Function)));
			}
		}
	}
}

FString FTurboMCPBlueprintRead::RenderSignatures(const UBlueprint* InBlueprint)
{
	if (InBlueprint == nullptr)
	{
		return FString();
	}

	TArray<FString> Events;
	TArray<FString> Functions;
	CollectEventsAndFunctions(InBlueprint, Events, Functions);

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
	TArray<FString>& OutNotes)
{
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

			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				const FString FunctionName = Call->GetFunctionName().ToString();
				if (!FunctionName.IsEmpty())
				{
					CallsByGraph.FindOrAdd(GraphName).Add(FunctionName);
					CalledFunctions.Add(FunctionName);
				}
			}
			else if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
			{
				const FString VariableName = Set->GetVarName().ToString();
				WritersByVariable.FindOrAdd(VariableName).Add(GraphName);
			}
			else if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
			{
				const FString VariableName = Get->GetVarName().ToString();
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

	// Variable map, in the Blueprint's own variable order so it matches the
	// variables section above.
	Out += FString::Printf(TEXT("\nvariable map: %d variable(s)"), InBlueprint->NewVariables.Num());
	if (InBlueprint->NewVariables.Num() == 0)
	{
		Out += TEXT("\n  none");
	}
	for (const FBPVariableDescription& Variable : InBlueprint->NewVariables)
	{
		const FString Name = Variable.VarName.ToString();

		const TSet<FString>* Writers = WritersByVariable.Find(Name);
		const TSet<FString>* Readers = ReadersByVariable.Find(Name);

		// A variable nothing touches is a fact worth printing, and it is the kind
		// of thing that is obvious here and invisible in a body.
		const FString WriteText = Writers != nullptr && Writers->Num() > 0
			? FString::Join(Writers->Array(), TEXT(", ")) : TEXT("nothing");
		const FString ReadText = Readers != nullptr && Readers->Num() > 0
			? FString::Join(Readers->Array(), TEXT(", ")) : TEXT("nothing");

		Out += FString::Printf(TEXT("\n  %s"), *Name);
		Out += FString::Printf(TEXT("\n      read by:  %s"), *ReadText);
		Out += FString::Printf(TEXT("\n      written by: %s"), *WriteText);

		if ((Writers == nullptr || Writers->Num() == 0)
			&& (Readers == nullptr || Readers->Num() == 0))
		{
			Out += TEXT("\n      never read and never written in any graph");
		}
		else if (Writers == nullptr || Writers->Num() == 0)
		{
			Out += TEXT("\n      never written, so it keeps its default");
		}
	}

	// Dead code. Only functions of this Blueprint are checked, and an event is
	// reported as unbound rather than dead, because an event is called by the
	// engine and not by a node.
	TArray<FString> Uncalled;
	for (const FString& FunctionName : OwnFunctionNames)
	{
		if (!CalledFunctions.Contains(FunctionName))
		{
			Uncalled.Add(FunctionName);
		}
	}
	Uncalled.Sort();

	Out += TEXT("\nfunctions nothing calls:");
	if (Uncalled.Num() == 0)
	{
		Out += TEXT(" none");
	}
	for (const FString& FunctionName : Uncalled)
	{
		Out += FString::Printf(TEXT("\n  %s"), *FunctionName);
	}
	if (Uncalled.Num() > 0)
	{
		// A function the ENGINE calls is not dead, and saying so prevents a caller
		// from deleting something that runs. `UserConstructionScript` is the case
		// in this kit: it is invoked when an instance is constructed, so no node
		// refers to it and it is still live.
		Out += TEXT("\n  ; this lists functions no NODE calls. A function the engine")
			TEXT("\n  ; calls itself, such as UserConstructionScript, appears here and")
			TEXT("\n  ; is not dead.");
	}

	Out += FString::Printf(TEXT("\nevents bound: %d"), EventNames.Num());
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

	// The filter is applied once, here, so every section sees the same graphs and
	// two sections cannot disagree about what was in scope.
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

	// Every section is built at the requested level. The budget is applied after,
	// by dropping whole sections, so a caller knows exactly what was lost rather
	// than finding a line cut in half.
	Result.Text = FString::Printf(TEXT("read: %s"), *BlueprintPath(InBlueprint));

	Result.Text += TEXT("\n") + RenderManifest(InBlueprint);

	if (InRequest.Detail != EDetail::Manifest)
	{
		Result.Text += TEXT("\n") + RenderVariables(InBlueprint);
		Result.Text += TEXT("\n") + RenderSignatures(InBlueprint);
		Result.Text += TEXT("\n") + RenderRelations(InBlueprint, Graphs, AllGraphs.Num(), Result.Notes);
	}

	if (PrintsBodies(InRequest.Detail))
	{
		Result.Text += RenderBodies(InBlueprint, Graphs, InRequest.Detail, Result.Notes);
	}

	// The budget, if one was given. It drops whole graph bodies, biggest first,
	// which is what `03_TOOLS.md` asks for, and never silently truncates a line.
	if (InRequest.TokenBudgetChars > 0 && Result.Text.Len() > InRequest.TokenBudgetChars)
	{
		const int32 OriginalLength = Result.Text.Len();
		Result.bReduced = true;

		if (PrintsBodies(InRequest.Detail))
		{
			// Step 1: drop the bodies, which are almost always the largest part.
			FRequest Lower = InRequest;
			Lower.Detail = EDetail::Signatures;
			FResult Without = Read(InBlueprint, Lower);

			Result.Reductions.Add(FString::Printf(
				TEXT("graph bodies were dropped to fit the %d character budget. ")
				TEXT("Use ReadTurboMCPGraphLoad with one graph name to read a body, ")
				TEXT("or raise the budget."),
				InRequest.TokenBudgetChars));

			if (Without.Text.Len() <= InRequest.TokenBudgetChars)
			{
				Result.Text = Without.Text;
				Result.DetailUsed = EDetail::Signatures;
			}
			else
			{
				// Step 2: even without bodies it does not fit, so step down to the
				// manifest rather than truncating mid-section.
				Lower.Detail = EDetail::Manifest;
				FResult Minimal = Read(InBlueprint, Lower);
				Result.Text = Minimal.Text;
				Result.DetailUsed = EDetail::Manifest;
				Result.Reductions.Add(FString::Printf(
					TEXT("the relation layer and the signatures were dropped too, ")
					TEXT("because even without bodies the read was %d characters over ")
					TEXT("the budget."),
					Without.Text.Len() - InRequest.TokenBudgetChars));
			}
		}
		else if (InRequest.Detail != EDetail::Manifest)
		{
			// Already at or below signatures, so the only lever left is the
			// manifest.
			FRequest Lower = InRequest;
			Lower.Detail = EDetail::Manifest;
			FResult Minimal = Read(InBlueprint, Lower);
			Result.Text = Minimal.Text;
			Result.DetailUsed = EDetail::Manifest;
			Result.Reductions.Add(FString::Printf(
				TEXT("everything above the manifest was dropped to fit the ")
				TEXT("%d character budget (the full read was %d characters)."),
				InRequest.TokenBudgetChars, OriginalLength));
		}
		else
		{
			// *** THE FLOOR OF THE LADDER, AND THE END OF THE RECURSION. THIS BRANCH USED TO SET `Lower.Detail =
			// Manifest` AND CALL `Read` AGAIN - FROM A CALL THAT WAS ALREADY AT `Manifest`. NOTHING CHANGED BETWEEN
			// ONE CALL AND THE NEXT, SO A MANIFEST THAT EXCEEDED THE BUDGET RECURSED UNTIL THE STACK DIED. That is
			// the reported crash, and the report's own words fit it: a read AT `manifest` DETAIL, with a graph
			// filter, on a Blueprint with 38 graphs and about 960 nodes. ***
			// *** THERE IS NO LEVER LEFT AT THE FLOOR, SO THE ANSWER SAYS SO AND NAMES THE ASSET AND THE TWO NUMBERS.
			// A BOUNDED ANSWER THAT STATES ITS LIMIT IS A RESULT; THE RECURSION WAS A DEAD SESSION. ***
			Result.Reductions.Add(FString::Printf(
				TEXT("the budget cannot be met: this read is already at manifest detail, which is the smallest ")
				TEXT("thing this reader can print, and %s prints %d characters against a budget of %d. Nothing ")
				TEXT("further can be dropped without dropping the manifest itself. Raise the budget, or read one ")
				TEXT("graph at a time."),
				InBlueprint != nullptr ? *BlueprintPath(InBlueprint) : TEXT("(no asset)"),
				Result.Text.Len(), InRequest.TokenBudgetChars));
		}
	}

	if (Result.Reductions.Num() > 0)
	{
		// `03_TOOLS.md`: end with a block that lists what was reduced and the exact
		// call that gets the rest.
		Result.Text += FString::Printf(TEXT("\n\n; reduced to detail %s to fit the budget:"),
			Result.DetailUsed == EDetail::Full ? TEXT("full")
				: Result.DetailUsed == EDetail::Bodies ? TEXT("bodies")
				: Result.DetailUsed == EDetail::Signatures ? TEXT("signatures")
				: TEXT("manifest"));
		for (const FString& Reduction : Result.Reductions)
		{
			Result.Text += FString::Printf(TEXT("\n;   %s"), *Reduction);
		}
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

	return Result;
}
