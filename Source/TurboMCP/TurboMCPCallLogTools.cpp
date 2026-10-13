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

	// *** A LARGE NUMBER IS CLAMPED, A NON-NUMBER IS REFUSED, AND THOSE ARE NOT THE SAME REQUEST. *** `P6`
	// wants a bounded answer, so "a million lines" is a caller asking for as much as this tool will give, and
	// it is clamped at 500 below. `banana` is not a request: turning it into the default would answer a
	// question nobody asked, so it is refused by name - the shape `TurboMCPBlueprintLoad.cpp:100-114` uses.
	constexpr int32 DefaultLimit = 50;
	int32 Max = DefaultLimit;
	if (!Limit.TrimStartAndEnd().IsEmpty())
	{
		if (!Limit.TrimStartAndEnd().IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a limit. Pass a whole number of entries, or leave it empty for the ")
				TEXT("default of %d."), *Limit, DefaultLimit);
		}
		Max = FCString::Atoi(*Limit);
		if (Max <= 0)
		{
			return FString::Printf(
				TEXT("error: a limit of %d is not usable. Pass a positive number of entries, or leave it empty ")
				TEXT("for the default of %d."), Max, DefaultLimit);
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
				|| Entry.Source.Contains(FilterText, ESearchCase::IgnoreCase)
			// *** THE ASSET AND THE STATUS TOO, WHICH `03_TOOLS.md` ASKS FOR AND WHICH THE FILTER DID NOT REACH: an
			// asset only ever appears inside the arguments, and a status is `bError` rather than a text field, so
			// neither could be found by the three fields above. ONE WORD STILL FINDS EITHER. ***
			|| Entry.Arguments.Contains(FilterText, ESearchCase::IgnoreCase)
			|| FString(Entry.bError ? TEXT("error") : TEXT("ok")).Contains(FilterText, ESearchCase::IgnoreCase);
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

	// ONE GROUP PER BATCH SCRIPT: the session id is the same for every call of one script, so a change of id starts a
	// new group and is stated once, ahead of its rows.
	FString PreviousSession;
	bool bFirstGroup = true;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
		// *** GROUPED BY BATCH, WHICH `03_TOOLS.md` ASKS FOR: THE CALLS OF ONE SCRIPT SHARE A SESSION ID, SO A CHANGE
		// OF ID IS THE START OF A NEW GROUP AND IS STATED ONCE, AHEAD OF ITS ROWS. A COLUMN WOULD REPEAT IT ON EVERY
		// LINE, WHICH IS THE ONE THING A GROUPING IS FOR. ***
		if (bFirstGroup || Entry.SessionId != PreviousSession)
		{
			bFirstGroup = false;
			PreviousSession = Entry.SessionId;
			Out += FString::Printf(TEXT("\n; session %s"),
				Entry.SessionId.IsEmpty() ? TEXT("(none)") : *Entry.SessionId);
		}

		Out += FString::Printf(TEXT("\n%s  %-5s  %-6s  %s"),
			*Entry.Timestamp, *Entry.Phase, *Entry.Source,
			*FString::Printf(TEXT("%s.%s"), *Entry.Toolset, *Entry.Tool));

		if (Entry.DurationMs > 0.0)
		{
			Out += FString::Printf(TEXT("  %.1fms  "), Entry.DurationMs)
			+ FTurboMCPCallLog::FEntry::DescribeSize(Entry.ResultChars) + TEXT(" chars  ~")
			+ FTurboMCPCallLog::FEntry::DescribeSize(Entry.TokenEstimate) + TEXT(" tokens");
				
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
