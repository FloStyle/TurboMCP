// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPCallLog.h"

#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#include "TurboMCPSettings.h"
#include "TurboMCPCallTimeline.h"
#include "TurboMCPTokenEstimate.h"

namespace
{
	bool IsHighSurrogate(uint32 Value)
	{
		return Value >= 0xd800 && Value <= 0xdbff;
	}

	bool IsLowSurrogate(uint32 Value)
	{
		return Value >= 0xdc00 && Value <= 0xdfff;
	}

	// Keep a prefix within the escaped UTF-8 byte budget. Count omitted bytes of the
	// input value, without JSON escaping. Invalid units are replaced with U+FFFD.
	int64 AppendJsonValue(FString& Out, const FString& In, int32 ByteBudget)
	{
		int64 Omitted = 0;
		bool bClipped = false;
		for (int32 Index = 0; Index < In.Len(); ++Index)
		{
			const int32 Start = Index;
			uint32 Scalar = static_cast<uint32>(In[Index]);
			bool bInvalid = false;
			if constexpr (sizeof(TCHAR) == 2)
			{
				if (IsHighSurrogate(Scalar) && Index + 1 < In.Len() && IsLowSurrogate(In[Index + 1]))
				{
					Scalar = 0x10000 + ((Scalar - 0xd800) << 10) + (In[++Index] - 0xdc00);
				}
				else
				{
					bInvalid = IsHighSurrogate(Scalar) || IsLowSurrogate(Scalar);
				}
			}
			else
			{
				bInvalid = Scalar > 0x10ffff || IsHighSurrogate(Scalar) || IsLowSurrogate(Scalar);
			}
			if (bInvalid)
			{
				Scalar = 0xfffd;
			}

			const TCHAR* Escape = nullptr;
			switch (Scalar)
			{
			case '"':  Escape = TEXT("\\\""); break;
			case '\\': Escape = TEXT("\\\\"); break;
			case '\n': Escape = TEXT("\\n"); break;
			case '\r': Escape = TEXT("\\r"); break;
			case '\t': Escape = TEXT("\\t"); break;
			default: break;
			}
			const int32 RawBytes = Scalar < 0x80 ? 1 : Scalar < 0x800 ? 2 : Scalar < 0x10000 ? 3 : 4;
			const int32 EscapedBytes = Escape ? 2 : Scalar < 0x20 ? 6 : RawBytes;
			if (bClipped || EscapedBytes > ByteBudget)
			{
				bClipped = true;
				Omitted += RawBytes;
				continue;
			}
			ByteBudget -= EscapedBytes;
			if (Escape)
			{
				Out += Escape;
			}
			else if (Scalar < 0x20)
			{
				constexpr TCHAR Hex[] = TEXT("0123456789abcdef");
				Out += TEXT("\\u00");
				Out.AppendChar(Hex[Scalar >> 4]);
				Out.AppendChar(Hex[Scalar & 15]);
			}
			else if (bInvalid)
			{
				Out.AppendChar(static_cast<TCHAR>(Scalar));
			}
			else
			{
				Out.AppendChars(*In + Start, Index - Start + 1);
			}
		}
		return Omitted;
	}
}


FTurboMCPCallLog& FTurboMCPCallLog::Get()
{
	static FTurboMCPCallLog Instance;
	return Instance;
}

#if WITH_DEV_AUTOMATION_TESTS
FTurboMCPCallLog* FTurboMCPCallLog::CreateForTest(bool bEnabled)
{
	FTurboMCPCallLog* Log = new FTurboMCPCallLog;
	Log->bUseSettings = false;
	Log->bTestEnabled = bEnabled;
	Log->bDiskSink = false;
	return Log;
}

FTurboMCPCallLog* FTurboMCPCallLog::CreateDiskSinkForTest(FString InPath, int64 InFileLimit,
	TFunction<bool(EDiskStage)> InRefuseOperation)
{
	check(!InPath.IsEmpty() && InFileLimit > 0);
	FTurboMCPCallLog* Log = CreateForTest();
	Log->bDiskSink = true;
	Log->TestDiskPath = MoveTemp(InPath);
	Log->FileLimit = InFileLimit;
	Log->RefuseDiskOperation = MoveTemp(InRefuseOperation);
	return Log;
}
#endif

