// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPFunction.h"

#include "BlueprintEditorLibrary.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}

	/** True when the name is usable as a function name. */
	bool IsValidFunctionName(const FString& InName, FString& OutWhy)
	{
		if (InName.IsEmpty())
		{
			OutWhy = TEXT("it is empty");
			return false;
		}
		if (!FChar::IsAlpha(InName[0]) && InName[0] != TEXT('_'))
		{
			OutWhy = TEXT("it does not start with a letter or underscore");
			return false;
		}
		for (const TCHAR Character : InName)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				OutWhy = FString::Printf(
					TEXT("'%c' is not a letter, a digit or an underscore"), Character);
				return false;
			}
		}
		return true;
	}
}

FTurboMCPFunction::FNameCheck FTurboMCPFunction::CheckName(
	UBlueprint* InBlueprint, const FString& InName)
{
	FNameCheck Check;

	if (InBlueprint == nullptr)
	{
		Check.Reason = TEXT("no Blueprint");
		return Check;
	}

	FString Why;
	if (!IsValidFunctionName(InName, Why))
	{
		Check.Reason = FString::Printf(TEXT("'%s' cannot be a function name: %s"),
			*InName, *Why);
		return Check;
	}

	// USED ON THIS BLUEPRINT: a function graph of its own, an event, or a custom event. All
	// three occupy the name, and each is NAMED so the refusal says what is in the way.
	for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
	{
		if (Graph != nullptr && NameMatches(Graph->GetFName(), InName))
		{
			Check.Reason = FString::Printf(
				TEXT("'%s' is already a function on this Blueprint"),
				*InName);
			Check.ExistingGraph = Graph->GetName();
			return Check;
		}
	}

	for (UEdGraph* Graph : InBlueprint->UbergraphPages)
	{
		if (Graph == nullptr)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
			{
				if (NameMatches(Custom->CustomFunctionName, InName))
				{
					Check.Reason = FString::Printf(
						TEXT("'%s' is already a custom event in '%s'"),
						*InName, *Graph->GetName());
					Check.ExistingGraph = Graph->GetName();
					return Check;
				}
			}
			else if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
			{
				if (NameMatches(Event->GetFunctionName(), InName))
				{
					// An event that overrides a parent function is the parent case, and it is
					// reported as such so the caller is pointed at the override tool.
					Check.Reason = FString::Printf(
						TEXT("'%s' is already an event in '%s'"),
						*InName, *Graph->GetName());
					Check.ExistingGraph = Graph->GetName();
					return Check;
				}
			}
		}
	}

	// A PARENT FUNCTION IS REFUSED DIFFERENTLY. `03_TOOLS.md` says the refusal "points to
	// P/E-OVERRIDE-ADD", because a caller who wants to implement a parent function wants an
	// override and not a new function that would leave the parent's version untouched and
	// never called.
	if (const UClass* Parent = InBlueprint->ParentClass)
	{
		if (Parent->FindFunctionByName(FName(*InName)) != nullptr)
		{
			Check.bIsParentFunction = true;
			Check.Reason = FString::Printf(
				TEXT("'%s' is declared by the parent class %s, so a new function of that name ")
				TEXT("would override it. Use P/E-OVERRIDE-ADD instead."),
				*InName, *Parent->GetName());
			return Check;
		}
	}

	// OTHER USERS: a call node in any graph of this Blueprint that names the function, which
	// would be left pointing at nothing if the name were taken. Listed rather than treated as
	// fatal, because the caller may be about to build exactly that function.
	TArray<UEdGraph*> AllGraphs;
	for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
	{
		if (Graph != nullptr) { AllGraphs.Add(Graph); }
	}
	for (UEdGraph* Graph : InBlueprint->UbergraphPages)
	{
		if (Graph != nullptr) { AllGraphs.Add(Graph); }
	}
	for (UEdGraph* Graph : InBlueprint->MacroGraphs)
	{
		if (Graph != nullptr) { AllGraphs.Add(Graph); }
	}

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
			if (Call != nullptr && NameMatches(Call->GetFunctionName(), InName))
			{
				Check.OtherUsers.Add(FString::Printf(TEXT("a call node in '%s'"),
					*Graph->GetName()));
			}
		}
	}

	// The Blueprint's own commented variable bindings use function names too, which is worth
	// naming because a caller would not think to look there.
