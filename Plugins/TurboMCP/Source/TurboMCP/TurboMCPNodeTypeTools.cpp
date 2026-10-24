// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPNodeType.h"

// Discovery loads Blueprint context; pin inspection constructs a temporary probe node.
// Neither wrapper requests asset compilation or saving.

FString UTurboMCPPreviewToolset::PreviewTurboMCPNodeType(
	const FString& Asset, const FString& GraphName, const FString& Words, const FString& Limit)
{
	FTurboMCPNodeType::FPlan Plan;
	FString Error;
	if (!FTurboMCPNodeType::Plan(Asset, GraphName, Words, Limit, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPNodeType::Render(Plan);
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPNodePins(
	const FString& Asset, const FString& GraphName, const FString& TypeId)
{
	FTurboMCPNodePins::FPlan Plan;
	FString Error;
	if (!FTurboMCPNodePins::Plan(Asset, GraphName, TypeId, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPNodePins::Render(Plan);
}
