// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphCheck.h"

// Render the checks performed by Plan; this is not a promise of writer acceptance.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphCheck(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphCheck::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphCheck::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphCheck::Render(Plan);
}
