// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialLabelsPromote.h"

FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialLabelPromote(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to name values in, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialLabelsPromote::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialLabelsPromote::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// A MATERIAL WITH NOTHING TO PROMOTE IS AN ANSWER RATHER THAN A REFUSAL, and it is the answer this
	// capability exists to give on a material that is already named. `RenderPreview` prints it with the
	// count of values examined, so "no label is worth adding" can never be confused with "nothing was
	// looked at".
	return FTurboMCPMaterialLabelsPromote::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPMaterialLabelPromote(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to name values in, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialLabelsPromote::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialLabelsPromote::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPMaterialLabelsPromote::Apply(Plan);
}
