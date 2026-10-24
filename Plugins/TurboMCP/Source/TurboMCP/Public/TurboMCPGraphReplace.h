// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "TurboMCPGraphAppend.h"

class UBlueprint;
class UEdGraph;

/** Plans and applies graph-body replacement using Append's parser and writer.
 * Entries, results, events and comments are retained. Other engine-protected nodes
 * must be explicitly retained or the request is refused. Application owns one
 * transaction, verifies the remaining node set, and does not compile or save.
 * Failure attempts owned undo; restoration is not claimed without independent evidence.
 */
class FTurboMCPGraphReplace
{
public:
	/** One node this plan will remove, named so the report can list it. */
	struct FRemoval
	{
		/** The node's key, in the long form `FTurboMCPNodeId` produces. */
		FString Key;

		/** The node class, so a reader can tell a call from a variable get. */
		FString ClassName;

		/** The title an author would recognise, when the node has one. */
		FString Title;
	};

	/** What the plan decided, so `Apply` acts without deciding anything. */
	struct FPlan
	{
		/** The append half: the parse, the refusals, the nodes to create and the links to make. */
		FTurboMCPGraphAppend::FPlan Append;

		/**
		 * THE NODES THAT WILL BE REMOVED, NAMED. This is the whole difference from `APPEND`, and the spec
		 * requires it to be LISTED rather than counted - a count tells a reader how much is going, a list
		 * tells them WHAT.
		 */
		TArray<FRemoval> Removals;

		/** The entry/result nodes that are KEPT because the text writes into them. */
		TArray<FString> ReusedKeys;

		/**
		 * THE NODES THE TEXT DESCRIBES, which is neither a reuse nor a removal, COUNTED.
		 *
		 * Recorded rather than derived, because `NodeCountBefore - Removals.Num()` is NOT this number: that
		 * quantity also holds the re-used entry and result nodes and the comments. The report prints the four
		 * buckets separately, so each one has to be counted rather than inferred from the total.
		 */
		int32 DescribedKeysKept = 0;

		/**
		 * THE COMMENTS KEPT, COUNTED RATHER THAN ONLY SKIPPED.
		 *
		 * A comment is documentation rather than body, so it is never a removal candidate - but it is still a
		 * node the text does not describe, so a report that folds it into "described by the text" is wrong by
		 * exactly this number.
		 */
		int32 CommentKeysKept = 0;

		/** How many nodes the graph had before, so the report can state the change. */
		int32 NodeCountBefore = 0;

		/** Why the text cannot be applied at all, each naming its line. Carried from `Append`. */
		const TArray<FString>& Refusals() const { return Append.Refusals; }

		/** True when the text would be applied. */
		bool bAcceptable() const { return Append.Refusals.Num() == 0; }
	};

	/**
	 * Parses, checks, resolves, and decides what to KEEP and what to REMOVE. WRITES NOTHING.
	 *
	 * THE CHECKER AND THE APPEND PLAN BOTH RUN FIRST, in that order, because `P-GRAPH-CHECK` is the gate every
	 * writer depends on and because reusing `APPEND`'s plan is what keeps the two writers from disagreeing
	 * about the format.
	 *
	 * @param OutError  Why the plan could not even be attempted. Empty on success.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InText,
		FPlan& OutPlan, FString& OutError);

	/**
	 * Applies an accepted plan: ONE transaction that removes the old body and writes the new one, then
	 * verifies the remaining node set. Compilation and saving are separate operations.
	 *
	 * @param OutError  Why it could not be applied at all. Empty when a report was produced.
	 */
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);
private:
    friend class FTurboMCPReplacePlanTest;
    static bool ApplyInternal(const FPlan& InPlan, FString& OutReport, FString& OutError, bool bFailAfterRemoval);
};
