// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphDead.h"

/**
 * `P-GRAPH-DEAD` and `E-GRAPH-DEAD`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPGraphDead::Plan` finds the unreachable groups and decides
 * which are safe; the Preview lists every group with its reason and the Edit removes the candidates
 * (`D-02`). The three reasons to keep a group are checked in `Plan`, so the PREVIEW states them
 * rather than the edit surprising a caller who had already agreed.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphDead(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphDead::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphDead::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphDead::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphDead(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphDead::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphDead::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphDead::Apply(Plan);
}