bool FTurboMCPCallLog::IsEnabled() const
{
	if (!bUseSettings)
	{
		return bTestEnabled;
	}
	// The setting is the single source of truth, so the panel's checkbox and this agree by
	// construction rather than by both being updated.
	const UTurboMCPSettings* Settings = UTurboMCPSettings::Get();
	return Settings != nullptr && Settings->bEnableCallLog;
}

void FTurboMCPCallLog::SetEnabled(bool bInEnabled)
{
	if (!bUseSettings)
	{
		// Isolated test state only; never the editor settings or disk.
		bTestEnabled = bInEnabled;
		return;
	}
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
#if WITH_DEV_AUTOMATION_TESTS
	if (!TestDiskPath.IsEmpty())
	{
		return TestDiskPath;
	}
#endif
	// Beside the project's own logs.
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("TurboMCPCalls.jsonl"));
}

FString FTurboMCPCallLog::ToJsonLine(const FEntry& InEntry, int64& OutOmittedBytes)
{
	FString Out = TEXT("{");
	Out.Reserve(384);
	FString Omissions;
	OutOmittedBytes = 0;
	auto Field = [&](const TCHAR* InName, const FString& InValue, int32 InBudget)
	{
		if (Out.Len() > 1)
		{
			Out.AppendChar(TEXT(','));
		}
		Out += FString::Printf(TEXT("\"%s\":\""), InName);
		const int64 Omitted = AppendJsonValue(Out, InValue, InBudget);
		Out.AppendChar(TEXT('"'));
		if (Omitted > 0)
		{
			if (!Omissions.IsEmpty())
			{
				Omissions.AppendChar(TEXT(','));
			}
			Omissions += FString::Printf(TEXT("\"%s\":%lld"), InName, Omitted);
			OutOmittedBytes += Omitted;
		}
	};

	// The field budgets leave room for measured values and omission counts.
	Field(TEXT("time"), InEntry.Timestamp, 256);
	Field(TEXT("phase"), InEntry.Phase, 64);
	Field(TEXT("source"), InEntry.Source, 64);
	if (!InEntry.SessionId.IsEmpty())
	{
		Field(TEXT("session"), InEntry.SessionId, 512);
	}
	Field(TEXT("toolset"), InEntry.Toolset, 512);
	Field(TEXT("tool"), InEntry.Tool, 512);
	if (!InEntry.Arguments.IsEmpty())
	{
		Field(TEXT("arguments"), InEntry.Arguments, 6144);
	}
	if (!InEntry.Result.IsEmpty())
	{
		Field(TEXT("result"), InEntry.Result, 6144);
	}
	Out += FMath::IsFinite(InEntry.DurationMs)
		? FString::Printf(TEXT(",\"duration_ms\":%.3f"), InEntry.DurationMs)
		: FString(TEXT(",\"duration_ms\":null"));
	Out += InEntry.ResultChars == FEntry::NotMeasured ? FString(TEXT(",\"result_chars\":null")) : FString::Printf(TEXT(",\"result_chars\":%d"), InEntry.ResultChars);
	Out += InEntry.TokenEstimate == FEntry::NotMeasured ? FString(TEXT(",\"token_estimate\":null")) : FString::Printf(TEXT(",\"token_estimate\":%d"), InEntry.TokenEstimate);
	Out += FString::Printf(TEXT(",\"error\":%s"), InEntry.bError ? TEXT("true") : TEXT("false"));
	if (!Omissions.IsEmpty())
	{
		Out += TEXT(",\"omitted_utf8_bytes\":{") + Omissions + TEXT("}");
	}
	Out += TEXT("}");
	Out += LINE_TERMINATOR;
	return Out;
}

