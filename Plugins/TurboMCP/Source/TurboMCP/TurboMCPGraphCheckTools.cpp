// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphCheck.h"

/**
 * `P-GRAPH-CHECK`, the gate every write entry point depends on.
 *
 * THE DECISION TO ACCEPT OR REFUSE IS MADE IN `FTurboMCPGraphCheck::Plan`, NOT HERE. This function renders
 * what that returned and nothing else. `03_TOOLS.md` requires every check to live in a `Plan` so a
 * Preview and its Edit twin refuse the SAME input, and that is not merely a convention here: `P/E-GRAPH-APPEND`,
 * `REPLACE` and `PATCH` all take this same text, so the write tools must be able to reach the decision this
 * checker reached rather than re-deriving it. Re-deriving it is exactly the two-implementations bug this
 * project has recorded for the graph text and the node ID, and a checker that passes text a writer then
 * refuses is worse than no checker at all, because the caller believes it.
 *
 * IT WRITES NOTHING, AND NOT BECAUSE IT IS CAREFUL. It is in `UTurboMCPPreviewToolset`, which by this
 * project's design holds no tool that can change an asset; the guard is the CLASS. Inside, the graph is
 * held as a `const UEdGraph*` from the moment it is found, so a mutating call would fail to compile.
 */
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphCheck(
	const FString& Asset, const FString& GraphName, const FString& Text)
{
	FTurboMCPGraphCheck::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphCheck::Plan(Asset, GraphName, Text, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}

	return FTurboMCPGraphCheck::Render(Plan);
}
