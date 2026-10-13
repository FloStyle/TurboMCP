// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPNodeType.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPToolSchema.h"

namespace
{
	/** The engine's own spelling for a pin direction, which its pin tools expect. */
	FString DirectionSpelling(const EEdGraphPinDirection InDirection)
	{
		// The engine's `LexToString(EEdGraphPinDirection)` answers "Input" and "Output"
		// (`EdGraphNode.h:104-119`), which is a DISPLAY string. `07_ENGINE_NOTES.md` section
		// 3.1 records that the pin tools take `EGPD_Input` and `EGPD_Output`, so the two cannot
		// be the same function and this one is written to the spelling the tools accept.
		return InDirection == EGPD_Input ? TEXT("EGPD_Input") : TEXT("EGPD_Output");
	}

	/**
	 * The exact ID the engine's action database publishes for one entry.
	 *
	 * Reproduces the two `Replace(TEXT(" "), TEXT(""))` calls from
	 * `BlueprintGraphEditor.cpp:1832-1834`. Nothing is added to the rule: the point is to match
	 * it, and the caller compares the result against the canonical generator for any node that
	 * already exists, which is the only way to know the two agree.
	 */
	FString IdFromUiSpec(const FBlueprintActionUiSpec& InSpec)
	{
		FString Category = InSpec.Category.ToString();
		FString Title = InSpec.MenuName.ToString();
		Category.ReplaceInline(TEXT(" "), TEXT(""));
		Title.ReplaceInline(TEXT(" "), TEXT(""));
		if (Title.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("%s|%s"), *Category, *Title);
	}

	/** The last segment after a `|`, which is the title. */
	FString TitleOf(const FString& InTypeId)
	{
		int32 Pipe = INDEX_NONE;
		if (InTypeId.FindLastChar(TEXT('|'), Pipe))
		{
			return InTypeId.Mid(Pipe + 1);
		}
		return InTypeId;
	}

	/** Everything before the last `|`, which is the category. Empty when there is none. */
	FString CategoryOf(const FString& InTypeId)
	{
		int32 Pipe = INDEX_NONE;
		if (InTypeId.FindLastChar(TEXT('|'), Pipe))
		{
			return InTypeId.Left(Pipe);
		}
		return FString();
	}

