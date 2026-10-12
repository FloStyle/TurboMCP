// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallCost.h"

#include "Containers/UnrealString.h"
#include "HAL/PlatformString.h"

namespace
{
	/**
	 * The value of `"Key"` in an arguments blob, or empty.
	 *
	 * WRITTEN BY HAND RATHER THAN BY REGEX, FOR THE REASON THIS PROJECT HAS RECORDED REPEATEDLY: a
	 * pattern that ASSUMES a shape is indistinguishable from a real absence until the text is
	 * printed beside it. This walks the text and stops at the closing quote, so a value containing
	 * spaces, dots or commas - all of which tool arguments routinely contain - comes back whole.
	 *
	 * It does NOT pretend to be a JSON parser. `arguments` is free text written by a caller, and the
	 * log itself already parsed the LINE; this reads one value out of a field that is a string by
	 * design, and it says so rather than implying a schema that is not there.
	 */
	FString ValueOfKey(const FString& InText, const FString& InKey)
	{
		if (InText.IsEmpty() || InKey.IsEmpty())
		{
			return FString();
		}

		const FString Needle = FString::Printf(TEXT("\"%s\""), *InKey);
		int32 At = InText.Find(Needle, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (At == INDEX_NONE)
		{
			return FString();
		}

		int32 Cursor = At + Needle.Len();
		while (Cursor < InText.Len() && (InText[Cursor] == TEXT(' ') || InText[Cursor] == TEXT('\t')))
		{
			++Cursor;
		}
		if (Cursor >= InText.Len() || InText[Cursor] != TEXT(':'))
		{
			return FString();
		}
		++Cursor;
		while (Cursor < InText.Len() && (InText[Cursor] == TEXT(' ') || InText[Cursor] == TEXT('\t')))
		{
			++Cursor;
		}
		if (Cursor >= InText.Len())
		{
			return FString();
		}

		// A QUOTED VALUE RUNS TO THE CLOSING QUOTE - NOT TO A SPACE, because an asset path contains
		// neither but an asset NAME often does in the messages this log records.
		if (InText[Cursor] == TEXT('"'))
		{
			const int32 Close = InText.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor + 1);
			if (Close == INDEX_NONE)
			{
				return InText.Mid(Cursor + 1);
			}
			return InText.Mid(Cursor + 1, Close - Cursor - 1);
		}

		// An unquoted value runs to the first separator.
		int32 End = Cursor;
		while (End < InText.Len()
			&& InText[End] != TEXT(',') && InText[End] != TEXT('}')
			&& InText[End] != TEXT(' ') && InText[End] != TEXT('\n'))
		{
			++End;
		}
		return InText.Mid(Cursor, End - Cursor);
	}
}

FString FTurboMCPCallCost::NormaliseToolset(const FString& InToolset)
{
	const FString Trimmed = InToolset.TrimStartAndEnd();

	// THE ENGINE'S PLACEHOLDER IS NOT A TOOLSET NAME. It marks the engine's own top-level events,
	// which belong to no toolset; slicing it would be harmless, but INVENTING a name for it would
	// not, so it is passed through as what it is.
	if (Trimmed.Equals(TEXT("(top-level)"), ESearchCase::IgnoreCase) || Trimmed.IsEmpty())
	{
		return Trimmed;
	}

	// *** THE LAST DOTTED SEGMENT, AND THE REASON IS MEASURED RATHER THAN ASSUMED. ***
	//
	// The first version of this function STRIPPED A LEADING `TurboMCP.`, which is the shape the brief
	// described. IT WAS WRONG, AND THE TOOL'S OWN OUTPUT SAID SO ON ITS FIRST REAL RUN:
	//
	//     TOTALS PER TOOL (3 row(s), each with its call count)
	//       TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog | 1 call(s) | ...
	//
	// The qualification is not a PREFIX, IT IS A WHOLE PATH APPEARING TWICE - because the analytics
	// provider resolves BOTH hashes against a registry that maps them to FULLY QUALIFIED names
	// (`TurboMCPAnalyticsProvider.cpp:55` stores `InName` as given, and the engine's own list gives
	// paths). So `ToolsetName` is already `TurboMCP.TurboMCPEditToolset`, and `ToolName` is already
	// `TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog` - which means appending one to the other
	// produces the doubling, and stripping a prefix cannot undo it.
	//
	// The LAST segment is the toolset's own name, and it is stable against BOTH writers: this
	// plugin's `FScopedCall` records the bare `TurboMCPEditToolset`, whose last segment is itself.
	// So one rule collapses both spellings, and no prefix list has to be kept in step with anything.
	int32 LastDot = INDEX_NONE;
	if (!Trimmed.FindLastChar(TEXT('.'), LastDot) || LastDot < 0 || LastDot + 1 >= Trimmed.Len())
	{
		return Trimmed;
	}
	return Trimmed.Mid(LastDot + 1);
}

FString FTurboMCPCallCost::AssetFromArguments(const FString& InArguments)
{
	if (InArguments.IsEmpty())
	{
		return FString();
	}

	// THE KEYS THE CONVENTIONS USE, in the order they are tried. `assets` is tried before `asset`
	// because a plural key's value lists several and `ValueOfKey` returns the first, which is the
	// honest answer for a per-asset total rather than an invented split.
	static const TCHAR* const Keys[] =
	{
		TEXT("assets"),
		TEXT("asset"),
		TEXT("blueprint"),
		TEXT("material"),
		TEXT("graphAsset"),
		TEXT("path"),
	};

	for (const TCHAR* Key : Keys)
	{
		const FString Found = ValueOfKey(InArguments, Key);
		if (!Found.IsEmpty())
		{
			return Found.TrimStartAndEnd();
		}
	}
	return FString();
}

bool FTurboMCPCallCost::AnyEntryNamesAnAsset(const TArray<FTurboMCPCallLog::FEntry>& Entries)
{
	for (const FTurboMCPCallLog::FEntry& Entry : Entries)
	{
		if (!AssetFromArguments(Entry.Arguments).IsEmpty())
		{
			return true;
		}
	}
	return false;
}

FString FTurboMCPCallCost::DescribeEmptiness(EEmptiness InEmptiness)
{
	switch (InEmptiness)
	{
	case EEmptiness::LoggingOff:
		return TEXT("logging is OFF, so nothing has been recorded and nothing is cheap. "
			"`E-CALL-LOG` turns it on; this is NOT a result of zero.");
	case EEmptiness::LogOnAndEmpty:
		return TEXT("logging is ON and the log is empty, so no tool has been called yet.");
	case EEmptiness::FilterMatchedNothing:
	default:
		return TEXT("the log has entries but NONE matched the filter.");
	}
}
