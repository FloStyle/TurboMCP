// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphKnots.h"

// Both calls plan against current state; Apply can still fail during mutation.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphKnots(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphKnots::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphKnots::Plan(Asset, GraphName, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphKnots::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphKnots(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphKnots::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphKnots::Plan(Asset, GraphName, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphKnots::Apply(Plan);
}
