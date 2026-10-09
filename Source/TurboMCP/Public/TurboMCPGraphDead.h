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

/**
 * Dead copies, behind `P-GRAPH-DEAD` and `E-GRAPH-DEAD`.
 *
 * `03_TOOLS.md` names the shape precisely, and each clause is a way to be wrong:
 *
 *  - candidates are DISCONNECTED GROUPS that cannot be reached from any entry AND whose writes are
 *    never read;
 *  - NEVER a group that holds an event, a custom event or a function entry;
 *  - the Preview says for each group whether it REPEATS LIVE LOGIC;
 *  - the Edit removes them and then reports node counts.
 *
 * WHY "REACHABLE" MEANS BOTH EXEC AND DATA, AND WHY THAT IS THE SAFE READING
 * -------------------------------------------------------------------------
 * A node with no exec wire reaching it can still be live if the value it computes is read by a node
 * that IS live: pulling a pure node into a graph is how a Blueprint computes anything. So a group is
 * reachable when ANY of its nodes is reachable by any wire, exec or data. Judging liveness on exec
 * alone would delete pure nodes that live code depends on, which is the worst outcome this tool can
 * produce. The stricter reading is the only safe one, and the cost is that this tool errs toward
 * keeping things.
 *
 * "WHOSE WRITES ARE NEVER READ" IS CHECKED ACROSS THE WHOLE BLUEPRINT, not only inside the group.
 * A group that writes a variable another graph reads is live, because removing it would change what
 * that other graph sees. The earlier plan of looking only inside the group would have deleted it.
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

	/** Reads the graph and works out which groups are safe to remove. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		FPlan& OutPlan, FString& OutError);

	/** Lists every group, says whether each repeats live logic, and what would be removed. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Removes the candidates and reports the node counts. */
	static FString Apply(const FPlan& InPlan);
};