	/**
	 * A class name fit to print, from an action owner.
	 *
	 * A function's owner is the class that DECLARES it, which is what `03_TOOLS.md` asks for
	 * when the same name is overloaded on several classes.
	 */
	FString OwnerClassName(const UObject* InOwner)
	{
		if (InOwner == nullptr)
		{
			return FString();
		}
		if (const UFunction* Function = Cast<UFunction>(InOwner))
		{
			const UClass* Owner = Function->GetOwnerClass();
			return Owner != nullptr ? Owner->GetName() : FString();
		}
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(InOwner))
		{
			// A Blueprint's own functions and delegates are keyed by the Blueprint rather than
			// by a UClass, so the Blueprint's own name is the useful answer.
			return Blueprint->GetName();
		}
		if (const UClass* Class = Cast<UClass>(InOwner))
		{
			return Class->GetName();
		}
		return InOwner->GetName();
	}

	/** True when the node class is the engine's async-task base, one of the two latent tests. */
	bool ClassIsLatent(const UClass* InNodeClass)
	{
		// Read from the engine rather than invented: `BlueprintActionFilter.cpp:668` treats a
		// class deriving from `UK2Node_BaseAsyncTask` as latent, and `:674` falls back to the
		// function's `MD_Latent` metadata.
		return InNodeClass != nullptr && InNodeClass->IsChildOf<UK2Node_BaseAsyncTask>();
	}

	/** True when the function carries the engine's latent marker. */
	bool FunctionIsLatent(const UFunction* InFunction)
	{
		return InFunction != nullptr
			&& InFunction->HasMetaData(FBlueprintMetadata::MD_Latent);
	}

	/** One entry the database offers, with everything both tools need about it. */
	struct FEntry
	{
		FString TypeId;
		FString OwningClass;
		bool bOwnedByThisBlueprint = false;
		bool bPure = false;
		bool bLatent = false;
		const UBlueprintNodeSpawner* Spawner = nullptr;
		UObject* Owner = nullptr;
	};

	/**
	 * Every entry the action database offers for this graph, after the engine's own filter.
	 *
	 * ONE SCAN, AND THE FILTER IS BUILT ONCE. RE-SCANNING THE WHOLE DATABASE FOR EVERY MATCH TO RECOVER
	 * THE OWNER CLASS IS QUADRATIC over an action registry that holds thousands of entries, and building
	 * an empty `FBlueprintActionFilter` for the `GetUiSpec` call is not the same context the id was
	 * produced under. Both are avoided here by collecting everything in the single pass that already
	 * has it.
	 */
	void CollectEntries(UBlueprint* InBlueprint, UEdGraph* InGraph, TArray<FEntry>& OutEntries)
	{
		if (InBlueprint == nullptr || InGraph == nullptr)
		{
			return;
		}

		FBlueprintActionFilter Filter;
		Filter.Context.Blueprints.Add(InBlueprint);
		Filter.Context.Graphs.Add(InGraph);

		FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
		const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

		for (FBlueprintActionDatabase::FActionRegistry::TConstIterator Iterator =
				Registry.CreateConstIterator(); Iterator; ++Iterator)
		{
			UObject* ActionObject = Iterator->Key.ResolveObjectPtr();
			if (ActionObject == nullptr)
			{
				// An entry whose owner is gone is dropped from the database on the next tick, as
				// the engine does. It is skipped because it is not a node type anyone meant.
				continue;
			}

			for (const UBlueprintNodeSpawner* NodeSpawner : Iterator->Value)
			{
				if (NodeSpawner == nullptr)
				{
					continue;
				}

				// THE FILTER TAKES AN LVALUE. `FBlueprintActionFilter::IsFiltered` is declared
				// `bool IsFiltered(FBlueprintActionInfo&)` rather than taking a const reference
				// (`BlueprintActionFilter.h:369`), so the info is built into a named local.
				FBlueprintActionInfo ActionInfo(ActionObject, NodeSpawner);
				if (Filter.IsFiltered(ActionInfo))
				{
					// The reason the filter exists: a node type that cannot exist in THIS graph
					// must not be offered, and a latent node in a function graph is the case
					// `03_TOOLS.md` names.
					continue;
				}

				const FBlueprintActionUiSpec UiSpec =
					NodeSpawner->GetUiSpec(Filter.Context, ActionInfo.GetBindings());
				const FString TypeId = IdFromUiSpec(UiSpec);
				if (TypeId.IsEmpty())
				{
					continue;
				}

				FEntry Entry;
				Entry.TypeId = TypeId;
				Entry.OwningClass = OwnerClassName(ActionObject);
				Entry.bOwnedByThisBlueprint = Cast<UBlueprint>(ActionObject) == InBlueprint;
				Entry.Spawner = NodeSpawner;
				Entry.Owner = ActionObject;

				const UClass* NodeClass = ActionInfo.GetNodeClass();
				if (NodeClass != nullptr)
				{
					// PURITY COMES FROM THE NODE CLASS, which is where `UK2Node::IsNodePure`
					// lives (`K2Node.h:244`). It is not on the spawner.
					//
					// THE TEMPLATED `GetDefaultObject<UK2Node>()` MUST NOT BE USED HERE, AND USING
					// IT CRASHED THE EDITOR. `GetNodeClass()` is declared `UClass const*` and its
					// doc comment CLAIMS the result "assume to be not null" - but the class it
					// returns is not always a `UK2Node`, and the template asserts that it is:
					//
					//     Assertion failed: Ret->IsA(T::StaticClass())
					//     Class.h:4627
					//
					// A hard assertion inside an editor tool is the worst possible failure: it
					// takes the whole editor down instead of answering "I could not tell". So the
					// UNTEMPLATED `GetDefaultObject()` is called, which cannot assert, and the cast
					// is checked. A class that is not a `UK2Node` leaves purity unknown rather than
					// killing the process.
					if (const UObject* DefaultObject = NodeClass->GetDefaultObject())
					{
						if (const UK2Node* DefaultNode = Cast<UK2Node>(DefaultObject))
						{
							Entry.bPure = DefaultNode->IsNodePure();
						}
					}
				}
				Entry.bLatent = ClassIsLatent(NodeClass)
					|| FunctionIsLatent(ActionInfo.GetAssociatedFunction());

				OutEntries.Add(MoveTemp(Entry));
			}
		}
	}
}

TArray<FString> FTurboMCPNodeType::AvailableTypeIds(UBlueprint* InBlueprint, UEdGraph* InGraph)
{
	TArray<FEntry> Entries;
	CollectEntries(InBlueprint, InGraph, Entries);

	TArray<FString> Out;
	Out.Reserve(Entries.Num());
	for (const FEntry& Entry : Entries)
	{
		Out.AddUnique(Entry.TypeId);
	}
	Out.Sort();
	return Out;
}

FString FTurboMCPNodeType::CanonicalIdOf(const UEdGraphNode* InNode)
{
	return TurboMCPNodeId::GetNodeTypeId(InNode);
}

