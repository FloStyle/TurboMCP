// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPLocalVars.h"

/**
 * `P-LOCAL-VARS` and `E-LOCAL-VARS`.
 *
 * ONE CODE PATH, TWO OUTCOMES, and here the shared half matters more than usual: the SAFETY decision
 * lives in `Plan`, so the Preview states the reason and the Edit promotes only what the Preview
 * called safe. An Edit that judged for itself could promote a value the Preview had just refused,
 * which is the disagreement `D-02` exists to prevent.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPLocalVars(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPLocalVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPLocalVars::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPLocalVars::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPLocalVars(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPLocalVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPLocalVars::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPLocalVars::Apply(Plan);
}
