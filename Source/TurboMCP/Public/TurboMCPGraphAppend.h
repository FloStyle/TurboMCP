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
#include "TurboMCPGraphTextLex.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * `P/E-GRAPH-APPEND`: add a block to a graph from graph text.
 *
 * WHAT MAKES THIS APPEND AND NOT REPLACE, AND IT IS STRUCTURAL RATHER THAN A POLICY: this file has no
 * code path that removes a node. There is no `RemoveNode`, no `Modify` that drops anything, and no
 * branch that deletes. `Spec/03_TOOLS.md` says of this capability "Cannot remove anything", and the
 * cheapest way to keep that true is for the removal to be ABSENT rather than forbidden - a guard can
 * be deleted by a later edit, an absent function cannot be called by accident.
 *
 * THE TEXT IS NOT LEXED HERE. `FTurboMCPGraphTextLex` holds the one set of rules, and this consumes
 * them. A second lexer is how a second dialect appears, which the spec forbids in its first rule for
 * every write entry point, and this project has already recorded two-tools-one-format bugs twice.
 *
 * THE CHECKS ARE IN `Plan` AND NOT IN `Apply`, so a Preview and its Edit twin refuse the same input.
 * `Apply` consumes a `FPlan` and never re-decides anything.
 */
class FTurboMCPGraphAppend
{
public:
	/** One node the plan will create. */
	struct FNewNode
	{
		/** The type id from the head of the form. */
		FString TypeId;

		/** The node id the form carried, or empty when the form describes a NEW node. */
		FString ExistingNodeId;

		/** True when this form names a node that is already in the graph, so it is linked to, not made. */
		bool bLinksToExisting = false;

		/** 1-based line of the head, so a refusal can point at it. */
		int32 Line = 0;

		/** 1-based column of the type id. */
		int32 Column = 0;

		/** The pins as written, in order. */
		TArray<TurboMCPGraphTextLex::FPinForm> Pins;

		/** Filled by `Plan` when the type id resolves: the node class the engine will build. */
		FString ResolvedClass;

		/**
		 * The pins the node will HAVE, read off the probe `Plan` built and then removed.
		 *
		 * WHY A FORM NEEDS THIS. A link is `(-> Node:Pin)` and its SOURCE may be a node this same text creates,
		 * which does not exist yet when `Plan` runs. The probe is the only place the pin set of a not-yet-created
		 * node can be READ while still keeping `Plan` free of any change to the asset, so the names are CAPTURED
		 * here and the link is validated against them. Without it, a link onto a created node could only be
		 * refused at apply time, and `P1` requires the Preview and the Edit to refuse the SAME input.
		 */
		TArray<FString> ProbePinNames;
	};

	/** One link the plan will make, by node id or by a node this same text creates. */
	struct FNewLink
	{
		/**
		 * EACH END IS EITHER A NODE ALREADY IN THE GRAPH OR A FORM THIS SAME TEXT CREATES, and which one it is is a
		 * FIELD RATHER THAN A GUESS. The earlier shape carried `bUsesCreatedNode`, a SINGLE BOOL FOR A TWO-SIDED
		 * FACT, so a link FROM a created node TO an existing one was indistinguishable from the reverse, and the
		 * apply step would have had to re-derive what the plan already knew.
		 */
		FString FromNodeKey;
		int32 FromFormIndex = INDEX_NONE;
		FString FromPin;

		FString ToNodeKey;
		int32 ToFormIndex = INDEX_NONE;
		FString ToPin;

		/** 1-based line of the pin value that wrote the link, so a refusal can point at it. */
		int32 Line = 0;

		/** 1-based column of the pin value. */
		int32 Column = 0;
	};

	/** What the plan decided, so `Apply` can act without deciding anything. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;

		/** Every form the text contains, in order. */
		TArray<FNewNode> Nodes;

		/** Every link the text describes. */
		TArray<FNewLink> Links;

		/** Every way the text cannot be applied, as a sentence naming its line. */
		TArray<FString> Refusals;

		/** How many nodes the graph had before, so the report can state the change. */
		int32 NodeCountBefore = 0;

		/** True when there is nothing to do: no forms at all, or every form is a link to an existing node. */
		bool bNothingToDo = false;

		/** True when the text would be applied: no refusals AND something to do. */
		bool bAcceptable() const { return Refusals.Num() == 0 && !bNothingToDo; }
	};

	/**
	 * Parses, checks, resolves and decides. WRITES NOTHING, so a Preview and an Edit twin behave alike.
	 *
	 * THE CHECKER IS CALLED FIRST. `P-GRAPH-CHECK` is the gate every writer depends on, and this plan
	 * refuses whatever the checker refuses rather than re-deciding the same questions differently.
	 *
	 * @param OutError  Why the plan could not even be attempted, such as a missing asset. Empty on success.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InText,
		FPlan& OutPlan, FString& OutError);

	/**
	 * Applies an accepted plan: one transaction, create, link, lay out, compile, then read back and compare.
	 *
	 * THE CALLER MUST HAVE CALLED `Plan` AND CHECKED `bAcceptable`. This function does not re-decide, and it
	 * returns a report whose FIRST LINE is `changed` or `unchanged` as the spec requires.
	 *
	 * @param OutError  Why it could not be applied at all. Empty when a report was produced.
	 */
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);
};
