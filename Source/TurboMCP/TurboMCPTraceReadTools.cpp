// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPTrace.h"

/**
 * `R-TRACE`, the read half of the trace-point pair.
 *
 * THE DECISION LIVES IN `TurboMCPTrace::Plan`, NOT HERE, so this function and its Edit twin reach the same
 * answer for the same input. `Spec/03_TOOLS.md` requires an action of `status` from the read side because a
 * caller has to be able to see what is armed and how often it ran without changing anything.
 *
 * IT CANNOT CHANGE AN ASSET, AND NOT BECAUSE IT IS CAREFUL. It is in `UTurboMCPReadToolset`, which by this
 * project's design holds no tool that can write; the guard is the CLASS. That matters more here than usual:
 * the state this tool reports is an in-memory registry, so a read of it is a read of nothing on disk.
 */
FString UTurboMCPReadToolset::ReadTurboMCPTrace(const FString& Action, const FString& NodeId)
{
	// THE MUTATING-ACTION REFUSAL COMES BEFORE THE ARGUMENT CHECK, AND THE ORDER IS THE POINT.
	//
	// MEASURED: asking the read tool for `arm` with no node ID used to answer `arm needs at least one node ID`
	// - a TRUE answer to the wrong question. It told the caller how to FIX THE ARGUMENT when the thing wrong
	// was that THIS TOOL MUST NOT ARM AT ALL. A caller following that advice would pass an ID and be refused
	// again for a different reason, and the promise that a Read tool cannot change anything would have been
	// reported as an argument problem instead of as the boundary it is.
	//
	// So the action is classified first and the arguments second. A read tool refuses a mutation on SIGHT.
	const FString Wanted = Action.TrimStartAndEnd().ToLower();
	if (Wanted == TEXT("arm") || Wanted == TEXT("clear"))
	{
		return FString::Printf(
			TEXT("error: '%s' changes the trace registry, so the read tool does not perform it. Call ")
			TEXT("E-TRACE for arm and clear, or R-TRACE with status to see what is armed.\n\n%s"),
			*Wanted, *TurboMCPTrace::Status());
	}

	TurboMCPTrace::FPlan Plan;
	FString Error;
	if (!TurboMCPTrace::Plan(Action.IsEmpty() ? TEXT("status") : Action, NodeId, Plan, Error))
	{
		return FString::Printf(TEXT("error: %s"), *Error);
	}

	return TurboMCPTrace::Status();
}