FString FTurboMCPCallLog::RenderTimeline() const
{
	const TArray<FEntry> Records = GetEntries();
	if (Records.Num() == 0)
	{
		return TEXT("; no calls recorded yet this session\n") + RenderDiskStatus();
	}

	// ONE ROW PER CALL, NOT PER RECORD. Since the plugin's own tools began writing a `start` and an
	// `end` around every call, one call leaves up to four records in the log, and a renderer that
	// walked the records would print the plugin's `end` and the engine's event for the same call as
	// two calls - and would print the plugin's `start` as `running` for the rest of the session
	// because the engine's records do not carry the plugin's spelling of the key.
	// `FTurboMCPCallTimeline` is where that assembly lives, and `R-CALL-COST` counts the same way.
	const TArray<FEntry> Calls = FTurboMCPCallTimeline::WithSessionsInEffect(
		FTurboMCPCallTimeline::RepresentativeRecords(Records));

	/** The row text comes from the timeline, so the panel's list and this text are one rendering. */
	FString Out = FString::Printf(
		TEXT("; %d call(s) from %d record(s), oldest first. A call with no OUT yet reads `completion not observed`."),
		Calls.Num(), Records.Num());

	// CALLS OF ONE BATCH SCRIPT ARE GROUPED, which is a requirement of the timeline rather than a
	// decoration: a session id is the same for every call of one script, so a change of id is a new
	// group and is stated once, ahead of its rows.
	FString PreviousSession;
	bool bFirstGroup = true;

	for (const FEntry& Call : Calls)
	{
		if (bFirstGroup || Call.SessionId != PreviousSession)
		{
			bFirstGroup = false;
			PreviousSession = Call.SessionId;
			Out += FString::Printf(TEXT("\n; session %s"),
				Call.SessionId.IsEmpty() ? TEXT("(none)") : *Call.SessionId);
		}
		Out += TEXT("\n  ") + FTurboMCPCallTimeline::RenderRow(Call);
	}

	Out += TEXT("\n") + RenderDiskStatus();
	return Out;
}

bool FTurboMCPCallLog::ShouldRefuseDiskOperation(EDiskStage InStage) const
{
#if WITH_DEV_AUTOMATION_TESTS
	return RefuseDiskOperation && RefuseDiskOperation(InStage);
#else
	return false;
#endif
}

void FTurboMCPCallLog::RecordDiskFailure(EDiskStage InStage, const TCHAR* InReason)
{
	++DiskFailures[static_cast<int32>(InStage)];
	LastDiskFailure = InReason;
}

FString FTurboMCPCallLog::RenderDiskStatus() const
{
	FScopeLock Lock(&Mutex);
	if (!bDiskSink)
	{
		return TEXT("disk sink: disabled");
	}

	FString Out = FString::Printf(
		TEXT("disk sink: up to %d UTF-8 bytes per record and %lld per log file"),
		MaxRecordBytes, FileLimit);
	if (ShortenedRecords > 0)
	{
		Out += FString::Printf(
			TEXT("\ndisk omissions this session: %llu written record(s), %llu UTF-8 value byte(s)"),
			ShortenedRecords, OmittedBytes);
	}
	if (!LastDiskFailure.IsEmpty())
	{
		constexpr const TCHAR* Stages[] = {
			TEXT("directory"), TEXT("size"), TEXT("backup deletion"), TEXT("rotation move"),
			TEXT("append open"), TEXT("write"), TEXT("flush"), TEXT("write restoration")
		};
		static_assert(UE_ARRAY_COUNT(Stages) == static_cast<int32>(EDiskStage::Count));

		Out += TEXT("\ndisk failures this session:");
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Stages); ++Index)
		{
			if (DiskFailures[Index] > 0)
			{
				Out += FString::Printf(TEXT(" %s=%llu"), Stages[Index], DiskFailures[Index]);
			}
		}
		Out += TEXT("\nlast disk failure: ") + LastDiskFailure;
	}
	if (PriorBackupLosses > 0)
	{
		Out += FString::Printf(TEXT("\nprevious backups lost during failed rotation: %llu"), PriorBackupLosses);
	}
	if (bDiskAppendBlocked)
	{
		Out += TEXT("\ndisk writes paused after incomplete write restoration. Inspect the active log before restarting the editor.");
	}
	return Out;
}

