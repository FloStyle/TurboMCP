// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPOverrideAdd.h"

#include "BlueprintEditorLibrary.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPDescribe.h"

namespace
{
	/** True when one of the two name comparisons matches, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	/** The graph that already holds an override of this name, or null. */
	UEdGraph* FindExistingGraph(UBlueprint* InBlueprint, const FName InName)
	{
		for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
		{
			if (Graph != nullptr && NameMatches(Graph->GetFName(), InName.ToString()))
			{
				return Graph;
			}
		}
		for (UEdGraph* Graph : InBlueprint->UbergraphPages)
		{
			if (Graph == nullptr)
			{
				continue;
			}
			// An event override lives as a node in the ubergraph rather than a graph of its
			// own, so the node is what is looked for.
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
				{
					if (NameMatches(Event->GetFunctionName(), InName.ToString()))
					{
						return Graph;
					}
				}
			}
		}
		return nullptr;
	}
}

bool FTurboMCPOverrideAdd::Plan(const FString& InAsset, const FString& InFunction,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();

	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.Wanted = InFunction.TrimStartAndEnd();

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}
	if (OutPlan.Wanted.IsEmpty())
	{
		OutError = TEXT("no function or event given. Pass a name from R-OVERRIDES, which ")
			TEXT("lists what this Blueprint can override.");
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
			TEXT("%s is not a Blueprint, so it cannot override anything."),
			*Resolved.ObjectPath);
		return false;
	}

	// THE OVERRIDABLE LIST IS THE EDITOR'S OWN, from ListEvents and ListFunctions, so a name
	// accepted here is a name the editor accepts. A hand-rolled filter missed interfaces
	// implemented natively in C++ the first time this project tried it.
	bool bFound = false;
	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListEvents(Blueprint))
	{
		if (NameMatches(Info.Name, OutPlan.Wanted))
		{
			OutPlan.bIsEvent = true;
			OutPlan.ResolvedName = Info.Name;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListFunctions(Blueprint))
		{
			if (NameMatches(Info.Name, OutPlan.Wanted))
			{
				OutPlan.bIsEvent = false;
				OutPlan.ResolvedName = Info.Name;
				bFound = true;
				break;
			}
		}
	}

	if (!bFound)
	{
		// The real names are listed, so a caller corrects itself in one step (`P7`). BOTH
		// lists are printed, because a caller that guessed wrong does not know which it
		// guessed wrong about.
		TArray<FString> EventNames;
		for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListEvents(Blueprint))
		{
			if (!Info.bIsImplemented)
			{
				EventNames.Add(Info.Name.ToString());
			}
		}
		TArray<FString> FunctionNames;
		for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListFunctions(Blueprint))
		{
			if (!Info.bIsImplemented)
			{
				FunctionNames.Add(Info.Name.ToString());
			}
		}
		EventNames.Sort();
		FunctionNames.Sort();

		OutError = FString::Printf(
			TEXT("'%s' is not something %s can override. Events still available: %s. ")
			TEXT("Functions still available: %s."),
			*OutPlan.Wanted, *Resolved.ObjectPath,
			EventNames.Num() > 0 ? *FString::Join(EventNames, TEXT(", ")) : TEXT("(none)"),
			FunctionNames.Num() > 0 ? *FString::Join(FunctionNames, TEXT(", "))
				: TEXT("(none)"));
		return false;
	}

	// The signature, from the real reflected UFunction rather than rebuilt from pins, and
	// through the shared describer so the text matches what R-OVERRIDES and R-BP-LOAD print.
	const UClass* SignatureClass = nullptr;
	if (const UClass* Parent = Blueprint->ParentClass)
	{
		SignatureClass = Parent;
	}

	UFunction* Signature = nullptr;
	if (OutPlan.bIsEvent)
	{
		// An event's function is on the parent class, or on the Blueprint's own skeleton for
		// a custom event.
		Signature = const_cast<UFunction*>(
			SignatureClass != nullptr
				? SignatureClass->FindFunctionByName(OutPlan.ResolvedName) : nullptr);
		if (Signature == nullptr && Blueprint->SkeletonGeneratedClass != nullptr)
		{
			Signature = const_cast<UFunction*>(
				Blueprint->SkeletonGeneratedClass->FindFunctionByName(OutPlan.ResolvedName));
		}
	}
	else if (SignatureClass != nullptr)
	{
		Signature = const_cast<UFunction*>(
			SignatureClass->FindFunctionByName(OutPlan.ResolvedName));
	}

	OutPlan.Signature = Signature != nullptr
		? FString::Printf(TEXT("%s%s"), *OutPlan.ResolvedName.ToString(),
			*TurboMCPDescribe::FunctionParameters(Signature))
		: FString::Printf(TEXT("%s(?)"), *OutPlan.ResolvedName.ToString());

	// ALREADY OVERRIDDEN IS CHECKED WHILE PLANNING, so the Preview refuses too. The engine's
	// own AddFunctionOverride does not check: it would quietly add a second graph, which is
	// how a Blueprint ends up with two implementations of one interface function.
	if (UEdGraph* Existing = FindExistingGraph(Blueprint, OutPlan.ResolvedName))
	{
		OutPlan.AlreadyImplementedIn = Existing->GetName();
	}

	return true;
}

