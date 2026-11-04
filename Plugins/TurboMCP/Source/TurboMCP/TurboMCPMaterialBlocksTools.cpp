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

	FString RollbackNote;
	FString Out = FTurboMCPMaterialBlocks::Apply(Plan, RollbackNote);
	Out += RollbackNote;
	return Out;
}
