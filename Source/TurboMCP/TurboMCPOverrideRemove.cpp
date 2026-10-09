// Copyright FloStyle. All Rights Reserved.

#include "Tools/TurboMCPToolsets.h"

#include "BlueprintEditorLibrary.h"
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
			if (Event == nullptr || !NameMatches(Event->GetFunctionName(), Wanted))
			{
				continue;
			}

			// THE NODE IS REMOVED, NOT THE GRAPH THAT HOLDS IT.
			//
			// The first version called `RemoveGraph` on the event's graph, which for an event
			// override is the EVENT GRAPH — the one holding every other event and the whole
			// body of the Blueprint. Removing one override that way destroyed the other three
			// and the graph with them, taking the overridden count from 4 to 1. That is the
			// loss `P8` forbids, and it was caught only because the count after the removal was
			// read back and compared with the count before.
			//
			// `Modify`, `DestroyNode` and `MarkPackageDirty` are the three steps the editor's
			// own node deletion performs.
			Graph->Modify();
			const FString RemovedName = Event->GetName();
			const FString FromGraph = Graph->GetName();
			Event->DestroyNode();
			Graph->MarkPackageDirty();

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
			UBlueprintEditorLibrary::RemoveFunctionGraph(Blueprint, Graph->GetFName());

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
