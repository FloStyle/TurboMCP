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

/**
 * `P/E-GRAPH-PATCH`: change only what changed.
 *
 * WHAT THE SPEC REQUIRES, VERBATIM FROM `03_TOOLS.md`:
 *   "Reads the live graph, matches the submitted text by node ID, applies only the differences. Nodes the
 *    text does not mention are left alone."
 *
 * AND WHAT ITS ACCEPTANCE ROW REQUIRES, FROM `06_ACCEPTANCE.md`: `T-WRITE-5` - "Changing two lines changes
 * exactly two nodes". The letter of that is as important as the spirit: A PATCH THAT TOUCHES A THIRD NODE HAS
 * FAILED EVEN IF THE GRAPH STILL COMPILES, so the change is measured by COMPARING NODE IDS AND PIN VALUES
 * BEFORE AND AFTER rather than by reading the tool's own summary.
 *
 * *** IT SHARES EVERYTHING WITH `APPEND` RATHER THAN REIMPLEMENTING ANY OF IT. *** The parse, the one-dialect
 * lexing, the checker gate, the type-id resolution, the link planning and the refusal wording are all
 * `FTurboMCPGraphAppend::Plan`'s, and this class CALLS it. A second parser would be a second dialect, which
 * this project has recorded as a bug twice.
 *
 * THE ONE THING `APPEND` CANNOT DO, AND THEREFORE THE WHOLE OF THIS CAPABILITY. `APPEND`'s plan resolves a form
 * that names a node ALREADY IN THE GRAPH into a LINK TARGET and nothing else - `FTurboMCPGraphAppend::Apply`
 * literally `continue`s past such a form, so no pin value written under it is ever applied. That is CORRECT for
 * append, whose spec says it "Cannot remove anything" and whose job is to add. IT IS EXACTLY THE GAP PATCH
 * FILLS: the text names an existing node BY ID and gives its pins, and the difference between what the graph
 * holds and what the text says is what gets written.
 *
 * SO IT REUSES `APPEND`'s `Plan` AND DOES ITS OWN `Apply`, and that is a deliberate division rather than an
 * omission: routing the update through `APPEND`'s apply would mean teaching a tool whose contract is "add" to
 * also modify, and the cheapest way to keep append honest is to leave that code path absent from it.
 *
 * WHAT IT DOES NOT DO, EACH BECAUSE THE SPEC GIVES IT TO ANOTHER TOOL:
 *   - IT CREATES NO NODES. A form with no id is REFUSED BY NAME and told which tool adds nodes, because
 *     "applies only the differences" is not "adds a block" - that is `P/E-GRAPH-APPEND`;
 *   - IT REMOVES NO NODES. A node the text does not mention is LEFT ALONE, and a node the text does mention
 *     keeps its place even when the text describes it differently - removing is `P/E-GRAPH-REPLACE`, and
 *     "Nodes the text does not mention are left alone" would be a strange sentence in a tool that also removes.
 */
class FTurboMCPGraphPatch
{
public:
	/** One existing node the text describes, and the pins whose value it changes. */
	struct FUpdate
	{
		/** The node's key, in the long form `FTurboMCPNodeId` produces. */
		FString Key;

		/** The class the graph reports, for the report. */
		FString ClassName;

		/** The pins the text wrote a value for, in order. Links are planned separately. */
		TArray<TurboMCPGraphTextLex::FPinForm> Pins;

		/** The type id the text wrote, so a mismatch against the graph can be refused rather than ignored. */
		FString TypeId;

		/** 1-based line of the form's head, so a refusal can point at it. */
		int32 Line = 0;
	};

	/** One link the plan will make, reusing `APPEND`'s shape so there is one link vocabulary. */
	using FLink = FTurboMCPGraphAppend::FNewLink;

	/** What the plan decided, so `Apply` acts without deciding anything. */
	struct FPlan
	{
		/** The append half: the parse, the refusals and the links. Its `Nodes` are the matched forms. */
		FTurboMCPGraphAppend::FPlan Append;

		/** The existing nodes whose pins the text changes, one entry per form that survived the checks. */
		TArray<FUpdate> Updates;

		/** Every way the text cannot be applied, as a sentence naming its line. */
		TArray<FString> Refusals;

		/**
		 * Every refusal, INCLUDING THE ONES INHERITED FROM `APPEND`'s PLAN.
		 *
		 * THE INHERITED ONES ARE REAL AND WERE BEING DROPPED. `APPEND` validates the pins of a form that names an
		 * existing node, so it refuses things this class never gets to see: when it refuses, `Plan` returns early
		 * and `Refusals` above is empty. MEASURED, a text writing `NoSuchPin "x"` produced a preview saying
		 * `0 node(s) in the graph now` with NO REFUSAL AT ALL - a preview reporting that nothing is wrong, which
		 * is worse than no preview. Any caller asking "will this be refused" must ask THIS, not `Refusals`.
		 */
		TArray<FString> AllRefusals() const
		{
			TArray<FString> Out = Refusals;
			for (const FString& Refusal : Append.Refusals)
			{
				if (!Out.Contains(Refusal))
				{
					Out.Add(Refusal);
				}
			}
			return Out;
		}

		/** How many nodes the graph had before, so the report can state the change. */
		int32 NodeCountBefore = 0;

		/** True when the text would be applied: no refusals of EITHER kind AND something to change. */
		bool bAcceptable() const { return AllRefusals().Num() == 0; }
	};

	/**
	 * Parses, checks and decides WHICH EXISTING NODES DIFFER FROM THE TEXT. WRITES NOTHING.
	 *
	 * `P-GRAPH-CHECK` and `APPEND`'s plan both run first, in that order, because the checker is the gate every
	 * writer depends on and because reusing `APPEND`'s plan is what keeps the three writers from disagreeing
	 * about the format.
	 *
	 * @param OutError  Why the plan could not even be attempted. Empty on success.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InText,
		FPlan& OutPlan, FString& OutError);

	/**
	 * Applies an accepted plan: ONE transaction that writes only the pin values that DIFFER, then compiles,
	 * reads back and reports exactly which nodes changed.
	 *
	 * @param OutError  Why it could not be applied at all. Empty when a report was produced.
	 */
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);
};
