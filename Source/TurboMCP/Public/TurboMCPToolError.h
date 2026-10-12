// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"

/**
 * THE ERROR PREFIX, ADDED IN EXACTLY ONE PLACE, AND GUARDED BY READING THE STRING RATHER THAN BY A FLAG.
 *
 * `Plan` FILLS A BARE SENTENCE, while `FTurboMCPAssetResolver::Resolve` - which `Plan` passes through - fills one
 * that ALREADY CARRIES `error:`. Two layers each assuming the other leaves the prefix off is how a result ends
 * up reading `error: error: ...`, so the guard CHECKS BEFORE ADDING.
 *
 * THE TEST IS ON THE RESULT RATHER THAN ON A FLAG, because a flag can disagree with the string it describes.
 *
 * *** AND IT IS ONE FUNCTION BECAUSE IT WAS FOUR. *** The rule existed in four files under three names -
 * `AsError` twice, `BlocksAsError`, `LabelPromoteAsError` - as four BYTE-IDENTICAL bodies. That is the shape this
 * project records as two implementations of one rule: they agree until one of them is changed.
 *
 * *** THE CARD THAT ASKED FOR THIS WAS RIGHT ABOUT THE REPAIR AND WRONG ABOUT THE EXPOSURE. *** It claimed eight
 * wrappers could emit `error: error:`; measured, the two sites that reformat an `Apply` result both format an
 * out-parameter that the implementation writes UNPREFIXED (the `bool Apply(Plan, Report, ApplyError)` family), so
 * THEY WERE ALREADY CORRECT. The duplication was the real defect, and it is what this consolidates.
 */
struct FTurboMCPToolError
{
	/** `error: <what failed>`, with the prefix added exactly once. */
	static FORCEINLINE FString AsError(const FString& InError)
	{
		if (InError.StartsWith(TEXT("error:")))
		{
			return InError;
		}
		return FString::Printf(TEXT("error: %s"), *InError);
	}
};
