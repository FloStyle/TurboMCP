// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPComment.h"

/**
 * `P-COMMENT` and `E-COMMENT`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPComment::Plan` reads the graph, resolves the box or the
 * nodes, and refuses anything it cannot carry out; the Preview prints that plan and the Edit applies
 * exactly it (`D-02`). The "is this a box in this graph" checks live in `Plan` rather than in
 * `Apply` on purpose: a preview that offered a plan the edit would refuse is the disagreement the
 * rule exists to prevent.
 *
 * All four actions — create, set, fit, delete — go through the same two functions, so a new action
 * cannot be half-implemented in one of them.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPComment(
	const FString& Asset, const FString& GraphName, const FString& Action,
	const FString& Nodes, const FString& Box, const FString& Text,
	const FString& Color, const FString& Size)
{
	FTurboMCPComment::FPlan Plan;
	FString Error;
	if (!FTurboMCPComment::Plan(Asset, GraphName, Action, Nodes, Box, Text, Color, Size,
			Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPComment::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPComment(
	const FString& Asset, const FString& GraphName, const FString& Action,
	const FString& Nodes, const FString& Box, const FString& Text,
	const FString& Color, const FString& Size)
{
	FTurboMCPComment::FPlan Plan;
	FString Error;
	if (!FTurboMCPComment::Plan(Asset, GraphName, Action, Nodes, Box, Text, Color, Size,
			Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPComment::Apply(Plan);
}
