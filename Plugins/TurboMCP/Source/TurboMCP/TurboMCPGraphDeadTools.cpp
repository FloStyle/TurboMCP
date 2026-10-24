// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphDead.h"

// Preview and edit recompute the same static candidate selection; this is not runtime proof.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphDead(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphDead::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphDead::Plan(Asset, GraphName, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphDead::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphDead(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphDead::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphDead::Plan(Asset, GraphName, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphDead::Apply(Plan);
}
