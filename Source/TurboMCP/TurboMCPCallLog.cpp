// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallLog.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "TurboMCPSettings.h"
#include "TurboMCPTokenEstimate.h"

namespace
{
	/** One JSON string, escaped. */
	FString JsonString(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 8);
		for (const TCHAR Character : In)
		{
			switch (Character)
			{
			case TEXT('"'):  Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n");  break;
			case TEXT('\r'): Out += TEXT("\\r");  break;
			case TEXT('\t'): Out += TEXT("\\t");  break;
			default:
				if (Character < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Character));
				}
				else
				{
					Out.AppendChar(Character);
				}
			}
		}
		return Out;
	}
}

FTurboMCPCallLog& FTurboMCPCallLog::Get()
{
	static FTurboMCPCallLog Instance;
	return Instance;
}

bool FTurboMCPCallLog::IsEnabled() const
{
	// The setting is the single source of truth, so the panel's checkbox and this agree by
	// construction rather than by both being updated.
	const UTurboMCPSettings* Settings = UTurboMCPSettings::Get();
	return Settings != nullptr && Settings->bEnableCallLog;
}

void FTurboMCPCallLog::SetEnabled(bool bInEnabled)
{
	if (UTurboMCPSettings* Settings = UTurboMCPSettings::GetMutable())
	{
		Settings->bEnableCallLog = bInEnabled;
		// Persisted at once, because `03_TOOLS.md` requires the choice to persist and a user who
		// ticks the box then closes the editor must still have it ticked.
		Settings->SaveConfig();
	}
}

FString FTurboMCPCallLog::GetLogFilePath() const
{
	// Beside the project's own logs, so a reader looks where the rest of the editor's logs are.
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("TurboMCPCalls.jsonl"));
}

FString FTurboMCPCallLog::ToJsonLine(const FEntry& InEntry)
{
	// Built by hand rather than through FJsonObject, because the ORDER matters for a log a human
	// reads with their eyes: the time, then what was called, then what came back.
	FString Out;
	Out += TEXT("{\"time\":\"") + JsonString(InEntry.Timestamp) + TEXT("\"");
	Out += TEXT(",\"phase\":\"") + JsonString(InEntry.Phase) + TEXT("\"");
	Out += TEXT(",\"source\":\"") + JsonString(InEntry.Source) + TEXT("\"");
	if (!InEntry.SessionId.IsEmpty())
	{
		Out += TEXT(",\"session\":\"") + JsonString(InEntry.SessionId) + TEXT("\"");
	}
	Out += TEXT(",\"toolset\":\"") + JsonString(InEntry.Toolset) + TEXT("\"");
	Out += TEXT(",\"tool\":\"") + JsonString(InEntry.Tool) + TEXT("\"");
	if (!InEntry.Arguments.IsEmpty())
	{
		Out += TEXT(",\"arguments\":\"") + JsonString(InEntry.Arguments) + TEXT("\"");
	}
	if (!InEntry.Result.IsEmpty())
	{
		Out += TEXT(",\"result\":\"") + JsonString(InEntry.Result) + TEXT("\"");
	}
	Out += FString::Printf(TEXT(",\"duration_ms\":%.3f"), InEntry.DurationMs);
	Out += FString::Printf(TEXT(",\"result_chars\":%d"), InEntry.ResultChars);
	Out += FString::Printf(TEXT(",\"token_estimate\":%d"), InEntry.TokenEstimate);
	Out += FString::Printf(TEXT(",\"error\":%s"), InEntry.bError ? TEXT("true") : TEXT("false"));
	Out += TEXT("}");
	return Out;
}

