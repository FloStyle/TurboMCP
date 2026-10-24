// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPTrace.h"

// Trace controls update the in-memory registry; event delivery depends on engine instrumentation.
FString UTurboMCPEditToolset::EditTurboMCPTrace(const FString& Action, const FString& NodeId)
{
	TurboMCPTrace::FPlan Plan;
	FString Error;
	if (!TurboMCPTrace::Plan(Action, NodeId, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return TurboMCPTrace::Apply(Plan);
}
