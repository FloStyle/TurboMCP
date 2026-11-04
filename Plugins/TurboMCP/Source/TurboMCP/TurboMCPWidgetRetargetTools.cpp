// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPWidgetRetarget.h"

/** Preview inspects delegate presence; edit removes bindings with missing targets.
 * Neither operation certifies function signatures or changes graph nodes.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRetarget(const FString& Asset)
{
	FTurboMCPWidgetRetarget::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetRetarget::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPWidgetRetarget::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPWidgetRetarget(const FString& Asset)
{
	FTurboMCPWidgetRetarget::FPlan Plan;
	FString Error;
	if (!FTurboMCPWidgetRetarget::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPWidgetRetarget::Apply(Plan);
}
