// Copyright (c) 2026 FloStyle
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

/** Static Blueprint diagnostics with per-check coverage and explicit unavailable results.
 * Findings are review suggestions, not runtime proof or permission to delete content.
 * Dead-node and layout checks reuse production planners. Declaration usage uses
 * collected Blueprint facts; external and dynamic uses can remain unknown.
 * Package-size checks use saved registry disk sizes, not runtime memory.
 * Lint may load assets but does not request graph edits, compilation or saving.
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

	/** One finding: where it is, how bad, why, and which existing tool can inspect or address it. */
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
