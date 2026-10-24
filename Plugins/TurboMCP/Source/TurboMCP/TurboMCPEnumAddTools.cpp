// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPEnumAdd.h"

// Preview and edit share input parsing; edit checks the current target again.
FString UTurboMCPPreviewToolset::PreviewTurboMCPEnumAdd(
	const FString& Path, const FString& Values)
{
	FTurboMCPEnumAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPEnumAdd::Plan(Path, Values, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPEnumAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPEnumAdd(
	const FString& Path, const FString& Values)
{
	// Rebuild the plan for this call; an earlier preview does not reserve the target.
	FTurboMCPEnumAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPEnumAdd::Plan(Path, Values, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPEnumAdd::Apply(Plan);
}
