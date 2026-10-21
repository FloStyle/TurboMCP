// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0.

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPToolError.h"
#include "TurboMCPGraphSwapCall.h"

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphSwapCall(
	const FString& Asset, const FString& GraphName, const FString& NodeId,
	const FString& ReplacementTypeId, const FString& KeepOld, const FString& Options)
{
	FTurboMCPGraphSwapCall::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphSwapCall::Plan(Asset, GraphName, NodeId, ReplacementTypeId, KeepOld, Plan, Error, Options))
		return FTurboMCPToolError::AsError(Error);
	return FTurboMCPGraphSwapCall::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphSwapCall(
	const FString& Asset, const FString& GraphName, const FString& NodeId,
	const FString& ReplacementTypeId, const FString& KeepOld, const FString& Options)
{
	FTurboMCPGraphSwapCall::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphSwapCall::Plan(Asset, GraphName, NodeId, ReplacementTypeId, KeepOld, Plan, Error, Options))
		return FTurboMCPToolError::AsError(Error);
	if (!Plan.bAcceptable()) return FTurboMCPGraphSwapCall::RenderPreview(Plan);
	FString Report;
	if (!FTurboMCPGraphSwapCall::Apply(Plan, Report, Error))
		return FTurboMCPToolError::AsError(Error);
	return Report;
}