#if WITH_EDITORONLY_DATA
	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(InBlueprint))
	{
		for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
		{
			if (NameMatches(Binding.FunctionName, InName))
			{
				Check.OtherUsers.Add(FString::Printf(
					TEXT("a widget binding on '%s'"), *Binding.ObjectName));
			}
		}
	}
#endif

	Check.bCanAdd = true;
	return Check;
}

FString FTurboMCPFunction::Add(const FString& InAsset, const FString& InName)
{
	const FString Wanted = InName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return TEXT("error: no function name given. Pass a name, for example ")
			TEXT("ComputeScore.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it cannot hold functions."),
			*Resolved.ObjectPath);
	}

	// THE CHECK RUNS BEFORE ANYTHING IS CREATED, so a refusal changes nothing and the Blueprint
	// is never left with a half-made function.
	const FNameCheck Check = CheckName(Blueprint, Wanted);
	if (!Check.bCanAdd)
	{
		// `bIsParentFunction` is its own opening word, because a caller who meant to override
		// should not have to read the whole message to find that out.
		return FString::Printf(TEXT("unchanged: %s"), *Check.Reason);
	}

	UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint, Wanted);
	if (Graph == nullptr)
	{
		return FString::Printf(
			TEXT("error: the engine's AddFunctionGraph returned no graph for '%s'."), *Wanted);
	}

	// THE ENGINE RESOLVES A NAME COLLISION SILENTLY, SO THE CREATED NAME IS CHECKED.
	//
	// `AddFunctionGraph` does not use the name it was given: it UNIQUES it against the objects
	// already in the package, so asking for `ComputeScore` when something still held that name
	// created a graph called `ComputeScore_0`. REPORTING "added function 'ComputeScore'" ON THAT
	// ANSWER WOULD BE WRONG — the caller would then call `ComputeScore`, find nothing, and have no
	// idea why.
	//
	// This was found by REMOVING a function, re-ADDING the same name, and reading the functions
	// back: the list said `ComputeScore_0`. Nothing in the add's own report showed it.
	//
	// There is no way to ask for the exact name through this API, so a silently renamed graph is
	// UNDONE and refused rather than reported as a success. Undone because leaving a
	// `ComputeScore_0` behind would be worse than not creating it: the caller did not ask for
	// that function and would have to remove it by hand.
	if (Graph->GetName() != Wanted)
	{
		const FString Actual = Graph->GetName();

		// The extra users are named too, because the whole reason the name was taken is the
		// thing the caller needs to know about.
		FString Extra;
		if (Check.OtherUsers.Num() > 0)
		{
			Extra = FString::Printf(TEXT(" These already name it: %s."),
				*FString::Join(Check.OtherUsers, TEXT(", ")));
		}

		UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());

		return FString::Printf(
			TEXT("unchanged: the engine would not create a function named '%s' and made ")
			TEXT("'%s' instead, because something in the package still holds that name. The ")
			TEXT("graph was removed again, so nothing was added.%s If a function of that name ")
			TEXT("was just removed, the package may not have released the name yet; read the ")
			TEXT("functions back with R-BP-LOAD and try again."),
			*Wanted, *Actual, *Extra);
	}

	// The read-back. A tool that reported what it added without checking would be reporting
	// its own intention, and the name mismatch above is exactly why that is not enough.
	bool bNowImplemented = false;
	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListFunctions(Blueprint))
	{
		if (NameMatches(Info.Name, Wanted))
		{
			bNowImplemented = Info.bIsImplemented;
			break;
		}
	}

	FString Out = FString::Printf(TEXT("changed: added function '%s' to %s"),
		*Wanted, *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\ncreated: graph '%s'"), *Graph->GetName());
	Out += FString::Printf(TEXT("\nthe editor now reports it as implemented: %s"),
		bNowImplemented ? TEXT("yes") : TEXT("no"));

	if (!bNowImplemented)
	{
		Out += TEXT("\n; the graph was created but the editor does not list it as implemented ");
		Out += TEXT("yet. It may need a compile, which E-COMPILE does.");
	}

	if (Check.OtherUsers.Num() > 0)
	{
		// Named rather than silently accepted: the caller may be about to build that function,
		// or the call node may be a mistake. Either way it is a fact worth having.
		Out += FString::Printf(TEXT("\n; these already name it and will now resolve: %s"),
			*FString::Join(Check.OtherUsers, TEXT(", ")));
	}

	Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
	return Out;
}