void FTurboMCPCallLog::AppendToFile(const FString& InLine) const
{
	const FString Path = GetLogFilePath();
	const FString Directory = FPaths::GetPath(Path);

	// The directory is made first: a first-ever call would otherwise have nothing to write into
	// and the log would silently stay empty, which is the failure this whole tool exists to make
	// visible.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// ROTATION, BECAUSE THE FILE GREW WITHOUT BOUND.
	//
	// Measured before this was added: 402 KB and 1229 lines from ONE ROUND of testing, because
	// every tool call writes a line — and every READ of the log is a tool call, so reading the log
	// grows the log. Left alone, a long session produces a file nobody will open and a disk cost
	// nobody asked for. `P6` caps the in-memory list at 1000 entries for the same reason; the file
	// had no cap at all.
	//
	// ONE rotation, not a numbered series. A second file is what makes a crash readable: the
	// current file holds what just happened, the previous one holds what led up to it, and a
	// directory of a hundred rotated files is a directory nobody reads. The older file is REPLACED
	// rather than kept, so the on-disk cost has a hard ceiling of about twice the limit.
	if (PlatformFile.FileSize(*Path) >= MaxFileBytes)
	{
		const FString PreviousPath = Path + TEXT(".1");

		// The previous file is removed BEFORE the move, because `MoveFile` fails when the
		// destination exists and a failed move would leave rotation silently not happening — the
		// exact shape of failure this project keeps having to look for.
		PlatformFile.DeleteFile(*PreviousPath);
		PlatformFile.MoveFile(*PreviousPath, *Path);

		// A marker line, so a reader of the older file can see WHY it stops where it does rather
		// than wondering whether the log lost data. It is written with the same field set as every
		// other line, so a parser needs no special case.
		FTurboMCPCallLog::FEntry Marker;
		Marker.Timestamp = FDateTime::UtcNow().ToIso8601();
		Marker.Phase = TEXT("rotate");
		Marker.Source = TEXT("plumbing");
		Marker.Result = FString::Printf(
			TEXT("the log reached %d bytes and was rotated to %s.1"),
			MaxFileBytes, *Path);

		FFileHelper::SaveStringToFile(ToJsonLine(Marker) + LINE_TERMINATOR, *Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(), FILEWRITE_Append);
	}

	FFileHelper::SaveStringToFile(InLine + LINE_TERMINATOR, *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

void FTurboMCPCallLog::Write(const FEntry& InEntry)
{
	if (!IsEnabled())
	{
		return;
	}

	// The file FIRST, and flushed, because `03_TOOLS.md` requires the record to survive a crash.
	// A record held only in memory would be lost by exactly the event the start record exists for.
	AppendToFile(ToJsonLine(InEntry));

	FScopeLock Lock(&Mutex);
	Entries.Add(InEntry);
	if (Entries.Num() > MaxEntries)
	{
		// Bounded, per `P6`. The oldest go first, and the FILE keeps everything.
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}
}

TArray<FTurboMCPCallLog::FEntry> FTurboMCPCallLog::GetEntries() const
{
	FScopeLock Lock(&Mutex);
	return Entries;
}

void FTurboMCPCallLog::Clear()
{
	FScopeLock Lock(&Mutex);
	Entries.Reset();
}

FTurboMCPCallLog::FScopedCall::FScopedCall(
	const FString& InToolset, const FString& InTool, const FString& InArguments)
	: Toolset(InToolset), Tool(InTool), Arguments(InArguments)
{
	// The start record, written and flushed BEFORE the call runs.
	StartStamp = FDateTime::UtcNow().ToIso8601();
	StartSeconds = FPlatformTime::Seconds();

	FEntry Entry;
	Entry.Timestamp = StartStamp;
	Entry.Phase = TEXT("start");
	Entry.Source = TEXT("plugin");
	Entry.Toolset = Toolset;
	Entry.Tool = Tool;
	Entry.Arguments = Arguments;
	FTurboMCPCallLog::Get().Write(Entry);
}

void FTurboMCPCallLog::FScopedCall::Finish(const FString& InResult, bool bInError)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	FEntry Entry;
	Entry.Timestamp = FDateTime::UtcNow().ToIso8601();
	Entry.Phase = TEXT("end");
	Entry.Source = TEXT("plugin");
	Entry.Toolset = Toolset;
	Entry.Tool = Tool;
	Entry.Arguments = Arguments;
	// The result is CAPPED in the log: a graph dump can be hundreds of kilobytes, and a log that
	// grows without bound is one nobody keeps switched on. The size and the estimate are always
	// the FULL figures, so the cap never hides how big the answer really was.
	if (InResult.Len() > 4000)
	{
		Entry.Result = InResult.Left(4000)
			+ FString::Printf(TEXT("\n... [%d more characters]"), InResult.Len() - 4000);
	}
	else
	{
		Entry.Result = InResult;
	}
	Entry.DurationMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Entry.ResultChars = InResult.Len();
	Entry.TokenEstimate = TurboMCPTokenEstimate(InResult);
	Entry.bError = bInError;

	FTurboMCPCallLog::Get().Write(Entry);
}

FTurboMCPCallLog::FScopedCall::~FScopedCall()
{
	if (!bFinished)
	{
		// THE CALL THREW OR RETURNED EARLY, so no end record was written by the caller. One is
		// written here, saying so, because a log whose last line is a start record with no end is
		// exactly the shape of a crash and must not be confused with one.
		Finish(TEXT("(no result recorded: the call did not finish)"), /*bInError=*/true);
	}
}
