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

class UBaseWidgetBlueprint;

/**
 * Compact widget context used by BlueprintLoad and its JSON representation.
 * Designer bindings come from UWidgetBlueprint::Bindings, not runtime delegates.
 * Editable property overrides are compared with class defaults. This is not a
 * complete usage analysis: variable flags do not prove whether a widget is used.
 * The standalone WidgetTree tool uses the separate WidgetTreeSync reader.
 */
class FTurboMCPWidgetTree
{
public:
	/** One widget's facts. */
	struct FEntry
	{
		FName Name;
		FString ClassName;

		/** True when the designer marked it as a variable, which is a fact, not a use. */
		bool bIsVariable = false;

		/** The visibility as the designer set it. */
		FString Visibility;

		/** The slot's padding, printed only when it is not zero. */
		FString Padding;

		/** Which named slot hosts it, when it is inside one. */
		FName NamedSlotHost;

		/** How deep it sits under the root, used for the indentation. */
		int32 Depth = 0;

		/** Designer binding descriptions; runtime delegate connections are not enumerated. */
		TArray<FString> BoundEvents;

		/**
		 * For a placed user widget, the instance-editable properties whose value on this
		 * placed copy differs from the class default. A placed copy stores its own value,
		 * which is exactly what an agent has to know before editing either one.
		 */
		TArray<FString> NonDefaultProperties;
	};

	/**
	 * Reads the tree and renders it as text.
	 *
	 * Returns an empty string for an asset that is not a Widget Blueprint, so a caller
	 * can tell "no widget tree" from "an empty widget tree".
	 */
	static FString Render(UBaseWidgetBlueprint* InWidgetBlueprint, FString& OutError);

	/** The entries, for a caller that wants the facts rather than the text. */
	static bool Collect(UBaseWidgetBlueprint* InWidgetBlueprint,
		TArray<FEntry>& OutEntries, FString& OutError);
};
