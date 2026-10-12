// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterial.h"

/**
 * The four material readers: `R-MAT-LABELS`, `R-MAT-LOAD`, `R-MAT-TRACE` and `R-MAT-BLOCKS`.
 *
 * ONE RENDERER, FOUR VIEWS. Every printing decision lives in `FTurboMCPMaterial`, so these functions
 * are only the boundary: resolve, refuse by name, or print. That is the same shape `P-COMMENT` uses.
 *
 * `R-MAT-TRACE` SHARES ITS PRINTING CODE WITH `R-MAT-LOAD`, which `03_TOOLS.md` requires by name:
 * "Shares its printing code with R-MAT-LOAD, so the two cannot disagree." Both call `RenderTree` and
 * differ in one argument, so they cannot disagree about the dialect, the label rule or the shared-value
 * rule - only about which outputs are printed.
 *
 * THE ERROR PREFIX IS ADDED IN EXACTLY ONE PLACE. `FTurboMCPMaterial::Plan` fills a bare sentence,
 * while `FTurboMCPAssetResolver::Resolve` - which `Plan` passes through - fills one that already
 * carries `error:`. Two layers each assuming the other leaves the prefix off is how a result ends up
 * reading `error: error: ...`, so `AsError` checks before adding.
 */
namespace
{
	/** `error: <what failed>`, with the prefix added exactly once. */
	FString AsError(const FString& InError)
	{
		if (InError.StartsWith(TEXT("error:")))
		{
			return InError;
		}
		return FString::Printf(TEXT("error: %s"), *InError);
	}

	/**
	 * Reads the asset once and renders it for the given kind, so the four tools share one path.
	 *
	 * `InTokenBudget` of zero means "no cap given", which `BudgetNote` treats as "print the size but
	 * do not reduce". A budget is a tool argument rather than a plan field because the plan is about
	 * WHAT was read and the budget is about HOW MUCH to print, and a plan reused across two calls
	 * with different budgets must not carry the first one's.
	 */
	FString ReadMaterial(const FString& InAsset, FTurboMCPMaterial::ERead InKind,
		const FString& InOutputProperty, bool bInIncludeInstanceOverrides, int32 InTokenBudget)
	{
		FTurboMCPMaterial::FPlan Plan;
		FString Error;
		if (!FTurboMCPMaterial::Plan(InAsset, InKind, InOutputProperty,
				bInIncludeInstanceOverrides, Plan, Error))
		{
			return AsError(Error);
		}

		switch (InKind)
		{
		case FTurboMCPMaterial::ERead::Labels:
			// A label report is a short list by construction - one line per label plus its readers -
			// so a budget would have nothing to reduce. It still reports its size.
			return FTurboMCPMaterial::RenderLabels(Plan);

		case FTurboMCPMaterial::ERead::Trace:
			return FTurboMCPMaterial::RenderTree(Plan, /*bTraceOnly=*/true, InTokenBudget);

		case FTurboMCPMaterial::ERead::Blocks:
			return FTurboMCPMaterial::RenderBlocks(Plan, InTokenBudget);

		case FTurboMCPMaterial::ERead::Load:
		default:
			return FTurboMCPMaterial::RenderTree(Plan, /*bTraceOnly=*/false, InTokenBudget);
		}
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialLabels(const FString& Asset)
{
	// `03_TOOLS.md` says to call this first on any material, so it takes no output property and no
	// budget: the answer is short and an agent that had to pass anything would be tempted to skip it.
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Labels, FString(),
		/*bInIncludeInstanceOverrides=*/false, /*InTokenBudget=*/0);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialLoad(
	const FString& Asset, bool bIncludeInstanceOverrides, int32 TokenBudget)
{
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Load, FString(),
		bIncludeInstanceOverrides, TokenBudget);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialTrace(
	const FString& Asset, const FString& OutputProperty, int32 TokenBudget)
{
	// The output property is required, and `Plan` refuses an empty or unconnected one by listing the
	// connected ones rather than by printing an empty tree.
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Trace, OutputProperty,
		/*bInIncludeInstanceOverrides=*/false, TokenBudget);
}

FString UTurboMCPReadToolset::ReadTurboMCPMaterialBlocks(const FString& Asset, int32 TokenBudget)
{
	return ReadMaterial(Asset, FTurboMCPMaterial::ERead::Blocks, FString(),
		/*bInIncludeInstanceOverrides=*/false, TokenBudget);
}

