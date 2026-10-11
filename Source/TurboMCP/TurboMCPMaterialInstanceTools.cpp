// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPMaterialInstance.h"

/**
 * R-MAT-INST-LOAD: read a material instance - its parent chain, what it overrides, and what it inherits.
 *
 * A READER LIVES IN UTurboMCPReadToolset, WHICH IS STRUCTURALLY UNABLE TO WRITE. That is the safety model and the
 * spec requirement, not a convention: an agent that wants to know whether a parameter is on the asset it holds must
 * be able to ask WITHOUT holding a tool that could change it.
 *
 * THE LIST THAT MATTERS IS THE INHERITED ONE, in the spec words: an agent asked to change a parameter needs to know
 * whether the value is REALLY ON THIS INSTANCE or inherited from a parent it does not own. What this reader adds to
 * what existed is exactly that inverse - TurboMCPMaterial.cpp RenderTree already printed the parameters an instance
 * OVERRIDES, and NOTHING PRINTED THE ONES IT DOES NOT.
 *
 * A BASE MATERIAL IS REFUSED BY NAME rather than answered with every parameter marked inherited. D-43 refuses the
 * opposite direction for the same reason - "a text that hides them invites an edit that changes the wrong asset" -
 * and here the analogous harm is telling an agent that a parameter is not editable on the asset it is holding when
 * it is. The refusal NAMES the tools to use instead.
 */
FString UTurboMCPReadToolset::ReadTurboMCPMaterialInstance(const FString& Asset)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass a material instance, for example MI_TurboMCPFixture.");
	}

	FTurboMCPMaterialInstance::FResult Result;
	FString Error;
	if (!FTurboMCPMaterialInstance::Read(Asset, Result, Error))
	{
		// Read FILLS A MESSAGE THAT ALREADY STARTS WITH error:, so a caller tests ONE prefix rather than matching a
		// sentence - and the refusal is never an empty string a caller could mistake for success.
		return Error;
	}

	return FTurboMCPMaterialInstance::Render(Result);
}
