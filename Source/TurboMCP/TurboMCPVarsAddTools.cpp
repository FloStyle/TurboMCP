// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPVarsAdd.h"

/**
 * `P-VARS-ADD` and `E-VARS-ADD`.
 *
 * ONE CODE PATH, TWO OUTCOMES. `FTurboMCPVarsAdd::Plan` parses and validates the list, the
 * Preview prints it, and the Edit applies exactly that list. Parsing is the part that must not
 * differ between the two: a preview that accepted a list the edit then refused would be worse
 * than no preview at all.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPVarsAdd::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPVarsAdd(
	const FString& Asset, const FString& Variables)
{
	FTurboMCPVarsAdd::FPlan Plan;
	FString Error;
	if (!FTurboMCPVarsAdd::Plan(Asset, Variables, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return FTurboMCPVarsAdd::Apply(Plan);
}
