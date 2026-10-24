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
 * Analyze repeatedly consumed pure-node outputs in function graphs.
 * Input ancestry excludes mutable variable reads and impure sources, but does not
 * prove determinism or safe evaluation timing. Promotion is currently refused:
 * calculated-value assignment and execution placement are not implemented.
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

		/** True when input ancestry passes the exclusions; not proof of semantic safety. */
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

		/** True when the graph supports local variables. */
		bool bSupportsLocals = false;
		FString NoLocalsReason;
	};

	/** Reads the function graph and checks candidate input ancestry. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		FPlan& OutPlan, FString& OutError);

	/** Lists candidates, exclusions and the current promotion limitation. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Refuses nonempty promotion plans until value-preserving assignment is implemented. */
	static FString Apply(const FPlan& InPlan);
};
