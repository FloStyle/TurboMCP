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
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"

class UBlueprint;
class UEdGraph;

/**
 * Analyze repeatedly consumed pure-node outputs in function graphs.
 * Promotion supports reviewed scalar arithmetic with stable input ancestry.
 * It retains calculations, assigns at function entry and replaces consumer reads.
 * Unsupported value lifetimes and execution entry shapes are refused.
 */
class FTurboMCPLocalVars
{
public:
	/** One candidate value. */
	struct FCandidate
	{
		/** Short display key; never sufficient to identify a node for mutation. */
		FString Key;

        /** Full node and output identity: different outputs must never share a local. */
        FGuid NodeGuid;
        FName OutputPinName;

		/** The node's title, so a reader knows what the value is. */
		FString Title;

		/** The node's class. */
		FString ClassName;

		/** How many distinct input pins consume this output value. */
		int32 ConsumerCount = 0;

		/** True when reviewed arithmetic and input-ancestry checks pass; execution placement is still required. */
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

	/** Lists candidate outputs, exclusions and edit prerequisites. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Replans against live state, assigns supported values, compiles and verifies links; does not save. */
	static FString Apply(const FPlan& InPlan);
};
