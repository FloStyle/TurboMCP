// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPNodeType.h"

/**
 * `P-NODE-TYPE` and `P-NODE-PINS`.
 *
 * BOTH ARE READ-ONLY IN THE SENSE THAT MATTERS: neither writes to the asset. `P-NODE-PINS`
 * builds a node, but it builds it in a THROWAWAY graph whose outer is the Blueprint and which is
 * never added to the Blueprint's graph lists, so the asset's own graph keeps its node count
 * (`P3`). That is deliberately different from the engine's own reader, which adds a probe to the
 * real graph and leaves it there - recorded as a trap in `07_ENGINE_NOTES.md` section 3.5.
 *
 * They live in the PREVIEW toolset rather than Look because a node is BUILT to answer, even
 * though nothing is kept. A caller reading the toolset list learns that "preview" means "answers
 * by doing it on the side, and keeps nothing".
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPNodeType(
	const FString& Asset, const FString& GraphName, const FString& Words, const FString& Limit)
{
	FTurboMCPNodeType::FPlan Plan;
	FString Error;
	if (!FTurboMCPNodeType::Plan(Asset, GraphName, Words, Limit, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPNodeType::Render(Plan);
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPNodePins(
	const FString& Asset, const FString& GraphName, const FString& TypeId)
{
	FTurboMCPNodePins::FPlan Plan;
	FString Error;
	if (!FTurboMCPNodePins::Plan(Asset, GraphName, TypeId, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPNodePins::Render(Plan);
}
