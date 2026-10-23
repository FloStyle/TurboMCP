// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "HAL/PlatformString.h"
#include "Misc/Paths.h"

#include "TurboMCPCallCost.h"
#include "TurboMCPCallLog.h"
#include "TurboMCPCallTimeline.h"

namespace
{
	/**
	 * The last dotted segment of a name, or the name unchanged when it has none.
	 *
	 * MEASURED NEED, NOT DEFENSIVENESS. The analytics provider resolves a tool's hash against a
	 * registry of FULLY QUALIFIED names (`TurboMCPAnalyticsProvider.cpp:55`), so `Entry.Tool` is
	 * already `TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog` - and the tool's own first real run
	 * printed
	 *
	 *     TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog | 1 call(s) | ...
	 *
	 * because the qualified toolset was PREPENDED to an already-qualified tool. The last segment is
	 * the tool's own name, and it is stable against this plugin's own writer too, which records the
	 * bare name whose last segment is itself.
	 */
	FString LastSegment(const FString& InName)
	{
		// *** A PLACEHOLDER IS ATOMIC, AND THIS BRANCH EXISTS BECAUSE THE TOOL'S OWN OUTPUT SHOWED THE
		// DEFECT. *** An unresolved engine hash is written as `(engine tool ending 866fe74ba22c)` -
		// HUMAN-READABLE TEXT WITH DOTS IN IT - so slicing at the last dot produced
		// `TurboMCPReadToolset.(engine tool ending 866fe74ba22c)`, whose "tool" is the fragment
		// `866fe74ba22c)`. That label is A LIE ABOUT A NAME THAT WAS HONESTLY MARKED AS UNKNOWN.
		// A parenthesised name is a statement, not a path, so it is returned whole.
		if (InName.StartsWith(TEXT("("), ESearchCase::CaseSensitive))
		{
			return InName;
		}

		int32 LastDot = INDEX_NONE;
		if (!InName.FindLastChar(TEXT('.'), LastDot) || LastDot < 0 || LastDot + 1 >= InName.Len())
		{
			return InName;
		}
		return InName.Mid(LastDot + 1);
	}

	/** `Toolset.Tool`, BOTH COMPACTED, which is the label a per-tool row is keyed on. */
	FString ToolLabel(const FTurboMCPCallLog::FEntry& InEntry)
	{
		return FString::Printf(TEXT("%s.%s"),
			*FTurboMCPCallCost::NormaliseToolset(InEntry.Toolset),
			*LastSegment(InEntry.Tool));
	}
}

/**
 * `R-CALL-COST`.
 *
 * A READER IN THE READ TOOLSET, WHICH IS STRUCTURALLY UNABLE TO WRITE. It answers the four questions
 * `03_TOOLS.md` names for it - which calls cost the most TOKENS, which are SLOWEST, which FAILED, and
 * what the totals are PER TOOL and PER ASSET - and it keeps the first two SEPARATE, because a call
 * that returns a huge payload and a call that blocks the game thread are different problems.
 *
 * IT READS THE SAME OBJECT `R-CALL-LOG` READS, which is how it "shares its parser" as the spec
 * requires: `FTurboMCPCallLog::GetEntries()` hands over `FEntry` structs the log itself parsed, so
 * there is no second JSON reader that could drift from the first.
 *
 * EVERY TOTAL CARRIES ITS DENOMINATOR. A per-tool row with no call count is a number nobody can
 * judge, and this project has recorded ten cases of a check that passed while the thing it verified
 * was broken - the purest of which is an empty result with no denominator.
 */
