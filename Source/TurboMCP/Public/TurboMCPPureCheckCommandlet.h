// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Commandlets/Commandlet.h"

#include "TurboMCPPureCheckCommandlet.generated.h"

/**
 * ASSERTS THE TWO PURE FUNCTIONS THAT AN MCP SUITE STRUCTURALLY CANNOT TEST, BY RELOCATION RATHER THAN BY REPAIR.
 *
 * WHY THIS FILE EXISTS AT ALL, AND WHY IT IS NOT `TurboMCPCallCostCommandlet`
 * -------------------------------------------------------------------------
 * `TurboMCPCallCostCommandlet` was written to make exactly these assertions, and IT CANNOT BE RUN IN THIS TREE.
 * MEASURED, TWICE, IN `TestRecord.md` section 73:
 *
 *     -run=TurboMCPCallCostCommandlet          -> "looked like a commandlet, but was not found"
 *     -run=TurboMCP.TurboMCPCallCostCommandlet -> Ensure condition failed: InModuleName != NAME_None,
 *                                                 then Assertion failed: IsValid() in FModuleManager, exit 3
 *
 * AND THE ALTERNATIVE ROUTE WAS ALREADY CLOSED BEFORE THAT: `TurboMCPKitCommandlet.h` records that a static
 * initialiser calling `LoadModule` WAS TRIED AND CRASHED WITH AN ACCESS VIOLATION. So both routes are closed, and
 * the answer is NOT to repair what does not run - it is to PUT THE ASSERTIONS SOMEWHERE THAT IS ALREADY PROVEN TO
 * RUN, which is what this commandlet is. It is modelled on `TurboMCPKitCommandlet`, which runs, writes, verifies its
 * own structure, exits zero, and is invoked with the module prefix by `Tools/make-kit-assets.ps1:57`.
 *
 * THE CLAIM THAT MATTERS, AND THE ONE AN MCP SUITE CANNOT MAKE
 * -----------------------------------------------------------
 * `FTurboMCPCallCost::NormaliseToolset` collapses the log's TWO toolset spellings to ONE key. The log's two writers
 * disagree - the analytics provider records `TurboMCP.TurboMCPReadToolset` while this plugin's own `FScopedCall`
 * records the bare `TurboMCPReadToolset` - and a per-tool total that does not collapse them REPORTS ONE TOOLSET AS
 * TWO ROWS WITH A DENOMINATOR EACH, which is exactly the "totals per tool" `T-HELP-6` asks for, REPORTED WRONG.
 *
 * *** THE SUITE'S BREAK TEST ON THIS FUNCTION DID NOT BITE: replacing the body with `return Trimmed;` STILL GAVE
 * `RESULT: PASS (29 check(s) run)`, BECAUSE THE CHECK NEEDS THE LOG TO CARRY A QUALIFIED ENGINE FORM AND THE SUITE
 * CANNOT MAKE THE ENGINE EMIT ONE. That is the whole reason this file takes LITERAL inputs instead. ***
 *
 * AND IT NEEDS NO EDITOR, NO ASSET AND NO LOG - which is the point: it computes strings, so it is not affected by
 * the editor restarts that have made this project's suites move their failing names between runs.
 *
 * HOW IT IS INVOKED. `-run=TurboMCP.TurboMCPPureCheckCommandlet`, WITH THE MODULE PREFIX, because the engine calls
 * `LoadModule` only when the token contains a period. There is deliberately NO static initialiser calling
 * `LoadModule`, for the reason recorded above.
 */
UCLASS()
class UTurboMCPPureCheckCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTurboMCPPureCheckCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};
