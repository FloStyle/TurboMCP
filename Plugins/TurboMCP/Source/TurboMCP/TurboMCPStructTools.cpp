// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPStruct.h"
#include "Editor.h"
#include "TurboMCPSourceControlWrite.h"

// Shared planning validates syntax; apply checks current objects and can still fail.
FString UTurboMCPPreviewToolset::PreviewTurboMCPStructNew(
	const FString& Path, const FString& Fields)
{
	FTurboMCPStruct::FPlanNew Plan;
	FString Error;
	if (!FTurboMCPStruct::PlanNew(Path, Fields, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPStruct::RenderPreviewNew(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPStructNew(
	const FString& Path, const FString& Fields)
{
    // Refuse before type loading, source-control checkout or asset mutation.
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: struct editing requires an idle editor outside PIE and active transactions; checkout=not_requested; save=not_requested");
	// Rebuild the plan; an earlier preview does not reserve a path or freeze editor state.
	FTurboMCPStruct::FPlanNew Plan;
	FString Error;
	if (!FTurboMCPStruct::PlanNew(Path, Fields, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPStruct::ApplyNew(Plan);
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPStructFields(
	const FString& Struct, const FString& Changes)
{
	FTurboMCPStruct::FPlanFields Plan;
	FString Error;
	if (!FTurboMCPStruct::PlanFieldEdits(Struct, Changes, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// Append checkout information after the field report so it cannot be parsed as a field.
	TArray<FString> Targets;
	Targets.Add(Plan.ObjectPath);
	const FTurboMCPWriteCheckoutPlan Checkout = FTurboMCPSourceControlWrite::Plan(Targets);

	FString Out = FTurboMCPStruct::RenderPreviewFields(Plan);
	Out += TEXT("\n\n");
	Out += FTurboMCPSourceControlWrite::Render(Checkout, TEXT("PreviewTurboMCPStructFields"));
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPStructFields(
	const FString& Struct, const FString& Changes)
{
    // Refuse before type loading, source-control checkout or asset mutation.
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: struct editing requires an idle editor outside PIE and active transactions; checkout=not_requested; save=not_requested");
	FTurboMCPStruct::FPlanFields Plan;
	FString Error;
	if (!FTurboMCPStruct::PlanFieldEdits(Struct, Changes, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// Attempt checkout before editing; report failures without implying the asset was saved.
	TArray<FString> Targets;
	Targets.Add(Plan.ObjectPath);
	const FString CheckoutReport = FTurboMCPSourceControlWrite::EnsureCheckedOut(
		FTurboMCPSourceControlWrite::Plan(Targets), TEXT("EditTurboMCPStructFields"));

	FString Out = FTurboMCPStruct::ApplyFields(Plan);
	Out += TEXT("\n\n");
	Out += CheckoutReport;
	return Out;
}
