// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * `R-BP-LINT`: what is wrong with this Blueprint, with every check NAMED.
 *
 * *** THE ROW THAT DEFINES THIS CAPABILITY IS ONE SENTENCE, AND ITS SECOND HALF IS THE POINT:
 * "every check named, each reporting how many things it looked at". *** A LINT THAT SAYS "no problems
 * found" WITHOUT A DENOMINATOR IS INDISTINGUISHABLE FROM A LINT THAT LOOKED AT NOTHING, and `P1` says an
 * empty result must say why it is empty. So every check here carries a `Scanned` count BESIDES its
 * findings, and a check that could not run reports `Ran=false` with a reason rather than a zero - the two
 * are different answers and a caller needs to tell them apart.
 *
 * IT REUSES THE EXISTING DETECTORS RATHER THAN WRITING SECOND ONES, WHICH IS WHY ITS ANSWERS AGREE WITH THE
 * TOOLS A CALLER WILL USE TO FIX THEM:
 *
 *  - dead nodes come from `FTurboMCPGraphDead::Plan`, so THE ENTRY RULE IS ALREADY APPLIED and nothing an
 *    event reaches is ever reported. A second walk would eventually disagree with `E-GRAPH-DEAD` about what
 *    is removable, and the two would then send a caller in opposite directions.
 *  - backward getters and origin strays come from `FTurboMCPGraphFixBackward::Plan`, read through its `FMove`
 *    rule identifiers (`R2` is a getter under its consumer, `R3` a lone node at the origin) - THE SAME
 *    PREDICATES `P-GRAPH-FIX-BACKWARD` ACTS ON, so the finding and the fix cannot drift.
 *  - unused declarations are PARSED FROM `FTurboMCPBlueprintRead::RenderRelations`, the relation layer itself,
 *    because `D-05` says print only what the relation layer can PROVE. Recomputing them would be a second
 *    implementation of the thing the reader already answers.
 *  - heavy hard references read the asset registry the same way `R-ASSET-COST` does and report THE SAME
 *    NUMBERS, because a lint that quoted different sizes than the cost tool would be worse than no lint.
 *
 * *** AND ONE TRAP IS INHERITED RATHER THAN REDISCOVERED: `UEdGraphNode_Comment` HAS NO PINS, so a walk from
 * the entry nodes can never reach one, AND TREATING THAT AS DEAD ONCE QUEUED AN AUTHOR'S DOCUMENTATION FOR
 * DELETION. `FTurboMCPGraphDead` already skips comment seeds, and this tool reports whatever THAT says -
 * it does not add a walk of its own that could forget. ***
 *
 * THIS IS A READ TOOL AND IT NEVER EDITS. It lives in the reader toolset, which is structurally unable to
 * write, and every finding that has a fix NAMES THE EXISTING TOOL rather than describing one.
 */
class FTurboMCPBPLint
{
public:
	/** One check's name, as a caller passes it to the filter. Stable identifiers, never numbers. */
	struct FCheckName
	{
		FString Name;
		FString Description;
	};

	/** Every check name this tool knows, in report order. For `checks=list` and for a failed filter (`P1`). */
	static TArray<FCheckName> KnownChecks();

	/** One finding: where it is, how bad, why, and which existing tool fixes it. */
	struct FFinding
	{
		FString Check;

		/** The node key when the finding is about a node; empty when it is about the asset or a declaration. */
		FString NodeKey;

		/** The graph the node is in, so a caller can open it without searching. */
		FString GraphName;

		/** `error`, `warning` or `info`. Never invented: see the per-check comments in the .cpp. */
		FString Severity;

		/** ONE line saying why this is a finding. */
		FString Why;

		/** The existing tool that fixes it, or empty when no fix exists from here. */
		FString FixTool;
	};

	/** One check's whole answer, INCLUDING ITS DENOMINATOR. */
	struct FCheckResult
	{
		FString Name;

		/** False when the check could not run, and then `Reason` says why (`P1`). */
		bool bRan = false;
		FString Reason;

		/**
		 * HOW MANY THINGS THIS CHECK LOOKED AT.
		 *
		 * **THIS FIELD IS THE CAPABILITY, NOT A DETAIL.** Zero findings with a zero denominator is a check
		 * that examined nothing, and the caller must be able to see that. It is what makes "no problems
		 * found" worth believing.
		 */
		int32 Scanned = 0;

		TArray<FFinding> Findings;
	};

	/** The whole report for one Blueprint. */
	struct FReport
	{
		FString Asset;
		TArray<FCheckResult> Checks;

		int32 CheckCount() const { return Checks.Num(); }
	};

	/**
	 * Lints one Blueprint.
	 *
	 * `InChecks` is a comma or space separated filter; EMPTY MEANS EVERY CHECK. A name that matches nothing
	 * is REFUSED BY NAME with the list of names that exist, because `P1` requires a filter that matches
	 * nothing to say so and list what is there.
	 */
	static bool Lint(const FString& InAsset, const FString& InChecks,
		FReport& OutReport, FString& OutError);

	/** Renders the report. Contains each check's name, its denominator, and its findings. */
	static FString Render(const FReport& InReport, int32 InTokenBudget);
};
