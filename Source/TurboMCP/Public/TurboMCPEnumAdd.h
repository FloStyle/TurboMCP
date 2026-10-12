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

class UUserDefinedEnum;

/**
 * Creating an enum asset, shared by `P-ENUM-ADD` and `E-ENUM-ADD`.
 *
 * `Spec/02_PRINCIPLES.md` `D-02` says a Preview and its Edit twin share one code path up to
 * the write step, so the two cannot disagree about what would happen. That is why this is one
 * file rather than two tools with similar bodies: the plan is built once, the Preview prints
 * it, and the Edit applies exactly that plan.
 *
 * The enum is created through `FEnumEditorUtils`, the editor's own utility, rather than by
 * assembling a `UUserDefinedEnum` by hand. The engine's version maintains the display-name
 * map and the internal `_MAX` entry that every user-defined enum carries, and a hand-built
 * enum is one the editor cannot edit afterwards.
 */
class FTurboMCPEnumAdd
{
public:
	/** One value the caller asked for. */
	struct FValue
	{
		/** The name a graph writes. Must be a valid identifier. */
		FString Name;

		/** The name the editor shows. Empty means the name is shown as it is. */
		FString DisplayName;
	};

	/** Everything one call needs, validated before anything is created. */
	struct FPlan
	{
		/** The package path, for example `/Game/Tests/TA11_TestEnum`. */
		FString PackagePath;

		/** The asset's own name, taken from the last part of the path when empty. */
		FString AssetName;

		TArray<FValue> Values;
	};

	/**
	 * Works out what the call would do, without doing it.
	 *
	 * Fills OutError and returns false when the request cannot be carried out at all, so a
	 * refusal names the reason rather than being discovered halfway through a write.
	 */
	static bool Plan(const FString& InPath, const FString& InValues,
		FPlan& OutPlan, FString& OutError);

	/**
	 * Renders the plan as the Preview result.
	 *
	 * Names every value and its display name, and says whether the asset already exists,
	 * so the two facts a caller has to check are in the preview rather than in the edit.
	 */
	static FString RenderPreview(const FPlan& InPlan);

	/**
	 * Carries the plan out.
	 *
	 * Returns the tool result, which opens with `changed:` or `unchanged:` as `P4` requires.
	 * A plan whose asset already exists is REFUSED rather than overwritten, because
	 * overwriting a name someone else chose is the kind of loss `P8` forbids.
	 */
	static FString Apply(const FPlan& InPlan);
};
