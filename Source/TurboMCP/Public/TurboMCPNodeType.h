// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;

/**
 * `P-NODE-TYPE` and `P-NODE-PINS`: the node type ID the engine will actually accept, and the
 * pins that ID produces.
 *
 * WHY THE LIST OF REAL IDS IS BUILT HERE AND NOT CALLED
 * -----------------------------------------------------
 * The engine's own list comes from `UBlueprintGraphEditor::ListAvailableNodes`, whose behaviour
 * this reproduces. It CANNOT BE CALLED: `UBlueprintGraphEditor` is declared
 * `UCLASS(Transient, BlueprintType, HideDropDown)` with NO module API macro
 * (`BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphEditor.h:53-54`), so its
 * methods are not exported and a second module cannot link them. Verified in the generated code:
 * the only `BLUEPRINTEDITORLIBRARY_API` symbols for that class are the reflection
 * constructors.
 *
 * So the ALGORITHM is reproduced from `BlueprintGraphEditor.cpp:1788-1838`, using three
 * primitives that ARE exported as `BLUEPRINTGRAPH_API`:
 * `FBlueprintActionDatabase::GetAllActions` (`BlueprintActionDatabase.h:94`),
 * `FBlueprintActionFilter::IsFiltered` (`BlueprintActionFilter.h:369`) and
 * `FBlueprintNodeSpawner::GetUiSpec` (`BlueprintNodeSpawner.h:198`).
 *
 * THE ID FORMAT IS NOT REPRODUCED - IT IS REUSED. `TurboMCPNodeId::GetNodeTypeId` already owns
 * the rule `03_TOOLS.md` requires, and its own comment records that it deliberately matches the
 * engine's Python `_get_node_type_id`. The action database reports a `Category` and a `MenuName`
 * separately rather than a node, so the same two `Replace(TEXT(" "), TEXT(""))` calls the engine
 * makes are applied here - and the result is checked against the canonical generator for any node
 * that already exists, which is the only way to know the two agree.
 */
class FTurboMCPNodeType
{
public:
	/** One node type the engine will accept in the graph. */
	struct FMatch
	{
		/** The exact ID, `<category>|<menu name>` with whitespace removed. */
		FString TypeId;

		/** The category part, before the last `|`. */
		FString Category;

		/** The title part, after the last `|`. */
		FString Title;

		/** The class that owns the function or variable, when the database names one. */
		FString OwningClass;

		/** True when the entry is owned by this Blueprint rather than by an engine class. */
		bool bOwnedByThisBlueprint = false;

		/** True when the engine reports the node as pure. */
		bool bPure = false;

		/** Why this entry matched, in a few words. */
		FString Reason;

		/** The ID this plugin's graph text uses, when it differs from `TypeId`. */
		FString GraphTextId;

		/**
		 * True when the two IDs differ, which `03_TOOLS.md` asks to be printed.
		 *
		 * A node that already exists in the graph is written by the text writer under the
		 * canonical `TurboMCPNodeId::GetNodeTypeId` form; the engine's creation tool uses the
		 * action database form. Where they differ, the caller needs BOTH: one to match the
		 * graph text, one to create.
		 */
		bool bIdsDiffer = false;
	};

	/** What the caller asked for, resolved. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;

		/** The guess, as written. */
		FString Guess;

		/** True when the caller supplied a node type id rather than plain words. */
		bool bLooksLikeTypeId = false;

		/** The matches, best first, capped at the caller's limit. */
		TArray<FMatch> Matches;

		/** How many matched before the cap. */
		int32 TotalMatches = 0;

		/** The cap that was applied. */
		int32 Limit = 10;

		/** Names added in this session that the engine's list does not carry yet. */
		TArray<FString> SessionOnly;

		/**
		 * True when the asset must be compiled before the engine's creation tool will accept
		 * an ID that this session added. `03_TOOLS.md` requires the tool to SAY so.
		 */
		bool bCompileRecommended = false;

