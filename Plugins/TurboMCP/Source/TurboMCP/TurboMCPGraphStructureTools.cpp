// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphStructure.h"

// Plans describe supported operations; Apply performs additional guards and can fail.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(
	const FString& SourceAsset, const FString& SourceGraph, const TArray<FString>& NodeIds,
	const FString& TargetAsset, const FString& TargetGraph, const FString& AnchorNodeId)
{
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCopy(SourceAsset, SourceGraph, NodeIds,
		TargetAsset, TargetGraph, AnchorNodeId, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphStructure::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphCopy(
	const FString& SourceAsset, const FString& SourceGraph, const TArray<FString>& NodeIds,
	const FString& TargetAsset, const FString& TargetGraph, const FString& AnchorNodeId)
{
	// Rebuild the plan from current state before applying it.
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCopy(SourceAsset, SourceGraph, NodeIds,
		TargetAsset, TargetGraph, AnchorNodeId, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	if (!Plan.bAcceptable())
	{
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their reason, and the first
		// line is `unchanged`, so a caller that reads only the first line still learns nothing was written.
		return FTurboMCPGraphStructure::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphStructure::Apply(Plan, Report, ApplyError))
	{
		// Preserve Apply diagnostics, including partial changes or rollback failures.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphCollapse(
	const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds,
	const FString& MemberName, const FString& MemberKind)
{
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCollapse(Asset, GraphName, NodeIds, MemberName, MemberKind,
		Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphStructure::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphCollapse(
	const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds,
	const FString& MemberName, const FString& MemberKind)
{
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCollapse(Asset, GraphName, NodeIds, MemberName, MemberKind,
		Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	// THE PLAN RUNS FIRST SO ITS REFUSALS ARE REPORTED, and only an ACCEPTABLE plan reaches `Apply` -
	// which then refuses with the one question the measurement left open. Reporting the plan's own
	// refusals before the not-built reason matters: a caller whose selection contains an event should
	// learn THAT, because it is what they can act on.
	if (!Plan.bAcceptable())
	{
		return FTurboMCPGraphStructure::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphStructure::Apply(Plan, Report, ApplyError))
	{
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
