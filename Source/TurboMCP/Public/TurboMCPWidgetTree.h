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
 * The compact widget tree behind `R-BP-LOAD`'s widget section.
 *
 * `03_TOOLS.md` asks for one line per widget with its name, class, whether it is a
 * variable, its visibility, its slot padding when that is not zero, the bound events,
 * and — for a placed user widget — the properties whose value on this placed copy
 * differs from the class default.
 *
 * WHY THE BOUND EVENTS COME FROM REFLECTION
 * ------------------------------------------
 * A widget's delegates are not stored one-per-widget anywhere convenient. When a Widget
 * Blueprint is compiled, every widget marked as a variable gets a property on the
 * generated class, and a delegate bound in the designer becomes a multicast delegate
 * property on that same object. So the bindings are found by walking the generated
 * class's properties, which is the same source the engine's own binding editor writes.
 *
 * WHY THIS DOES NOT CLAIM A WIDGET IS UNUSED
 * -------------------------------------------
 * The spec is explicit: "Not a variable" is a fact, "nothing uses it" is a claim that
 * needs proof. This reader reports the first and never the second. A widget that is not
 * a variable may still be driven by a binding or an animation, and nothing here checks
 * that.
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

		/** One line per bound delegate: the event property and the function bound to it. */
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
