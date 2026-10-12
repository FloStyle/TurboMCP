// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPPlayCheck.h"

/**
 * `R-PLAY-CHECK`: play the project for a few seconds and report what it complained about.
 *
 * THE DECISION TO REPORT IS MADE IN `TurboMCPPlayCheck::Run`, NOT HERE. This function renders what that
 * returned and nothing else: it decides nothing about whether a session may start, whether one is already
 * running, or whether a window is finished. `P/E` pairs must refuse the same input through their shared
 * plan, and a tool with a single entry point gets the same protection by keeping the decision out of the
 * renderer - there is exactly one place that can answer "no", and it is the same place every caller reaches.
 *
 * BOTH PARAMETERS ARE STRINGS BECAUSE THE ENGINE'S TOOL REGISTRY MARKS AN FString PARAMETER REQUIRED
 * whatever its default, which is the note already carried by `R-TOOL-SCHEMA`. So "not given" is spelled as
 * an EMPTY STRING rather than by omitting the argument.
 */
FString UTurboMCPReadToolset::ReadTurboMCPPlayCheck(const FString& Seconds, const FString& Map)
{
	const TurboMCPPlayCheck::FAnswer Answer = TurboMCPPlayCheck::Run(Seconds, Map);

	switch (Answer.Outcome)
	{
	case TurboMCPPlayCheck::EOutcome::Refused:
		// Refusals open with `error:` the way every other TurboMCP tool's refusals do, so a caller that
		// checks the prefix keeps working.
		return FString::Printf(TEXT("error: R-PLAY-CHECK was refused. %s"), *Answer.Reason);

	case TurboMCPPlayCheck::EOutcome::AlreadyRunning:
		return FString::Printf(TEXT("already playing: %s"), *Answer.Reason);

	case TurboMCPPlayCheck::EOutcome::Pending:
		// `pending:` IS A REAL ANSWER AND NOT AN ERROR. The session is running; the caller is being told to
		// ask again, which is the only honest thing a single call can say.
		return FString::Printf(TEXT("pending: %s"), *Answer.Reason);

	case TurboMCPPlayCheck::EOutcome::Reported:
		break;
	}

	FString Out = FString::Printf(TEXT("play check: %s"), *Answer.Reason);

	Out += FString::Printf(TEXT("\n%d warning(s) and %d error(s) in the window"),
		Answer.WarningCount, Answer.ErrorCount);

	if (Answer.WarningCount == 0 && Answer.ErrorCount == 0)
	{
		// `P1`: never return a bare zero. A clean run is a RESULT, so it is stated rather than left as an
		// empty list that a caller has to interpret.
		Out += TEXT("\nThe session raised no warnings and no errors. It ran and stopped cleanly.");
		return Out;
	}

	Out += Answer.Body;
	return Out;
}
