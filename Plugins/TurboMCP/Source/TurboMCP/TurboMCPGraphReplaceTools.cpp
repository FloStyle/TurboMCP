// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphReplace.h"

// Both tools plan from current state; Apply checks the complete base again just before mutation.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphReplace::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphReplace::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphReplace::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphReplace(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	// Replan rather than trusting an earlier preview.
	FTurboMCPGraphReplace::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphReplace::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (!Plan.bAcceptable())
	{
		return FTurboMCPGraphReplace::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphReplace::Apply(Plan, Report, ApplyError))
	{
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
