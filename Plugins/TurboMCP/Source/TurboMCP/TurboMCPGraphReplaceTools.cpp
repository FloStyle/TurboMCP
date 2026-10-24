// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphReplace.h"

// Both tools plan from current state; Apply also checks edit guards and removal targets.
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
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their lines, the first line is
		// `unchanged`, and the render says explicitly that NOTHING WOULD BE REMOVED - which a caller of the
		// only destructive writer in the plugin needs to be told rather than left to infer.
		return FTurboMCPGraphReplace::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphReplace::Apply(Plan, Report, ApplyError))
	{
		// `Apply` reports its own failures with the first line `changed` or `unchanged`, because the spec
		// requires a failed write to SAY it failed rather than to look like a success. Only a failure to even
		// reach the graph comes back as an `error:` line.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
