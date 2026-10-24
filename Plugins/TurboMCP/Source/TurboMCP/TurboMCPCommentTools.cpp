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

// Both calls plan from current state; edit guards and current geometry can still reject apply.
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
