// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPVarsAdd.h"

// Share declaration parsing; target resolution and name-conflict checks occur only in Apply.

FString UTurboMCPPreviewToolset::PreviewTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPVarsAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPVarsAdd::Apply(Plan);
}
