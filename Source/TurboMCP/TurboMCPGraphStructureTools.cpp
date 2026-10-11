// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPGraphStructure.h"

/**
 * `P/E-GRAPH-COPY` and `P/E-GRAPH-COLLAPSE`, the pairs as `03_TOOLS.md` requires them.
 *
 * THE DECISION IS MADE IN `FTurboMCPGraphStructure::PlanCopy` AND `PlanCollapse`, NOT HERE. These
 * functions render or apply what those returned and decide nothing themselves, which is what makes a
 * Preview and its Edit twin refuse the SAME input - the rule this project requires of every pair.
 *
 * COPY IS BUILT AND COLLAPSE IS NOT, AND THE TWO ARE HONEST ABOUT IT IN DIFFERENT WORDS. COPY applies.
 * COLLAPSE runs its plan - so a caller learns what its input is missing - and then `Apply` refuses with
 * the measurement's own reason rather than a vague failure. Refusing a capability that is not built is
 * the outcome this project prefers to a half-built one: the whole breakpoint group was closed the same
 * way, with the refusal written into the descriptions of the tools an agent would reach for instead.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(
	const FString& SourceAsset, const FString& SourceGraph, const TArray<FString>& NodeIds,
	const FString& TargetAsset, const FString& TargetGraph, const FString& AnchorNodeId)
{
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCopy(SourceAsset, SourceGraph, NodeIds,
		TargetAsset, TargetGraph, AnchorNodeId, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphStructure::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphCopy(
	const FString& SourceAsset, const FString& SourceGraph, const TArray<FString>& NodeIds,
	const FString& TargetAsset, const FString& TargetGraph, const FString& AnchorNodeId)
{
	// THE PLAN IS BUILT ONCE, HERE, AND THE DECISION IS ITS. Everything the edit refuses, the preview
	// refused first with the same words, because both reach this same construction.
	FTurboMCPGraphStructure::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphStructure::PlanCopy(SourceAsset, SourceGraph, NodeIds,
		TargetAsset, TargetGraph, AnchorNodeId, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
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
		// `Apply` reports its own failures with `NOTHING was written`, because the spec requires a failed
		// write to SAY it failed rather than to look like a success.
		return FString::Printf(TEXT("error: %s"), *ApplyError);
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
		return FString::Printf(TEXT("error: %s"), *Error);
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
		return FString::Printf(TEXT("error: %s"), *Error);
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
		return FString::Printf(TEXT("error: %s"), *ApplyError);
	}

	return Report;
}
