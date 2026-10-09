// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPLogCapture.h"

#include "HAL/PlatformTLS.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceRedirector.h"

namespace
{
	/**
	 * How many log lines one capture may keep.
	 *
	 * `P6` forbids flooding a result, and a save can raise hundreds of lines. The
	 * count stays exact past this cap because it is kept separately; only the
	 * stored text is capped, and nothing in a result depends on text that was
	 * dropped.
	 */
	constexpr int32 MaxStoredWarnings = 256;

	/** Characters of one log line kept for the summary. */
	constexpr int32 MaxStoredLineChars = 200;
}

FTurboMCPLogCapture::FTurboMCPLogCapture()
{
	OwnerThreadId = FPlatformTLS::GetCurrentThreadId();

	if (GLog != nullptr)
	{
		// GLog routes to every registered device, so adding this one leaves the
		// editor's own log output untouched.
		GLog->AddOutputDevice(this);
		bAttached = true;
	}
}

FTurboMCPLogCapture::~FTurboMCPLogCapture()
{
	Detach();
}

void FTurboMCPLogCapture::Detach()
{
	if (bAttached && GLog != nullptr)
	{
		GLog->RemoveOutputDevice(this);
	}
	bAttached = false;
}

void FTurboMCPLogCapture::Serialize(
	const TCHAR* InMessage,
	ELogVerbosity::Type InVerbosity,
	const FName& InCategory)
{
	// Only the thread that owns this capture. Engine log lines arrive from many
	// threads, and a warning raised elsewhere during a save belongs to other
	// work, so counting it would blame the wrong asset.
	if (FPlatformTLS::GetCurrentThreadId() != OwnerThreadId)
	{
		return;
	}

	const bool bIsWarning = InVerbosity == ELogVerbosity::Warning;
	const bool bIsError = InVerbosity == ELogVerbosity::Error
		|| InVerbosity == ELogVerbosity::Fatal;

	if (!bIsWarning && !bIsError)
	{
		return;
	}

	if (bIsError)
	{
		// Errors are counted but not stored: the caller reports its own error for
		// a failed save, and repeating the engine's would be the log noise `P6`
		// forbids.
		++ErrorCountValue;
		return;
	}

	if (WarningEntries.Num() >= MaxStoredWarnings)
	{
		// Past the cap only the count grows, so the reported number stays true.
		return;
	}

	FEntry Entry;
	Entry.Category = InCategory.ToString();
	Entry.Message = InMessage != nullptr ? InMessage : TEXT("");
	Entry.Message.TrimStartAndEndInline();

	// Keep the first line only. Engine warnings often carry a multi-line
	// callstack or a dump that has no place in a tool result.
	int32 Newline = INDEX_NONE;
	if (Entry.Message.FindChar(TEXT('\n'), Newline))
	{
		Entry.Message = Entry.Message.Left(Newline).TrimEnd();
	}

	if (Entry.Message.Len() > MaxStoredLineChars)
	{
		Entry.Message = Entry.Message.Left(MaxStoredLineChars).TrimEnd() + TEXT("...");
	}

	WarningEntries.Add(MoveTemp(Entry));
}

FString FTurboMCPLogCapture::SummariseSince(int32 InMark, int32 InMax) const
{
	const int32 FirstIndex = FMath::Clamp(InMark, 0, WarningEntries.Num());
	const int32 Available = WarningEntries.Num() - FirstIndex;
	if (Available <= 0)
	{
		return FString();
	}

	FString Summary;
	const int32 Shown = FMath::Min(Available, InMax);
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FEntry& Entry = WarningEntries[FirstIndex + Index];
		Summary += FString::Printf(TEXT("\n      %s: %s"), *Entry.Category, *Entry.Message);
	}

	if (Available > Shown)
	{
		Summary += FString::Printf(TEXT("\n      (%d more warning(s) not shown)"), Available - Shown);
	}

	return Summary;
}