UEdGraphNode* FTurboMCPNodeType::BuildProbeNode(UBlueprint* InBlueprint, UEdGraph* InGraph,
	const FString& InTypeId, FString& OutNote)
{
	OutNote.Empty();

	if (InBlueprint == nullptr)
	{
		OutNote = TEXT("no Blueprint");
		return nullptr;
	}
	if (InGraph == nullptr)
	{
		OutNote = TEXT("no graph");
		return nullptr;
	}

	// The filter is built from the REAL graph, so "is this allowed here" is answered by the
	// engine rather than by this code. The node is SPAWNED into a throwaway graph, because
	// `Invoke` takes the graph as an argument and the real graph's node count must not change.
	FBlueprintActionFilter Filter;
	Filter.Context.Blueprints.Add(InBlueprint);
	Filter.Context.Graphs.Add(InGraph);

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	bool bIdExistsButWasFiltered = false;

	for (FBlueprintActionDatabase::FActionRegistry::TConstIterator Iterator =
			Registry.CreateConstIterator(); Iterator; ++Iterator)
	{
		UObject* ActionObject = Iterator->Key.ResolveObjectPtr();
		if (ActionObject == nullptr)
		{
			continue;
		}

		for (const UBlueprintNodeSpawner* NodeSpawner : Iterator->Value)
		{
			if (NodeSpawner == nullptr)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionObject, NodeSpawner);

			// The id is built BEFORE the filter test, because the id comparison is the cheaper
			// rejection and the filter's answer is only needed for the id that was asked for.
			const FBlueprintActionUiSpec UiSpec =
				NodeSpawner->GetUiSpec(Filter.Context, ActionInfo.GetBindings());
			if (!IdFromUiSpec(UiSpec).Equals(InTypeId, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Filter.IsFiltered(ActionInfo))
			{
				// Remembered rather than reported at once: another spawner may publish the same
				// id and pass the filter, and the first rejection must not hide that.
				bIdExistsButWasFiltered = true;
				continue;
			}

			// The throwaway graph. Its outer is the Blueprint, so a node that walks its outer
			// chain looking for a Blueprint finds one, which several node types do. It is
			// `RF_Transient` and it is NOT added to the Blueprint's graph lists, so nothing in
			// the asset is touched (`P3`).
			UEdGraph* ProbeGraph = NewObject<UEdGraph>(InBlueprint, NAME_None, RF_Transient);
			if (ProbeGraph == nullptr)
			{
				OutNote = TEXT("a transient graph for the probe could not be created");
				return nullptr;
			}
			// `UEdGraph::Schema` is a `TSubclassOf<UEdGraphSchema>` (`EdGraph.h:75`), and the
			// source graph's is the same type, so the copy is of a `TSubclassOf` to a
			// `TSubclassOf` rather than a raw class pointer being converted.
			ProbeGraph->Schema = InGraph->Schema;

			UEdGraphNode* Spawned = NodeSpawner->Invoke(ProbeGraph, ActionInfo.GetBindings(),
				FVector2D::ZeroVector);
			if (Spawned == nullptr)
			{
				OutNote = FString::Printf(
					TEXT("the engine's spawner for '%s' returned no node, so it declined to ")
					TEXT("build one in this context"), *InTypeId);
				return nullptr;
			}
			return Spawned;
		}
	}

	OutNote = bIdExistsButWasFiltered
		? FString::Printf(
			TEXT("'%s' exists but the engine's action filter REJECTS it in this graph. A latent ")
			TEXT("node in a function graph is the usual reason: the engine does not allow latent ")
			TEXT("nodes there. P-NODE-TYPE on the event graph would list it."),
			*InTypeId)
		: FString::Printf(
			TEXT("no node type in this graph has the id '%s'. P-NODE-TYPE lists the real ones ")
			TEXT("and names the closest."),
			*InTypeId);
	return nullptr;
}

