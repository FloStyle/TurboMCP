// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallCostCommandlet.h"

#include "Containers/UnrealString.h"
#include "Logging.h"
#include "Misc/CommandLine.h"
#include "TurboMCPCallCost.h"

namespace
{
	int32 Failures = 0;
	int32 ChecksRun = 0;

	/** One check, and the count is always printed - a verdict without a denominator says nothing. */
	void Check(const FString& InName, bool bInOk, const FString& InDetail = FString())
	{
		++ChecksRun;
		if (bInOk)
		{
			UE_LOG(LogTurboMCP, Display, TEXT("  ok   %s"), *InName);
		}
		else
		{
			++Failures;
			UE_LOG(LogTurboMCP, Error, TEXT("  FAIL %s%s"), *InName,
				InDetail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  %s"), *InDetail));
		}
	}

	/**
	 * Both spellings of ONE toolset, and whether they collapse to one key.
	 *
	 * THE TWO INPUTS ARE THE TWO WRITERS' REAL OUTPUT, taken from the log rather than invented:
	 * the analytics provider resolves a hash against a registry of FULLY QUALIFIED names
	 * (`TurboMCPAnalyticsProvider.cpp:55`), and this plugin's own `FScopedCall` records the bare name.
	 */
	void CheckCollapse(const FString& InQualified, const FString& InBare, const FString& InExpected)
	{
		const FString CollapsedQualified = FTurboMCPCallCost::NormaliseToolset(InQualified);
		const FString CollapsedBare = FTurboMCPCallCost::NormaliseToolset(InBare);

		Check(FString::Printf(TEXT("the engine's qualified spelling '%s' collapses to '%s'"),
			*InQualified, *InExpected),
			CollapsedQualified == InExpected,
			FString::Printf(TEXT("got '%s'"), *CollapsedQualified));

		Check(FString::Printf(TEXT("this plugin's bare spelling '%s' is unchanged"),
			*InBare),
			CollapsedBare == InExpected,
			FString::Printf(TEXT("got '%s'"), *CollapsedBare));

		// *** THE CHECK THE BREAK TEST WOULD HAVE FAILED, AND THE ONE THE MCP SUITE COULD NOT MAKE. ***
		// It does not matter which spelling arrived; the KEY must be one key. A normaliser that
		// returned its input would produce two different keys here and THIS is what catches it.
		Check(FString::Printf(TEXT("both spellings of '%s' collapse to ONE key"), *InExpected),
			CollapsedQualified == CollapsedBare,
			FString::Printf(TEXT("'%s' vs '%s'"), *CollapsedQualified, *CollapsedBare));
	}
}

UTurboMCPCallCostCommandlet::UTurboMCPCallCostCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTurboMCPCallCostCommandlet::Main(const FString& Params)
{
	Failures = 0;
	ChecksRun = 0;

	UE_LOG(LogTurboMCP, Display, TEXT("=== R-CALL-COST pure functions, which need no editor ==="));

	// ---------------------------------------------------------------------------------------------
	// THE DOUBLING, WHICH IS THE DEFECT OBSERVED LIVE. The first real run of the tool printed
	// `TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog`, because the qualified
	// toolset was PREPENDED to an already-qualified tool.
	// ---------------------------------------------------------------------------------------------
	CheckCollapse(TEXT("TurboMCP.TurboMCPEditToolset"), TEXT("TurboMCPEditToolset"), TEXT("TurboMCPEditToolset"));
	CheckCollapse(TEXT("TurboMCP.TurboMCPReadToolset"), TEXT("TurboMCPReadToolset"), TEXT("TurboMCPReadToolset"));
	CheckCollapse(TEXT("TurboMCP.TurboMCPPreviewToolset"), TEXT("TurboMCPPreviewToolset"), TEXT("TurboMCPPreviewToolset"));
	CheckCollapse(TEXT("TurboMCP.TurboMCPLookToolset"), TEXT("TurboMCPLookToolset"), TEXT("TurboMCPLookToolset"));

	// ---------------------------------------------------------------------------------------------
	// AND THE CASE THAT MUST *NOT* BE FOLDED: the engine's top-level placeholder is NOT a toolset
	// name, and collapsing it would INVENT a name the engine deliberately withheld.
	// ---------------------------------------------------------------------------------------------
	Check(TEXT("the engine's (top-level) placeholder is passed through, not sliced"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("(top-level)")) == TEXT("(top-level)"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("(top-level)")));

	Check(TEXT("an empty toolset stays empty rather than becoming a name"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("")).IsEmpty(),
		FTurboMCPCallCost::NormaliseToolset(TEXT("")));

	// A name with no dot is its own last segment, so it is returned whole.
	Check(TEXT("a name with no dot is returned unchanged"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("NoDotHere")) == TEXT("NoDotHere"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("NoDotHere")));

	// ---------------------------------------------------------------------------------------------
	// A THREE-DEEP QUALIFICATION, WHICH IS THE SHAPE THE LOST WORK RECORDED. If a join is applied
	// twice the path nests twice, and the rule must still reach the toolset's own name.
	// ---------------------------------------------------------------------------------------------
	Check(TEXT("a DOUBLY qualified path still collapses to the toolset's own name"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("TurboMCP.TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset"))
			== TEXT("TurboMCPReadToolset"),
		FTurboMCPCallCost::NormaliseToolset(TEXT("TurboMCP.TurboMCPReadToolset.TurboMCP.TurboMCPReadToolset")));

	// ---------------------------------------------------------------------------------------------
	// THE EMPTINESS VOCABULARY, WHICH `P1` RESTS ON: three emptinesses that must read differently,
	// because "turn logging on", "call something" and "change the filter" are different actions.
	// ---------------------------------------------------------------------------------------------
	{
		const FString Off = FTurboMCPCallCost::DescribeEmptiness(FTurboMCPCallCost::EEmptiness::LoggingOff);
		const FString OnEmpty = FTurboMCPCallCost::DescribeEmptiness(FTurboMCPCallCost::EEmptiness::LogOnAndEmpty);
		const FString Filtered = FTurboMCPCallCost::DescribeEmptiness(FTurboMCPCallCost::EEmptiness::FilterMatchedNothing);

		Check(TEXT("logging OFF names the switch that turns it on"),
			Off.Contains(TEXT("E-CALL-LOG")), Off);
		Check(TEXT("logging OFF says explicitly that this is NOT a result of zero"),
			Off.Contains(TEXT("NOT a result of zero")), Off);
		Check(TEXT("ON and empty is a DIFFERENT answer from OFF"),
			OnEmpty != Off && OnEmpty.Contains(TEXT("ON")), OnEmpty);
		Check(TEXT("a filter matching nothing is a THIRD, different answer"),
			Filtered != Off && Filtered != OnEmpty && Filtered.Contains(TEXT("matched")), Filtered);
	}

	// ---------------------------------------------------------------------------------------------
	// THE ASSET EXTRACTION, WHICH IS THE OTHER PURE FUNCTION AND IS THE ONE THAT FAILS SILENTLY IF
	// IT IS WRONG: an asset name that comes back empty means the per-asset totals are quietly short.
	// ---------------------------------------------------------------------------------------------
	{
		Check(TEXT("an asset is read out of the conventional `asset` key"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"asset\":\"BP_NodeTest\"}")) == TEXT("BP_NodeTest"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"asset\":\"BP_NodeTest\"}")));

