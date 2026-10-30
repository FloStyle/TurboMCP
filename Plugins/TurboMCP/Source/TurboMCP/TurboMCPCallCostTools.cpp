// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
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
	
	FString LastSegment(const FString& InName)
	{
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

	FString ToolLabel(const FTurboMCPCallLog::FEntry& InEntry)
	{
		return FString::Printf(TEXT("%s.%s"),
			*FTurboMCPCallCost::NormaliseToolset(InEntry.Toolset),
			*LastSegment(InEntry.Tool));
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPCallCost(
	const FString& Since,
	const FString& Filter,
	const FString& Cap)
{
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

	const TArray<FTurboMCPCallLog::FEntry> All = FTurboMCPCallTimeline::RepresentativeRecords(Records);

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

	TArray<FTurboMCPCallLog::FEntry> Selected;
	int32 SkippedBySince = 0;
	int32 SkippedByFilter = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : All)
	{
		if (!SinceText.IsEmpty() && Entry.Timestamp <= SinceText)
		{
			++SkippedBySince;
			continue;
		}
		if (!FilterText.IsEmpty())
		{
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

	const FTurboMCPCallLog::FEntry* Slowest = nullptr;
	const FTurboMCPCallLog::FEntry* Priciest = nullptr;
	int32 WithTokens = 0;
	int32 WithDuration = 0;
	int32 Errors = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		if (Entry.Phase == TEXT("end"))
		{
			++WithDuration;
			if (Slowest == nullptr || Entry.DurationMs > Slowest->DurationMs)
			{
				Slowest = &Entry;
			}
		}
		if (Entry.TokenEstimate >= 0)
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
		Out += FString::Printf(
			TEXT("\n  none of the %d considered call(s) recorded a token estimate."),
			Selected.Num());
		Out += TEXT("\n  Unknown estimates are not zero cost. Engine-only records do not carry result text; unfinished calls have no final result estimate.");
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
			TEXT("\n  none of the %d considered call(s) recorded a completed-call duration."), Selected.Num());
		Out += TEXT("\n  Completion was not observed for these calls; elapsed time and outcome are unknown.");
	}

	// Track measurement coverage separately: missing observations must not look like zero cost.
	struct FCoverage { int32 Tokens = 0; int32 Chars = 0; int32 Completed = 0; };
	TMap<FString, FCoverage> ToolCoverage, AssetCoverage;
	auto AddCoverage = [](FCoverage& Coverage, const FTurboMCPCallLog::FEntry& Entry)
	{
		Coverage.Tokens += Entry.TokenEstimate >= 0 ? 1 : 0;
		Coverage.Chars += Entry.ResultChars >= 0 ? 1 : 0;
		Coverage.Completed += Entry.Phase == TEXT("end") ? 1 : 0;
	};
	auto MeasuredTotal = [](int32 Total, int32 Known, int32 Calls)
	{
		return Known == 0 ? FString(TEXT("unknown")) : FString::Printf(TEXT("%d (%d/%d measured)"), Total, Known, Calls);
	};
	TMap<FString, FTurboMCPCallCost::FTotal> ByTool;
	TMap<FString, FTurboMCPCallCost::FTotal> ByAsset;
	int32 Unattributed = 0;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		const FString Key = ToolLabel(Entry);
		FTurboMCPCallCost::FTotal& Row = ByTool.FindOrAdd(Key);
		Row.Key = Key;
		AddCoverage(ToolCoverage.FindOrAdd(Key), Entry);

		++Row.Calls;
		Row.TotalMs += Entry.DurationMs;
		Row.TotalChars += Entry.MeasuredChars();
		Row.TotalTokens += Entry.MeasuredTokens();
		if (Entry.bError)
		{
			++Row.Errors;
		}
		if (Entry.TokenEstimate >= 0)
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
			AddCoverage(AssetCoverage.FindOrAdd(Asset), Entry);
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
		const FCoverage& Coverage = ToolCoverage.FindChecked(Row.Key);
		Out += FString::Printf(TEXT("\n  %s | %d call(s) | %d observed error(s) | %s tokens | %s chars | %.1fms observed total | %d/%d completed"),
			*Row.Key, Row.Calls, Row.Errors, *MeasuredTotal(Row.TotalTokens, Coverage.Tokens, Row.Calls),
			*MeasuredTotal(Row.TotalChars, Coverage.Chars, Row.Calls), Row.TotalMs, Coverage.Completed, Row.Calls);
	}

	const TArray<FTurboMCPCallCost::FTotal> AssetRows = SortedRows(ByAsset);
	if (AssetRows.Num() == 0)
	{
		Out += TEXT("\n\nTOTALS PER ASSET: none can be computed.");
		Out += FString::Printf(
			TEXT("\n  %d considered call(s) name no asset in their arguments."),
			Unattributed);
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
			Out += FString::Printf(TEXT("\n  %s | %d call(s) | %d observed error(s) | %s tokens | %.1fms observed total | %d/%d completed"),
				*Row.Key, Row.Calls, Row.Errors, *MeasuredTotal(Row.TotalTokens, AssetCoverage.FindChecked(Row.Key).Tokens, Row.Calls), Row.TotalMs, AssetCoverage.FindChecked(Row.Key).Completed, Row.Calls);
		}
		if (Unattributed > 0)
		{
			Out += FString::Printf(
				TEXT("\n  ; %d further call(s) name no asset and are NOT in these totals."),
				Unattributed);
		}
	}

	Out += TEXT("\n\n; Token counts estimate returned text, not model billing or total conversation usage. Unknown measurements are excluded from observed totals. Asset attribution uses the first recognized argument path; it is not a complete list of assets touched.");
	Out += TEXT("\n; intervals: `now` is not accepted, because the log's clock is the editor's and a");
	Out += TEXT("\n; caller's is not. Pass `since` an ISO-8601 stamp taken from a previous answer.");
	return Out;
}