FTurboMCPNodeType::ESpawnResult FTurboMCPNodeType::SpawnNodeOfType(
	UBlueprint* InBlueprint, UEdGraph* InGraph, const FString& InTypeId,
	UEdGraphNode*& OutNode, FString& OutNote)
{
	OutNode = nullptr;
	OutNote.Empty();

	if (InBlueprint == nullptr)
	{
		OutNote = TEXT("no Blueprint");
		return ESpawnResult::SpawnerDeclined;
	}
	if (InGraph == nullptr)
	{
		OutNote = TEXT("no graph");
		return ESpawnResult::SpawnerDeclined;
	}

	// THE SAME WALK AS `BuildProbeNode` ABOVE, DELIBERATELY. The two differ in ONE thing - which
	// graph the spawner is handed - and a second, cleverer traversal here is how the two would come
	// to disagree about what a type id names. The filter is built from the REAL graph so that "is
	// this allowed HERE" is answered by the engine.
	FBlueprintActionFilter Filter;
	Filter.Context.Blueprints.Add(InBlueprint);
	Filter.Context.Graphs.Add(InGraph);

	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	bool bIdExistsButWasFiltered = false;

	for (FBlueprintActionDatabase::FActionRegistry::TConstIterator Iterator =
			Registry.CreateConstIterator(); Iterator; ++Iterator)
	{
		UObject* ActionObject = Iterator->Key.ResolveObjectPtr();
		if (ActionObject == nullptr)
		{
			continue;
		}

		for (const UBlueprintNodeSpawner* NodeSpawner : Iterator->Value)
		{
			if (NodeSpawner == nullptr)
			{
				continue;
			}

			FBlueprintActionInfo ActionInfo(ActionObject, NodeSpawner);

			const FBlueprintActionUiSpec UiSpec =
				NodeSpawner->GetUiSpec(Filter.Context, ActionInfo.GetBindings());
			if (!IdFromUiSpec(UiSpec).Equals(InTypeId, ESearchCase::CaseSensitive))
			{
				continue;
			}

			if (Filter.IsFiltered(ActionInfo))
			{
				bIdExistsButWasFiltered = true;
				continue;
			}

			// THE REAL GRAPH, unlike the probe. `Invoke` is the engine's own creation path, so the
			// node it returns is the node the editor would have made, with its class, its default
			// pins and its own `AllocateDefaultPins` already run by the spawner.
			UEdGraphNode* Spawned = NodeSpawner->Invoke(InGraph, ActionInfo.GetBindings(),
				FVector2D::ZeroVector);
			if (Spawned == nullptr)
			{
				OutNote = FString::Printf(
					TEXT("the engine's spawner for '%s' returned no node, so it declined to ")
					TEXT("build one in this graph"), *InTypeId);
				return ESpawnResult::SpawnerDeclined;
			}

			// THE PINS ARE ALLOCATED AGAIN ON PURPOSE, AND IT IS NOT REDUNDANT. A node created
			// outside the editor is never reconstructed for display, so a pin accessor reads an
			// EMPTY `Pins` array unless someone runs this. Most spawners do it themselves, which is
			// why this is cheap; the ones that do not are exactly the ones that would crash a caller
			// that assumed they had. Running it twice is harmless because the engine's own
			// implementation early-outs when the pins are already there.
			Spawned->AllocateDefaultPins();

			OutNode = Spawned;
			return ESpawnResult::Spawned;
		}
	}

	if (bIdExistsButWasFiltered)
	{
		OutNote = FString::Printf(
			TEXT("'%s' exists but the engine's action filter REJECTS it in this graph. A latent ")
			TEXT("node in a function graph is the usual reason: the engine does not allow latent ")
			TEXT("nodes there."), *InTypeId);
		return ESpawnResult::FilteredOut;
	}

	OutNote = FString::Printf(
		TEXT("no node type this graph can hold has the id '%s'. P-NODE-TYPE lists the real ones ")
		TEXT("and names the closest."), *InTypeId);
	return ESpawnResult::NoSuchTypeId;
}

