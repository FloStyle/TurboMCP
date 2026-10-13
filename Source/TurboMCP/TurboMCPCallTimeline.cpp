// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallTimeline.h"

#include "Containers/Map.h"

namespace
{
	/** The record values that mean something to this file, in one place rather than as literals. */
	const TCHAR* const PhaseStart = TEXT("start");
	const TCHAR* const PhaseEnd = TEXT("end");
	const TCHAR* const SourcePlugin = TEXT("plugin");
	const TCHAR* const SourceEngine = TEXT("engine");

	bool IsStart(const FTurboMCPCallLog::FEntry& InEntry)
	{
		return InEntry.Phase == PhaseStart;
	}

	bool IsEnd(const FTurboMCPCallLog::FEntry& InEntry)
	{
		return InEntry.Phase == PhaseEnd;
	}
}

FString FTurboMCPCallTimeline::Key(const FString& InToolset, const FString& InTool)
{
	// THE LAST DOTTED SEGMENT OF EACH FIELD, lower-cased. See the header for why both fields need it:
	// the toolset has two spellings and the TOOL has two as well, because the engine's inner event
	// carries `toolset.tool` in the tool slot.
	auto LastSegment = [](const FString& InName) -> FString
	{
		int32 LastDot = INDEX_NONE;
		if (!InName.FindLastChar(TEXT('.'), LastDot) || LastDot < 0 || LastDot + 1 >= InName.Len())
		{
			return InName.ToLower();
		}
		return InName.Mid(LastDot + 1).ToLower();
	};

	return LastSegment(InToolset) + TEXT("|") + LastSegment(InTool);
}

TArray<FTurboMCPCallLog::FEntry> FTurboMCPCallTimeline::RepresentativeRecords(
	const TArray<FTurboMCPCallLog::FEntry>& InRecords)
{
	TArray<FTurboMCPCallLog::FEntry> Out;

	/** The call each open `start` belongs to, by key. One entry per call still running. */
	TMap<FString, int32> Open;

	/**
	 * The call each key last finished, by key.
	 *
	 * THIS IS WHAT MERGES THE ENGINE'S VIEW. The engine's event for a plugin call arrives AFTER the
	 * plugin's own `end` record, so by the time it is seen the call is closed and its index is here.
	 * A key that is not here and is not open is an engine tool's own call.
	 */
	TMap<FString, int32> LastFinished;

	for (const FTurboMCPCallLog::FEntry& Record : InRecords)
	{
		const FString RecordKey = Key(Record.Toolset, Record.Tool);
		const bool bPlugin = Record.Source == SourcePlugin;

		if (IsStart(Record) && bPlugin)
		{
			Out.Add(Record);
			Open.Add(RecordKey, Out.Num() - 1);
			continue;
		}

		if (IsEnd(Record))
		{
			if (bPlugin)
			{
				// THE PLUGIN'S OWN END CLOSES ITS START, and the row keeps the START'S TIMESTAMP while
				// taking the end's duration, size and status. A timeline row that said when a call
				// FINISHED would be a log, not a timeline.
				if (int32* OpenIndex = Open.Find(RecordKey))
				{
					const FString StartStamp = Out[*OpenIndex].Timestamp;
					Out[*OpenIndex] = Record;
					Out[*OpenIndex].Timestamp = StartStamp;
					LastFinished.Add(RecordKey, *OpenIndex);
					Open.Remove(RecordKey);
				}
				else
				{
					// A plugin end with no start: logging was switched on between the two records.
					// It is still one call, and it is not thrown away for the missing half.
					Out.Add(Record);
					LastFinished.Add(RecordKey, Out.Num() - 1);
				}
				continue;
			}

			// THE ENGINE'S RECORD. It never closes a plugin call - a call that has a start but no end
			// is exactly the shape a crash leaves, and letting the engine's event close it would erase
			// the only evidence that the plugin's own end never ran.
			if (const int32* FinishedIndex = LastFinished.Find(RecordKey))
			{
				// THE OLDER CLOSED CALL IS PREFERRED, and that is a measurement rather than a
				// preference: the engine emits this event as soon as the dispatch returns, so it
				// arrives before the next call to the same tool can start. A key cannot tell two
				// consecutive calls apart on its own, and merging into the closed call is right for
				// the ordering the engine actually produces.
				FTurboMCPCallLog::FEntry& Representative = Out[*FinishedIndex];
				if (Representative.SessionId.IsEmpty())
				{
					// THE SESSION ID COMES FROM HERE, because the plugin's record cannot carry one:
					// the registry it records at is below the MCP session.
					Representative.SessionId = Record.SessionId;
				}
				// And the engine's own duration/size is NOT copied over the plugin's, because the
				// plugin's is the measurement at the tool boundary that `T02`'s `done when` 2 asks for.
				continue;
			}

			if (Open.Contains(RecordKey))
			{
				// An engine event for a call whose plugin end has not arrived yet. It is still the
				// same call, so it is neither a new row nor a close.
				continue;
			}

			// AN ENGINE TOOL'S OWN CALL: no plugin ever opened it, so it is a call in its own right.
			Out.Add(Record);
			continue;
		}

		// Anything else - the rotation marker - is a record in its own right.
		Out.Add(Record);
	}

	return Out;
}

TArray<FTurboMCPCallLog::FEntry> FTurboMCPCallTimeline::WithSessionsInEffect(
	const TArray<FTurboMCPCallLog::FEntry>& InRecords)
{
	TArray<FTurboMCPCallLog::FEntry> Out = InRecords;
	if (Out.Num() == 0)
	{
		return Out;
	}

	// FORWARD FIRST: a record with no session takes the last one seen, which is what groups the
	// plugin's pair with the call BEFORE it in a batch.
	FString InEffect;
	for (FTurboMCPCallLog::FEntry& Record : Out)
	{
		if (!Record.SessionId.IsEmpty())
		{
			InEffect = Record.SessionId;
		}
		else if (!InEffect.IsEmpty())
		{
			Record.SessionId = InEffect;
		}
	}

	// THEN BACKWARD, for the records before the FIRST session id in the list - AND FOR THE LAST ONE
	// TOO, WHICH IS A BUG I WROTE AND THE SUITE CAUGHT. Starting the backward pass with an empty
	// "in effect" left the NEWEST record with no session whenever it had none of its own, and the
	// newest record is the reader's own `start` record - so the tool printed `; session (none)` for
	// the call that was asking. The rule below only fills a record when a session has actually been
	// seen.
	InEffect.Reset();
	for (int32 Index = Out.Num() - 1; Index >= 0; --Index)
	{
		FTurboMCPCallLog::FEntry& Record = Out[Index];
		if (!Record.SessionId.IsEmpty())
		{
			InEffect = Record.SessionId;
		}
		else if (!InEffect.IsEmpty())
		{
			Record.SessionId = InEffect;
		}
	}

	return Out;
}
