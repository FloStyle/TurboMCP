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

/** Shared declaration parsing and variable creation; preview does not validate the target. */
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

	/** Adds variables, publishes structural changes and checks their names. */
	static FString Apply(const FPlan& InPlan);
};