void FTurboMCPCallLog::AppendToFile(const FEntry& InEntry)
{
	if (!bDiskSink || bDiskAppendBlocked)
	{
		return;
	}

	int64 RecordOmittedBytes = 0;
	const FString Line = ToJsonLine(InEntry, RecordOmittedBytes);
	const int32 LineBytes = StringCast<UTF8CHAR>(*Line, Line.Len()).Length();
	if (LineBytes > MaxRecordBytes || LineBytes > FileLimit)
	{
		RecordDiskFailure(EDiskStage::Size, TEXT("The encoded record exceeds the disk byte limit."));
		return;
	}

	const FString Path = GetLogFilePath();
	const FString Directory = FPaths::GetPath(Path);
	const FString PreviousPath = Path + TEXT(".1");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory)
		&& (ShouldRefuseDiskOperation(EDiskStage::Directory) || !PlatformFile.CreateDirectoryTree(*Directory)))
	{
		RecordDiskFailure(EDiskStage::Directory, TEXT("Could not create the log directory."));
		return;
	}

	if (ShouldRefuseDiskOperation(EDiskStage::Size))
	{
		RecordDiskFailure(EDiskStage::Size, TEXT("Could not determine the disk log sizes."));
		return;
	}
	const int64 PreviousSize = PlatformFile.FileSize(*PreviousPath);
	if (PreviousSize < 0 && (PlatformFile.FileExists(*PreviousPath) || PlatformFile.DirectoryExists(*PreviousPath)))
	{
		RecordDiskFailure(EDiskStage::Size, TEXT("Could not determine the previous log file size."));
		return;
	}
	if (PreviousSize > FileLimit)
	{
		RecordDiskFailure(EDiskStage::Size, TEXT("The previous log already exceeds the byte limit. Archive it before recording to disk again."));
		return;
	}

	auto OpenForAppend = [&]() -> TUniquePtr<IFileHandle>
	{
		if (ShouldRefuseDiskOperation(EDiskStage::OpenAppend))
		{
			return {};
		}
		return TUniquePtr<IFileHandle>(PlatformFile.OpenWrite(*Path, /*bAppend=*/true, /*bAllowRead=*/true));
	};

	TUniquePtr<IFileHandle> Handle = OpenForAppend();
	if (!Handle)
	{
		RecordDiskFailure(EDiskStage::OpenAppend, TEXT("Could not open the active log for append."));
		return;
	}

	// The stock Windows OpenWrite append implementation does not return the
	// result of its seek. Check the position ourselves before any write.
	const int64 ActiveSize = Handle->SeekFromEnd() ? Handle->Tell() : -1;
	if (ActiveSize < 0 || ActiveSize > FileLimit)
	{
		RecordDiskFailure(EDiskStage::Size, ActiveSize < 0
			? TEXT("Could not determine the active log file size.")
			: TEXT("The active log already exceeds the byte limit. Archive it before recording to disk again."));
		return;
	}
	if (ActiveSize > 0)
	{
		uint8 Tail = 0;
		if (!Handle->Seek(ActiveSize - 1) || !Handle->Read(&Tail, 1) || Tail != '\n' || !Handle->Seek(ActiveSize))
		{
			RecordDiskFailure(EDiskStage::Size, TEXT("Could not verify a complete final line in the active log. Inspect or archive it before recording to disk again."));
			return;
		}
	}

	const bool bRotate = LineBytes > FileLimit - ActiveSize;
	FString Payload;
	if (bRotate || bRotationMarkerPending)
	{
		FEntry Marker;
		Marker.Timestamp = FDateTime::UtcNow().ToIso8601();
		Marker.Phase = TEXT("rotate");
		Marker.Source = TEXT("plumbing");
		Marker.Result = TEXT("The previous active log was moved to the .1 file.");
		int64 MarkerOmittedBytes = 0;
		Payload = ToJsonLine(Marker, MarkerOmittedBytes);
	}
	Payload += Line;
	const auto EncodedPayload = StringCast<UTF8CHAR>(*Payload, Payload.Len());
	const int64 AppendOffset = bRotate ? 0 : ActiveSize;
	if (EncodedPayload.Length() > FileLimit - AppendOffset)
	{
		RecordDiskFailure(EDiskStage::Size, TEXT("The record and rotation marker do not fit within the file byte limit."));
		return;
	}

	if (bRotate)
	{
		Handle.Reset();
		const bool bHadBackup = PreviousSize >= 0;
		if (bHadBackup && (ShouldRefuseDiskOperation(EDiskStage::DeleteBackup) || !PlatformFile.DeleteFile(*PreviousPath)))
		{
			RecordDiskFailure(EDiskStage::DeleteBackup, TEXT("Could not remove the previous backup. The active log was not rotated."));
			return;
		}
		if (ShouldRefuseDiskOperation(EDiskStage::MoveActive) || !PlatformFile.MoveFile(*PreviousPath, *Path))
		{
			if (bHadBackup)
			{
				++PriorBackupLosses;
			}
			RecordDiskFailure(EDiskStage::MoveActive, bHadBackup
				? TEXT("Could not move the active log. The prior backup was removed.")
				: TEXT("Could not move the active log to the backup path."));
			return;
		}

		bRotationMarkerPending = true;
		Handle = OpenForAppend();
		if (!Handle)
		{
			RecordDiskFailure(EDiskStage::OpenAppend, TEXT("Rotation moved the active log to the backup, but the new active log could not be opened."));
			return;
		}
		if (!Handle->SeekFromEnd() || Handle->Tell() != 0)
		{
			RecordDiskFailure(EDiskStage::Size, TEXT("The new active log could not be verified empty after rotation."));
			return;
		}
	}

	if (ShouldRefuseDiskOperation(EDiskStage::Write)
		|| !Handle->Write(reinterpret_cast<const uint8*>(EncodedPayload.Get()), EncodedPayload.Length()))
	{
		RecordDiskFailure(EDiskStage::Write, TEXT("Could not append the complete record."));
		// A failed write may already have emitted bytes. Restore the prior
		// boundary before another record can be appended to that file.
		if (ShouldRefuseDiskOperation(EDiskStage::RestoreWrite) || !Handle->Truncate(AppendOffset) || !Handle->Flush())
		{
			bDiskAppendBlocked = true;
			RecordDiskFailure(EDiskStage::RestoreWrite, TEXT("Could not verify restoration after a failed disk write."));
		}
		return;
	}

	bRotationMarkerPending = false;
	if (RecordOmittedBytes > 0)
	{
		++ShortenedRecords;
		OmittedBytes += RecordOmittedBytes;
	}
	if (ShouldRefuseDiskOperation(EDiskStage::Flush) || !Handle->Flush())
	{
		RecordDiskFailure(EDiskStage::Flush, TEXT("The write returned success, but its flush failed. Disk persistence was not verified."));
	}
}

