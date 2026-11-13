// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/ARFilter.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_Variable.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPGraphScratch.h"
#include "K2Node_FunctionEntry.h"

// Resolve exact reader identities and expose pin details for edit planning.
// Type labels describe identity, not guaranteed support by a creation tool.
namespace
{
	/** How many node IDs one resolve call may handle. */
	constexpr int32 MaxNodeIdsPerCall = 25;

	/** How many nodes a find call prints before it caps. */
	constexpr int32 MaxFindResults = 40;

	/** How many pins per node are printed before the list is capped. */
	constexpr int32 MaxPinsPerNode = 40;

	/** How many near-miss node titles are offered when an ID misses. */
	constexpr int32 MaxNearMissTitles = 6;

	/** Explain why a parsed node key cannot identify a node. */
	FString InvalidNodeKeyReason(const FString& InKey)
	{
		const FString Key = InKey.TrimStartAndEnd();
		if (Key.IsEmpty())
		{
			return TEXT("the key is empty");
		}

		for (const TCHAR Ch : Key)
		{
			const bool bIsHex =
				(Ch >= TEXT('0') && Ch <= TEXT('9'))
				|| (Ch >= TEXT('a') && Ch <= TEXT('f'))
				|| (Ch >= TEXT('A') && Ch <= TEXT('F'));
			if (!bIsHex)
			{
				return FString::Printf(TEXT("character '%c' is not hexadecimal"), Ch);
			}
		}

		return TEXT("the key has fewer than six hexadecimal characters");
	}

	/**
	 * The reference the engine's tools accept for a node.
	 *
	 * The engine serialises a UObject pointer as `{"refPath": "..."}`, so a caller
	 * can paste this straight into another tool.
	 */
	FString NodeReference(const UEdGraphNode* InNode)
	{
		return FString::Printf(TEXT("{\"refPath\": \"%s\"}"), *InNode->GetPathName());
	}

	/**
	 * One node's pins, by name, with direction and the index within that
	 * direction.
	 *
	 * The index is per direction, which is what the engine's pin addressing uses:
	 * an input pin index and an output pin index are counted separately.
	 */
	FString DescribePins(const UEdGraphNode* InNode)
	{
		FString Lines;
		int32 InputIndex = 0;
		int32 OutputIndex = 0;
		int32 Shown = 0;
		int32 TotalPins = 0;
		for (const UEdGraphPin* Pin : InNode->Pins) if (Pin) ++TotalPins;
		auto SingleLine = [](FString Value)
		{
			Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
			Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			Value.ReplaceInline(TEXT("\t"), TEXT("\\t"));
			return Value;
		};


		for (int32 RawIndex = 0; RawIndex < InNode->Pins.Num(); ++RawIndex)
		{
			const UEdGraphPin* Pin = InNode->Pins[RawIndex];
			if (Pin == nullptr)
			{
				continue;
			}

			if (Shown >= MaxPinsPerNode)
			{
				Lines += FString::Printf(TEXT("\n      (%d more pin(s) not shown)"),
					TotalPins - Shown);
				break;
			}

			++Shown;
			const bool bIsInput = Pin->Direction == EGPD_Input;
			const int32 Index = bIsInput ? InputIndex++ : OutputIndex++;

			// The real pin name, as `2.1.5` requires: when the display name and
			// the real name differ, the real name is printed.
			const FString Direction = bIsInput ? TEXT("in ") : TEXT("out");

			// Addressing indices are distinct; defaults below are shown only for unlinked pins.
			Lines += FString::Printf(TEXT("\n      %s direction-index=%d array-index=%d %s [pin-guid=%s]"),
				*Direction, Index, RawIndex, *Pin->PinName.ToString(), *Pin->PinId.ToString());

            const TCHAR* ContainerName = TEXT("unknown");
            switch (Pin->PinType.ContainerType)
            {
            case EPinContainerType::None: ContainerName = TEXT("scalar"); break;
            case EPinContainerType::Array: ContainerName = TEXT("array"); break;
            case EPinContainerType::Set: ContainerName = TEXT("set"); break;
            case EPinContainerType::Map: ContainerName = TEXT("map"); break;
            }
            Lines += FString::Printf(TEXT(" [category=%s subcategory=%s container=%d container-name=%s]"),
                *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString(),
                static_cast<int32>(Pin->PinType.ContainerType), ContainerName);
            if (const UObject* TypeObject = Pin->PinType.PinSubCategoryObject.Get())
                Lines += FString::Printf(TEXT(" type=%s"), *TypeObject->GetPathName());
            if (Pin->PinType.bIsReference) Lines += TEXT(" reference");
            if (Pin->PinType.bIsConst) Lines += TEXT(" const");
            if (Pin->bHidden) Lines += TEXT(" hidden");
            if (Pin->bOrphanedPin) Lines += TEXT(" orphaned");
            if (Pin->bDefaultValueIsReadOnly) Lines += TEXT(" default-read-only");
            if (Pin->bDefaultValueIsIgnored) Lines += TEXT(" default-ignored");
            if (Pin->LinkedTo.Num() > 0) Lines += TEXT(" connected-default-inactive");

            if (Pin->PinType.ContainerType == EPinContainerType::Map)
            {
                const FEdGraphTerminalType& ValueType = Pin->PinType.PinValueType;
                Lines += FString::Printf(TEXT(" map-value=[category=%s subcategory=%s]"),
                    *ValueType.TerminalCategory.ToString(), *ValueType.TerminalSubCategory.ToString());
                if (const UObject* ValueObject = ValueType.TerminalSubCategoryObject.Get())
                    Lines += FString::Printf(TEXT(" map-value-type=%s"), *ValueObject->GetPathName());
            }
			const bool bLinked = Pin->LinkedTo.Num() > 0;
			if (bLinked)
			{
				Lines += FString::Printf(TEXT("  -> %d link(s)"), Pin->LinkedTo.Num());
                for(const UEdGraphPin* Other:Pin->LinkedTo)
                {
                    if(!Other || !Other->GetOwningNodeUnchecked()) { Lines+=TEXT(" [invalid endpoint]"); continue; }
                    Lines+=FString::Printf(TEXT(" [%s:%s %s]"),
                        *TurboMCPNodeId::MakeForNode(Other->GetOwningNodeUnchecked(),true),
                        *Other->PinName.ToString(),Other->Direction==EGPD_Input?TEXT("input"):TEXT("output"));
                }
			}
            else if (Pin->DefaultObject)
                Lines += FString::Printf(TEXT("  = object:%s"), *Pin->DefaultObject->GetPathName());
            else if (!Pin->DefaultTextValue.IsEmpty())
                Lines += FString::Printf(TEXT("  = text:%s"), *SingleLine(Pin->DefaultTextValue.ToString()));
			else if (!Pin->DefaultValue.IsEmpty())
			{
				Lines += FString::Printf(TEXT("  = %s"), *SingleLine(Pin->DefaultValue));
			}
		}

		if (Lines.IsEmpty())
		{
			// P1: a node with no pins says so rather than leaving a gap.
			return TEXT("\n      no pins");
		}
		return Lines;
	}

