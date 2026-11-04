// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/** Read local overrides and effective inherited values without changing the instance.
 * Parameter identity includes type, association and index. The report separates local
 * storage from inherited values and retains orphaned local overrides for diagnosis.
 */
class FTurboMCPMaterialInstance
{
public:
	/** One parameter, on one level of the chain, with its value rendered. */
	struct FParameter
	{
		FString Name;
		FString Association;
		int32 Index = INDEX_NONE;
		FString Identity;
		FString Type;
		FString Value;
		/** The level this parameter belongs to, for an inherited one: the nearest level that declares it. */
		FString DeclaredAt;
	};

	/** One link of the parent chain, nearest first. */
	struct FLevel
	{
		FString Path;
		FString ClassName;
		/** True for the asset that was asked for, false for every ancestor. */
		bool bIsTheAssetAskedFor = false;
	};

	struct FResult
	{
		FString Asset;
		TArray<FLevel> Chain;

		/**
		 * Parameters this instance overrides, WITH their values.
		 *
		 * REPEATED HERE RATHER THAN LEFT TO R-MAT-LOAD, because a reader that returns half its contract forces the
		 * caller to make two calls and join them, AND THE JOIN IS WHERE THE WRONG-ASSET EDIT COMES FROM.
		 */
		TArray<FParameter> Overridden;

		/**
		 * Parameters a level of the chain declares that THIS INSTANCE DOES NOT OVERRIDE, marked as inherited.
		 *
		 * THE LIST THE SPEC SAYS MATTERS, AND THE ONE NOTHING PRINTED BEFORE. DeclaredAt names the level, so an
		 * agent can tell which asset it would have to edit.
		 */
		TArray<FParameter> Inherited;

		/**
		 * Parameters this instance sets that NO level of the chain declares.
		 *
		 * REPORTED RATHER THAN DROPPED, per the spec: "a renamed parameter leaves one behind". An orphaned override
		 * still EXISTS on the instance and still does nothing, so dropping it would HIDE A DEFECT in the asset.
		 */
		TArray<FParameter> Orphaned;
	};

	/**
	 * Read one material instance.
	 *
	 * RETURNS FALSE AND FILLS OutError RATHER THAN AN EMPTY RESULT, for the reason D-43 gives in the other
	 * direction: a reader that cannot tell "this asset has no overrides" from "this asset is not an instance" is a
	 * reader an agent will edit the wrong asset over. OutError STARTS WITH error: so a caller tests one prefix
	 * rather than matching a sentence.
	 */
	static bool Read(const FString& InAsset, FResult& OutResult, FString& OutError);

	/** Render the result as the text the tool returns. */
	static FString Render(const FResult& InResult);

	/**
	 * The parameter types this reader enumerates.
	 *
	 * NAMED RATHER THAN IMPLIED, because the contract is about "every parameter the parent has" and a reader that
	 * silently covered only scalars would satisfy the sentence while missing most parameters. Render states which
	 * types were covered and how many LEVELS were looked at, ON THE MODEL R-BP-LINT SETTLED: A COUNT WITH NO
	 * DENOMINATOR CANNOT BE TOLD FROM A READER THAT LOOKED AT NOTHING.
	 */
	static const TArray<FString>& CoveredTypes();
};
