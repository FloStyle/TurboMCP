// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;

/**
 * Adding and removing Blueprint functions, behind `E-FUNCTION-ADD` and `E-FUNCTION-REMOVE`.
 *
 * `03_TOOLS.md` names three things, and the separation between them is the point:
 *
 *  - a name already used ON THIS BLUEPRINT is refused, and the result names what uses it;
 *  - a name that matches a PARENT function is refused differently, and points at
 *    `P/E-OVERRIDE-ADD`, because the caller wants an override and not a new function that would
 *    shadow nothing;
 *  - REMOVE frees the name at once, which is why the pair belongs together: without remove, a
 *    name taken by mistake can only be freed in the editor by hand.
 *
 * There is no Preview twin. `03_TOOLS.md` lists this as `E-FUNCTION-ADD and E-FUNCTION-REMOVE`
 * with no `P-` counterpart, and the shape is the one `E-WIDGETS-AS-VARS` already follows: the
 * result reports what it did in enough detail to act on, and the name checks happen before
 * anything is created so a refusal changes nothing.
 */
class FTurboMCPFunction
{
public:
	/** The result of checking a name, so the two tools refuse the same way. */
	struct FNameCheck
	{
		/** True when the name can be used for a new function. */
		bool bCanAdd = false;

		/** Why it cannot, when it cannot. Empty when it can. */
		FString Reason;

		/** True when the reason is that the parent already declares it. */
		bool bIsParentFunction = false;

		/** The graph already holding the name, when one does. */
		FString ExistingGraph;

		/** Other things using the name, so a refusal can name them. */
		TArray<FString> OtherUsers;
	};

	/** Checks whether a name can be used for a new function on this Blueprint. */
	static FNameCheck CheckName(UBlueprint* InBlueprint, const FString& InName);

	/** Adds a function and reports the graph it made. */
	static FString Add(const FString& InAsset, const FString& InName);

	/** Removes a function and reports what was freed. */
	static FString Remove(const FString& InAsset, const FString& InName);
};