FString UTurboMCPReadToolset::ReadTurboMCPCallCost(
	const FString& Since,
	const FString& Filter,
	const FString& Cap)
{
	// Validate even with an empty log. Saturate only after validating every digit:
	// arbitrarily large positive caps request the supported maximum without overflow.
	const FString CapText = Cap.TrimStartAndEnd();
	int32 ShowRows = CapText.IsEmpty() ? 10 : 0;
	for (TCHAR Digit : CapText)
	{
		if (Digit < TEXT('0') || Digit > TEXT('9'))
			return TEXT("error: cap must contain positive whole-number digits only, or be empty for default 10.");
		ShowRows = FMath::Min(50, ShowRows * 10 + (Digit - TEXT('0')));
	}
	if (ShowRows == 0)
		return TEXT("error: cap must be positive; leave it empty for default 10. Maximum displayed rows is 50.");

	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const TArray<FTurboMCPCallLog::FEntry> Records = Log.GetEntries();

	// *** ONE RECORD PER CALL, WHICH IS A CORRECTION THIS TOOL NEEDED THE MOMENT THE PLUGIN'S OWN TOOLS
	// BEGAN RECORDING. *** Every total below counts ENTRIES, and while every entry was an engine `end`
	// record an entry WAS a call. Now one call leaves up to four records - the plugin's `start` and
	// `end` and the engine's two events - so counting entries would have doubled every total, counted
	// the `call_tool` dispatch as a second call, and reported `2 call(s)` per asset because BOTH
	// records of one plugin pair carry the arguments. `FTurboMCPCallTimeline` merges them, and
	// `RenderTimeline` uses the same assembly, so the list and the total cannot disagree.
	const TArray<FTurboMCPCallLog::FEntry> All = FTurboMCPCallTimeline::RepresentativeRecords(Records);

	// ---------------------------------------------------------------------------------------------
	// `P1` FIRST, BEFORE ANY NUMBER IS COMPUTED: an empty answer must say WHICH emptiness it is.
	// The spec names three and they need different actions from whoever reads them - "logging is
	// off" means turn it on, "on and empty" means call something, and "nothing matched the filter"
	// means change the filter. Reporting zeros for all three would tell an agent its tools are cheap
	// when in fact nothing was recorded at all.
	// ---------------------------------------------------------------------------------------------
	if (All.Num() == 0)
	{
		FString Out = TEXT("read: what the calls cost");
		Out += TEXT("\nrow cap: maximum 50 per list; cap does not limit totals.");
		Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
		Out += FString::Printf(TEXT("\nrecording: %s"),
			Log.IsEnabled() ? TEXT("on") : TEXT("OFF"));
		Out += TEXT("\n\nNo recorded calls are available for cost analysis:");
		Out += FString::Printf(TEXT("\n  %s"), *FTurboMCPCallCost::DescribeEmptiness(
			Log.IsEnabled()
				? FTurboMCPCallCost::EEmptiness::LogOnAndEmpty
				: FTurboMCPCallCost::EEmptiness::LoggingOff));
		return Out;
	}

	const FString SinceText = Since.TrimStartAndEnd();
	const FString FilterText = Filter.TrimStartAndEnd();


	// ---------------------------------------------------------------------------------------------
	// FILTER, and every rejected entry is counted so the answer can say how much it did not look at.
	// ---------------------------------------------------------------------------------------------
	TArray<FTurboMCPCallLog::FEntry> Selected;
	int32 SkippedBySince = 0;
	int32 SkippedByFilter = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : All)
	{
		// `since` is matched against the ISO-8601 timestamp, which sorts lexically because it is
		// fixed-width UTC - the same rule `R-CALL-LOG` uses, so the two agree on what "later" means.
		if (!SinceText.IsEmpty() && Entry.Timestamp <= SinceText)
		{
			++SkippedBySince;
			continue;
		}
		if (!FilterText.IsEmpty())
		{
			// MATCHED AGAINST THE NORMALISED TOOLSET, so a filter written the engine's way and one
			// written this plugin's way find the SAME calls. Without this the two spellings of one
			// toolset would answer differently to the same question.
			// MATCHED AGAINST THE COMPACTED LABEL AND THE RAW FIELDS, so a filter written the engine's
			// way, this plugin's way, or as a bare tool name ALL find the same calls. Without the
			// compacted label the two spellings of one toolset answer differently to one question.
			const bool bMatch =
				ToolLabel(Entry).Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Toolset.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Tool.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Source.Contains(FilterText, ESearchCase::IgnoreCase);
			if (!bMatch)
			{
				++SkippedByFilter;
				continue;
			}
		}
		Selected.Add(Entry);
	}

	FString Out = TEXT("read: what the calls cost");
		Out += TEXT("\nrow cap: maximum 50 per list; cap does not limit totals.");
	Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
	Out += FString::Printf(TEXT("\nrecording: %s"),
		Log.IsEnabled() ? TEXT("on") : TEXT("OFF"));
	Out += FString::Printf(
		TEXT("\ncounts: %d call(s) from %d record(s) in the log, %d considered, %d skipped before %s, ")
		TEXT("%d did not match the filter"),
		All.Num(), Records.Num(), Selected.Num(), SkippedBySince,
		SinceText.IsEmpty() ? TEXT("(no bound given)") : *SinceText, SkippedByFilter);

	if (Selected.Num() == 0)
	{
		Out += TEXT("\n\nNo recorded calls are available for cost analysis:");
		Out += FString::Printf(TEXT("\n  %s"), *FTurboMCPCallCost::DescribeEmptiness(
			FTurboMCPCallCost::EEmptiness::FilterMatchedNothing));
		Out += FString::Printf(TEXT("\n  %d call%s in the log, none matching '%s'."),
			All.Num(), All.Num() == 1 ? TEXT(" is") : TEXT("s are"), *FilterText);
		return Out;
	}

	// ---------------------------------------------------------------------------------------------
	// THE TWO RANKINGS, COMPUTED SEPARATELY AND FROM THE START.
	//
	// THEY ARE DIFFERENT QUESTIONS WITH DIFFERENT ANSWERS: the call with the largest payload is not
	// the call that blocked longest, and a single "most expensive" would answer whichever one the
	// author had in mind. Each ranking below also counts HOW MANY ENTRIES RECORDED A NON-ZERO value
	// for the thing it is ranking, because a ranking over all-zero rows IMPLIES A RANKING EXISTS.
	// ---------------------------------------------------------------------------------------------
	const FTurboMCPCallLog::FEntry* Slowest = nullptr;
	const FTurboMCPCallLog::FEntry* Priciest = nullptr;
	int32 WithTokens = 0;
	int32 WithDuration = 0;
	int32 Errors = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		if (Entry.DurationMs > 0.0)
		{
			++WithDuration;
			if (Slowest == nullptr || Entry.DurationMs > Slowest->DurationMs)
			{
				Slowest = &Entry;
			}
		}
		if (Entry.TokenEstimate > 0)
		{
			++WithTokens;
			if (Priciest == nullptr || Entry.TokenEstimate > Priciest->TokenEstimate)
			{
				Priciest = &Entry;
			}
		}
		if (Entry.bError)
		{
			++Errors;
		}
	}

	// The line shape is PIPES rather than column alignment, and that is a MEASURED choice: a tool
	// name contains dots and the engine's tool names contain SPACES, so a whitespace-aligned format
	// cannot be split back apart. The lost work on this capability paid for that lesson - it changed
	// the OUTPUT rather than writing a cleverer regex, which is the right direction for the fix.
	Out += FString::Printf(
		TEXT("\n\nranked %d call(s) of %d, %d recording a duration and %d recording a token estimate; %d failed"),
		Selected.Num(), All.Num(), WithDuration, WithTokens, Errors);

	Out += TEXT("\n\nMOST EXPENSIVE BY TOKENS");
	if (Priciest != nullptr)
	{
		Out += FString::Printf(TEXT("\n  %s | %s | %s token estimate | %s chars | %.1fms"),
			*Priciest->Timestamp,
			*ToolLabel(*Priciest),
			*FTurboMCPCallLog::FEntry::DescribeSize(Priciest->TokenEstimate), *FTurboMCPCallLog::FEntry::DescribeSize(Priciest->ResultChars), Priciest->DurationMs);
	}
	else
	{
		// `P1` AND `P7`: this is not a zero, it is an absence of the measurement, and it says what to
		// do about it. A report that printed a winner here would be ranking rows that all read zero.
		Out += FString::Printf(
			TEXT("\n  none of the %d considered call(s) recorded a token estimate above zero."),
			Selected.Num());
		// *** AND THE REASON IS NOW A NARROW ONE, WHICH IS A CORRECTION RATHER THAN A REWORDING. ***
		// This branch used to say a token estimate "is written by this plugin's own tools", which was
		// TRUE OF THE DESIGN AND FALSE OF THE CODE: nothing called `FScopedCall`, so no plugin record
		// existed and every estimate really was absent. The tools now record, so the only way to reach
		// this branch is a considered set made entirely of ENGINE-ONLY calls - an engine tool, or the
		// `call_tool` meta-tool - whose event carries no result text to estimate from.
		Out += TEXT("\n  THIS IS NOT ZERO COST. A token estimate is written at this plugin's tool boundary,");
		Out += TEXT("\n  which only this plugin's own tools pass through; an engine tool's call arrives");
		Out += TEXT("\n  through the engine's event, which carries no result text at all. Use SLOWEST below,");
		Out += TEXT("\n  which a call can answer from its duration alone.");
	}

	Out += TEXT("\n\nSLOWEST BY DURATION");
	if (Slowest != nullptr)
	{
		Out += FString::Printf(TEXT("\n  %s | %s | %.1fms | %s chars | %s token estimate"),
			*Slowest->Timestamp,
			*ToolLabel(*Slowest),
			Slowest->DurationMs, *FTurboMCPCallLog::FEntry::DescribeSize(Slowest->ResultChars), *FTurboMCPCallLog::FEntry::DescribeSize(Slowest->TokenEstimate));
	}
	else
	{
		Out += FString::Printf(
			TEXT("\n  none of the %d considered call(s) recorded a duration above zero."), Selected.Num());
		Out += TEXT("\n  A start record with no end records no duration by design, so this means every");
		Out += TEXT("\n  considered line is a start record - a call that began and whose end is missing.");
	}

	// ---------------------------------------------------------------------------------------------
	// TOTALS, KEYED ON THE NORMALISED TOOLSET. THIS IS WHERE THE DOUBLY-QUALIFIED NAME WOULD BITE:
	// without `NormaliseToolset` the engine's `TurboMCP.TurboMCPReadToolset` and this plugin's
	// `TurboMCPReadToolset` would be TWO ROWS for ONE toolset, each with its own denominator.
	// ---------------------------------------------------------------------------------------------
	TMap<FString, FTurboMCPCallCost::FTotal> ByTool;
	TMap<FString, FTurboMCPCallCost::FTotal> ByAsset;
	int32 Unattributed = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		const FString Key = ToolLabel(Entry);
		FTurboMCPCallCost::FTotal& Row = ByTool.FindOrAdd(Key);
		Row.Key = Key;

		// A START RECORD CARRIES NO OUTCOME, so it is counted as a call but contributes nothing to
		// the totals. Counting it as a zero-cost call would UNDERSTATE the average, and dropping it
		// would UNDERSTATE the count - so it is counted, and the interesting case, a start with no
		// end, is what the duration line above reports.
		++Row.Calls;
		Row.TotalMs += Entry.DurationMs;
		Row.TotalChars += Entry.MeasuredChars();
		Row.TotalTokens += Entry.MeasuredTokens();
		if (Entry.bError)
		{
			++Row.Errors;
		}
		if (Entry.TokenEstimate > 0)
		{
			Row.bAllTokensZero = false;
		}
		if (Entry.DurationMs > Row.SlowestMs)
		{
			Row.SlowestMs = Entry.DurationMs;
			Row.SlowestStamp = Entry.Timestamp;
		}
		if (Entry.TokenEstimate > Row.MostTokens)
		{
			Row.MostTokens = Entry.TokenEstimate;
			Row.MostTokensStamp = Entry.Timestamp;
		}

		const FString Asset = FTurboMCPCallCost::AssetFromArguments(Entry.Arguments);
		if (!Asset.IsEmpty())
		{
			FTurboMCPCallCost::FTotal& AssetRow = ByAsset.FindOrAdd(Asset);
			AssetRow.Key = Asset;
			++AssetRow.Calls;
			AssetRow.TotalMs += Entry.DurationMs;
			AssetRow.TotalChars += Entry.MeasuredChars();
			AssetRow.TotalTokens += Entry.MeasuredTokens();
			if (Entry.bError)
			{
				++AssetRow.Errors;
			}
		}
		else
		{
			++Unattributed;
		}
	}

	/** Sorted by the total that was asked for, with the key as the tie-break so the answer is stable. */
	auto SortedRows = [](const TMap<FString, FTurboMCPCallCost::FTotal>& InMap)
	{
		TArray<FTurboMCPCallCost::FTotal> Rows;
		InMap.GenerateValueArray(Rows);
		Rows.Sort([](const FTurboMCPCallCost::FTotal& A, const FTurboMCPCallCost::FTotal& B)
		{
			if (A.TotalMs != B.TotalMs)
			{
				return A.TotalMs > B.TotalMs;
			}
			return A.Key < B.Key;
		});
		return Rows;
	};

	const TArray<FTurboMCPCallCost::FTotal> ToolRows = SortedRows(ByTool);
	Out += FString::Printf(TEXT("\n\nTOTALS PER TOOL (%d row(s), each with its call count)"), ToolRows.Num());
	for (int32 Index = 0; Index < ToolRows.Num(); ++Index)
	{
		if (Index >= ShowRows)
		{
			Out += FString::Printf(TEXT("\n  ... and %d more. Increase cap up to 50, or narrow the filter; totals include all matching calls."), ToolRows.Num() - ShowRows);
			break;
		}
		const FTurboMCPCallCost::FTotal& Row = ToolRows[Index];
		Out += FString::Printf(TEXT("\n  %s | %d call(s) | %d error(s) | %d tokens | %d chars | %.1fms total | %.1fms slowest"),
			*Row.Key, Row.Calls, Row.Errors, Row.TotalTokens, Row.TotalChars, Row.TotalMs, Row.SlowestMs);
		if (Row.bAllTokensZero)
		{
			// Said per row rather than once, because a reader scanning one line needs to know that
			// this row's zero is an absent measurement and not a cheap tool.
			Out += TEXT(" | NO TOKEN ESTIMATE RECORDED");
		}
	}

	const TArray<FTurboMCPCallCost::FTotal> AssetRows = SortedRows(ByAsset);
	if (AssetRows.Num() == 0)
	{
		// `P1` AGAIN: the section's absence is stated with its reason, because an empty per-asset
		// section would otherwise read as "no asset cost anything" instead of "no call named one".
		Out += TEXT("\n\nTOTALS PER ASSET: none can be computed.");
		Out += FString::Printf(
			TEXT("\n  %d considered call(s) name no asset in their arguments."),
			Unattributed);
		// *** THE SECOND SENTENCE IS NOW FALSE AND IS CORRECTED RATHER THAN KEPT. *** It said this
		// section "can only fill from this plugin's own tools", which was true while nothing recorded
		// arguments. The plugin's tools record now, so the section fills from every call that passed
		// through the boundary - and the only calls that cannot name an asset are the engine-only
		// ones, whose event carries no arguments at all.
		Out += TEXT("\n  An engine tool's call carries NO ARGUMENTS AT ALL, so only a call that passed");
		Out += TEXT("\n  through this plugin's own boundary can name one - an absent measurement, not zero cost.");
	}
	else
	{
		Out += FString::Printf(TEXT("\n\nTOTALS PER ASSET (%d row(s))"), AssetRows.Num());
		for (int32 Index = 0; Index < AssetRows.Num(); ++Index)
		{
			if (Index >= ShowRows)
			{
				Out += FString::Printf(TEXT("\n  ... and %d more. Increase cap up to 50, or narrow the filter; totals include all matching calls."), AssetRows.Num() - ShowRows);
				break;
			}
			const FTurboMCPCallCost::FTotal& Row = AssetRows[Index];
			Out += FString::Printf(TEXT("\n  %s | %d call(s) | %d error(s) | %d tokens | %.1fms total"),
				*Row.Key, Row.Calls, Row.Errors, Row.TotalTokens, Row.TotalMs);
		}
		if (Unattributed > 0)
		{
			// NAMED RATHER THAN DROPPED: silently omitting these would make the per-asset totals look
			// complete when they are missing part of the log.
			Out += FString::Printf(
				TEXT("\n  ; %d further call(s) name no asset and are NOT in these totals."),
				Unattributed);
		}
	}

	Out += TEXT("\n\n; the token estimate is an estimate, per 04_FORMATS.md section 5.");
	Out += TEXT("\n; intervals: `now` is not accepted, because the log's clock is the editor's and a");
	Out += TEXT("\n; caller's is not. Pass `since` an ISO-8601 stamp taken from a previous answer.");
	return Out;
}
