// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPSourceControl.h"

// Planning refreshes provider state; only the Edit wrapper requests checkout.
// Preview and Edit plan separately, so provider state may change between calls.
FString UTurboMCPReadToolset::ReadTurboMCPVcsState(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Render(Plan, false);
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPVcsCheckout(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Render(Plan, true);
}

FString UTurboMCPEditToolset::EditTurboMCPVcsCheckout(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Checkout(Plan);
}
