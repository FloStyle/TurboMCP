// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * The reader behind `R-BP-LOAD`, the main Read tool.
 *
 * `03_TOOLS.md` calls it "one call gives an agent everything it needs to plan a
 * change", so it is built from the shared pieces rather than its own:
 * `FTurboMCPGraphText` writes the bodies, `TurboMCPNodeId` names the nodes, and
 * `FTurboMCPAssetResolver` resolves the assets. That is `04_FORMATS.md` section
 * 1's rule: a format is defined once and used everywhere, because most bugs came
 * from two tools disagreeing about one.
 *
 * The four detail levels are strictly nested, so `bodies` contains everything
 * `signatures` prints and more. A caller that steps down a level loses detail but
 * never loses a section.
 */
class FTurboMCPBlueprintRead
{
public:
	/** How much of a Blueprint to print. */
	enum class EDetail : uint8
	{
		/** Asset path, class, parent, interfaces, counts, and the names of every graph, variable and component. */
		Manifest,
		/** Manifest, plus every variable in full, every function and event signature, the relation layer. No bodies. */
		Signatures,
		/** Signatures, plus every graph body in the graph text format. */
		Bodies,
		/** Bodies, plus the editing details: node classes and pin indexes. */
		Full,
	};

	static bool ParseDetail(const FString& InName, EDetail& OutDetail);
	static FString DetailNames();

	/** True when this level prints graph bodies. */
	static bool PrintsBodies(EDetail InDetail);

	/** True when this level prints the relation layer. */
	static bool PrintsRelations(EDetail InDetail);

	/**
	 * Everything one call needs.
	 *
	 * Collected into a struct rather than passed as eight arguments, because the
	 * graph filter and the budget interact: when the budget is passed, whole
	 * graphs are dropped, and the caller has to be told which and why.
	 */
	struct FRequest
	{
		EDetail Detail = EDetail::Manifest;

		/** A list of name fragments. A graph matches when its name contains one. */
		TArray<FString> GraphFilter;

		/**
		 * The soft ceiling on the result, in characters.
		 *
		 * Zero means no ceiling. `03_TOOLS.md` requires that passing it steps the
		 * detail level down per graph, biggest first, and that the result ends by
		 * naming what was reduced and the exact call that gets the rest.
		 */
		int32 TokenBudgetChars = 0;
	};

	/** What one call produced, and what it had to leave out. */
	struct FResult
	{
		FString Text;

		/** The level actually used, which may be lower than the one asked for. */
		EDetail DetailUsed = EDetail::Manifest;

		/** True when the budget forced a reduction. */
		bool bReduced = false;

		/** One line per reduction, naming what was left out and the call that gets it. */
		TArray<FString> Reductions;

		/** Anything that could not be printed, as an honest note rather than a gap. */
		TArray<FString> Notes;
	};

	/**
	 * Reads one Blueprint at the requested level.
	 *
	 * Never modifies the Blueprint. This is the Read half of the plugin and `P3`
	 * says a Read tool cannot change anything.
	 */
	static FResult Read(const UBlueprint* InBlueprint, const FRequest& InRequest);

	/** The graphs of a Blueprint, in a stable order. */
	static void CollectGraphs(const UBlueprint* InBlueprint, TArray<UEdGraph*>& OutGraphs);

	/**
	 * The graphs whose name matches the filter.
	 *
	 * An empty filter matches everything, which is what makes the filter optional
	 * rather than surprising.
	 */
	static bool GraphMatchesFilter(const FString& InGraphName,
		const TArray<FString>& InFilter);

	/**
	 * The events and functions a Blueprint carries, as name-plus-signature lines.
	 *
	 * Public because the JSON writer is a second rendering of the same facts and must not
	 * collect them its own way: a signature spelled two different ways in two renderings is
	 * the drift `04_FORMATS.md` warns about, even when both spellings are right. The text
	 * writer calls this too, so there is one collection and two renderings.
	 */
	static void CollectEventsAndFunctions(const UBlueprint* InBlueprint,
		TArray<FString>& OutEvents, TArray<FString>& OutFunctions);

private:
	/** The asset path, class, parent, interfaces and counts. */
	static FString RenderManifest(const UBlueprint* InBlueprint);

	/** Variables in full: type, default, category and flags. */
	static FString RenderVariables(const UBlueprint* InBlueprint);

	/** Every function and event with its signature. */
	static FString RenderSignatures(const UBlueprint* InBlueprint);

	/**
	 * The relation layer: call graph, variable map, assets touched, dead code.
	 *
	 * @param InTotalGraphCount  How many graphs the Blueprint has, before the
	 *        filter. When it is more than InGraphs.Num() the section says so,
	 *        because a filtered read that silently omits a relation looks like the
	 *        relation does not exist.
	 */
	static FString RenderRelations(const UBlueprint* InBlueprint,
		const TArray<UEdGraph*>& InGraphs, int32 InTotalGraphCount,
		TArray<FString>& OutNotes);

	/** Every graph body, using the shared graph text writer. */
	static FString RenderBodies(const UBlueprint* InBlueprint,
		const TArray<UEdGraph*>& InGraphs, EDetail InDetail,
		TArray<FString>& OutNotes);

public:
	/**
	 * A variable's type as the text an agent can write back.
	 *
	 * Public for the same reason `CollectEventsAndFunctions` is: the JSON writer renders
	 * the same type and must spell it the same way. Two spellings of one type is the drift
	 * `04_FORMATS.md` warns about.
	 */
	static FString DescribeVariableType(const struct FEdGraphPinType& InType);

	/** A function or event's inputs and outputs, with direction marked. */
	static FString DescribeSignature(const UFunction* InFunction);
};
