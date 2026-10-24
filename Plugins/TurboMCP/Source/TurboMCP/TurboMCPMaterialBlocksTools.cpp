// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialBlocks.h"

FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialBlocks(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to arrange into blocks, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialBlocks::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialBlocks::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// A MATERIAL THAT ALREADY FOLLOWS D-17 IS AN ANSWER RATHER THAN A REFUSAL, and it is the answer this
	// capability exists to give on a material that is already shaped correctly. `RenderPreview` prints it
	// with the block count and the number of expressions in no block, so "nothing to change" can never be
	// confused with "nothing was looked at".
	return FTurboMCPMaterialBlocks::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPMaterialBlocks(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to arrange into blocks, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	FTurboMCPMaterialBlocks::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialBlocks::Plan(Asset, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// THE ROLLBACK MEASUREMENT IS APPENDED HERE RATHER THAN INSIDE `Apply`, because it is produced in a
	// destructor there - after the report has already been built - so `Apply` can only hand it back through
	// this out-parameter. A successful pass leaves it empty.
	FString RollbackNote;
	FString Out = FTurboMCPMaterialBlocks::Apply(Plan, RollbackNote);
	Out += RollbackNote;
	return Out;
}