	/** Match stored node metadata, with explicit any/all semantics and optional input literals. */
	bool NodeMatchesWords(const UEdGraphNode* InNode, const TArray<FString>& InWords, bool bAny = false, bool bIncludeLiterals = false)
	{
		// Every field the spec lists is searched: the title, the called function,
		// the variable, the node class, and the pin names.
		TArray<FString> Fields;
		Fields.Add(TurboMCPNodeId::GetNodeTitle(InNode));
		Fields.Add(InNode->GetClass()->GetName());
        Fields.Add(InNode->NodeComment);

		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode))
		{
			Fields.Add(Call->FunctionReference.GetMemberName().ToString());
		}
		if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(InNode))
		{
			Fields.Add(Variable->GetVarName().ToString());
		}

        if (const UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(InNode))
            Fields.Add(Delegate->GetFunctionName().ToString());
        if (const UK2Node_BaseMCDelegate* DelegateMember = Cast<UK2Node_BaseMCDelegate>(InNode))
            Fields.Add(DelegateMember->GetPropertyName().ToString());

		FString PinNames;
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr)
			{
				PinNames += Pin->PinName.ToString();
				PinNames += TEXT(" ");
                if (bIncludeLiterals && Pin->Direction == EGPD_Input && Pin->LinkedTo.IsEmpty())
                {
                    Fields.Add(Pin->DefaultValue);
                    Fields.Add(Pin->DefaultTextValue.ToString());
                    if (Pin->DefaultObject) Fields.Add(Pin->DefaultObject->GetPathName());
                }
			}
		}
		Fields.Add(PinNames);

		// All is the legacy narrowing behavior. Any explicitly widens the search.
		for (const FString& Word : InWords)
		{
			bool bFound = false;
			for (const FString& Field : Fields)
			{
				if (Field.Contains(Word, ESearchCase::IgnoreCase))
				{
					bFound = true;
					break;
				}
			}
            if (bAny && bFound) return true;
            if (!bAny && !bFound) return false;
		}
		return !bAny;
	}

	/** Every graph of a Blueprint, or just one when a name was given. */
	void CollectGraphs(UBlueprint* InBlueprint, const FString& InGraphName,
		TArray<UEdGraph*>& OutGraphs)
	{
		if (!InGraphName.IsEmpty())
		{
			if (UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName))
			{
				OutGraphs.Add(Graph);
			}
			return;
		}

		FTurboMCPBlueprintRead::CollectGraphs(InBlueprint, OutGraphs);
	}

	/** The real graph names, for an error that has to name what does exist. */
	FString ListGraphNames(UBlueprint* InBlueprint)
	{
		TArray<UEdGraph*> Graphs;
		CollectGraphs(InBlueprint, FString(), Graphs);

		FString Names;
		for (const UEdGraph* Graph : Graphs)
		{
			const FString Label = FString::Printf(TEXT("%s [%s]"), *Graph->GetName(), *Graph->GetPathName());
			Names += Names.IsEmpty() ? Label : TEXT(", ") + Label;
		}
		return Names.IsEmpty() ? TEXT("(none)") : Names;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPNodeResolve(const FString& NodeIds)
{
	const TArray<FString> Parts = FTurboMCPAssetResolver::SplitList(NodeIds);
	if (Parts.Num() == 0)
	{
		return TEXT("error: no node ID given. Pass one or more IDs separated by ")
			TEXT("commas or pipes, in the form AssetName:GraphName:NodeKey, ")
			TEXT("for example WBP_ShopCard:RefreshPrice:3fa91c.");
	}
	if (Parts.Num() > MaxNodeIdsPerCall)
	{
		return FString::Printf(
			TEXT("error: %d IDs given, and one call resolves at most %d. Split the list."),
			Parts.Num(), MaxNodeIdsPerCall);
	}

	FString Result = FString::Printf(TEXT("read: resolving %d node ID(s)"), Parts.Num());

	int32 ResolvedCount = 0;
	int32 RefusedCount = 0;
	FString Lines;
	FString Errors;

	// The Blueprint of the previous ID is kept, because IDs in one list usually
	// share an asset and loading is the expensive part.
	UBlueprint* LastBlueprint = nullptr;
	FString LastAssetName;

	for (const FString& Part : Parts)
	{
		const TurboMCPNodeId::FParsed Parsed = TurboMCPNodeId::Parse(Part);

		if (!Parsed.IsComplete())
		{
			// Name what is missing rather than reporting a malformed ID. P7 wants
			// the error to lead to the next call.
			Errors += FString::Printf(
				TEXT("\nerror: '%s' is not a complete node ID. All three parts are needed: ")
				TEXT("<AssetName>:<GraphName>:<NodeKey>."),
				*Part);
			++RefusedCount;
			continue;
		}

		if (TurboMCPNodeId::NormaliseKey(Parsed.NodeKey).IsEmpty())
		{
			Errors += FString::Printf(
				TEXT("\nerror: node ID '%s' has an invalid key: %s. Pass at least six hexadecimal characters."),
				*Part, *InvalidNodeKeyReason(Parsed.NodeKey));
			++RefusedCount;
			continue;
		}

		// The asset is resolved through the shared resolver, so every path form
		// works and an ambiguous name is refused with its candidates (3.2).
		FTurboMCPAssetResolver::FResolved AssetResolved;
		FString AssetError;

		if (Parsed.bIsLongForm)
		{
			AssetError = FTurboMCPAssetResolver::Resolve(Parsed.AssetName, AssetResolved);
		}
		else
		{
			// The ID carries the asset's own name, not a path, so the exact-name
			// search runs first. It can return several, and 3.2 requires all of
			// them to be listed rather than one being picked.
			TArray<FTurboMCPAssetResolver::FResolved> Candidates;
			FTurboMCPAssetResolver::FindAllByName(Parsed.AssetName, Candidates,
				/*bContainsMatch=*/false);

			if (Candidates.Num() == 0)
			{
				AssetError = FString::Printf(
					TEXT("error: no asset is named '%s'."), *Parsed.AssetName);
			}
			else if (Candidates.Num() > 1)
			{
				AssetError = FString::Printf(
					TEXT("error: '%s' fits %d assets, so it is ambiguous. ")
					TEXT("Use the long form with the full path:"),
					*Parsed.AssetName, Candidates.Num());
				for (const FTurboMCPAssetResolver::FResolved& Candidate : Candidates)
				{
					AssetError += FString::Printf(TEXT("\n      %s"), *Candidate.ObjectPath);
				}
			}
			else
			{
				AssetResolved = Candidates[0];
			}
		}

		if (!AssetError.IsEmpty())
		{
			Errors += TEXT("\n") + AssetError;
			++RefusedCount;
			continue;
		}

		// One load per asset, reused while the list stays on the same asset.
		if (LastBlueprint == nullptr || LastAssetName != AssetResolved.ObjectPath)
		{
			LastBlueprint = Cast<UBlueprint>(
				LoadObject<UObject>(nullptr, *AssetResolved.ObjectPath));
			LastAssetName = AssetResolved.ObjectPath;
		}

		if (LastBlueprint == nullptr)
		{
			// The asset exists but is not a Blueprint. Its graphs cannot be read,
			// and saying which class it is prevents a second identical attempt.
			Errors += FString::Printf(
				TEXT("\nerror: %s is not a Blueprint, so it has no graphs to resolve a node in."),
				*AssetResolved.ObjectPath);
			++RefusedCount;
			continue;
		}

		UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(LastBlueprint, Parsed.GraphName);
		if (Graph == nullptr)
		{
			Errors += FString::Printf(
				TEXT("\nerror: %s has no graph named '%s'. Its graphs are: %s"),
				*AssetResolved.ObjectPath, *Parsed.GraphName, *ListGraphNames(LastBlueprint));
			++RefusedCount;
			continue;
		}

		TArray<UEdGraphNode*> Matches;
		const int32 MatchCount = TurboMCPNodeId::FindNodesByKey(Graph, Parsed.NodeKey, Matches);

		if (MatchCount == 0)
		{
			// 3.2: an ID whose node is gone says so, and lists the nodes in that
			// graph whose key is close, so the caller can find the survivor of a
			// rebuild without guessing.
			FString NearMisses;
			int32 Offered = 0;
			const FString Wanted = Parsed.NodeKey.ToLower();
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node == nullptr)
				{
					continue;
				}
				if (Offered >= MaxNearMissTitles)
				{
					break;
				}
				const FString Key = Node->NodeGuid.ToString(EGuidFormats::DigitsLower);
				// Matching prefixes are navigation suggestions only, not identity evidence.
				if (Key.Left(2) == Wanted.Left(2))
				{
					NearMisses += FString::Printf(TEXT("\n      %s  %s"),
						*TurboMCPNodeId::Make(Parsed.AssetName, Parsed.GraphName, Node->NodeGuid),
						*TurboMCPNodeId::GetNodeTitle(Node));
					++Offered;
				}
			}

			Errors += FString::Printf(
				TEXT("\nerror: no node in %s:%s has the key %s. ")
				TEXT("The node may have been deleted, or the graph rebuilt. Use ReadTurboMCPNodeFind to obtain a fresh ID."),
				*AssetResolved.AssetName, *Parsed.GraphName, *Parsed.NodeKey);
			if (!NearMisses.IsEmpty())
			{
				Errors += FString::Printf(
					TEXT("\n    Similar key prefixes (suggestions only, not replacement identities); use ReadTurboMCPNodeFind to identify the intended node:%s"),
					*NearMisses);
			}
			++RefusedCount;
			continue;
		}

		if (MatchCount > 1)
		{
			// 3.1 says to widen the key when two nodes share the first six digits.
			// Handing back one of them would be a guess.
			Errors += FString::Printf(
				TEXT("\nerror: the key %s fits %d nodes in %s, so it is ambiguous. ")
				TEXT("Use more digits:"),
				*Parsed.NodeKey, MatchCount, *Parsed.GraphName);
			for (const UEdGraphNode* Node : Matches)
			{
				Errors += FString::Printf(TEXT("\n      %s"),
					*TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
			}
			++RefusedCount;
			continue;
		}

		const UEdGraphNode* Node = Matches[0];

		// Print the long form, because 3.2 requires every reader to start its
		// output with the long asset path.
		Lines += FString::Printf(TEXT("\n%s"),
			*TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		Lines += FString::Printf(TEXT("\n    reference: %s"), *NodeReference(Node));
		Lines += FString::Printf(TEXT("\n    title: %s"),
			*TurboMCPNodeId::GetNodeTitle(Node));
		Lines += FString::Printf(TEXT("\n    type ID: %s  (TurboMCP identity; custom/operator writer support varies, and fallback labels may be localized)"),
			*TurboMCPNodeId::GetNodeTypeId(Node));
		Lines += FString::Printf(TEXT("\n    class: %s"), *Node->GetClass()->GetName());
		Lines += FString::Printf(TEXT("\n    graph: %s"), *Graph->GetName());

		const int32 PinCount = Node->Pins.Num();
		Lines += FString::Printf(TEXT("\n    pins: %d"), PinCount);
		Lines += DescribePins(Node);

		++ResolvedCount;
	}

	Result += FString::Printf(TEXT(": %d resolved, %d refused"), ResolvedCount, RefusedCount);
	Result += Lines;
	Result += Errors;
	Result += TEXT("\nNo edit, compile or save was requested. Resolving may load assets. Pin indices describe the current graph; obtain fresh IDs after structural changes.");

	if (ResolvedCount == 0 && RefusedCount == 0)
	{
		// Not reachable with a non-empty list, but a silent empty result would
		// break P1 if it ever were.
		Result += TEXT("\nNothing was resolved.");
	}

	return Result;
}

namespace
{
// Loading a Blueprint can load its hard dependencies. Refuse known map/redirector
// packages and incomplete dependency metadata; arbitrary native PostLoad hooks are
// not sandboxed by this preflight.
FString NodeSearchLoadBlocker(IAssetRegistry& Registry, FName Seed)
{
    using namespace UE::AssetRegistry;
    TArray<FName> Pending{Seed};
    TSet<FName> Seen;
    Seen.Add(Seed);
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(Pending[Index], Assets, true, false);
        if (Assets.ContainsByPredicate([](const FAssetData& Entry)
            { return Entry.HasAnyPackageFlags(PKG_ContainsMap) || Entry.IsRedirector(); }))
            return TEXT("map or redirector dependency");
        TArray<FName> Dependencies;
        if (!Registry.GetDependencies(Pending[Index], Dependencies, EDependencyCategory::Package, EDependencyQuery::Hard))
            return TEXT("dependency metadata unavailable");
        for (FName Dependency : Dependencies)
        {
            if (Dependency.ToString().StartsWith(TEXT("/Script/")) || Seen.Contains(Dependency)) continue;
            if (Pending.Num() >= 2048) return TEXT("dependency inspection budget exceeded");
            Seen.Add(Dependency);
            Pending.Add(Dependency);
        }
    }
    return FString();
}
}

