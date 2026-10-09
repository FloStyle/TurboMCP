// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPOverrideAdd.h"

/**
 * `P-OVERRIDE-ADD` and `E-OVERRIDE-ADD`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPOverrideAdd::Plan` resolves the name against the editor's
 * own overridable lists, builds the signature and checks whether it is already overridden; the
 * Preview prints all of that and the Edit applies exactly it. The "already overridden" check lives
 * in `Plan` rather than in `Apply` on purpose, so the PREVIEW refuses too: a preview that showed a
 * plan the edit would not carry out is the disagreement `D-02` exists to prevent.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPOverrideAdd(
	const FString& Asset, const FString& FunctionOrEvent)
{
	FTurboMCPOverrideAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPOverrideAdd::Plan(Asset, FunctionOrEvent, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPOverrideAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPOverrideAdd(
	const FString& Asset, const FString& FunctionOrEvent)
{
	FTurboMCPOverrideAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPOverrideAdd::Plan(Asset, FunctionOrEvent, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPOverrideAdd::Apply(Plan);
}
