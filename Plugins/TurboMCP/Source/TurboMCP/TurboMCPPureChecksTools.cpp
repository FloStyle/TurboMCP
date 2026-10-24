// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPPureChecks.h"

// Report literal helper results without invoking tools or resolving assets.
FString UTurboMCPReadToolset::ReadTurboMCPPureChecks(
	const FString& Toolsets,
	const FString& Arguments)
{
	return FTurboMCPPureChecks::Report(Toolsets, Arguments);
}
