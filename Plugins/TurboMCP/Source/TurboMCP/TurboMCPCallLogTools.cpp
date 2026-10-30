// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

#include "TurboMCPCallLog.h"
#include "TurboMCPCallTimeline.h"
#include "TurboMCPTokenEstimate.h"

namespace
{
	// Keep each record field on one line and explicitly mark omitted text.
	FString CallLogExcerpt(const FString& Text)
	{
		FString Escaped = Text.Replace(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Escaped.Len() > 200 ? Escaped.Left(200) + TEXT(" [truncated; see disk log]") : Escaped;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPCallLog(
	const FString& Filter, const FString& Limit, const FString& Since)
{
	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();

	const TArray<FTurboMCPCallLog::FEntry> All =
		FTurboMCPCallTimeline::WithSessionsInEffect(Log.GetEntries());

	const FString LimitText = Limit.TrimStartAndEnd();
	int32 Cap = LimitText.IsEmpty() ? 50 : 0;
	for (TCHAR Digit : LimitText)
	{
		if (Digit < TEXT('0') || Digit > TEXT('9'))
			return TEXT("error: Limit must contain positive whole-number digits only. Leave it empty for 50 entries; the maximum is 500.");
		Cap = FMath::Min(500, Cap * 10 + (Digit - TEXT('0')));
	}
	if (Cap == 0)
		return TEXT("error: Limit must be greater than zero. Leave it empty for 50 entries; the maximum is 500.");

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
			const bool bMatch =
				Entry.Toolset.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Tool.Contains(FilterText, ESearchCase::IgnoreCase)
				|| Entry.Source.Contains(FilterText, ESearchCase::IgnoreCase)
			|| Entry.Arguments.Contains(FilterText, ESearchCase::IgnoreCase)
			|| FString(Entry.Phase == TEXT("start") ? TEXT("incomplete") : (Entry.bError ? TEXT("error") : TEXT("ok"))).Contains(FilterText, ESearchCase::IgnoreCase);
			if (!bMatch)
			{
				continue;
			}
		}
		Selected.Add(Entry);
	}

	Algo::Reverse(Selected);
	const int32 Total = Selected.Num();
	if (Selected.Num() > Cap)
	{
		Selected.SetNum(Cap);
	}

	FString Out = TEXT("read: the TurboMCP call log");
	Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
	Out += FString::Printf(TEXT("\nrecording: %s"),
		Log.IsEnabled() ? TEXT("on") : TEXT("off (use EditTurboMCPCallLog to enable recording)"));
	Out += FString::Printf(TEXT("\ncounts: %d in memory, showing %d newest of %d matching"),
		All.Num(), Selected.Num(), Total);

	if (Selected.Num() == 0)
	{
		if (All.Num() == 0)
		{
			Out += Log.IsEnabled()
				? TEXT("\nRecording is on, but no entries are currently retained. The log may have been cleared.")
				: TEXT("\nRecording is off and no entries are currently retained. Use EditTurboMCPCallLog to enable recording.");
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

	FString PreviousSession;
	bool bFirstGroup = true;

	for (const FTurboMCPCallLog::FEntry& Entry : Selected)
	{
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

		if (Entry.Phase == TEXT("end"))
		{
			Out += FString::Printf(TEXT("  %.1fms  "), Entry.DurationMs)
			+ FTurboMCPCallLog::FEntry::DescribeSize(Entry.ResultChars) + TEXT(" chars  ~")
			+ FTurboMCPCallLog::FEntry::DescribeSize(Entry.TokenEstimate) + TEXT(" tokens");
				
		}
		if (Entry.Phase == TEXT("start"))
		{
			Out += TEXT("  outcome not observed in this record");
		}
		if (Entry.bError)
		{
			Out += TEXT("  ERROR");
		}
		if (!Entry.Arguments.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n      args: %s"),
				*CallLogExcerpt(Entry.Arguments));
		}
		if (!Entry.Result.IsEmpty())
		{
			Out += FString::Printf(TEXT("\n      result: %s"),
				*CallLogExcerpt(Entry.Result));
		}
	}

	if (Total > Selected.Num())
	{
		Out += FString::Printf(
			TEXT("\n; %d older matching records are omitted. Increase Limit (maximum 500) or narrow Filter. Since='%s' selects newer records; it does not retrieve these omitted records."),
			Total - Selected.Num(), *Selected[0].Timestamp);
	}

	Out += TEXT("\n; Sessions group connection activity, not necessarily one script. Missing session IDs are inferred from nearby records.");
	Out += TEXT("\n; Token counts are approximate, not measured with a model tokenizer.");
	Out += TEXT("\n; one call can leave several records: this plugin writes a start and an end, and ");
	Out += TEXT("the engine writes its own event for the same call. `source` says which wrote a line.");
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPCallLog(const FString& Enabled)
{
	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const bool bWas = Log.IsEnabled();

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
			TEXT("error: Enabled='%s' is not a supported boolean value. Pass true or false."), *Enabled);
	}

	Log.SetEnabled(bWanted);
	if (Log.IsEnabled() != bWanted)
		return TEXT("error: Recording settings did not change. Check editor settings and try again.");

	if (bWas == bWanted)
	{
		return FString::Printf(
			TEXT("unchanged: Recording was already %s. Editor settings persistence was requested; disk persistence was not verified."),
			bWanted ? TEXT("on") : TEXT("off"));
	}

	FString Out = FString::Printf(TEXT("changed: the call log is now %s"),
		bWanted ? TEXT("on") : TEXT("off"));
	Out += FString::Printf(TEXT("\nlog file: %s"), *Log.GetLogFilePath());
	Out += TEXT("\n; Editor settings persistence was requested; disk persistence was not verified.");
	if (bWanted)
	{
		Out += TEXT("\n; Recording is enabled for subsequent calls. Existing records are retained; calls made while recording was off are not recovered.");
	}
	return Out;
}

FString UTurboMCPEditToolset::EditTurboMCPCallLogClear(const FString& Confirm)
{
	if (!Confirm.TrimStartAndEnd().Equals(TEXT("yes"), ESearchCase::IgnoreCase)
		&& !Confirm.TrimStartAndEnd().Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return TEXT("error: clearing needs confirmation. Pass confirm=yes. Only the ")
			TEXT("in-memory entries are dropped; the log file on disk is left alone.");
	}

	FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const int32 Count = Log.GetEntries().Num();
	Log.Clear();

	return FString::Printf(
		TEXT("changed: Removed %d log records from memory. The log file at %s is untouched."),
		Count, *Log.GetLogFilePath());
}
