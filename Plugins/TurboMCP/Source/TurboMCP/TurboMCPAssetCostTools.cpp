// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPAssetCost.h"

// Report recorded package sizes and direct dependency categories without an explicit asset edit.
FString UTurboMCPReadToolset::ReadTurboMCPAssetCost(const FString& Asset, int32 Cap)
{
	return FTurboMCPAssetCostReader::Read(Asset, Cap);
}
