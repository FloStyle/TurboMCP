// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPPureCheckCommandlet.h"

#include "TurboMCPCallCost.h"

// `LogTurboMCP` IS DECLARED HERE, NOT IN TurboMCPModule.h - verified by grepping for the declaration, which is at
// `Logging.h:20`. The Kit commandlet includes BOTH, and only this one carries the category.
#include "Logging.h"

#include "Logging/LogMacros.h"

namespace
{
	/** How many assertions ran, and how many failed - printed as a VERDICT rather than as prose. */
	int32 GChecksRun = 0;
	int32 GChecksFailed = 0;

	/**
	 * ONE ASSERTION, WITH THE INPUT THAT MAKES IT FAIL PRINTED BESIDE IT.
	 *
	 * THE PROJECT'S RULE, APPLIED TO THIS FILE'S OWN CHECKS: for each check, NAME THE INPUT THAT MAKES IT FAIL. A
	 * check that cannot state its failing input passes for the wrong reason - four workstreams in four rounds have
	 * reported exactly that, and one suite has an else-branch that literally checks `True`. So `Detail` is not
	 * decoration here; it is the evidence, and it prints on BOTH outcomes.
	 */
	void Check(const FString& Name, bool bPassed, const FString& Detail)
	{
		++GChecksRun;
		if (bPassed)
		{
			UE_LOG(LogTurboMCP, Display, TEXT("  ok   %s"), *Name);
		}
		else
		{
			++GChecksFailed;
			UE_LOG(LogTurboMCP, Error, TEXT("  FAIL %s"), *Name);
		}
		// THE DETAIL PRINTS WHETHER THE CHECK PASSED OR FAILED, because a green check whose detail would have
		// explained a red one is how a reader learns what the check was actually looking at.
		UE_LOG(LogTurboMCP, Display, TEXT("         %s"), *Detail);
	}
}