FString FTurboMCPFunction::Remove(const FString& InAsset, const FString& InName)
{
	const FString Wanted = InName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return TEXT("error: no function name given. Pass the name of a function this ")
			TEXT("Blueprint has.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
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

	// ONLY A FUNCTION GRAPH IS REMOVED HERE, and the name is checked against the function list
	// first. Removing by NAME through the engine's `RemoveFunctionGraph` would be enough, but
	// finding the graph first is what lets the refusal name the functions this Blueprint does
	// have, and lets the result say what came out.
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph == nullptr || !NameMatches(Graph->GetFName(), Wanted))
		{
			continue;
		}

		const FString RemovedGraph = Graph->GetName();

		// The engine's own removal, which is what the editor's delete does. The graph is a
		// function's whole body, so removing it is exactly what was asked for; unlike an EVENT
		// override, a function graph holds nothing but that function.
		UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());

		// THE READ-BACK, so the removal is checked rather than asserted. What CAN be checked is
		// that no function graph holds the name any more. What CANNOT be checked this way is
		// whether the PACKAGE has released it: `AddFunctionGraph` uniques a name against the
		// objects still in the package, and that set is not something a `CheckName` over the
		// Blueprint's graphs can see.
		//
		// CLAIMING "the name is free again: yes" ON THAT WEAKER CHECK IS WRONG: the next add creates
		// `ComputeScore_0` instead of `ComputeScore`.
		const FNameCheck AfterCheck = CheckName(Blueprint, Wanted);

		FString Out = FString::Printf(TEXT("changed: removed function '%s' from %s"),
			*Wanted, *Resolved.ObjectPath);
		Out += FString::Printf(TEXT("\nremoved: graph '%s'"), *RemovedGraph);
		Out += FString::Printf(TEXT("\nno function graph holds that name now: %s"),
			AfterCheck.bCanAdd ? TEXT("yes") : TEXT("no"));

		if (!AfterCheck.bCanAdd)
		{
			// Something else still holds the name, and it is named, because the next add would
			// otherwise be refused or silently renamed.
			Out += FString::Printf(TEXT("\n; %s"), *AfterCheck.Reason);
		}
		else
		{
			// Said plainly rather than as a guarantee this check cannot make. A caller who adds
			// the same name straight back may still get a numbered graph, and the ADD reports
			// that honestly rather than creating one.
			Out += TEXT("\n; this does NOT guarantee the package has released the name. The ")
				+ FString(TEXT("engine uniques a new function's name against the objects still "))
				+ FString(TEXT("in the package, so adding the same name back immediately may "))
				+ FString(TEXT("still be renamed; E-FUNCTION-ADD detects that and refuses "))
				+ FString(TEXT("rather than creating a numbered graph."));
		}

		Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
		return Out;
	}

	// Nothing matched. The functions are listed, so a caller corrects itself in one step.
	TArray<FString> Names;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr)
		{
			Names.Add(Graph->GetName());
		}
	}
	Names.Sort();

	return FString::Printf(
		TEXT("unchanged: %s has no function named '%s', so nothing was removed. Its ")
		TEXT("functions are: %s"),
		*Resolved.ObjectPath, *Wanted,
		Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
}