FString UTurboMCPReadToolset::ReadTurboMCPNodeFind(
    const FString& Asset, const FString& Words, const FString& GraphName,
    const FString& MatchMode, bool IncludeLiterals, const FString& NodeClass,
    const FString& Scope, bool Recursive, int32 PageSize, const FString& Cursor, bool CountOnly, const FString& Format)
{
    const bool bJson = Format == TEXT("json");
    auto SerializeJson = [](const TSharedRef<FJsonObject>& Object)
    {
        FString Output;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
        FJsonSerializer::Serialize(Object, Writer);
        return Output;
    };
    // Preserve the existing text error paths while giving JSON callers one envelope.
    bool bJsonSuccess = false;
    auto Search = [&]() -> FString
    {
    if (Format != TEXT("text") && !bJson)
        return TEXT("error: Format must be text or json.");
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: node search requires an idle editor outside play and transactions.");
    if (MatchMode != TEXT("all") && MatchMode != TEXT("any"))
        return TEXT("error: MatchMode must be all or any.");
    if (Scope != TEXT("asset") && Scope != TEXT("folder") && Scope != TEXT("project"))
        return TEXT("error: Scope must be asset, folder or project.");
    if (PageSize < 1 || PageSize > 40)
        return TEXT("error: PageSize must be between 1 and 40.");
    FString SearchWords = Words;
    for (int32 Index = 0; Index < SearchWords.Len(); ++Index)
        if (FChar::IsWhitespace(SearchWords[Index]) || SearchWords[Index] == TEXT('|')) SearchWords[Index] = TEXT(',');
    const TArray<FString> WordList = FTurboMCPAssetResolver::SplitList(SearchWords);
    if (WordList.IsEmpty()) return TEXT("error: no words given. Pass words separated by whitespace, commas or pipes.");
    if (Words.Len() > 2048 || NodeClass.Len() > 512 || GraphName.Len() > 512)
        return TEXT("error: search text exceeds the supported input length.");
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets()) return TEXT("error: Asset Registry discovery is still running. Retry when discovery finishes.");
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    TArray<FAssetData> Candidates;
    if (Scope == TEXT("asset"))
    {
        if (Asset.IsEmpty()) return TEXT("error: no asset given. Pass a Blueprint path.");
        FTurboMCPAssetResolver::FResolved Resolved;
        const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
        if (!Error.IsEmpty()) return Error;
        Filter.SoftObjectPaths.Add(FSoftObjectPath(Resolved.ObjectPath));
    }
    else if (Scope == TEXT("folder"))
    {
        if (!Asset.StartsWith(TEXT("/")) || Asset.EndsWith(TEXT("/")) ||
            !FPackageName::IsValidLongPackageName(Asset + TEXT("/_Search"), true))
            return TEXT("error: folder scope requires a content folder such as /Game/UI, without a trailing slash.");
        Filter.PackagePaths.Add(FName(*Asset));
        Filter.bRecursivePaths = Recursive;
    }
    else
    {
        if (!Asset.IsEmpty()) return TEXT("error: leave Asset empty for project scope, or use folder scope.");
        Filter.PackagePaths.Add(TEXT("/Game"));
        for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
            if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project && Plugin->CanContainContent())
            {
                FString Mount = Plugin->GetMountedAssetPath();
                Mount.RemoveFromEnd(TEXT("/"));
                Filter.PackagePaths.AddUnique(FName(*Mount));
            }
        Filter.bRecursivePaths = true;
    }
    Registry.GetAssets(Filter, Candidates);
    // Registry discovery does not include every unsaved Blueprint. Reuse ready
    // in-memory assets without loading dependencies or registering synthetic data.
    auto IsReadableLoadedBlueprint = [](UBlueprint* Blueprint)
    {
        return IsValid(Blueprint) && Blueprint->IsAsset()
            && !Blueprint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
            && !Blueprint->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap);
    };
    TMap<FString, UBlueprint*> LoadedBlueprints;
    TSet<FString> CandidatePaths;
    for (const FAssetData& Candidate : Candidates) CandidatePaths.Add(Candidate.GetObjectPathString());
    for (TObjectIterator<UBlueprint> It; It; ++It)
    {
        UBlueprint* Blueprint = *It;
        if (!IsReadableLoadedBlueprint(Blueprint)) continue;
        const FAssetData LoadedAsset(Blueprint);
        const FString ObjectPath = LoadedAsset.GetObjectPathString();
        bool bInScope = false;
        if (Scope == TEXT("asset"))
        {
            bInScope = Filter.SoftObjectPaths.Contains(FSoftObjectPath(ObjectPath));
        }
        else
        {
            const FString PackagePath = LoadedAsset.PackagePath.ToString();
            for (FName Root : Filter.PackagePaths)
            {
                const FString RootPath = Root.ToString();
                if (PackagePath.Equals(RootPath, ESearchCase::IgnoreCase)
                    || (Filter.bRecursivePaths && PackagePath.StartsWith(RootPath + TEXT("/"), ESearchCase::IgnoreCase)))
                {
                    bInScope = true;
                    break;
                }
            }
        }
        if (!bInScope) continue;
        LoadedBlueprints.Add(ObjectPath, Blueprint);
        if (!CandidatePaths.Contains(ObjectPath))
        {
            CandidatePaths.Add(ObjectPath);
            Candidates.Add(LoadedAsset);
        }
    }
    Candidates.Sort([](const FAssetData& A, const FAssetData& B)
        { return A.GetObjectPathString() < B.GetObjectPathString(); });
    if (Scope == TEXT("asset") && Candidates.IsEmpty()) return TEXT("error: the selected asset is not a registered or fully loaded Blueprint.");

    // Bind continuation to the query and registry candidate set. Graph edits between
    // calls still require restarting: this is an index cursor, not a frozen snapshot.
    FString Fingerprint = Scope + TEXT("|") + Asset + TEXT("|") + Words + TEXT("|") + GraphName
        + TEXT("|") + MatchMode + TEXT("|") + NodeClass + FString::Printf(TEXT("|%d|%d|%d"), IncludeLiterals, Recursive, CountOnly);
    for (const FAssetData& Candidate : Candidates) Fingerprint += TEXT("|") + Candidate.GetObjectPathString();
    const FString Revision = FMD5::HashAnsiString(*Fingerprint);
    int32 AssetIndex = 0, NodeIndex = 0;
    int64 MatchedBefore = 0, SkippedBefore = 0;
    if (!Cursor.IsEmpty() && Cursor != TEXT("first"))
    {
        TArray<FString> Parts;
        Cursor.ParseIntoArray(Parts, TEXT(":"), false);
        if (Cursor.Len() > 160 || Parts.Num() != 5 || Parts[4] != Revision || !LexTryParseString(AssetIndex, *Parts[0])
            || !LexTryParseString(NodeIndex, *Parts[1]) || !LexTryParseString(MatchedBefore, *Parts[2])
            || !LexTryParseString(SkippedBefore, *Parts[3]) || AssetIndex < 0 || AssetIndex >= Candidates.Num()
            || NodeIndex < 0 || MatchedBefore < 0 || MatchedBefore > MAX_int32 || SkippedBefore < 0 || SkippedBefore > AssetIndex)
            return TEXT("error: invalid or stale Cursor. Restart with an empty Cursor and unchanged search arguments.");
    }
    int32 Visited = 0, Searched = 0, Matched = 0, Skipped = 0, Characters = 0;
    TArray<FString> Lines, Refusals;
    TArray<TSharedPtr<FJsonValue>> JsonHits, JsonSkips;
    auto AddSkip = [&](const FString& Path, const FString& Reason)
    {
        Refusals.Add(Path + TEXT(": ") + Reason);
        if (bJson)
        {
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("asset"), Path);
            Row->SetStringField(TEXT("reason"), Reason);
            JsonSkips.Add(MakeShared<FJsonValueObject>(Row));
        }
    };
    bool bStop = false;
    while (AssetIndex < Candidates.Num() && Visited < 16 && !bStop)
    {
        const FAssetData& Candidate = Candidates[AssetIndex];
        ++Visited;
        FString Blocker;
        UBlueprint* Blueprint = LoadedBlueprints.FindRef(Candidate.GetObjectPathString());
        if (!Blueprint)
        {
            Blocker = NodeSearchLoadBlocker(Registry, Candidate.PackageName);
            if (Blocker.IsEmpty())
            {
                Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr,
                    *Candidate.GetObjectPathString(), nullptr, LOAD_DisableCompileOnLoad | LOAD_NoRedirects | LOAD_NoWarn));
                if (!Blueprint) Blocker = TEXT("Blueprint load failed");
            }
        }
        if (!Blocker.IsEmpty())
        {
            AddSkip(Candidate.GetObjectPathString(), Blocker);
            ++Skipped; ++AssetIndex; NodeIndex = 0; continue;
        }
        TArray<UEdGraph*> Graphs;
        CollectGraphs(Blueprint, GraphName, Graphs);
        if (!GraphName.IsEmpty())
        {
            int32 GraphMatches = 0;
            TurboMCPNodeId::FindGraphByName(Blueprint, GraphName, GraphMatches);
            if (GraphMatches > 1 || (Graphs.IsEmpty() && Scope == TEXT("asset")))
            {
                if (Scope == TEXT("asset"))
                    return FString::Printf(TEXT("error: graph '%s' is missing or ambiguous in %s. Graphs: %s"),
                        *GraphName, *Candidate.GetObjectPathString(), *ListGraphNames(Blueprint));
                AddSkip(Candidate.GetObjectPathString(), TEXT("graph name is ambiguous"));
                ++Skipped; ++AssetIndex; NodeIndex = 0; continue;
            }
        }
        Graphs.Sort([](const UEdGraph& A, const UEdGraph& B) { return A.GetPathName() < B.GetPathName(); });
        TArray<UEdGraphNode*> Nodes;
        for (UEdGraph* Graph : Graphs) for (UEdGraphNode* Node : Graph->Nodes) if (Node) Nodes.Add(Node);
        if (NodeIndex > Nodes.Num()) return TEXT("error: graph contents changed during pagination. Restart with an empty Cursor.");
        while (NodeIndex < Nodes.Num())
        {
            if (Searched >= 10000) { bStop = true; break; }
            UEdGraphNode* Node = Nodes[NodeIndex];
            const bool bMatches = (NodeClass.IsEmpty() || NodeClass == TEXT("any") || Node->GetClass()->GetName() == NodeClass || Node->GetClass()->GetPathName() == NodeClass)
                && NodeMatchesWords(Node, WordList, MatchMode == TEXT("any"), IncludeLiterals);
            FString Line;
            if (bMatches && !CountOnly)
            {
                Line = FString::Printf(TEXT("%s\n    title: %s\n    type ID: %s\n    class: %s"),
                    *TurboMCPNodeId::MakeForNode(Node, true), *TurboMCPNodeId::GetNodeTitle(Node).Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n")),
                    *TurboMCPNodeId::GetNodeTypeId(Node).Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n")), *Node->GetClass()->GetName());
                if (Lines.Num() >= PageSize || (Characters + Line.Len() > 24000 && !Lines.IsEmpty())) { bStop = true; break; }
                // Keep a pathological display label from exceeding the response budget;
                // the exact node identity remains available on its own line.
                if (Line.Len() > 24000) Line = TurboMCPNodeId::MakeForNode(Node, true) + TEXT("\n    display fields omitted: oversized labels");
                Characters += Line.Len(); Lines.Add(Line);
                if (bJson)
                {
                    TSharedRef<FJsonObject> Hit = MakeShared<FJsonObject>();
                    Hit->SetStringField(TEXT("asset"), Candidate.GetObjectPathString());
                    Hit->SetStringField(TEXT("graph"), Node->GetGraph()->GetName());
                    Hit->SetStringField(TEXT("node_id"), TurboMCPNodeId::MakeForNode(Node, true));
                    const bool bOmitted = Line.Contains(TEXT("display fields omitted: oversized labels"));
                    Hit->SetBoolField(TEXT("display_fields_omitted"), bOmitted);
                    if (!bOmitted)
                    {
                        Hit->SetStringField(TEXT("title"), TurboMCPNodeId::GetNodeTitle(Node));
                        Hit->SetStringField(TEXT("type_id"), TurboMCPNodeId::GetNodeTypeId(Node));
                        Hit->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
                    }
                    JsonHits.Add(MakeShared<FJsonValueObject>(Hit));
                }
            }
            ++Searched; ++NodeIndex;
            if (bMatches) ++Matched;
        }
        if (NodeIndex == Nodes.Num()) { ++AssetIndex; NodeIndex = 0; }
    }
    const bool bScanFinished = AssetIndex == Candidates.Num();
    const int64 TotalMatched = static_cast<int64>(MatchedBefore) + Matched;
    const int64 TotalSkipped = static_cast<int64>(SkippedBefore) + Skipped;
    const FString ScopeNote = TEXT("Search covers local Blueprint graphs, not Data Asset properties, inherited assets outside the selected scope, native implementations or runtime dispatch. A zero match count does not prove an asset unused. Use ReadTurboMCPRefsBatch, then ReadTurboMCPDataAssetProperty for Data Asset references.");
    if (bJson)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("schema_version"), 1);
        Object->SetStringField(TEXT("status"), TEXT("ok"));
        Object->SetStringField(TEXT("scope"), Scope);
        TSharedRef<FJsonObject> Query = MakeShared<FJsonObject>();
        Query->SetStringField(TEXT("asset"), Asset);
        Query->SetStringField(TEXT("words"), Words);
        Query->SetStringField(TEXT("graph_name"), GraphName);
        Query->SetStringField(TEXT("match_mode"), MatchMode);
        Query->SetStringField(TEXT("node_class"), NodeClass);
        Query->SetBoolField(TEXT("include_literals"), IncludeLiterals);
        Query->SetBoolField(TEXT("recursive"), Recursive);
        Query->SetBoolField(TEXT("count_only"), CountOnly);
        Object->SetObjectField(TEXT("query"), Query);
        Object->SetArrayField(TEXT("hits"), JsonHits);
        Object->SetArrayField(TEXT("skipped"), JsonSkips);
        Object->SetNumberField(TEXT("eligible_assets"), Candidates.Num());
        Object->SetNumberField(TEXT("nodes_searched_this_page"), Searched);
        Object->SetNumberField(TEXT("matches_this_page"), Matched);
        Object->SetNumberField(TEXT("matches_so_far"), TotalMatched);
        Object->SetNumberField(TEXT("skipped_assets_so_far"), TotalSkipped);
        Object->SetBoolField(TEXT("scan_finished"), bScanFinished);
        Object->SetBoolField(TEXT("complete"), bScanFinished && TotalSkipped == 0);
        if (bScanFinished) Object->SetField(TEXT("next_cursor"), MakeShared<FJsonValueNull>());
        else Object->SetStringField(TEXT("next_cursor"), FString::Printf(TEXT("%d:%d:%lld:%lld:%s"), AssetIndex, NodeIndex, TotalMatched, TotalSkipped, *Revision));
        Object->SetStringField(TEXT("scope_note"), ScopeNote);
        Object->SetStringField(TEXT("pagination_note"), TEXT("Follow every cursor without editing assets. matches_so_far is final only when scan_finished is true. Skips prevent exhaustive conclusions. This is not a frozen snapshot."));
        Object->SetStringField(TEXT("effects"), TEXT("No edit, compile, save, checkout, map load or PIE was requested. Loading may execute native load hooks; this is not a sandbox."));
        bJsonSuccess = true;
        return SerializeJson(Object);
    }
    FString Result = FString::Printf(TEXT("read: nodes matching %s words [%s]\ncoverage: scan_finished=%s; complete=%s; eligible_assets=%d; next_asset_index=%d; nodes_searched_this_page=%d; matches_this_page=%d; matches_so_far=%lld; skipped_assets_so_far=%lld; results shown=%d\n"),
        *MatchMode, *FString::Join(WordList, TEXT(", ")), bScanFinished ? TEXT("true") : TEXT("false"),
        bScanFinished && TotalSkipped == 0 ? TEXT("true") : TEXT("false"), Candidates.Num(), AssetIndex, Searched, Matched, TotalMatched, TotalSkipped, Lines.Num());
    Result += ScopeNote + TEXT("\n");
    Result += TEXT("Scope includes nested local graphs; inherited Blueprint assets must match the asset scope separately; native implementations and runtime dispatch are not searched. Null node entries are skipped.\n");
    Result += FString::Printf(TEXT("filters: literals=%s; exact node class=%s; count_only=%s\n"), IncludeLiterals ? TEXT("included") : TEXT("excluded"), NodeClass.IsEmpty() ? TEXT("any") : *NodeClass, CountOnly ? TEXT("true") : TEXT("false"));
    if (Matched == 0) Result += bScanFinished && TotalMatched == 0 && TotalSkipped == 0 ? TEXT("Nothing matched in the completed scan. Try fewer or broader terms.\n") : TEXT("No matches on this page; this is not proof of absence.\n");
    for (const FString& Line : Lines) Result += Line + TEXT("\n");
    for (const FString& Refusal : Refusals) Result += TEXT("skipped: ") + Refusal + TEXT("\n");
    Result += bScanFinished ? TEXT("next_cursor: none\n") : FString::Printf(TEXT("next_cursor: %d:%d:%lld:%lld:%s\n"), AssetIndex, NodeIndex, TotalMatched, TotalSkipped, *Revision);
    Result += TEXT("Follow every cursor without editing assets. matches_so_far is a final total only when scan_finished=true; skipped assets prevent exhaustive conclusions. No edit, compile, save, checkout, map load or PIE was requested. Asset loading may execute native load hooks; this is not a sandbox.");
    return Result;
    };
    const FString Output = Search();
    if (!bJson || bJsonSuccess) return Output;
    TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetNumberField(TEXT("schema_version"), 1);
    Error->SetStringField(TEXT("status"), TEXT("error"));
    Error->SetStringField(TEXT("message"), Output);
    Error->SetBoolField(TEXT("complete"), false);
    return SerializeJson(Error);
}

