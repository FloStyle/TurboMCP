// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPLocalVars.h"

// Both tools use the same candidate analysis; the edit revalidates live state before mutation.
FString UTurboMCPPreviewToolset::PreviewTurboMCPLocalVars(
	const FString& Asset, const FString& GraphName, const FString& Names)
{
	FTurboMCPLocalVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPLocalVars::Plan(Asset, GraphName, Plan, Error, Names))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPLocalVars::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPLocalVars(
	const FString& Asset, const FString& GraphName, const FString& Names)
{
	FTurboMCPLocalVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPLocalVars::Plan(Asset, GraphName, Plan, Error, Names))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPLocalVars::Apply(Plan);
}
