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
class UEdGraphNode;

/**
 * Conservative static cleanup of disconnected Blueprint node groups.
 * Candidates contain a recognized call or variable write, contain no entry,
 * and write no variable used by reachable nodes anywhere in the Blueprint.
 * Comments and wire knots are excluded. Execution and data dependencies both
 * contribute to reachability; this heuristic does not prove runtime behavior
 * or that a user wants a disconnected group removed.
 * Preview reports selection reasons. Apply revalidates the selection before
 * mutation, removes nodes in one undo transaction, and never compiles or saves.
 */
class FTurboMCPGraphDead
{
public:
	/** One disconnected group of nodes. */
	struct FGroup
	{
		/** The keys of the nodes in it, sorted, so a report is stable. */
		TArray<FString> Keys;

		/** The node classes present, so a reader can see what the group does. */
		TArray<FString> Classes;

		/** Variables this group writes. */
		TArray<FString> Writes;

		/** Functions it calls, by name. */
		TArray<FString> Calls;

		/** True when nothing outside the group reads a variable it writes. */
		bool bWritesAreUnread = true;

		/** True when the group cannot be reached from any entry by any wire. */
		bool bUnreachable = true;

		/** True when the group holds an event, a custom event or a function entry. */
		bool bHoldsEntry = false;

		/** True when the same call or variable access also happens in live code. */
		bool bRepeatsLiveLogic = false;

		/** What the live code does that this group repeats, for the message. */
		TArray<FString> RepeatedItems;

		/** A one-line description, for the report. */
		FString Summary;
	};

	/** The parsed and checked request. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;

		/** Every group found, whether or not it is a candidate. */
		TArray<FGroup> Groups;

		/** The indices of the groups the Edit would remove. */
		TArray<int32> CandidateIndices;

		int32 NodeCountBefore = 0;
	};

	/** Reads the graph and selects groups matching the conservative removal heuristic. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		FPlan& OutPlan, FString& OutError);

	/** Lists every group, says whether each repeats live logic, and what would be removed. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Removes the candidates and reports the node counts. */
	static FString Apply(const FPlan& InPlan);
};
