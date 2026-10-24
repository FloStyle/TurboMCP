// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPFunction.h"
#include "TurboMCPBlueprintRead.h"

#include "BlueprintEditorLibrary.h"
#include "Algo/Unique.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPTypeParser.h"

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
		// Validate before constructing FName: oversized user input can reach the
		// engine's fatal name-length guard, and None is its empty-name sentinel.
		if (InName.Len() >= NAME_SIZE)
		{
			OutWhy = FString::Printf(TEXT("it exceeds the maximum of %d characters"), NAME_SIZE - 1);
			return false;
		}
		if (InName.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			OutWhy = TEXT("None is reserved as Unreal's empty name");
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

	struct FSignaturePin { FString Name; FEdGraphPinType Type; };

	bool ParseSignature(const FString& InText, const TCHAR* InKind, TArray<FSignaturePin>& OutPins, FString& OutWhy)
	{
		OutPins.Reset();
		if (InText.TrimStartAndEnd().IsEmpty()) { return true; }
		TArray<FString> Items; int32 Depth = 0; int32 Start = 0;
		for (int32 Index = 0; Index < InText.Len(); ++Index)
		{
			if (InText[Index] == TEXT('<')) { ++Depth; }
			else if (InText[Index] == TEXT('>')) { --Depth; }
			else if (InText[Index] == TEXT(',') && Depth == 0) { Items.Add(InText.Mid(Start, Index - Start)); Start = Index + 1; }
		}
		Items.Add(InText.Mid(Start));
		TSet<FName> Names;
		for (const FString& Raw : Items)
		{
			FString Name; FString TypeText;
			if (!Raw.Split(TEXT(":"), &Name, &TypeText)) { OutWhy = FString::Printf(TEXT("%s '%s' must be 'Name: Type'"), InKind, *Raw.TrimStartAndEnd()); return false; }
			Name = Name.TrimStartAndEnd(); TypeText = TypeText.TrimStartAndEnd();
			FString NameWhy;
			if (!IsValidFunctionName(Name, NameWhy) || Names.Contains(FName(*Name))) { OutWhy = FString::Printf(TEXT("%s pin '%s' is invalid or duplicated"), InKind, *Name); return false; }
			FSignaturePin& Pin = OutPins.AddDefaulted_GetRef(); Pin.Name = Name;
			if (!FTurboMCPTypeParser::Resolve(TypeText, Pin.Type, OutWhy)) { OutWhy = FString::Printf(TEXT("%s pin '%s' has invalid type '%s': %s"), InKind, *Name, *TypeText, *OutWhy); return false; }
			Names.Add(FName(*Name));
		}
		return true;
	}

	// Caller reconstruction replaces pins: retain weak pin references.
	struct FCallLink { FString PinName; FWeakGraphPinPtr Pin; FWeakGraphPinPtr LinkedPin; };
	void CaptureCallLinks(UBlueprint* InBlueprint, const FString& InFunction, TArray<FCallLink>& OutLinks, TArray<UK2Node_CallFunction*>& OutCalls)
	{
		TArray<UK2Node_CallFunction*> Calls; FBlueprintEditorUtils::GetAllNodesOfClass(InBlueprint, Calls); OutCalls.Reset();
		for (UK2Node_CallFunction* Call : Calls)
		{
			if (Call == nullptr || !Call->FunctionReference.IsSelfContext() || !NameMatches(Call->GetFunctionName(), InFunction)) { continue; }
			OutCalls.Add(Call);
			for (UEdGraphPin* Pin : Call->Pins) { if (Pin != nullptr) for (UEdGraphPin* Linked : Pin->LinkedTo) { OutLinks.Add({ Pin->PinName.ToString(), Pin, Linked }); } }
		}
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
				TEXT("would override it. Use PreviewTurboMCPOverrideAdd and EditTurboMCPOverrideAdd instead."),
				*InName, *Parent->GetName());
			return Check;
		}
	}

	// OTHER USERS: a call node in any graph of this Blueprint that names the function, which
	// would be left pointing at nothing if the name were taken. Listed rather than treated as
	// fatal, because the caller may be about to build exactly that function.
	TArray<UEdGraph*> AllGraphs;
    FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, AllGraphs);

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

