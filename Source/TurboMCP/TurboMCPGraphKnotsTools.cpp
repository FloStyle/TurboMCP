// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphKnots.h"

/**
 * `P-GRAPH-KNOTS` and `E-GRAPH-KNOTS`.
 *
 * ONE CODE PATH, TWO OUTCOMES (`D-02`). `FTurboMCPGraphKnots::Plan` finds the wires that cross a
 * node and decides where a knot would go for each, so EVERY REFUSAL IS ALREADY MADE when the
 * Preview prints. The Edit calls the same `Plan` and then `Apply`, which means it refuses exactly
 * what the Preview refused - and the reason a check belongs in `Plan` rather than `Apply` is that
 * the reverse lets the Preview offer a plan the Edit then declines.
 *
 * The Edit is the half that MEASURES AND ROLLS BACK: `Apply` counts the crossings, places the
 * knots, counts again, and undoes the whole pass when the count did not fall.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphKnots(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphKnots::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphKnots::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphKnots::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphKnots(
	const FString& Asset, const FString& GraphName)
{
	FTurboMCPGraphKnots::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphKnots::Plan(Asset, GraphName, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPGraphKnots::Apply(Plan);
}