FString UTurboMCPReadToolset::ReadTurboMCPNodeContext(const FString& Asset,const FString& GraphName,const FString& NodeId,int32 NextHops)
{
    if(NextHops<0 || NextHops>32) return TEXT("error: NextHops must be an integer from 0 to 32. Use 0 for no downstream hops, or 3 for the default depth.");
    const TurboMCPNodeId::FParsed Identity = TurboMCPNodeId::Parse(NodeId);
    const FString AssetSelector = Asset.IsEmpty() && Identity.IsComplete() ? Identity.AssetName : Asset;
    const FString GraphSelector = GraphName.IsEmpty() && Identity.IsComplete() ? Identity.GraphName : GraphName;
    if (Identity.IsComplete() && !GraphName.IsEmpty() && GraphName != Identity.GraphName)
        return TEXT("error: GraphName conflicts with the graph in NodeId. Use matching selectors or leave GraphName empty.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(AssetSelector,Resolved);
    if(!Error.IsEmpty()) return Error;
    if (Identity.IsComplete())
    {
        FTurboMCPAssetResolver::FResolved IdAsset;
        const FString IdError = FTurboMCPAssetResolver::Resolve(Identity.AssetName, IdAsset);
        if (!IdError.IsEmpty() || IdAsset.ObjectPath != Resolved.ObjectPath)
            return TEXT("error: Asset conflicts with the asset in NodeId, or the ID asset is unavailable. No alternative target was selected.");
    }
    UBlueprint* Blueprint=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    int32 Matches=0; UEdGraph* Graph=Blueprint?TurboMCPNodeId::FindGraphByName(Blueprint,GraphSelector,Matches):nullptr;
    if(!Blueprint) return FString::Printf(TEXT("error: could not load %s as a Blueprint. Select a Blueprint asset and retry."), *Resolved.ObjectPath);
    if(!Graph || Matches!=1) return FString::Printf(TEXT("error: graph '%s' must resolve uniquely in %s. Available graphs: %s. Use an exact unique graph name."), *GraphSelector, *Resolved.ObjectPath, *ListGraphNames(Blueprint));
    UEdGraphNode* Selected=TurboMCPGraphScratch::Find(Graph,NodeId);
    // Bare reader keys are valid only within the explicitly resolved graph.
    // Never strip a qualified ID: a wrong graph or asset must remain a refusal.
    if (!Selected && !TurboMCPNodeId::NormaliseKey(NodeId).IsEmpty())
    {
        TArray<UEdGraphNode*> KeyMatches;
        const int32 Count = TurboMCPNodeId::FindNodesByKey(Graph, NodeId, KeyMatches);
        if (Count > 1)
            return TEXT("error: node key is ambiguous in this graph. Use more GUID digits or a full node ID from ReadTurboMCPNodeFind.");
        if (Count == 1) Selected = KeyMatches[0];
    }
    if(!Selected) return FString::Printf(TEXT("error: node ID '%s' does not resolve uniquely in graph '%s'. Use ReadTurboMCPNodeFind to obtain a fresh ID from this graph, then retry."), *NodeId, *GraphSelector);
    TSet<UEdGraphNode*> Context;
    TArray<UEdGraphNode*> Queue; Queue.Add(Selected);
    TArray<FString> Entries;
    // Full upstream static execution context, cycle-safe and not limited by NextHops.
    for(int32 Index=0;Index<Queue.Num();++Index)
    {
        UEdGraphNode* Node=Queue[Index]; if(!Node || Context.Contains(Node)) continue;
        Context.Add(Node);
        if(Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>()) Entries.AddUnique(TurboMCPNodeId::MakeForNode(Node,true));
        const UK2Node* K2=Cast<UK2Node>(Node);
        for(UEdGraphPin* P:Node->Pins) if(P)
            if((P->Direction==EGPD_Input && P->PinType.PinCategory==TEXT("exec"))
                || (K2 && K2->IsNodePure() && P->Direction==EGPD_Output && P->PinType.PinCategory!=TEXT("exec")))
                for(UEdGraphPin* Q:P->LinkedTo) if(Q) Queue.Add(Q->GetOwningNode());
    }
    TArray<TPair<UEdGraphNode*,int32>> Forward; Forward.Add({Selected,0}); TSet<UEdGraphNode*> ForwardSeen;
    for(int32 Index=0;Index<Forward.Num();++Index)
    {
        const auto Current=Forward[Index]; UEdGraphNode* Node=Current.Key;
        if(!Node || ForwardSeen.Contains(Node)) continue;
        ForwardSeen.Add(Node); Context.Add(Node);
        if(Current.Value>=NextHops) continue;
        for(UEdGraphPin* P:Node->Pins) if(P && P->Direction==EGPD_Output && P->PinType.PinCategory==TEXT("exec"))
            for(UEdGraphPin* Q:P->LinkedTo) if(Q) Forward.Add({Q->GetOwningNode(),Current.Value+1});
    }
    Entries.Sort();
    FString Out=TEXT("read: static execution context for ")+TurboMCPNodeId::MakeForNode(Selected,true);
    Out+=TEXT("\nreachable entry nodes: ")+(Entries.IsEmpty()?FString(TEXT("none found; dynamic/delegate dispatch is not inferred")):FString::Join(Entries,TEXT(", ")));
    TArray<UEdGraphNode*> Ordered=Context.Array();
    Ordered.Sort([](const UEdGraphNode& A,const UEdGraphNode& B){return A.NodeGuid.ToString()<B.NodeGuid.ToString();});
    for(UEdGraphNode* Node:Ordered)
    {
        Out+=TEXT("\nnode ")+TurboMCPNodeId::MakeForNode(Node,true)+TEXT(" type=")+TurboMCPNodeId::GetNodeTypeId(Node);
        Out += TEXT("\n  title: ") + TurboMCPNodeId::GetNodeTitle(Node).Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n"));
        // Report the stored selection even if the delegate is currently unbound or unresolved.
        if (const UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
            Out += FString::Printf(TEXT("\n  configured delegate function: %s (stored binding; runtime target not verified)"),
                Delegate->GetFunctionName().IsNone() ? TEXT("none (unbound)") : *Delegate->GetFunctionName().ToString());

        for(UEdGraphPin* P:Node->Pins) if(P)
        {
            if(P->Direction==EGPD_Input && P->PinType.PinCategory!=TEXT("exec") && P->LinkedTo.IsEmpty())
            {
                FString Value=P->DefaultObject?P->DefaultObject->GetPathName():!P->DefaultTextValue.IsEmpty()?P->DefaultTextValue.ToString():P->DefaultValue;
                Value.ReplaceInline(TEXT("\n"),TEXT("\\n")); Value.ReplaceInline(TEXT("\r"),TEXT("\\r"));
                Out+=TEXT("\n  input ")+P->PinName.ToString()+TEXT(" default=")+Value;
            }
            for(UEdGraphPin* Q:P->LinkedTo) if(Q && Q->GetOwningNodeUnchecked())
            {
                Out+=FString::Printf(TEXT("\n  %s %s %s %s:%s"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"),*P->PinName.ToString(),P->Direction==EGPD_Input?TEXT("<-"):TEXT("->"),*TurboMCPNodeId::MakeForNode(Q->GetOwningNode(),true),*Q->PinName.ToString());
                if (P->Direction == EGPD_Input && P->PinType.PinCategory != TEXT("exec"))
                    Out += TEXT(" source title: ") + TurboMCPNodeId::GetNodeTitle(Q->GetOwningNode()).Replace(TEXT("\r"), TEXT("\\r")).Replace(TEXT("\n"), TEXT("\\n"));
            }
        }
    }
    return Out+TEXT("\nContext follows upstream execution links (and consumers of pure nodes) plus the requested downstream execution hops. External input sources are named by ID and one-level source title. This is static connectivity, not proof of runtime execution. No edit, compile or save was requested.");
}
