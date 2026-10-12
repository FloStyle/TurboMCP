// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Commandlets/Commandlet.h"

#include "TurboMCPCallCostCommandlet.generated.h"

/**
 * Proves `FTurboMCPCallCost`'s PURE FUNCTIONS, which no MCP suite can reach.
 *
 * *** WHY THIS EXISTS AT ALL, AND IT IS A MEASURED GAP RATHER THAN A PREFERENCE. *** The suite
 * `Tools/test-call-cost.py` passes 29 checks, **AND ITS BREAK TEST DID NOT BITE**: I replaced the
 * whole body of `FTurboMCPCallCost::NormaliseToolset` with `return Trimmed;`, rebuilt, and the suite
 * still reported `PASS (29 check(s) run)`. The reason is that the check which should have caught it
 * needs the log's `toolset` field to be the QUALIFIED engine form, and the suite cannot make the
 * engine emit one - so **29 CHECKS IS NOT 29 INDEPENDENT CHECKS**, and two of them cannot fail for
 * the reason they exist.
 *
 * `NormaliseToolset` IS A PURE FUNCTION OF ITS ARGUMENT, so it does not need a log, an engine event
 * or an editor. This commandlet calls it with LITERAL inputs - both spellings the log's two writers
 * produce - and asserts they collapse to ONE KEY. **THAT IS THE CHECK THE BREAK WOULD HAVE FAILED,
 * AND IT IS THE CHECK AN MCP SUITE STRUCTURALLY CANNOT MAKE.**
 *
 * HOW IT IS INVOKED. `-run=TurboMCP.TurboMCPCallCostCommandlet`, WITH THE MODULE PREFIX, because the
 * engine calls `LoadModule` only when the token contains a period (`LaunchEngineLoop.cpp:4100`).
 * There is deliberately NO static initialiser calling `LoadModule`: `TurboMCPKitCommandlet.h`
 * records that this was tried and crashed with an access violation.
 *
 * IT NEEDS NO EDITOR, WHICH IS THE POINT: a `-run=` commandlet that only computes strings cannot be
 * affected by the editor restarts that have made this project's suites move their failing names.
 */
UCLASS()
class UTurboMCPCallCostCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPCallCostCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};