		/** The graph kinds known, for a refusal that names them. */
		TArray<FString> AllTypeIds;
	};

	/** Every node type id the engine will accept in this graph, sorted. */
	static TArray<FString> AvailableTypeIds(UBlueprint* InBlueprint, UEdGraph* InGraph);

	/**
	 * The ID of a node type, in the form `TurboMCPNodeId::GetNodeTypeId` produces.
	 *
	 * Exposed so `P-NODE-PINS` can label the node it built with the id the caller gave, and so
	 * a test can compare the two generators rather than trust that they agree.
	 */
	static FString CanonicalIdOf(const UEdGraphNode* InNode);

	/**
	 * Builds a node of this type in a THROWAWAY graph and returns it, or null.
	 *
	 * The node is created in a transient graph, so the real graph's node count does not change
	 * (`P3`, which `03_TOOLS.md` states as "graph node count must not change"). The caller owns
	 * the result and must not assume the real graph knows about it.
	 *
	 * @param OutNote  Why it could not be built, when it could not. Never empty on failure.
	 */
	static UEdGraphNode* BuildProbeNode(UBlueprint* InBlueprint, UEdGraph* InGraph,
		const FString& InTypeId, FString& OutNote);

	/** What the factory did, so a caller can report WHY rather than only that it failed. */
	enum class ESpawnResult : uint8
	{
		/** A node was built and added to the target graph. */
		Spawned,
		/** No node type in this graph has that id. */
		NoSuchTypeId,
		/** The id exists but the engine's action filter rejects it HERE. */
		FilteredOut,
		/** The id matched and passed the filter, but the spawner declined to build. */
		SpawnerDeclined,
	};

	/**
	 * THE FACTORY: builds a node of this type and ADDS IT TO THE REAL GRAPH.
	 *
	 * WHY THIS EXISTS SEPARATELY FROM `BuildProbeNode`, WHICH IT MIRRORS. `BuildProbeNode` spawns
	 * into a THROWAWAY graph because `P-NODE-PINS` only wants to read the pins and `P3` forbids a
	 * read tool from changing the node count. A WRITER wants the opposite: a real node, in the real
	 * graph. The two differ in ONE argument and in nothing else, which is why the database walk
	 * below is deliberately the same code rather than a second, cleverer one.
	 *
	 * A TYPE ID IS NOT CLASS PLUS FUNCTION, WHICH IS WHY THIS CANNOT BE A LOOKUP. Measured from
	 * `TurboMCPNodeId.h:136-137`, the id is `<category>|<title>` and `:163` records that the title
	 * is LOCALIZED - on a French editor `PrintString` becomes `Afficher`. So the only authority for
	 * "what does this id name" is the engine's own action database and its `GetUiSpec`, which is
	 * what this walks. `FTurboMCPGraphCheck::ResolveTypeId` CANNOT serve here: it is GRAPH-SCOPED,
	 * matching against nodes ALREADY in the graph, so it could never create a type the graph does
	 * not already contain - which is the whole point of an append.
	 *
	 * THE NODE IS ADDED TO `InGraph` AND ITS PINS ARE ALLOCATED, because a node created outside
	 * the editor is never reconstructed for display: a pin accessor on an unallocated node reads an
	 * EMPTY `Pins` array, and that has already caused one crash and one silent corruption here.
	 * The caller still owns the TRANSACTION; this function does not open one.
	 *
	 * @param OutNote  Why it could not be built, when it could not. Never empty unless Spawned.
	 */
	static ESpawnResult SpawnNodeOfType(UBlueprint* InBlueprint, UEdGraph* InGraph,
		const FString& InTypeId, UEdGraphNode*& OutNode, FString& OutNote);

	/** Resolves a guess against the real ids and caps the answer. */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InGuess,
		const FString& InLimit, FPlan& OutPlan, FString& OutError);

	/** The matches, one per line, with the reason each matched. */
	static FString Render(const FPlan& InPlan);
};

/**
 * `P-NODE-PINS`: what pins a node type produces.
 *
 * SEPARATE FROM `FTurboMCPNodeType` because it has a different job: the type finder resolves a
 * NAME, and this builds the node and reports its pins. Folding them together would make one
 * `Plan` carry two unrelated kinds of failure.
 */
class FTurboMCPNodePins
{
public:
	/** One pin, as `03_TOOLS.md` asks it to be reported. */
	struct FPin
	{
		FString Name;

		/** The pin's category and, where it has one, its sub-category object. */
		FString Type;

		/** `EGPD_Input` or `EGPD_Output`, in the spelling the engine's pin tools expect. */
		FString Direction;

		/** The pin's default value, as the engine prints it. */
		FString Default;

		/**
		 * The pin's position among the pins of its own direction.
		 *
		 * Counted SEPARATELY for inputs and outputs, which is what the engine does and what
		 * `07_ENGINE_NOTES.md` section 3.5 records: exec in is input 0 and `self` is input 1.
		 */
		int32 Position = 0;

		/** True when the pin is an exec pin, which is what a caller usually wires first. */
		bool bExec = false;
	};

	/** What the caller asked for, resolved. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;
		FString TypeId;

		/** The node's pins, inputs first then outputs, each group in position order. */
		TArray<FPin> Pins;

		int32 InputCount = 0;
		int32 OutputCount = 0;

		/** True when the engine reports the node as pure. */
		bool bPure = false;

		/**
		 * True when the node is latent, which `03_TOOLS.md` says is not allowed in a function.
		 *
		 * Read from the node class rather than guessed: `UK2Node::IsLatentForMacros` is not the
		 * test - the engine's own test is on the spawned node's class through
		 * `UK2Node_LatentGameplayNode`, so the class ancestry is what is asked.
		 */
		bool bLatent = false;

		/** True when this graph kind accepts the node. */
		bool bAllowedHere = true;

		/** Why it is not allowed here, when it is not. */
		FString DisallowedReason;
	};

	/** Builds the node in a transient graph, reads its pins, and never touches the real graph. */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InTypeId,
		FPlan& OutPlan, FString& OutError);

	/** The pins, with the direction, the position and purity. */
	static FString Render(const FPlan& InPlan);
};