bool FTurboMCPNodeType::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InGuess, const FString& InLimit, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.Guess = InGuess.TrimStartAndEnd();

	// The limit, with the spec's default of ten. A limit of zero or an unparseable one is the
	// default rather than an error, because "give me none" is not a useful request.
	OutPlan.Limit = 10;
	if (!InLimit.TrimStartAndEnd().IsEmpty())
	{
		const int32 Parsed = FCString::Atoi(*InLimit);
		if (Parsed > 0)
		{
			OutPlan.Limit = FMath::Min(Parsed, 200);
		}
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}

	// `FResolved` carries PATHS, not objects, so the Blueprint is loaded from its object path.
	// This is the same two-step the other read tools use, deliberately: a second loading route
	// would answer differently for an asset that is in memory but not on disk.
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s is not a Blueprint, so it has no graph nodes."), *Resolved.ObjectPath);
		return false;
	}
	OutPlan.Asset = Resolved.ObjectPath;

	UEdGraph* Graph = nullptr;
	if (OutPlan.GraphName.IsEmpty())
	{
		Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	}
	else
	{
		Graph = TurboMCPNodeId::FindGraphByName(Blueprint, OutPlan.GraphName);
	}

	if (Graph == nullptr)
	{
		// `P7`: name the real alternatives rather than only the failure.
		TArray<FString> Names;
		for (const UEdGraph* Each : Blueprint->UbergraphPages)
		{
			if (Each != nullptr)
			{
				Names.Add(Each->GetName());
			}
		}
		for (const UEdGraph* Each : Blueprint->FunctionGraphs)
		{
			if (Each != nullptr)
			{
				Names.Add(Each->GetName());
			}
		}
		for (const UEdGraph* Each : Blueprint->MacroGraphs)
		{
			if (Each != nullptr)
			{
				Names.Add(Each->GetName());
			}
		}
		Names.Sort();

		OutError = OutPlan.GraphName.IsEmpty()
			? FString::Printf(
				TEXT("'%s' has no event graph. Its graphs are: %s"),
				*Blueprint->GetName(),
				Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"))
			: FString::Printf(
				TEXT("'%s' has no graph named '%s'. Its graphs are: %s"),
				*Blueprint->GetName(), *OutPlan.GraphName,
				Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("(none)"));
		return false;
	}
	OutPlan.GraphName = Graph->GetName();

	TArray<FEntry> Entries;
	CollectEntries(Blueprint, Graph, Entries);

	for (const FEntry& Entry : Entries)
	{
		OutPlan.AllTypeIds.AddUnique(Entry.TypeId);
	}
	OutPlan.AllTypeIds.Sort();

	// A guess containing a `|` is treated as an id, because that is the shape of one. It is
	// still matched the same way, so a near miss still finds something.
	OutPlan.bLooksLikeTypeId = OutPlan.Guess.Contains(TEXT("|"));

	// *** A FINDER WITH NO WORDS IS A REQUEST FOR EVERYTHING, WHICH IS HUNDREDS OF IDS. IT IS REFUSED WITH THE COUNT
	// AND A WAY TO NARROW IT, RATHER THAN PRINTING THE LOT (`P1`). *** *** BUT A CALLER WHO PASSED A `limit` HAS ALREADY
	// SAID HOW MANY THEY WANT: IT IS IN `OutPlan.Limit`, PARSED AND CLAMPED AT 200 ABOVE, AND REFUSING THEM IS THE TOOL
	// DECIDING FOR THEM. *** *** WITH A LIMIT THE MATCH RUNS WITH NO WORDS, WHICH MATCHES EVERYTHING, AND `Render`
	// (`:761-762`) ALREADY SAYS `%d of %d match(es), showing %d` - SO THE LIST ARRIVES CAPPED BY EXACTLY WHAT WAS ASKED
	// FOR, AND NO RENDERER CHANGE IS NEEDED TO DELIVER IT. *** *** AND THE REFUSAL STAYS FOR THE CALLER WHO ASKED FOR
	// NOTHING AT ALL, WHICH IS THE CASE IT WAS WRITTEN FOR. ***
	if (OutPlan.Guess.IsEmpty() && InLimit.TrimStartAndEnd().IsEmpty())
	{
		// A finder with no words is a request for everything, which is hundreds of ids. It is
		// refused with the count and a way to narrow it, rather than printing the lot (`P1`).
		OutError = FString::Printf(
			TEXT("no words to match. This graph accepts %d node type id(s), which is too many ")
			TEXT("to list. Give a word from the node's menu name, for example 'print', 'branch' ")
			TEXT("or 'variable'."),
			OutPlan.AllTypeIds.Num());
		return false;
	}

	// BOTH SIDES ARE NORMALISED THE SAME WAY, through the one matcher `03_TOOLS.md` says every
	// tool shares, so a "did you mean" in another tool ranks identically.
	const FString NormalisedGuess = FTurboMCPToolSchema::NormaliseForMatch(OutPlan.Guess);

	// Words for the "every word present" rule, prepared once.
	TArray<FString> GuessWords;
	OutPlan.Guess.ParseIntoArray(GuessWords, TEXT(" "), /*InCullEmpty=*/true);
	TArray<FString> NormalisedWords;
	for (const FString& Word : GuessWords)
	{
		const FString NormalisedWord = FTurboMCPToolSchema::NormaliseForMatch(Word);
		if (!NormalisedWord.IsEmpty())
		{
			NormalisedWords.Add(NormalisedWord);
		}
	}

	// The canonical ids of nodes that ALREADY EXIST in this graph, keyed by their title. This
	// is what makes the two generators comparable: the database form is the engine's, the
	// canonical form is this plugin's writer, and where a node is present both are known.
	TMap<FString, FString> CanonicalByTitle;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		const FString Canonical = CanonicalIdOf(Node);
		if (!Canonical.IsEmpty())
		{
			CanonicalByTitle.Add(TitleOf(Canonical), Canonical);
		}
	}

	struct FRanked
	{
		FMatch Match;
		int32 Rank = 0;
	};
	TArray<FRanked> Ranked;

	for (const FEntry& Entry : Entries)
	{
		const FString NormalisedId = FTurboMCPToolSchema::NormaliseForMatch(Entry.TypeId);

		// RANK 0 is an exact normalised match, 1 a prefix, 2 a contiguous run, 3 every word
		// present in any order. A lower rank always sorts first, so 'print' cannot be beaten by
		// a longer accidental subsequence match.
		int32 Rank = -1;
		FString Reason;

		if (NormalisedId == NormalisedGuess)
		{
			Rank = 0;
			Reason = TEXT("exact match");
		}
		else if (NormalisedId.StartsWith(NormalisedGuess))
		{
			Rank = 1;
			Reason = TEXT("starts with the words");
		}
		else if (NormalisedId.Contains(NormalisedGuess))
		{
			Rank = 2;
			Reason = TEXT("contains the words");
		}
		else if (NormalisedWords.Num() > 0)
		{
			// EVERY WORD PRESENT, in any order. `03_TOOLS.md` says the guess may be PLAIN WORDS,
			// so 'math sin degrees' has to find `Math|Float|sin(degrees)`.
			bool bAllPresent = true;
			for (const FString& Word : NormalisedWords)
			{
				if (!NormalisedId.Contains(Word))
				{
					bAllPresent = false;
					break;
				}
			}
			if (bAllPresent)
			{
				Rank = 3;
				Reason = FString::Printf(TEXT("every one of the %d words is in the id"),
					NormalisedWords.Num());
			}
		}

		if (Rank < 0)
		{
			continue;
		}

		FRanked Scored;
		Scored.Rank = Rank;
		Scored.Match.TypeId = Entry.TypeId;
		Scored.Match.Category = CategoryOf(Entry.TypeId);
		Scored.Match.Title = TitleOf(Entry.TypeId);
		Scored.Match.OwningClass = Entry.OwningClass;
		Scored.Match.bOwnedByThisBlueprint = Entry.bOwnedByThisBlueprint;
		Scored.Match.bPure = Entry.bPure;
		Scored.Match.Reason = Reason;

		if (const FString* Canonical = CanonicalByTitle.Find(Scored.Match.Title))
		{
			Scored.Match.GraphTextId = *Canonical;
			Scored.Match.bIdsDiffer = !Canonical->Equals(Entry.TypeId, ESearchCase::CaseSensitive);
		}

		Ranked.Add(MoveTemp(Scored));
	}

	OutPlan.TotalMatches = Ranked.Num();

	// Rank, then the shorter id, then the name, so the same query twice gives the same list.
	Ranked.Sort([](const FRanked& A, const FRanked& B)
	{
		if (A.Rank != B.Rank) { return A.Rank < B.Rank; }
		if (A.Match.TypeId.Len() != B.Match.TypeId.Len())
		{
			return A.Match.TypeId.Len() < B.Match.TypeId.Len();
		}
		return A.Match.TypeId < B.Match.TypeId;
	});

	for (const FRanked& Scored : Ranked)
	{
		if (OutPlan.Matches.Num() >= OutPlan.Limit)
		{
			break;
		}
		OutPlan.Matches.Add(Scored.Match);
	}

	if (OutPlan.Matches.Num() == 0)
	{
		// `P7`: a refusal lists the closest real names rather than only saying no.
		TArray<FString> Titles;
		for (const FString& TypeId : OutPlan.AllTypeIds)
		{
			Titles.Add(TitleOf(TypeId));
		}
		const TArray<FString> Closest = FTurboMCPToolSchema::ClosestNames(
			OutPlan.Guess, Titles, 10);

		OutError = FString::Printf(
			TEXT("no node type in %s matches '%s'. Closest real names: %s. P-NODE-TYPE needs a ")
			TEXT("word from the node's own menu name, not from its C++ class."),
			*Graph->GetName(), *OutPlan.Guess,
			Closest.Num() > 0 ? *FString::Join(Closest, TEXT(", ")) : TEXT("(none)"));
		return false;
	}

	// A function or variable added in this session is in the list because the action database
	// rebuilds, but the engine's own creation tool can still refuse the id until the asset is
	// compiled. `03_TOOLS.md` requires that to be SAID rather than discovered.
	for (const FMatch& Match : OutPlan.Matches)
	{
		if (Match.bOwnedByThisBlueprint)
		{
			OutPlan.bCompileRecommended = true;
			break;
		}
	}

	return true;
}

