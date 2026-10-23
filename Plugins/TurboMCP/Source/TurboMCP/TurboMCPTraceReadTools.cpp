// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPTrace.h"

// Status-only entry point. Explicit action checks prevent registry mutations.
FString UTurboMCPReadToolset::ReadTurboMCPTrace(const FString& Action, const FString& NodeId)
{
	// Reject mutations before validating node IDs so recovery guidance names the right tool.
	const FString Wanted = Action.TrimStartAndEnd().ToLower();
	if (Wanted == TEXT("arm") || Wanted == TEXT("clear"))
	{
		return FString::Printf(
			TEXT("error: '%s' changes the trace registry, so the read tool does not perform it. Call ")
			TEXT("EditTurboMCPTrace to arm or clear points, or ReadTurboMCPTrace with Action=status to inspect them.\n\n%s"),
			*Wanted, *TurboMCPTrace::Status());
	}

	TurboMCPTrace::FPlan Plan;
	FString Error;
	if (!TurboMCPTrace::Plan(Action.IsEmpty() ? TEXT("status") : Action, NodeId, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return TurboMCPTrace::Status();
}
