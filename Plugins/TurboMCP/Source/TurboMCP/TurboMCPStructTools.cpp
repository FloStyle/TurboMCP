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

	// =============================================================================================
	// ROW 10.2 / D-57: THE AUTOMATIC CHECKOUT, DECIDED IN ONE PLAN SO THE EDIT TWIN CANNOT DISAGREE.
	//
	// *** THE BLOCK IS APPENDED, NEVER PREPENDED, AND EVERY LINE OF IT IS INDENTED BY FOUR SPACES OR
	// STARTS AT COLUMN 0. *** That is a hard constraint rather than a style choice: the project's own
	// suite reads this answer, and its field reader takes exactly-two-space-indented lines carrying a
	// colon as FIELD NAMES. A prepended block would also break the "changed:" / "error:" opening every
	// caller relies on.
	// =============================================================================================
	TArray<FString> Targets;
	Targets.Add(Plan.ObjectPath);
	const FTurboMCPWriteCheckoutPlan Checkout = FTurboMCPSourceControlWrite::Plan(Targets);

	FString Out = FTurboMCPStruct::RenderPreviewFields(Plan);
	Out += TEXT("\n\n");
	Out += FTurboMCPSourceControlWrite::Render(Checkout, TEXT("P-STRUCT-FIELDS"));
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPStructFields(
	const FString& Struct, const FString& Changes)
{
	FTurboMCPStruct::FPlanFields Plan;
	FString Error;
	if (!FTurboMCPStruct::PlanFieldEdits(Struct, Changes, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// =============================================================================================
	// ROW 10.2 / D-57: THE AUTOMATIC CHECKOUT, ISSUED BEFORE THIS TOOL WRITES TO THE ASSET.
	//
	// The report is captured FIRST and appended LAST, so the write still opens with "changed:" and the
	// block can never be read as this tool's own outcome. The same Plan function the Preview called is
	// called here, so the two decide identically, and the report is never a gate: D-57 warns against
	// turning a source control problem into a write failure, and on Git it would refuse every write.
	// =============================================================================================
	TArray<FString> Targets;
	Targets.Add(Plan.ObjectPath);
	const FString CheckoutReport = FTurboMCPSourceControlWrite::EnsureCheckedOut(
		FTurboMCPSourceControlWrite::Plan(Targets), TEXT("E-STRUCT-FIELDS"));

	FString Out = FTurboMCPStruct::ApplyFields(Plan);
	Out += TEXT("\n\n");
	Out += CheckoutReport;
	return Out;
}
