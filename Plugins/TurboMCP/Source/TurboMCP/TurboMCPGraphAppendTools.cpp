// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphAppend.h"

// Both entry points share planning and scratch validation. Planning can load assets
// and construct temporary nodes; toolset membership does not enforce side effects.
// Apply performs additional live checks and reports execution failures separately.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text, const FString& Continuation)
{
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true))
	{
		return FTurboMCPToolError::AsError(Error);
	}

    Plan.Continuation = Continuation.TrimStartAndEnd();
    if (Plan.Continuation.IsEmpty()) Plan.Continuation = TEXT("auto");
    if (Plan.Continuation != TEXT("auto") && Plan.Continuation != TEXT("all") && !Plan.Continuation.Contains(TEXT(":")))
        Plan.Refusals.Add(TEXT("Continuation must be auto, all, or label:pin"));

    FString ExecutionReport;
    if (Plan.bAcceptable() && !FTurboMCPGraphAppend::Preflight(Plan, ExecutionReport, Error))
        return FTurboMCPToolError::AsError(Error);
    return FTurboMCPGraphAppend::RenderPreview(Plan) + (ExecutionReport.IsEmpty() ? FString() : TEXT("\n") + ExecutionReport);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text, const FString& Continuation)
{
	// Rebuild the plan against current state; a previous preview may be stale.
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true))
	{
		return FTurboMCPToolError::AsError(Error);
	}

    Plan.Continuation = Continuation.TrimStartAndEnd();
    if (Plan.Continuation.IsEmpty()) Plan.Continuation = TEXT("auto");
    if (Plan.Continuation != TEXT("auto") && Plan.Continuation != TEXT("all") && !Plan.Continuation.Contains(TEXT(":")))
        Plan.Refusals.Add(TEXT("Continuation must be auto, all, or label:pin"));

	if (!Plan.bAcceptable())
	{
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their lines, and the first line
		// is `unchanged`, so a caller that reads only the first line still learns nothing was written.
		return FTurboMCPGraphAppend::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError))
	{
		// Preserve the execution failure, including partial-change or rollback details.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