void FTurboMCPCallLog::Write(const FEntry& InEntry)
{
	if (!IsEnabled())
	{
		return;
	}

	FScopeLock Lock(&Mutex);
	AppendToFile(InEntry);

	Entries.Add(InEntry);
	if (Entries.Num() > MaxEntries)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}
	++Revision;
}

FTurboMCPCallLog::FSnapshot FTurboMCPCallLog::GetSnapshot() const
{
	FScopeLock Lock(&Mutex);
	FSnapshot Snapshot;
	Snapshot.Entries = Entries;
	Snapshot.Revision = Revision;
	return Snapshot;
}

uint64 FTurboMCPCallLog::GetRevision() const
{
	FScopeLock Lock(&Mutex);
	return Revision;
}

TArray<FTurboMCPCallLog::FEntry> FTurboMCPCallLog::GetEntries() const
{
	FScopeLock Lock(&Mutex);
	return Entries;
}

int32 FTurboMCPCallLog::NumEntries() const
{
	FScopeLock Lock(&Mutex);
	return Entries.Num();
}

void FTurboMCPCallLog::Clear()
{
	FScopeLock Lock(&Mutex);
	Entries.Reset();
	++Revision;
}

FTurboMCPCallLog::FScopedCall::FScopedCall(
	const FString& InToolset, const FString& InTool, const FString& InArguments)
	: Toolset(InToolset), Tool(InTool), Arguments(InArguments)
{
	// Attempt to record the start before the tool runs.
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
		int32 Retained = 4000;
		if constexpr (sizeof(TCHAR) == 2)
		{
			if (IsHighSurrogate(InResult[Retained - 1]) && IsLowSurrogate(InResult[Retained]))
			{
				--Retained;
			}
		}
		Entry.Result = InResult.Left(Retained)
			+ FString::Printf(TEXT("\n... [%d more characters]"), InResult.Len() - Retained);
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
