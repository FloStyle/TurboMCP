// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPAssetCost.h"

/**
 * `R-ASSET-COST` - the size map and hard references of one asset.
 *
 * IT LIVES IN THE READ TOOLSET, WHICH IS STRUCTURALLY UNABLE TO WRITE. That is the project's safety model and the
 * spec's: a cost reader that could write would be a cost reader nobody could trust to leave the project alone.
 *
 * AND THE WHOLE ANSWER IS SPLIT BY PROVENANCE (`D-52`), which is why this returns a rendered report rather than a
 * number: *"Rejected: one total size. Why: the total names no cause and cannot be acted on. The check that matters
 * is 'a heavy hard reference where a soft one would do', and that fact does not exist inside a total."*
 */
FString UTurboMCPReadToolset::ReadTurboMCPAssetCost(const FString& Asset, int32 Cap)
{
	return FTurboMCPAssetCostReader::Read(Asset, Cap);
}
