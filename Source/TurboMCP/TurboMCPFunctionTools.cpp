// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPFunction.h"

/**
 * `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`.
 *
 * NO PREVIEW TWIN, as `03_TOOLS.md` lists them. The name checks happen BEFORE anything is created,
 * so a refusal changes nothing, and the result reports what it did in enough detail to act on. That
 * is the same shape `E-WIDGETS-AS-VARS` uses.
 *
 * The two belong together because REMOVE is what frees a name an ADD took by mistake. Without it,
 * a mistyped function name could only be cleared in the editor by hand.
 */

FString UTurboMCPEditToolset::EditTurboMCPFunctionAdd(
	const FString& Asset, const FString& Name)
{
	return FTurboMCPFunction::Add(Asset, Name);
}

FString UTurboMCPEditToolset::EditTurboMCPFunctionRemove(
	const FString& Asset, const FString& Name)
{
	return FTurboMCPFunction::Remove(Asset, Name);
}