FString FTurboMCPOverrideAdd::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: override %s in %s"),
		*InPlan.ResolvedName.ToString(), *InPlan.Asset);
	Out += FString::Printf(TEXT("\nkind: %s"), InPlan.bIsEvent ? TEXT("event") : TEXT("function"));
	Out += FString::Printf(TEXT("\nsignature: %s"), *InPlan.Signature);

	if (!InPlan.AlreadyImplementedIn.IsEmpty())
	{
		// The refusal is in the PREVIEW as a warning rather than only in the Edit, because a
		// caller that previews and then edits should not be surprised by the edit.
		Out += FString::Printf(
			TEXT("\nALREADY OVERRIDDEN, in '%s': the edit will REFUSE rather than add a ")
			TEXT("second implementation. Remove the existing override first, or pick ")
			TEXT("another name."), *InPlan.AlreadyImplementedIn);
	}
	else
	{
		Out += TEXT("\nthe edit will create the entry node and its result node.");
	}

	Out += TEXT("\n; the override is created through the editor's own ")
		+ FString(TEXT("UBlueprintEditorLibrary, so it is the same thing the editor's "))
		+ FString(TEXT("Override dropdown makes."));
	return Out;
}

FString FTurboMCPOverrideAdd::Apply(const FPlan& InPlan)
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
		return FString::Printf(
			TEXT("error: %s is not a Blueprint."), *Resolved.ObjectPath);
	}

	// REFUSED RATHER THAN DOUBLED. The engine's call would add a second graph, which is a
	// Blueprint with two implementations of one function and no obvious way to tell them
	// apart afterwards.
	if (UEdGraph* Existing = FindExistingGraph(Blueprint, InPlan.ResolvedName))
	{
		return FString::Printf(
			TEXT("unchanged: %s is already overridden in '%s', so nothing was created. ")
			TEXT("Remove that override first, or pick another name."),
			*InPlan.ResolvedName.ToString(), *Existing->GetName());
	}

	FString Created;
	FString Kind;

	if (InPlan.bIsEvent)
	{
		// `AddEventOverride` places the node, so a position is given. Zero is the top-left of
		// the graph, which is where a caller expects a new node to be rather than off-screen.
		UK2Node_Event* Node = UBlueprintEditorLibrary::AddEventOverride(
			Blueprint, InPlan.ResolvedName, FIntPoint(0, 0));

		if (Node == nullptr)
		{
			return FString::Printf(
				TEXT("error: the engine's AddEventOverride returned nothing for '%s'."),
				*InPlan.ResolvedName.ToString());
		}

		Kind = TEXT("event");
		Created = FString::Printf(TEXT("%s in graph '%s'"),
			*Node->GetName(), *Node->GetGraph()->GetName());
	}
	else
	{
		UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionOverride(
			Blueprint, InPlan.ResolvedName);

		if (Graph == nullptr)
		{
			return FString::Printf(
				TEXT("error: the engine's AddFunctionOverride returned no graph for '%s'."),
				*InPlan.ResolvedName.ToString());
		}

		Kind = TEXT("function");
		Created = FString::Printf(TEXT("graph '%s'"), *Graph->GetName());
	}

	// THE READ-BACK, through the editor's own list rather than the return value alone: the
	// engine's call says what it made, and this says the Blueprint now reports it as made.
	// A tool reporting its own intention is not evidence, which this project has learned.
	bool bNowImplemented = false;
	const TArray<FBlueprintFunctionInfo>& List = InPlan.bIsEvent
		? UBlueprintEditorLibrary::ListEvents(Blueprint)
		: UBlueprintEditorLibrary::ListFunctions(Blueprint);
	for (const FBlueprintFunctionInfo& Info : List)
	{
		if (Info.Name == InPlan.ResolvedName)
		{
			bNowImplemented = Info.bIsImplemented;
			break;
		}
	}

	FString Out = FString::Printf(TEXT("changed: overrode %s '%s' in %s"),
		*Kind, *InPlan.ResolvedName.ToString(), *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\ncreated: %s"), *Created);
	Out += FString::Printf(TEXT("\nsignature: %s"), *InPlan.Signature);
	Out += FString::Printf(TEXT("\nthe editor now reports it as implemented: %s"),
		bNowImplemented ? TEXT("yes") : TEXT("NO"));

	if (!bNowImplemented)
	{
		// The engine's call returned a graph but the Blueprint does not report the function as
		// implemented. Saying so is the difference between a result a caller can trust and one
		// that only looks like success.
		Out += TEXT("\n; the override was created but the editor does not list it as ")
			TEXT("implemented yet. It may need a compile, which E-COMPILE does.");
	}

	Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");

	return Out;
}
