// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPMaterialTidy.h"

FString UTurboMCPPreviewToolset::PreviewTurboMCPMaterialTidy(const FString& Asset, const FString& Force)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to lay out, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	const bool bForce = !Force.TrimStartAndEnd().IsEmpty();

	FTurboMCPMaterialTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialTidy::Plan(Asset, bForce, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (Plan.Refusals.Num() > 0)
	{
		// A REFUSAL IS THE ANSWER, AND IT IS PRINTED RATHER THAN SWALLOWED. `Plan` answering true means
		// it could reach the material; a refusal means something it was asked to do it will not, and a
		// caller who sees positions without the refusal would believe the whole layout is available.
		FString Out = FString::Printf(TEXT("refused: %d thing(s) this pass will not do"), Plan.Refusals.Num());
		for (const FString& Refusal : Plan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Refusal);
		}
		return Out;
	}

	return FTurboMCPMaterialTidy::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPMaterialTidy(const FString& Asset, const FString& Force)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass the material to lay out, for example ")
			TEXT("/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture");
	}

	const bool bForce = !Force.TrimStartAndEnd().IsEmpty();

	FTurboMCPMaterialTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPMaterialTidy::Plan(Asset, bForce, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (Plan.Refusals.Num() > 0)
	{
		FString Out = FString::Printf(TEXT("unchanged: %d refusal(s), so nothing was written"),
			Plan.Refusals.Num());
		for (const FString& Refusal : Plan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Refusal);
		}
		return Out;
	}

	return FTurboMCPMaterialTidy::Apply(Plan);
}