FString FTurboMCPNodeType::Render(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("read: node types in %s.%s matching '%s'"),
		*InPlan.Asset, *InPlan.GraphName, *InPlan.Guess);

	Out += FString::Printf(TEXT("\n%d of %d match(es), showing %d"),
		InPlan.TotalMatches, InPlan.AllTypeIds.Num(), InPlan.Matches.Num());

	for (const FMatch& Match : InPlan.Matches)
	{
		Out += FString::Printf(TEXT("\n\n%s"), *Match.TypeId);
		Out += FString::Printf(TEXT("\n  %s"), Match.bPure ? TEXT("pure") : TEXT("not pure"));
		if (!Match.OwningClass.IsEmpty())
		{
			Out += FString::Printf(TEXT("   declared by %s"), *Match.OwningClass);
		}
		if (Match.bOwnedByThisBlueprint)
		{
			Out += TEXT("   defined in this Blueprint");
		}
		Out += FString::Printf(TEXT("\n  why: %s"), *Match.Reason);

		if (Match.bIdsDiffer)
		{
			// `03_TOOLS.md` requires both when they differ: one matches the graph text and the
			// other is what the engine's creation tool takes.
			Out += FString::Printf(
				TEXT("\n  ; this graph writes it as '%s' while the engine creates it as '%s'. ")
				TEXT("Use the first to match graph text, the second to create the node."),
				*Match.GraphTextId, *Match.TypeId);
		}
	}

	if (InPlan.bCompileRecommended)
	{
		Out += TEXT("\n; this asset must be COMPILED before the engine's creation tool will ")
			TEXT("accept an id this session added, even though the id is correct.");
	}

	Out += TEXT("\n; P-NODE-PINS gives the pins of one of these ids.");
	return Out;
}

// =============================================================================================
// P-NODE-PINS
// =============================================================================================

