// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphFixBackward.h"

/** Preview and edit plan against the current graph. Edit replans before applying;
 * a previous preview is not a reservation. Force only bypasses the estimated
 * wire-length improvement requirement, not identity, geometry or safety checks.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphFixBackward(
	const FString& Asset, const FString& GraphName, bool Force)
{
	FTurboMCPGraphFixBackward::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphFixBackward::Plan(Asset, GraphName, Force, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphFixBackward::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphFixBackward(
	const FString& Asset, const FString& GraphName, bool Force)
{
	FTurboMCPGraphFixBackward::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphFixBackward::Plan(Asset, GraphName, Force, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphFixBackward::Apply(Plan);
}
