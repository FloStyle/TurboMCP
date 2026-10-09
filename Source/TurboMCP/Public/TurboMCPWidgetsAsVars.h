// Copyright FloStyle. All Rights Reserved.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UWidgetBlueprint;

/**
 * Marking widgets as variables, behind `E-WIDGETS-AS-VARS`.
 *
 * WHY THERE IS NO PREVIEW TWIN
 * ----------------------------
 * `03_TOOLS.md` says so explicitly: "No Preview twin needed; it is a flag change and the Edit
 * reports old and new state." A preview exists to show the shape of a change a caller cannot see
 * without making it, and a boolean flag has no shape. So this is an Edit on its own, and its
 * result carries the OLD and the NEW value of the flag for every widget, which is what a preview
 * would have shown.
 *
 * The flag is set through the editor's own `FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable`,
 * the same call the details panel's "Is Variable" checkbox makes, so a widget marked here is
 * indistinguishable from one marked by hand.
 */
class FTurboMCPWidgetsAsVars
{
public:
	/** The parsed request. */
	struct FPlan
	{
		FString Asset;

		/** Widget names, as the designer shows them. */
		TArray<FString> Widgets;

		/** The value the flag should end up with. */
		bool bIsVariable = true;
	};

	/** Splits and validates the request. */
	static bool Plan(const FString& InAsset, const FString& InWidgets,
		bool bInIsVariable, FPlan& OutPlan, FString& OutError);

	/**
	 * Sets the flag and reports the old and the new state of every widget.
	 *
	 * A widget that is already in the requested state is reported as unchanged rather than
	 * quietly counted as done, because a caller that asked for ten and got three changed
	 * needs to know which three.
	 */
	static FString Apply(const FPlan& InPlan);
};
