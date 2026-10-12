// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPGraphReplace.h"

/**
 * `P-GRAPH-REPLACE` and `E-GRAPH-REPLACE`, the pair as `03_TOOLS.md` requires them.
 *
 * THE DECISION IS MADE IN `FTurboMCPGraphReplace::Plan`, NOT HERE. These two functions render or apply what
 * that returned and decide nothing themselves, which is what makes a Preview and its Edit twin refuse the
 * SAME input - the rule this project requires of every pair.
 *
 * THE PREVIEW HALF IS THE ONE THAT MATTERS MOST HERE. This is the first capability in the plugin that can
 * REMOVE nodes, so a caller needs to see the removal list BEFORE committing to it, and the preview prints the
 * list, the class and the title of every node that would go. APPLY RE-CHECKS NOTHING on purpose: a second
 * decision would be a second implementation, and this project has recorded that bug for the graph text and the
 * node ID already.
 *
 * THE TWO LIVE IN DIFFERENT TOOLSET CLASSES, AND THAT IS THE SAFETY MODEL RATHER THAN A LABEL. Every write can
 * only be written in `UTurboMCPEditToolset`, so a read tool that tried to write would not compile. The preview
 * half is in `UTurboMCPPreviewToolset`, which holds no tool that can change an asset.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphReplace::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphReplace::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphReplace::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphReplace(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	// THE PLAN IS BUILT ONCE, HERE, AND THE DECISION IS ITS. Everything the edit refuses, the preview refused
	// first with the same words, because both reach this same construction.
	FTurboMCPGraphReplace::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphReplace::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
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
		return FString::Printf(TEXT("error: %s"), *ApplyError);
	}

	return Report;
}
