// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_Variable.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"

/**
 * R-NODE-RESOLVE and R-NODE-FIND: the two tools that let an agent name a node.
 *
 * Everything else in the roadmap that reads or writes a graph starts from a node
 * ID, so these are built before any body reader. The ID scheme is verified here,
 * on its own, so that a broken ID cannot later look like a broken reader.
 *
 * `03_TOOLS.md` requires `R-NODE-RESOLVE` to print "the node type ID (the one the
 * engine's node creation tool accepts)". That format is not the C++ class name:
 * it is `<category>|<title>` with whitespace removed, matching the engine's own
 * Blueprint toolset. `TurboMCPNodeId::GetNodeTypeId` implements that rule.
 */
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


		for (const UEdGraphPin* Pin : InNode->Pins)
		{
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

			// A default value is only interesting when the pin is not linked, and
			// `2.1.8` says pins left at their default are not printed. It is named
			// here rather than printed, because this tool reports a node's shape,
			// not its graph text.
			Lines += FString::Printf(TEXT("\n      %s %-2d %s"),
				*Direction, Index, *Pin->PinName.ToString());

            Lines += FString::Printf(TEXT(" [category=%s subcategory=%s container=%d]"),
                *Pin->PinType.PinCategory.ToString(), *Pin->PinType.PinSubCategory.ToString(), static_cast<int32>(Pin->PinType.ContainerType));
            if (const UObject* TypeObject = Pin->PinType.PinSubCategoryObject.Get())
                Lines += FString::Printf(TEXT(" type=%s"), *TypeObject->GetPathName());
            if (Pin->PinType.bIsReference) Lines += TEXT(" reference");
            if (Pin->PinType.bIsConst) Lines += TEXT(" const");
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

	/** True when every word appears in the node's title, class, call, variable or pin names. */
	bool NodeMatchesWords(const UEdGraphNode* InNode, const TArray<FString>& InWords)
	{
		// Every field the spec lists is searched: the title, the called function,
		// the variable, the node class, and the pin names.
		TArray<FString> Fields;
		Fields.Add(TurboMCPNodeId::GetNodeTitle(InNode));
		Fields.Add(InNode->GetClass()->GetName());

		if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(InNode))
		{
			Fields.Add(Call->FunctionReference.GetMemberName().ToString());
		}
		if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(InNode))
		{
			Fields.Add(Variable->GetVarName().ToString());
		}

		FString PinNames;
		for (const UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr)
			{
				PinNames += Pin->PinName.ToString();
				PinNames += TEXT(" ");
			}
		}
		Fields.Add(PinNames);

		// Every word must appear somewhere, in any field. Requiring all of them
		// rather than any of them is what makes a two word search narrow instead
		// of widen.
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
			if (!bFound)
			{
				return false;
			}
		}
		return true;
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
		Add(InBlueprint->UbergraphPages);
		Add(InBlueprint->FunctionGraphs);
		Add(InBlueprint->MacroGraphs);
		Add(InBlueprint->EventGraphs);
	}

	/** The real graph names, for an error that has to name what does exist. */
	FString ListGraphNames(UBlueprint* InBlueprint)
	{
		TArray<UEdGraph*> Graphs;
		CollectGraphs(InBlueprint, FString(), Graphs);

		FString Names;
		for (const UEdGraph* Graph : Graphs)
		{
			Names += Names.IsEmpty() ? Graph->GetName() : TEXT(", ") + Graph->GetName();
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
				// Same first two hex digits is a cheap near miss and is enough to
				// suggest a node that may be the same one after a rebuild.
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
				TEXT("The node may have been deleted, or the graph rebuilt."),
				*AssetResolved.AssetName, *Parsed.GraphName, *Parsed.NodeKey);
			if (!NearMisses.IsEmpty())
			{
				Errors += FString::Printf(
					TEXT("\n    nodes in this graph starting with the same two digits:%s"),
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
		// The node type ID is the form the engine's own graph writer accepts, which
		// is what the spec asks for. Its category is unlocalised, but its title is
		// built at runtime from localised fragments, so on a non-English editor the
		// ID reads in that language and is only usable on an editor in the same
		// language. The C++ class is printed next to it because it is the one
		// stable name for the node, and it is what disambiguates two nodes whose
		// localised titles collide.
		Lines += FString::Printf(TEXT("\n    type ID: %s  (the engine writer's ID; its title half follows the editor's language)"),
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

	if (ResolvedCount == 0 && RefusedCount == 0)
	{
		// Not reachable with a non-empty list, but a silent empty result would
		// break P1 if it ever were.
		Result += TEXT("\nNothing was resolved.");
	}

	return Result;
}

FString UTurboMCPReadToolset::ReadTurboMCPNodeFind(
	const FString& Asset,
	const FString& Words,
	const FString& GraphName)
{
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass the Blueprint to search, in any path form.");
	}

	// Search words are AND terms, not implicit multi-word phrases. Keep list
	// delimiters local to this tool so asset/path list parsing is unchanged.
	FString SearchWords = Words;
	for (int32 Index = 0; Index < SearchWords.Len(); ++Index)
	{
		if (FChar::IsWhitespace(SearchWords[Index]) || SearchWords[Index] == TEXT('|'))
		{
			SearchWords[Index] = TEXT(',');
		}
	}
	const TArray<FString> WordList = FTurboMCPAssetResolver::SplitList(SearchWords);
	if (WordList.Num() == 0)
	{
		return TEXT("error: no words given. Pass one or more words separated by whitespace, commas ")
			TEXT("or pipes, for example PrintString, or MyVar.");
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
			TEXT("error: %s is not a Blueprint, so it has no nodes to search."),
			*Resolved.ObjectPath);
	}

	const FString Scope = GraphName.IsEmpty() ? TEXT("every graph") : GraphName;

	TArray<UEdGraph*> Graphs;
	CollectGraphs(Blueprint, GraphName, Graphs);

	if (Graphs.Num() == 0)
	{
		// A graph name that does not exist lists the real ones, as R-GRAPH-LOAD
		// does, so the caller can correct itself in one step (`P7`).
		return FString::Printf(
			TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
			*Resolved.ObjectPath, *GraphName, *ListGraphNames(Blueprint));
	}

	FString Result = FString::Printf(TEXT("read: nodes in %s matching all words [%s], in %s"),
		*Resolved.ObjectPath, *FString::Join(WordList, TEXT(", ")), *Scope);

	TArray<FString> Lines;
	int32 TotalMatched = 0;
	int32 Searched = 0;
	TArray<FString> GraphsWithHits;

	for (UEdGraph* Graph : Graphs)
	{
		int32 HitsInGraph = 0;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			++Searched;

			if (!NodeMatchesWords(Node, WordList))
			{
				continue;
			}

			++HitsInGraph;
			if (Lines.Num() < MaxFindResults)
			{
				// 2.1.4 and this tool's own spec: node ID, graph, title, type ID.
				// The class name is appended because the engine writer's type ID
				// carries a localised title half; see the note in R-NODE-RESOLVE.
				Lines.Add(FString::Printf(
					TEXT("%s\n    title: %s\n    type ID: %s\n    class: %s"),
					*TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true),
					*TurboMCPNodeId::GetNodeTitle(Node),
					*TurboMCPNodeId::GetNodeTypeId(Node),
					*Node->GetClass()->GetName()));
			}
		}

		if (HitsInGraph > 0)
		{
			GraphsWithHits.Add(FString::Printf(TEXT("%s (%d)"), *Graph->GetName(), HitsInGraph));
			TotalMatched += HitsInGraph;
		}
	}

	Result += FString::Printf(TEXT(": %d match(es), %d node(s) searched"),
		TotalMatched, Searched);

	if (TotalMatched == 0)
	{
		// P1 and P7: say what was searched and what would work instead.
		Result += FString::Printf(
			TEXT("\nNothing matched. All %d node(s) across %d graph(s) were searched. ")
			TEXT("The words are matched against each node's title, its called function, ")
			TEXT("its variable, its class and its pin names, and every word must appear. ")
			TEXT("Try one word, or fewer."),
			Searched, Graphs.Num());
		return Result;
	}

	Result += FString::Printf(TEXT(", in %s"), *FString::Join(GraphsWithHits, TEXT(", ")));

	for (const FString& Line : Lines)
	{
		Result += FString::Printf(TEXT("\n%s"), *Line);
	}

	if (TotalMatched > Lines.Num())
	{
		// 04_FORMATS.md section 1: a capped list says how many are left and the
		// exact call that shows them.
		Result += FString::Printf(
			TEXT("\n(%d more, use ReadTurboMCPNodeFind with a narrower word or a single ")
			TEXT("graph to see them)"),
			TotalMatched - Lines.Num());
	}

	return Result;
}
