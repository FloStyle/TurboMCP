// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "BlueprintEditorLibrary.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"

/**
 * Removing an override.
 *
 * WHY THIS EXISTS BESIDE `E-OVERRIDE-ADD`
 * ---------------------------------------
 * An Edit that can only create is an Edit that cannot be undone, and `P8` says never leave the
 * user worse off. It also matters for testing: a suite that creates an override and cannot remove
 * it CONSUMES a finite resource. That happened here — the override suite overrode one event per
 * run, ran `BP_NodeTest` out of free events after four runs, and broke two unrelated suites whose
 * assertions counted things on the same asset.
 *
 * It wraps `UBlueprintEditorLibrary::RemoveFunctionGraph` and, for an event, removes the node from
 * the ubergraph, so it removes exactly what `EditTurboMCPOverrideAdd` makes.
 *
 * `E-FUNCTION-REMOVE` in phase 4 will cover member functions as well; this covers overrides, which
 * is what the ADD beside it creates.
 */
namespace
{
	/** True when the two names match, case-insensitively. */
	bool NameMatches(const FName& InA, const FString& InB)
	{
		return InA.ToString().Equals(InB, ESearchCase::IgnoreCase);
	}
}

FString UTurboMCPEditToolset::EditTurboMCPOverrideRemove(
	const FString& Asset, const FString& FunctionOrEvent)
{
	if (!IsInGameThread() || !GEditor || !GEditor->CanTransact()
		|| GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
		return TEXT("unchanged: override removal requires an idle editor with undo available");
	const FString Wanted = FunctionOrEvent.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		return TEXT("error: no function or event given. Pass the name of an override ")
			TEXT("this Blueprint has, from R-OVERRIDES with include=overridden.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it has no overrides."),
			*Resolved.ObjectPath);
	}

	int32 MatchingTargets = 0;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
		if (Graph) for (UEdGraphNode* Node : Graph->Nodes)
			if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
				if (Event->bOverrideFunction && NameMatches(Event->GetFunctionName(), Wanted)) ++MatchingTargets;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		if (Graph && NameMatches(Graph->GetFName(), Wanted)) ++MatchingTargets;
	if (MatchingTargets > 1)
		return TEXT("unchanged: override name matches multiple implementations; resolve the ambiguity before removal");

	// An event override is a NODE in the ubergraph, and a function override is a GRAPH of its
	// own. Both are looked for, because a caller does not know which one they made.
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph == nullptr)
		{
			continue;
		}

		for (int32 Index = Graph->Nodes.Num() - 1; Index >= 0; --Index)
		{
			UK2Node_Event* Event = Cast<UK2Node_Event>(Graph->Nodes[Index]);
			if (Event == nullptr || !Event->bOverrideFunction || !NameMatches(Event->GetFunctionName(), Wanted))
			{
				continue;
			}

			// THE NODE IS REMOVED, NOT THE GRAPH THAT HOLDS IT.
			//
			// CALLING `RemoveGraph` ON THE EVENT'S GRAPH WOULD BE WRONG, because for an event
			// override that is the EVENT GRAPH — the one holding every other event and the whole
			// body of the Blueprint. Removing one override that way destroys the other three
			// and the graph with them, taking the overridden count from 4 to 1. That is the
			// loss `P8` forbids, and it was caught only because the count after the removal was
			// read back and compared with the count before.
			//
			// `Modify`, `DestroyNode` and `MarkPackageDirty` are the three steps the editor's
			// own node deletion performs.
			const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "RemoveEventOverride", "TurboMCP remove event override"));
			Blueprint->Modify();
			Graph->Modify();
			Event->Modify();
			const FString RemovedName = Event->GetName();
			const FString FromGraph = Graph->GetName();
			Event->DestroyNode();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			if (Graph->Nodes.Contains(Event))
				return TEXT("partial: event override remains after removal; inspect the asset or undo before retrying");

			FString Out = FString::Printf(
				TEXT("changed: removed the override of '%s' from %s"),
				*Wanted, *Resolved.ObjectPath);
			Out += FString::Printf(TEXT("\nremoved: %s from graph '%s'"),
				*RemovedName, *FromGraph);

			// THE READ-BACK through the editor's own list, as the Add does.
			bool bStillImplemented = false;
			for (const FBlueprintFunctionInfo& Info
					: UBlueprintEditorLibrary::ListEvents(Blueprint))
			{
				if (Info.Name.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
				{
					bStillImplemented = Info.bIsImplemented;
					break;
				}
			}
			Out += FString::Printf(
				TEXT("\nthe editor now reports it as implemented: %s"),
				bStillImplemented ? TEXT("yes") : TEXT("no"));
			Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
			return Out;
		}
	}

	// A function override is a graph of its own.
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph != nullptr && NameMatches(Graph->GetFName(), Wanted))
		{
			UFunction* OverrideFunction = nullptr;
			if (!FBlueprintEditorUtils::GetOverrideFunctionClass(Blueprint, Graph->GetFName(), &OverrideFunction) || !OverrideFunction)
				return TEXT("unchanged: the named function is not a parent/interface override; use the function removal tool for local functions");
			if (!Graph->bAllowDeletion)
				return TEXT("unchanged: the override graph does not allow deletion");
			const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "RemoveFunctionOverride", "TurboMCP remove function override"));
			Blueprint->Modify();
			Graph->Modify();
			UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());
			if (Blueprint->FunctionGraphs.Contains(Graph))
				return TEXT("partial: override graph remains after removal; inspect the asset or undo before retrying");

			FString Out = FString::Printf(
				TEXT("changed: removed the function override '%s' from %s"),
				*Wanted, *Resolved.ObjectPath);

			bool bStillImplemented = false;
			for (const FBlueprintFunctionInfo& Info
					: UBlueprintEditorLibrary::ListFunctions(Blueprint))
			{
				if (Info.Name.ToString().Equals(Wanted, ESearchCase::IgnoreCase))
				{
					bStillImplemented = Info.bIsImplemented;
					break;
				}
			}
			Out += FString::Printf(
				TEXT("\nthe editor now reports it as implemented: %s"),
				bStillImplemented ? TEXT("yes") : TEXT("no"));
			Out += TEXT("\n; the change is in memory and NOT saved. Call E-SAVE.");
			return Out;
		}
	}

	// Nothing matched. The overridden names are listed, so a caller corrects itself in one step.
	TArray<FString> Overridden;
	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListEvents(Blueprint))
	{
		if (Info.bIsImplemented)
		{
			Overridden.Add(Info.Name.ToString());
		}
	}
	for (const FBlueprintFunctionInfo& Info : UBlueprintEditorLibrary::ListFunctions(Blueprint))
	{
		if (Info.bIsImplemented)
		{
			Overridden.Add(Info.Name.ToString());
		}
	}
	Overridden.Sort();

	return FString::Printf(
		TEXT("unchanged: %s has no override named '%s', so nothing was removed. Its ")
		TEXT("overrides are: %s"),
		*Resolved.ObjectPath, *Wanted,
		Overridden.Num() > 0 ? *FString::Join(Overridden, TEXT(", ")) : TEXT("(none)"));
}
