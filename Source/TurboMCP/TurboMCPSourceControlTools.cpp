// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPSourceControl.h"

/**
 * `R-VCS-STATE`, `P-VCS-CHECKOUT` and `E-VCS-CHECKOUT`: the source control row of `03_TOOLS.md` section 17.
 *
 * ALL THREE CALL `FTurboMCPSourceControl::Plan` AND NOTHING ELSE DECIDES ANYTHING. The Read tool renders
 * the plan, the Preview renders the same plan plus what a checkout would do, and the Edit twin renders it
 * and then acts - so a Preview and its Edit twin cannot refuse different input, which is the rule this
 * project requires of every pair.
 *
 * THE REFUSAL IS PER ASSET AND IT IS THE ENGINE'S OWN STRING. With nothing selected, the default provider
 * answers `IsEnabled() == false` (`DefaultSourceControlProvider.cpp:41-44`) and the answer for every asset
 * is `"Revision control is not enabled."` (`SourceControlHelpers.cpp:67`). With Perforce selected and no
 * server it is `"Revision control server is currently not available."` (`:76`). No state is reported in
 * either case, because there is no state to report - `P1`.
 *
 * AND A CHECKOUT ON GIT IS A NO-OP, SAID IN WORDS. `UsesCheckout()` is false there
 * (`GitSourceControlProvider.cpp:286-289`) because Git has no lock command
 * (`GitSourceControlModule.cpp:21`), so the Edit tool states that before repeating the provider's own
 * answer verbatim.
 */
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