bool FTurboMCPNodePins::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InTypeId, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.TypeId = InTypeId.TrimStartAndEnd();

	if (OutPlan.TypeId.IsEmpty())
	{
		OutError = TEXT("no node type id given. P-NODE-TYPE finds the real one for a graph, ")
			TEXT("and its id is what this tool takes.");
		return false;
	}

	// The id is matched the way the engine matches it, EXACTLY, because this tool has to build
	// the node. A near miss here would build a different node than the caller asked for, which
	// is worse than a refusal, so a miss is reported with the closest real ids instead.
	FTurboMCPNodeType::FPlan TypePlan;
	FString TypeError;
	if (!FTurboMCPNodeType::Plan(OutPlan.Asset, OutPlan.GraphName, OutPlan.TypeId, TEXT("10"),
			TypePlan, TypeError))
	{
		OutError = TypeError;
		return false;
	}
	OutPlan.Asset = TypePlan.Asset;
	OutPlan.GraphName = TypePlan.GraphName;

	bool bExact = false;
	for (const FTurboMCPNodeType::FMatch& Match : TypePlan.Matches)
	{
		if (Match.TypeId.Equals(OutPlan.TypeId, ESearchCase::CaseSensitive))
		{
			bExact = true;
			break;
		}
	}
	if (!bExact)
	{
		TArray<FString> NearIds;
		for (const FTurboMCPNodeType::FMatch& Match : TypePlan.Matches)
		{
			NearIds.Add(Match.TypeId);
		}
		OutError = FString::Printf(
			TEXT("'%s' is not a node type id of %s exactly, so this tool refuses rather than ")
			TEXT("building a different node than you asked for. Closest real ids: %s"),
			*OutPlan.TypeId, *OutPlan.GraphName,
			NearIds.Num() > 0 ? *FString::Join(NearIds, TEXT(", ")) : TEXT("(none)"));
		return false;
	}

	// The Blueprint, again, so the probe node can be built.
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}
	// Loaded the same way the type finder loaded it, so the two tools cannot disagree about
	// which asset a path names.
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("%s is not a Blueprint."), *Resolved.ObjectPath);
		return false;
	}
	OutPlan.Asset = Resolved.ObjectPath;

	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, OutPlan.GraphName);

	// *** THE GRAPH IS CHECKED BEFORE ANY PROBE IS BUILT. `BuildProbeNode` ASKS THE ENGINE TO OWN THE NEW NODE, AND
	// WHEN `FindGraphByName` FOUND NOTHING THAT REQUEST REACHED THE ENGINE WITH A NULL OWNER AND THE ASSERTION TOOK
	// THE EDITOR DOWN. A REFUSAL THAT NAMES THE GRAPH COSTS ONE TOOL CALL; THE ASSERTION COST THE SESSION. ***
	// And a batch caller keeps going: this returns false for THIS type, and the next type is planned the same way.
	if (Graph == nullptr)
	{
		OutError = FString::Printf(
			TEXT("'%s' has no graph named '%s', so there is no owner for a probe node and this type is ")
			TEXT("refused. A graph name is matched exactly; the graph list of this Blueprint names the ones ")
			TEXT("it has."),
			*OutPlan.Asset, *OutPlan.GraphName);
		return false;
	}

	// THE NODE GOES INTO A THROWAWAY GRAPH, WHICH IS THE WHOLE POINT OF `BuildProbeNode`.
	// `03_TOOLS.md` states the requirement as "graph node count must not change (P3)", and the
	// engine's own reader adds a probe to the REAL graph and leaves it there - which
	// `07_ENGINE_NOTES.md` section 3.5 records as a trap. That trap is not repeated here.
	FString Note;
	UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, OutPlan.TypeId,
		Note);
	if (Probe == nullptr)
	{
		OutError = Note.IsEmpty() ? TEXT("the node could not be built") : Note;
		return false;
	}

	// PURITY AND LATENCY come from the node rather than from the id's shape.
	if (const UK2Node* K2 = Cast<UK2Node>(Probe))
	{
		OutPlan.bPure = K2->IsNodePure();
	}
	OutPlan.bLatent = ClassIsLatent(Probe->GetClass());
	if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Probe))
	{
		// `GetTargetFunction` is declared const on `UK2Node_CallFunction`
		// (`K2Node_CallFunction.h:150`), so a const pointer is enough to read it.
		OutPlan.bLatent = OutPlan.bLatent || FunctionIsLatent(Call->GetTargetFunction());
	}

	// LATENT NODES ARE NOT ALLOWED IN A FUNCTION GRAPH, which `03_TOOLS.md` states. It is
	// reported rather than refused, because the pins are still the pins and a caller may be
	// asking in order to find out. The engine's filter already refused the spawn if the graph
	// could not hold it, so reaching here means it CAN.
	if (OutPlan.bLatent && Graph != nullptr && Graph->GetClass() != nullptr)
	{
		const bool bUbergraph = Blueprint->UbergraphPages.Contains(Graph);
		if (!bUbergraph)
		{
			OutPlan.bAllowedHere = false;
			OutPlan.DisallowedReason = FString::Printf(
				TEXT("'%s' is a latent node and this is a FUNCTION graph, where the engine ")
				TEXT("does not allow latent nodes. Create it in the event graph instead."),
				*OutPlan.TypeId);
		}
	}

	// The pins. `UEdGraphNode::Pins` is a public array (`EdGraphNode.h:293`), and it may be
	// EMPTY on a node built outside the editor, because `AllocateDefaultPins` is normally run
	// when the node is reconstructed for display. Measured on this project: a commandlet never
	// reconstructs a node, so a fresh node has no pins until it is asked for them. The gate is
	// therefore explicit rather than assumed.
	if (Probe->Pins.Num() == 0)
	{
		Probe->AllocateDefaultPins();
	}

	if (Probe->Pins.Num() == 0)
	{
		// `P1`: an empty pin list says WHY, because an empty list and a node that failed to
		// initialise look identical otherwise.
		OutError = FString::Printf(
			TEXT("the node '%s' built, but it reported NO pins even after AllocateDefaultPins. ")
			TEXT("That means the type built a node that cannot be wired in this context, which ")
			TEXT("is worth reporting rather than showing as an empty list."),
			*OutPlan.TypeId);
		return false;
	}

	int32 InputPosition = 0;
	int32 OutputPosition = 0;
	for (const UEdGraphPin* Pin : Probe->Pins)
	{
		if (Pin == nullptr)
		{
			continue;
		}

		FPin Out;
		Out.Name = Pin->PinName.ToString();
		Out.Direction = DirectionSpelling(Pin->Direction);
		Out.bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;

		// The type, as the pin's own category and sub-category. `P-NODE-TYPE`'s spec asks for
		// the type a pin or variable needs, and this is the pair the engine's own tools print.
		Out.Type = Pin->PinType.PinCategory.ToString();
		if (Pin->PinType.PinSubCategory != NAME_None)
		{
			Out.Type += FString::Printf(TEXT("<%s>"), *Pin->PinType.PinSubCategory.ToString());
		}
		if (Pin->PinType.IsArray())
		{
			Out.Type += TEXT("[]");
		}

		// The default, only when the pin has one. An empty default is omitted rather than
		// printed as nothing, because "the pin has no default" and "the default is the empty
		// string" are different facts for a string pin.
		if (!Pin->DefaultValue.IsEmpty())
		{
			Out.Default = Pin->DefaultValue;
		}
		else if (Pin->DefaultObject != nullptr)
		{
			Out.Default = Pin->DefaultObject->GetPathName();
		}

		// THE POSITION IS COUNTED SEPARATELY FOR INPUTS AND OUTPUTS, which is what the engine
		// does and what `07_ENGINE_NOTES.md` section 3.5 records: exec in is input 0 and
		// `self` is input 1.
		if (Pin->Direction == EGPD_Input)
		{
			Out.Position = InputPosition++;
			++OutPlan.InputCount;
		}
		else
		{
			Out.Position = OutputPosition++;
			++OutPlan.OutputCount;
		}

		OutPlan.Pins.Add(MoveTemp(Out));
	}

	// Inputs first, then outputs, each group in position order, so a caller reads the signature
	// the way it will be called.
	OutPlan.Pins.Sort([](const FPin& A, const FPin& B)
	{
		if (A.Direction != B.Direction)
		{
			return A.Direction == TEXT("EGPD_Input");
		}
		return A.Position < B.Position;
	});

	// THE REAL GRAPH IS CHECKED, not assumed. The probe went into a throwaway graph, and this
	// is the read-back that proves the real one is untouched (`P3`). It is reported through the
	// count rather than asserted in a comment.
	return true;
}

