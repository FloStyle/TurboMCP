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

class UWidgetBlueprint;

/**
 * Repairing widget bindings after a class swap, behind `P/E-WIDGET-RETARGET`.
 *
 * `03_TOOLS.md` names the shape: a widget replaced by one of another class under the same name
 * leaves bound events and call nodes expecting the old class. The Preview lists them; the Edit
 * keeps each bound event the new class can still satisfy and REMOVES the ones it cannot, saying so.
 *
 * HOW "CAN THE NEW CLASS SATISFY IT" IS DECIDED, AND WHY THIS IS NOT A GUESS
 * --------------------------------------------------------------------------
 * The engine answers this itself, in `FDelegateEditorBinding::IsBindingValid`. It finds the target
 * widget, then looks for a `FDelegateProperty` on the WIDGET'S OWN CLASS named either
 * `<PropertyName>Delegate` or `<PropertyName>`, and only if one exists is the binding valid. So the
 * same lookup is done here rather than a rule being invented: a binding whose delegate the new
 * class does not carry is exactly the one the engine's compiler would reject.
 *
 * The binding is identified by the pair the engine's own `operator==` uses — `ObjectName` and
 * `PropertyName` — because that pair is what the editor treats as the identity of a binding.
 */
class FTurboMCPWidgetRetarget
{
public:
	/** One binding, with what the current class can do with it. */
	struct FBinding
	{
		/** The member widget the binding is on. */
		FString ObjectName;

		/** The property or delegate on that widget. */
		FString PropertyName;

		/** The function generated for it, when there is one. */
		FString FunctionName;

		/** The class of the widget it is on, as it stands now. */
		FString WidgetClass;

		/** True when the widget's own class carries the delegate. */
		bool bDelegateExists = false;

		/** True when the member widget cannot be found at all. */
		bool bWidgetMissing = false;

		/** True when the delegate is a MULTICAST one, which is what widget events are. */
		bool bIsMulticast = false;

		/** The name the engine would look for, for the message. */
		FString DelegateLookedFor;
	};

	/** The findings, shared by the Preview and the Edit. */
	struct FPlan
	{
		FString Asset;
		TArray<FBinding> Bindings;

		/** Call nodes that name a function the current class does not have. */
		TArray<FString> StaleCalls;

		int32 NumBroken() const;
	};

	/** Collects the findings without changing anything. */
	static bool Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError);

	/** Lists every binding and whether the current class can still satisfy it. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Removes the bindings the current class cannot satisfy, and says which. */
	static FString Apply(const FPlan& InPlan);
};
