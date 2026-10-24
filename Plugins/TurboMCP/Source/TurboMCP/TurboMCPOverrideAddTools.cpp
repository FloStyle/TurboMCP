// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPOverrideAdd.h"

// Both tools inspect the same plan; edit checks for an existing implementation again.

FString UTurboMCPPreviewToolset::PreviewTurboMCPOverrideAdd(
	const FString& Asset, const FString& FunctionOrEvent)
{
	FTurboMCPOverrideAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPOverrideAdd::Plan(Asset, FunctionOrEvent, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPOverrideAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPOverrideAdd(
	const FString& Asset, const FString& FunctionOrEvent)
{
	FTurboMCPOverrideAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPOverrideAdd::Plan(Asset, FunctionOrEvent, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPOverrideAdd::Apply(Plan);
}