FString FTurboMCPFunction::Add(const FString& InAsset, const FString& InName,
	const FString& InInputs, const FString& InOutputs)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: function creation requires an idle editor outside PIE and active transactions");
	const FString Wanted = InName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return TEXT("error: no function name given. Pass a name, for example ")
			TEXT("ComputeScore.");
	}

	TArray<FSignaturePin> Inputs;
	TArray<FSignaturePin> Outputs;
	FString SignatureWhy;
	if (!ParseSignature(InInputs, TEXT("input"), Inputs, SignatureWhy)
		|| !ParseSignature(InOutputs, TEXT("output"), Outputs, SignatureWhy))
	{
		return FString::Printf(TEXT("unchanged: %s."), *SignatureWhy);
	}

    // Inputs and outputs share one reflected function parameter namespace.
    TSet<FName> ParameterNames;
    for (const FSignaturePin& Pin : Inputs) ParameterNames.Add(FName(*Pin.Name));
    for (const FSignaturePin& Pin : Outputs)
    {
        if (ParameterNames.Contains(FName(*Pin.Name)))
            return FString::Printf(TEXT("unchanged: parameter '%s' occurs in both inputs and outputs"), *Pin.Name);
        ParameterNames.Add(FName(*Pin.Name));
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
			TEXT("error: the editor returned no function graph for '%s'. Asset state was not verified or rolled back; inspect it before retrying."), *Wanted);
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
		if (Blueprint->FunctionGraphs.Contains(Graph))
			return FString::Printf(TEXT("partial: the editor created '%s' instead of '%s', and cleanup left that graph present. Inspect it before retrying; no save was requested."), *Actual, *Wanted);

		return FString::Printf(
			TEXT("unchanged: the engine would not create a function named '%s' and made ")
			TEXT("'%s' instead, because something in the package still holds that name. The ")
			TEXT("graph was removed again; other asset effects were not restored.%s If a function of that name ")
			TEXT("was just removed, the package may not have released the name yet; read the ")
			TEXT("functions back with ReadTurboMCPBlueprintLoad and try again."),
			*Wanted, *Actual, *Extra);
	}

	// Add every signature pin before refreshing callers. Entry pins are outputs in the graph;
	// result pins are inputs. The function's public call node reverses those directions.
	TArray<UK2Node_FunctionEntry*> Entries;
	Graph->GetNodesOfClass(Entries);
	UK2Node_FunctionEntry* Entry = Entries.Num() == 1 ? Entries[0] : nullptr;
	if (Entry == nullptr)
	{
		UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());
		const bool bRemoved = !Blueprint->FunctionGraphs.Contains(Graph);
		return FString::Printf(TEXT("%s: the new function '%s' has no unique entry node; cleanup=%s. Other asset effects were not restored; inspect before retrying."),
			bRemoved ? TEXT("error") : TEXT("partial"), *Wanted,
			bRemoved ? TEXT("graph removed") : TEXT("graph still present"));
	}
	for (const FSignaturePin& Pin : Inputs)
	{
		Entry->Modify();
		UEdGraphPin* CreatedPin = Entry->CreateUserDefinedPin(FName(*Pin.Name), Pin.Type, EGPD_Output, /*bUseUniqueName=*/false);
        if (!CreatedPin || CreatedPin->PinName != FName(*Pin.Name)
            || CreatedPin->Direction != EGPD_Output || CreatedPin->PinType != Pin.Type)
        {
            UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());
            const bool bRemoved = !Blueprint->FunctionGraphs.Contains(Graph);
            return FString::Printf(TEXT("%s: signature pin '%s' could not be created with the requested name/type/direction; new function cleanup=%s"),
                bRemoved ? TEXT("error") : TEXT("partial"), *Pin.Name,
                bRemoved ? TEXT("removed") : TEXT("not_confirmed"));
        }
	}
	UK2Node_FunctionResult* Result = nullptr;
	if (Outputs.Num() > 0)
	{
		Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
		if (Result == nullptr)
		{
			UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());
			const bool bRemoved = !Blueprint->FunctionGraphs.Contains(Graph);
		return FString::Printf(TEXT("%s: the new function '%s' has no result node; cleanup=%s. Other asset effects were not restored; inspect before retrying."),
			bRemoved ? TEXT("error") : TEXT("partial"), *Wanted,
			bRemoved ? TEXT("graph removed") : TEXT("graph still present"));
		}
		for (const FSignaturePin& Pin : Outputs)
		{
			Result->Modify();
			UEdGraphPin* CreatedPin = Result->CreateUserDefinedPin(FName(*Pin.Name), Pin.Type, EGPD_Input, /*bUseUniqueName=*/false);
        if (!CreatedPin || CreatedPin->PinName != FName(*Pin.Name)
            || CreatedPin->Direction != EGPD_Input || CreatedPin->PinType != Pin.Type)
        {
            UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());
            const bool bRemoved = !Blueprint->FunctionGraphs.Contains(Graph);
            return FString::Printf(TEXT("%s: signature pin '%s' could not be created with the requested name/type/direction; new function cleanup=%s"),
                bRemoved ? TEXT("error") : TEXT("partial"), *Pin.Name,
                bRemoved ? TEXT("removed") : TEXT("not_confirmed"));
        }
		}
	}

	TArray<FCallLink> PreviousLinks;
	TArray<UK2Node_CallFunction*> RefreshedCalls;
	CaptureCallLinks(Blueprint, Wanted, PreviousLinks, RefreshedCalls);
	// Publish the completed signature before refreshing only its self-call sites.
    // Unrelated graphs and external same-named calls must not be reconstructed.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    for (UK2Node_CallFunction* Call : RefreshedCalls)
    {
        Call->Modify();
        if (const UEdGraphSchema* Schema = Call->GetGraph()->GetSchema())
            Schema->ReconstructNode(*Call, true);
    }
	TArray<FString> LostLinks;
	for (FCallLink& Link : PreviousLinks)
	{
        UEdGraphPin* Pin = Link.Pin.Get();
        UEdGraphPin* LinkedPin = Link.LinkedPin.Get();
		if (Pin == nullptr || LinkedPin == nullptr || !Pin->LinkedTo.Contains(LinkedPin)
            || !LinkedPin->LinkedTo.Contains(Pin))
		{
			LostLinks.Add(Link.PinName);
		}
	}
	LostLinks.Sort(); LostLinks.SetNum(Algo::Unique(LostLinks));

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

	const bool bComplete = bNowImplemented && LostLinks.IsEmpty();
	FString Out = FString::Printf(TEXT("%s: added function '%s' to %s"),
        bComplete ? TEXT("changed") : TEXT("partial"), *Wanted, *Resolved.ObjectPath);
	Out += FString::Printf(TEXT("\ncreated: graph '%s'"), *Graph->GetName());
	Out += FString::Printf(TEXT("\ncreated inputs: %s"), Inputs.Num() > 0 ? *FString::JoinBy(Inputs, TEXT(", "), [](const FSignaturePin& Pin) { return Pin.Name; }) : TEXT("(none)"));
	Out += FString::Printf(TEXT("\ncreated outputs: %s"), Outputs.Num() > 0 ? *FString::JoinBy(Outputs, TEXT(", "), [](const FSignaturePin& Pin) { return Pin.Name; }) : TEXT("(none)"));
	Out += FString::Printf(TEXT("\nrefreshed call nodes: %d"), RefreshedCalls.Num());
	Out += FString::Printf(TEXT("\nlost links by pin name: %s"), LostLinks.Num() > 0 ? *FString::Join(LostLinks, TEXT(", ")) : TEXT("(none)"));
	Out += FString::Printf(TEXT("\nthe editor now reports it as implemented: %s"),
		bNowImplemented ? TEXT("yes") : TEXT("no"));

	if (!bNowImplemented)
	{
		Out += TEXT("\n; implementation read-back failed; inspect the created graph before compiling or saving.");
	}

    if (!LostLinks.IsEmpty())
        Out += TEXT("\n; caller links were lost during refresh; do not save or retry creation blindly. Inspect the listed pins; rollback was not performed.");

	if (Check.OtherUsers.Num() > 0)
	{
		// Named rather than silently accepted: the caller may be about to build that function,
		// or the call node may be a mistake. Either way it is a fact worth having.
		Out += FString::Printf(TEXT("\n; these already reference this name; resolution and signature compatibility are not verified: %s"),
			*FString::Join(Check.OtherUsers, TEXT(", ")));
	}

	Out += bComplete ? TEXT("\n; the change is in memory and NOT saved. Review the result, then use EditTurboMCPCompile and EditTurboMCPSave as needed.")
        : TEXT("\n; partial change remains in memory; save=not_requested.");
	return Out;
}

