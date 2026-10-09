// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
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
 * Creating an override on purpose, shared by `P-OVERRIDE-ADD` and `E-OVERRIDE-ADD`.
 *
 * `Spec/02_PRINCIPLES.md` `D-02` says a Preview and its Edit twin share one code path up to the
 * write step, so the candidate is RESOLVED and validated once and the Preview prints exactly what
 * the Edit will do.
 *
 * WHY THIS IS A WRAPPER AND NOT A NEW MECHANISM
 * ---------------------------------------------
 * The engine already has the operation: `UBlueprintEditorLibrary::AddFunctionOverride` and
 * `AddEventOverride`. What it does not have is the three things `03_TOOLS.md` asks for, and those
 * are the whole reason this capability is in the roadmap:
 *
 *  - the Preview prints the exact signature, which means resolving the function and describing its
 *    parameters before anything is created;
 *  - it REFUSES when the function is already overridden, and says WHERE, which the engine's call
 *    does not do — it would quietly add a second graph;
 *  - the Edit reports what it created, with the graph and node so a caller can act on it.
 *
 * The overridable list comes from `ListEvents` and `ListFunctions`, the editor's own set, so a
 * name this tool accepts is a name the editor accepts. That also means interfaces implemented
 * natively in C++ are covered, which a hand-rolled filter missed the first time round.
 */
class FTurboMCPOverrideAdd
{
public:
	/** The resolved candidate. */
	struct FPlan
	{
		FString Asset;

		/** The name a caller passed. */
		FString Wanted;

		/** The name the editor knows it by, which may differ in case. */
		FName ResolvedName;

		/** True when it is an event rather than a function. */
		bool bIsEvent = false;

		/** The signature, ready to print. */
		FString Signature;

		/** Where it is already implemented, when it is. */
		FString AlreadyImplementedIn;
	};

	/** Resolves and validates the request without creating anything. */
	static bool Plan(const FString& InAsset, const FString& InFunction,
		FPlan& OutPlan, FString& OutError);

	/** Prints the exact signature the override would have. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Creates the override and reports the graph and node it made. */
	static FString Apply(const FPlan& InPlan);
};
