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

/**
 * `P-ENUM-ADD` and `E-ENUM-ADD`.
 *
 * ONE CODE PATH, TWO OUTCOMES. The plan is built and validated by `FTurboMCPEnumAdd::Plan`,
 * the Preview prints it, and the Edit applies exactly that plan. `02_PRINCIPLES.md` `D-02`
 * requires a Preview and its Edit twin to share everything up to the write step, because a
 * preview that computes something different from the edit is worse than no preview.
 */

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
	// The SAME plan the preview builds, from the same function, so a caller that previewed
	// and then edited cannot be surprised by a difference between them.
	FTurboMCPEnumAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPEnumAdd::Plan(Path, Values, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPEnumAdd::Apply(Plan);
}
