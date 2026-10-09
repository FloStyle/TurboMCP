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

class UBlueprint;

/**
 * Adding Blueprint variables in bulk, shared by `P-VARS-ADD` and `E-VARS-ADD`.
 *
 * `Spec/02_PRINCIPLES.md` `D-02` says a Preview and its Edit twin share one code path up to
 * the write step. So the list is PARSED and VALIDATED once, the Preview prints the parsed
 * result, and the Edit applies exactly that list. A preview that parsed differently from the
 * edit would be worse than no preview.
 *
 * WHY THE PARSING IS THE HARD PART
 * --------------------------------
 * `03_TOOLS.md` wants each item to carry a name, a type, a default, a category, flags and a
 * tooltip, and wants the type to cover anything `R-TYPE-FIND` can find plus containers. The
 * containers are why a plain comma split does not work: `Map<String, Int>` has a comma inside
 * it, and `Array<Struct>` has angle brackets that must balance. The splitter therefore counts
 * depth rather than splitting on every comma, and a type whose brackets do not balance is
 * refused by name rather than silently truncated.
 */
class FTurboMCPVarsAdd
{
public:
	/** One variable, fully parsed. */
	struct FVar
	{
		FString Name;

		/** The type as written, kept for the preview and for the refusal messages. */
		FString TypeText;

		/** The container family: none, array, set or map. */
		enum class EContainer : uint8 { None, Array, Set, Map };
		EContainer Container = EContainer::None;

		/** The inner type for a container, or the type itself. */
		FString InnerType;

		/** The value type of a map. Unused for the other containers. */
		FString ValueType;

		FString Default;
		FString Category;
		FString Tooltip;

		// The flags `03_TOOLS.md` names.
		bool bPrivate = false;
		bool bReadOnly = false;
		bool bInstanceEditable = false;
		bool bExposeOnSpawn = false;
	};

	/** The parsed and validated request. */
	struct FPlan
	{
		FString Asset;
		TArray<FVar> Variables;
	};

	/**
	 * Parses and validates the list.
	 *
	 * Fills OutError and returns false when the request cannot be carried out, so a refusal
	 * names the exact item and reason rather than being found halfway through a write.
	 */
	static bool Plan(const FString& InAsset, const FString& InVariables,
		FPlan& OutPlan, FString& OutError);

	/** Prints what the call would add, without adding it. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Adds the variables, compiles once, and reads them back. */
	static FString Apply(const FPlan& InPlan);
};