		Check(TEXT("a value containing a dot survives whole, rather than being cut at the dot"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"asset\":\"/Game/Tests/BP_NodeTest\"}"))
				== TEXT("/Game/Tests/BP_NodeTest"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"asset\":\"/Game/Tests/BP_NodeTest\"}")));

		Check(TEXT("`assets` is preferred, because a plural key is the one that lists assets"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"assets\":\"BP_A\",\"asset\":\"BP_B\"}")) == TEXT("BP_A"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"assets\":\"BP_A\",\"asset\":\"BP_B\"}")));

		Check(TEXT("arguments naming no asset give an empty string rather than an invented one"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"toolset\":\"X\"}")).IsEmpty(),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"toolset\":\"X\"}")));

		Check(TEXT("empty arguments give an empty string"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("")).IsEmpty(),
			FTurboMCPCallCost::AssetFromArguments(TEXT("")));

		// A key that merely CONTAINS an asset key must not match it: this is the difference between
		// reading a field and finding a substring.
		Check(TEXT("a longer key ending in `asset` is not mistaken for the `asset` key"),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"myasset\":\"BP_X\"}")).IsEmpty(),
			FTurboMCPCallCost::AssetFromArguments(TEXT("{\"myasset\":\"BP_X\"}")));
	}

	UE_LOG(LogTurboMCP, Display, TEXT(""));
	if (Failures > 0)
	{
		UE_LOG(LogTurboMCP, Error, TEXT("RESULT: FAIL (%d) of %d check(s)"), Failures, ChecksRun);
		return 1;
	}
	UE_LOG(LogTurboMCP, Display, TEXT("RESULT: PASS (%d check(s) run)"), ChecksRun);
	return 0;
}
