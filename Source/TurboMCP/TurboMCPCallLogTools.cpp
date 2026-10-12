// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

#include "TurboMCPCallLog.h"
#include "TurboMCPTokenEstimate.h"

/**
 * `R-CALL-LOG`, `E-CALL-LOG` and `E-CALL-LOG-CLEAR`.
 *
 * The log is written by two sources — the analytics provider for engine tool calls and
 * `FTurboMCPCallLog::FScopedCall` inside this plugin's own tools — and read through one tool, so a
 * reader does not have to know which source a line came from. The `source` field says it.
 */

FString UTurboMCPReadToolset::ReadTurboMCPCallLog(
	const FString& Filter, const FString& Limit, const FString& Since)
{
	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const TArray<FTurboMCPCallLog::FEntry> All = Log.GetEntries();

	// The limit is capped rather than refused, because `P6` wants a bounded answer and a caller
	// asking for a million lines wants "as many as you will give me" rather than an error.
	int32 Max = 50;
	if (!Limit.TrimStartAndEnd().IsEmpty())
	{
		Max = FCString::Atoi(*Limit);
		if (Max <= 0)
		{
			Max = 50;
		}
	}
	const int32 Cap = FMath::Min(Max, 500);

	// `since` is matched against the ISO-8601 timestamp, which sorts lexically because it is
	// fixed-width UTC. A caller resumes from the last time it saw rather than re-reading.
	const FString SinceText = Since.TrimStartAndEnd();
	const FString FilterText = Filter.TrimStartAndEnd();

	TArray<FTurboMCPCallLog::FEntry> Selected;
	for (const FTurboMCPCallLog::FEntry& Entry : All)
	{
		if (!SinceText.IsEmpty() && Entry.Timestamp <= SinceText)
		{
			continue;
		}
		if (!FilterText.IsEmpty())
		{
			// The filter matches the toolset, the tool or the source, so one word finds either a
			// toolset's calls or the engine's.
			const bool bMatch =
				Entry.Toolset.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Tool.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Source.Contains(FilterText, ESearchCase::IgnoreCase);
			if (!bMatch)
			{
				continue;
			}
		}
		Selected.Add(Entry);
	}

	// Newest first, because the newest is what a reader usually wants, then the cap takes the top.
	Algo::Reverse(Selected);
	const int32 Total = Selected.Num();
	if (Selected.Num() > Cap)
	{
		Selected.SetNum(Cap);
	}

	FString Out = TEXT("read: the TurboMCP call log");
	Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
	Out += FString::Printf(TEXT("\nrecording: %s"),
		Log.IsEnabled() ? TEXT("on") : TEXT("OFF (E-CALL-LOG turns it on)"));
	Out += FString::Printf(TEXT("\ncounts: %d in memory, showing %d newest of %d matching"),
		All.Num(), Selected.Num(), Total);

	if (Selected.Num() == 0)
	{
		// `P1`: never return nothing. Saying WHY it is empty is the answer.
		if (All.Num() == 0)
		{
			Out += Log.IsEnabled()
				? TEXT("\nthe log is on and empty, so no tool has been called yet this session.")
				: TEXT("\nthe log is OFF, so nothing has been recorded. E-CALL-LOG turns it on.");
		}
		else
		{
			Out += FString::Printf(
				TEXT("\n%d entries are in memory but none match '%s'%s."),
				All.Num(), *FilterText,
				SinceText.IsEmpty() ? TEXT("")
					: *FString::Printf(TEXT(" after %s"), *SinceText));
		}
		return Out;
	}

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		Out += FString::Printf(TEXT("\n%s  %-5s  %-6s  %s"),
			*Entry.Timestamp, *Entry.Phase, *Entry.Source,
			*FString::Printf(TEXT("%s.%s"), *Entry.Toolset, *Entry.Tool));

		if (Entry.DurationMs > 0.0)
		{
			Out += FString::Printf(TEXT("  %.1fms  %d chars  ~%d tokens"),
				Entry.DurationMs, Entry.ResultChars, Entry.TokenEstimate);
		}
		if (Entry.bError)
		{
			Out += TEXT("  ERROR");
		}
		if (!Entry.Arguments.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n      args: %s"),
				Entry.Arguments.Len() > 200
					? *Entry.Arguments.Left(200) : *Entry.Arguments);
		}
		if (!Entry.Result.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n      result: %s"),
				Entry.Result.Len() > 200 ? *Entry.Result.Left(200) : *Entry.Result);
		}
	}

	if (Total > Selected.Num())
	{
		Out += FString::Printf(
			TEXT("\n; %d more matched. Raise limit, or pass since='%s' to page forward."),
			Total - Selected.Num(), *Selected[0].Timestamp);
	}

	Out += TEXT("\n; the token estimate is an estimate, per 04_FORMATS.md section 5.");
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPCallLog(const FString& Enabled)
{
	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const bool bWas = Log.IsEnabled();

	// The value is parsed strictly rather than through `ToBool`, because `FString::ToBool`
	// answers false for anything it does not recognise and a caller who typed `on` would silently
	// turn the log OFF instead of being told their word was not understood.
	const FString Text = Enabled.TrimStartAndEnd().ToLower();
	bool bWanted = false;
	if (Text == TEXT("true") || Text == TEXT("on") || Text == TEXT("yes") || Text == TEXT("1"))
	{
		bWanted = true;
	}
	else if (Text == TEXT("false") || Text == TEXT("off") || Text == TEXT("no")
		|| Text == TEXT("0"))
	{
		bWanted = false;
	}
	else
	{
		return FString::Printf(
			TEXT("error: '%s' is not a yes or no. Pass true or false."), *Enabled);
	}

	Log.SetEnabled(bWanted);

	if (bWas == bWanted)
	{
		return FString::Printf(
			TEXT("unchanged: the call log was already %s. The choice is stored in the editor ")
			TEXT("settings, so it survives a restart."),
			bWanted ? TEXT("on") : TEXT("off"));
	}

	FString Out = FString::Printf(TEXT("changed: the call log is now %s"),
		bWanted ? TEXT("on") : TEXT("off"));
	Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
	Out += TEXT("\n; the choice is stored in the editor settings, so it survives a restart.");
	if (bWanted)
	{
		Out += TEXT("\n; calls made BEFORE this point are not in the log, including engine ")
			TEXT("tool calls, because the analytics provider only sees events from now on.");
	}
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPCallLogClear(const FString& Confirm)
{
	if (!Confirm.TrimStartAndEnd().Equals(TEXT("yes"), ESearchCase::IgnoreCase)
		&& !Confirm.TrimStartAndEnd().Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return TEXT("error: clearing needs confirmation. Pass confirm=yes. Only the ")
			TEXT("in-memory entries are dropped; the log FILE on disk is left alone, because ")
			TEXT("it is the record of what happened and erasing it is not this tool's job.");
	}

	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const int32 Count = Log.GetEntries().Num();
	Log.Clear();

	return FString::Printf(
		TEXT("changed: dropped %d entry(s) from memory. The log file at %s is untouched."),
		Count, *Log.GetLogFilePath());
}
