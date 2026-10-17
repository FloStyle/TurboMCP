// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphPatch.h"

/**
 * `P-GRAPH-PATCH` and `E-GRAPH-PATCH`, the pair as `03_TOOLS.md` requires them.
 *
 * THE DECISION IS MADE IN `FTurboMCPGraphPatch::Plan`, NOT HERE. These two functions render or apply what that
 * returned and decide nothing themselves, which is what makes a Preview and its Edit twin refuse the SAME input -
 * the rule this project requires of every pair.
 *
 * THE PREVIEW PRINTS THE COUNT THAT `T-WRITE-5` IS ABOUT. Its acceptance row says "Changing two lines changes
 * exactly two nodes", so a caller needs to see HOW MANY nodes would change - and which - BEFORE committing,
 * because a patch that touches a third node is a failure even when the graph still compiles. The preview states
 * that count, and the apply MEASURES it by comparing the node ids before and after rather than restating it.
 *
 * THE TWO LIVE IN DIFFERENT TOOLSET CLASSES, AND THAT IS THE SAFETY MODEL RATHER THAN A LABEL. Every write can
 * only be written in `UTurboMCPEditToolset`, so a read tool that tried to write would not compile. The preview
 * half is in `UTurboMCPPreviewToolset`, which holds no tool that can change an asset.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphPatch(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphPatch::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphPatch::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphPatch::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphPatch(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	// THE PLAN IS BUILT ONCE, HERE, AND THE DECISION IS ITS. Everything the edit refuses, the preview refused
	// first with the same words, because both reach this same construction.
	FTurboMCPGraphPatch::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphPatch::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (!Plan.bAcceptable())
	{
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their lines, the first line is
		// `unchanged`, and the render says explicitly that NOTHING WOULD BE WRITTEN.
		return FTurboMCPGraphPatch::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphPatch::Apply(Plan, Report, ApplyError))
	{
		// `Apply` reports its own failures with the first line `changed` or `unchanged`, because the spec
		// requires a failed write to SAY it failed rather than to look like a success. Only a failure to even
		// reach the graph comes back as an `error:` line.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
