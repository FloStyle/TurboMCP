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
#include "TurboMCPNodeId.h"

// Event overrides are individual nodes; function overrides own a complete graph.
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
			TEXT("this Blueprint has, from ReadTurboMCPOverrides with include=overridden.");
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

			// Delete only the event node, preserving the graph and its other nodes.
			const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "RemoveEventOverride", "TurboMCP remove event override"));
			Blueprint->Modify();
			Graph->Modify();
			Event->Modify();
			const FString RemovedName = TurboMCPNodeId::MakeForNode(Event,true);
            // Capture exact former endpoints before DestroyNode breaks their links.
            // These are retained neighbors, not a claim that their whole subgraph is unused.
            TArray<FString> Detached;
            for(const UEdGraphPin* Pin:Event->Pins) if(Pin)
                for(const UEdGraphPin* Linked:Pin->LinkedTo) if(Linked && Linked->GetOwningNode())
                    Detached.Add(FString::Printf(TEXT("%s:%s -> %s:%s"),*RemovedName,*Pin->PinName.ToString(),
                        *TurboMCPNodeId::MakeForNode(Linked->GetOwningNode(),true),*Linked->PinName.ToString()));
            Detached.Sort();
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
			Out += FString::Printf(TEXT("\ndetached connections: %s"),Detached.IsEmpty()?TEXT("none"):*FString::Join(Detached,TEXT("; ")));
            Out += TEXT("\nOnly the event node was removed. Its former neighbors and their other connections remain. Inspect their remaining uses before removing obsolete body nodes with PreviewTurboMCPGraphRemove and EditTurboMCPGraphRemove. Shared logic must be preserved.");

			// Check registration independently of the node-removal result.
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
			Out += TEXT("\n; Changes are in memory and are not saved. Skeleton regeneration and editor callbacks may have run; a full compile has not been performed.");
			Out += TEXT("\n; Removal used an undo transaction. Review the Blueprint, then use EditTurboMCPCompile and EditTurboMCPSave as needed.");
			if (bStillImplemented)
				Out += TEXT("\n; The editor still lists this override as implemented. Inspect it with ReadTurboMCPOverrides before making further changes; removal is not fully verified.");
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
				return TEXT("unchanged: the named function is not a parent/interface override; use EditTurboMCPFunctionRemove for local functions");
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
			Out += TEXT("\n; The entire override function graph and its contents were removed. Callers were not repaired.");

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
			Out += TEXT("\n; Changes are in memory and are not saved. Skeleton regeneration and editor callbacks may have run; a full compile has not been performed.");
			Out += TEXT("\n; Removal used an undo transaction. Review the Blueprint, then use EditTurboMCPCompile and EditTurboMCPSave as needed.");
			if (bStillImplemented)
				Out += TEXT("\n; The editor still lists this override as implemented. Inspect it with ReadTurboMCPOverrides before making further changes; removal is not fully verified.");
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
