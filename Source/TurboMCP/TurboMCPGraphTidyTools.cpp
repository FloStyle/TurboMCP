// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphTidy.h"

/**
 * The two tool bodies for `P/E-GRAPH-TIDY`.
 *
 * THE PREVIEW AND THE EDIT SHARE ONE CODE PATH UP TO THE WRITE (`D-02`). Both call `FTurboMCPGraphTidy
 * ::Plan` and then differ by ONE call - `RenderPreview` or `Apply`. Every refusal is decided inside
 * `Plan`, so a Preview can never offer a plan the Edit then declines, which is a disagreement this
 * project has been caught by twice.
 *
 * THE DECLARATIONS LIVE IN THE ORCHESTRATOR'S HEADER. This file was gated while they were absent, because
 * a `UFUNCTION` declaration whose body is missing fails the whole module at LINK and breaks every other
 * workstream's build - the knots workstream did exactly that once. The gate is gone now that both
 * declarations are present, which is why this file is live rather than conditional.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}
	return FTurboMCPGraphTidy::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}
	return FTurboMCPGraphTidy::Apply(Plan);
}