UTurboMCPPureCheckCommandlet::UTurboMCPPureCheckCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UTurboMCPPureCheckCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTurboMCP, Display, TEXT("TurboMCP: pure-function checks, with LITERAL inputs and no engine state"));

	// =============================================================================================
	// 1. THE TWO SPELLINGS THE LOG'S TWO WRITERS PRODUCE MUST COLLAPSE TO ONE KEY.
	//
	// THE EXACT STRINGS, TAKEN FROM THE LOG ITSELF AND NOT INVENTED: the analytics provider registers FULLY
	// QUALIFIED names (`TurboMCPAnalyticsProvider.cpp:55`), so `Toolset` arrives as `TurboMCP.TurboMCPReadToolset`
	// AND `Tool` as `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus` - APPENDING ONE TO THE OTHER DOUBLES IT -
	// while this plugin's own `FScopedCall` records the BARE `TurboMCPReadToolset`.
	//
	// *** A PREFIX STRIP CANNOT UNDO A DOUBLING. THE FIRST VERSION OF THIS NORMALISER STRIPPED A LEADING
	// `TurboMCP.`, AND THE TOOL'S FIRST REAL RUN PRINTED
	// `TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog` - THE QUALIFICATION IS NOT A PREFIX,
	// IT IS A WHOLE PATH APPEARING TWICE. THE RULE THAT WORKS IS THE LAST DOTTED SEGMENT, which is stable against
	// BOTH writers. ***
	// =============================================================================================
	const FString Qualified = TEXT("TurboMCP.TurboMCPReadToolset");
	const FString Bare = TEXT("TurboMCPReadToolset");

	const FString FromQualified = FTurboMCPCallCost::NormaliseToolset(Qualified);
	const FString FromBare = FTurboMCPCallCost::NormaliseToolset(Bare);

	Check(TEXT("the ANALYTICS provider's qualified spelling normalises to the toolset's own name"),
		FromQualified == TEXT("TurboMCPReadToolset"),
		FString::Printf(TEXT("NormaliseToolset(\"%s\") -> \"%s\", wanted \"TurboMCPReadToolset\""),
			*Qualified, *FromQualified));

	Check(TEXT("THIS PLUGIN's bare spelling normalises to the SAME name"),
		FromBare == TEXT("TurboMCPReadToolset"),
		FString::Printf(TEXT("NormaliseToolset(\"%s\") -> \"%s\", wanted \"TurboMCPReadToolset\""),
			*Bare, *FromBare));

	// *** THE ASSERTION THE WHOLE FILE EXISTS FOR: THE TWO SPELLINGS COLLAPSE TO ONE KEY. A per-tool total that
	// does not collapse them reports ONE TOOLSET AS TWO ROWS WITH A DENOMINATOR EACH. ***
	Check(TEXT("THE TWO SPELLINGS COLLAPSE TO ONE KEY - which is the whole reason this file exists"),
		FromQualified == FromBare,
		FString::Printf(TEXT("\"%s\" vs \"%s\" - ONE key or the totals are reported wrong"),
			*FromQualified, *FromBare));

	// AND A SECOND TOOLSET, BECAUSE ONE WOULD NOT SHOW A NORMALISER THAT COLLAPSED EVERYTHING TO A CONSTANT.
	const FString OtherQualified = TEXT("TurboMCP.TurboMCPEditToolset");
	const FString OtherBare = TEXT("TurboMCPEditToolset");
	const FString OtherNormalised = FTurboMCPCallCost::NormaliseToolset(OtherQualified);

	Check(TEXT("a SECOND toolset does not collapse into the first - so the rule is not a constant"),
		OtherNormalised == TEXT("TurboMCPEditToolset") && OtherNormalised != FromQualified,
		FString::Printf(TEXT("\"%s\" -> \"%s\", and it differs from \"%s\""),
			*OtherQualified, *OtherNormalised, *FromQualified));

	Check(TEXT("the edit toolset's two spellings also collapse to one key"),
		FTurboMCPCallCost::NormaliseToolset(OtherBare) == OtherNormalised,
		FString::Printf(TEXT("\"%s\" -> \"%s\""), *OtherBare,
			*FTurboMCPCallCost::NormaliseToolset(OtherBare)));

	// =============================================================================================
	// 2. THE EMPTY CASE IS A DIFFERENT ANSWER FROM A ZERO, WHICH IS PRINCIPLE `P1`.
	//
	// `AssetFromArguments` decides whether a call named an asset. AN ENGINE EVENT CARRIES NO ARGUMENTS AT ALL, so
	// the empty answer must be DISTINGUISHABLE from a call that named a real asset - and the tool's own output
	// says so: "TOTALS PER ASSET: none can be computed. 33 considered entries name no asset in its arguments.
	// An engine event carries NO ARGUMENTS AT ALL, so this section can only fill from this plugin's own tools -
	// AND THAT IS AN ABSENT MEASUREMENT, NOT ZERO COST."
	// =============================================================================================
	const FString NoArguments = FTurboMCPCallCost::AssetFromArguments(TEXT(""));

	Check(TEXT("an entry with NO arguments names NO asset - the engine-event case, which is the common one"),
		NoArguments.IsEmpty(),
		FString::Printf(TEXT("AssetFromArguments(\"\") -> \"%s\", wanted empty"), *NoArguments));

	// AND THE LITERAL THAT DOES NAME ONE, SO THE CHECK IS NOT SATISFIED BY A FUNCTION THAT ALWAYS RETURNS EMPTY.
	const FString WithAsset =
		FTurboMCPCallCost::AssetFromArguments(TEXT("{\"asset\":\"/Game/Tests/Kit/TA01_HandLaid\"}"));

	Check(TEXT("an entry WITH an asset argument names it - so the empty answer above is a real distinction"),
		!WithAsset.IsEmpty() && WithAsset.Contains(TEXT("TA01_HandLaid")),
		FString::Printf(TEXT("AssetFromArguments({\"asset\":...}) -> \"%s\""), *WithAsset));

	// =============================================================================================
	// RESULT
	//
	// THE VERDICT LINE IS PRINTED IN THE HOUSE FORM, because the graph suite reported NO verdict for four rounds
	// by computing a count and never printing one - and `run-suites.ps1` captures this line and nothing else.
	// =============================================================================================
	if (GChecksFailed > 0)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("RESULT: FAIL (%d of %d pure check(s) failed)"), GChecksFailed, GChecksRun);
		return 1;
	}

	UE_LOG(LogTurboMCP, Display, TEXT("RESULT: PASS (%d pure check(s) run)"), GChecksRun);
	return 0;
}
