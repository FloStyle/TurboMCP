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

class UBlueprint;
class UEdGraph;

/**
 * Promoting a value to a local variable, behind `P-LOCAL-VARS` and `E-LOCAL-VARS`.
 *
 * `03_TOOLS.md` gives two halves and one definition that is easy to get wrong:
 *
 *  - the Preview lists values computed by PURE nodes and used MORE THAN ONCE in a function, each
 *    marked safe or unsafe WITH THE REASON;
 *  - the Edit promotes ONLY the values the Preview marked safe;
 *  - and SAFE MEANS everything feeding the value is a function input or a literal, so capturing it
 *    once cannot change the result.
 *
 * WHY THE SAFETY RULE IS "INPUT OR LITERAL" AND NOT SOMETHING WEAKER
 * ------------------------------------------------------------------
 * Capturing a value once and reading it twice is only the same as computing it twice when the value
 * CANNOT CHANGE between the two reads. A value derived from a member variable can change if the exec
 * chain between the two reads writes that variable; a value derived from a function call can return
 * something different each time. Only a value derived from the function's own inputs and from
 * literals is fixed for the whole call, so only that is safe.
 *
 * The rule is applied TRANSITIVELY: a pure node fed by another pure node is safe only when THAT one
 * is safe, all the way back to an input or a literal. A one-level check would call
 * `A(input) -> B(A) -> used twice` safe for B without ever looking at A.
 *
 * WHY ONLY FUNCTION GRAPHS
 * ------------------------
 * A local variable belongs to a function. `FBlueprintEditorUtils::DoesSupportLocalVariables` decides
 * which graphs allow one, and the answer is checked rather than assumed: an event graph has no
 * locals, so a value used twice there is reported as unsafe with that as the reason rather than
 * being silently skipped.
 */
class FTurboMCPLocalVars
{
public:
	/** One candidate value. */
	struct FCandidate
	{
		/** The key of the pure node that computes it. */
		FString Key;

		/** The node's title, so a reader knows what the value is. */
		FString Title;

		/** The node's class. */
		FString ClassName;

		/** How many other nodes read the value. */
		int32 ConsumerCount = 0;

		/** True when every input traces back to a function input or a literal. */
		bool bSafe = false;

		/** Why it is unsafe, when it is. Empty when safe. */
		FString UnsafeReason;

		/** Where the value comes from, so a reader can check the reasoning. */
		TArray<FString> FeedsFrom;

		/** The name the local variable would take. */
		FString ProposedName;

		/** The position of the node, for the getter that would replace it. */
		double X = 0.0;
		double Y = 0.0;

		/** The node's pin type, carried so the variable matches. */
		FString TypeText;
	};

	/** The parsed and checked request. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;
		TArray<FCandidate> Candidates;
		int32 NodeCountBefore = 0;

		/** True when the graph cannot hold local variables, with the reason. */
		bool bSupportsLocals = false;
		FString NoLocalsReason;
	};

	/** Reads the function graph and works out which values are safe to promote. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		FPlan& OutPlan, FString& OutError);

	/** Lists every candidate with its safety and the reason. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Promotes the safe candidates and reports what it made. */
	static FString Apply(const FPlan& InPlan);
};
