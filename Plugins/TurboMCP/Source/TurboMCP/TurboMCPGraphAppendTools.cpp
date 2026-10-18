// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphAppend.h"

/**
 * `P-GRAPH-APPEND` and `E-GRAPH-APPEND`, the pair as `03_TOOLS.md` requires them.
 *
 * THE DECISION IS MADE IN `FTurboMCPGraphAppend::Plan`, NOT HERE. These two functions render or apply what
 * that returned and decide nothing themselves, which is what makes a Preview and its Edit twin refuse the
 * SAME input - the rule this project requires of every pair. `Apply` re-checks nothing on purpose: a second
 * decision would be a second implementation, and this project has recorded that bug for the graph text and
 * the node ID already.
 *
 * THE TWO LIVE IN DIFFERENT TOOLSET CLASSES, AND THAT IS THE SAFETY MODEL RATHER THAN A LABEL. Every write
 * can only be written in `UTurboMCPEditToolset`, and every read in the Read and Look toolsets, so a read tool
 * that tried to write would not compile. The preview half is in `UTurboMCPPreviewToolset`, which holds no
 * tool that can change an asset.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphAppend::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	// THE PLAN IS BUILT ONCE, HERE, AND THE DECISION IS ITS. Everything the edit refuses, the preview
	// refused first with the same words, because both reach this same construction.
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (!Plan.bAcceptable())
	{
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their lines, and the first line
		// is `unchanged`, so a caller that reads only the first line still learns nothing was written.
		return FTurboMCPGraphAppend::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError))
	{
		// `Apply` reports its own failures with the first line `unchanged`, because the spec requires a failed
		// write to SAY it failed rather than to look like a success. Only a failure to even reach the graph
		// comes back as an `error:` line.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