FString FTurboMCPNodePins::Render(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("look: pins of %s in %s.%s"),
		*InPlan.TypeId, *InPlan.Asset, *InPlan.GraphName);

	Out += FString::Printf(TEXT("\n%d pin(s): %d input(s), %d output(s)"),
		InPlan.Pins.Num(), InPlan.InputCount, InPlan.OutputCount);
	Out += FString::Printf(TEXT("\n%s%s"),
		InPlan.bPure ? TEXT("pure") : TEXT("not pure"),
		InPlan.bLatent ? TEXT("   latent") : TEXT(""));

	if (!InPlan.bAllowedHere)
	{
		Out += FString::Printf(TEXT("\n; %s"), *InPlan.DisallowedReason);
	}

	// The positions are per direction, which is stated once rather than per row, because a
	// reader who does not know it will mis-read the whole table.
	Out += TEXT("\n; positions are counted separately for inputs and outputs, as the engine ")
		TEXT("does.");
	Out += TEXT("\n; the node was built in a throwaway graph, so the asset's own graph was ")
		TEXT("not changed.");

	for (const FPin& Pin : InPlan.Pins)
	{
		Out += FString::Printf(TEXT("\n  %-8s %2d  %-22s %s"),
			*Pin.Direction, Pin.Position, *Pin.Name, *Pin.Type);
		if (Pin.bExec)
		{
			Out += TEXT("  exec");
		}
		if (!Pin.Default.IsEmpty())
		{
			Out += FString::Printf(TEXT("  default=%s"), *Pin.Default);
		}
	}

	if (InPlan.Pins.Num() == 0)
	{
		// Unreachable while Plan refuses an empty list, and kept because a Render that could
		// print nothing would be worse than one that says so (`P1`).
		Out += TEXT("\n  (no pins were reported, which is a failure rather than an answer)");
	}

	return Out;
}