FString FTurboMCPFunction::Remove(const FString& InAsset, const FString& InName)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: function removal requires an idle editor outside PIE and active transactions");
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

    int32 Matches = 0;
    for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
        if (Candidate && NameMatches(Candidate->GetFName(), Wanted)) ++Matches;
    if (Matches > 1) return TEXT("unchanged: function name is ambiguous; no graph was removed");

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
        bool bStillPresent = Blueprint->FunctionGraphs.Contains(Graph);
        for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
            if (Candidate && NameMatches(Candidate->GetFName(), Wanted)) bStillPresent = true;
        if (bStillPresent)
            return FString::Printf(TEXT("partial: function removal did not remove '%s'; inspect the graph before retrying; save=not_requested"), *Wanted);
		const FNameCheck AfterCheck = CheckName(Blueprint, Wanted);

		FString Out = FString::Printf(TEXT("changed: removed function '%s' from %s"),
			*Wanted, *Resolved.ObjectPath);
		Out += FString::Printf(TEXT("\nremoved: graph '%s'"), *RemovedGraph);
		Out += TEXT("\n; The entire function graph and its nodes were removed. Callers and widget bindings were not repaired.");
		Out += TEXT("\n; Editor callbacks and skeleton regeneration may have run. This tool does not perform a full compile or create a dedicated undo transaction.");
		Out += FString::Printf(TEXT("\nno function graph holds that name now: %s"),
			TEXT("yes"));

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
				+ FString(TEXT("still be renamed; EditTurboMCPFunctionAdd detects that and refuses "))
				+ FString(TEXT("rather than creating a numbered graph."));
		}

		Out += TEXT("\n; the change is in memory and NOT saved. Review the result, then use EditTurboMCPCompile and EditTurboMCPSave as needed.");
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
