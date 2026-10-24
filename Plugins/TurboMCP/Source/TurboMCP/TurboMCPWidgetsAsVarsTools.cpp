// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetsAsVars.h"

// Validate the request before resolving the asset and changing widget flags.
FString UTurboMCPEditToolset::EditTurboMCPWidgetsAsVars(
	const FString& Asset, const FString& Widgets, bool bIsVariable)
{
	FTurboMCPWidgetsAsVars::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetsAsVars::Plan(Asset, Widgets, bIsVariable, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPWidgetsAsVars::Apply(Plan);
}
