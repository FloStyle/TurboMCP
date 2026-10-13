// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"
#include "Misc/OutputDevice.h"

/**
 * Counts engine log warnings raised while one tool call runs.
 *
 * `E-SAVE` and `E-COMPILE` must report "no engine log text" (`02_PRINCIPLES.md`
 * P6) while still telling the caller that a save raised warnings, and which
 * asset raised them. Counting is how both can be true: the text never reaches
 * the result, but the number and the owning asset do.
 *
 * Use one instance only for the span of a single tool call. It attaches to
 * `GLog` on construction and detaches on destruction, so a nested or leaked
 * capture cannot swallow warnings from unrelated work.
 *
 * Only messages raised on the thread that created the capture are recorded.
 * A tool call runs on the game thread, and engine logging from other threads
 * during a save belongs to other work, so counting it would attribute a warning
 * to the wrong asset.
 */
class FTurboMCPLogCapture : public FOutputDevice
{
public:
	/** One captured warning, with the text kept for the count and the summary. */
	struct FEntry
	{
		FString Category;
		FString Message;
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
	};

	FTurboMCPLogCapture();
	~FTurboMCPLogCapture();

	// Not copyable: it owns a registration on GLog.
	FTurboMCPLogCapture(const FTurboMCPLogCapture&) = delete;
	FTurboMCPLogCapture& operator=(const FTurboMCPLogCapture&) = delete;

	//~ Begin FOutputDevice
	virtual void Serialize(const TCHAR* InMessage, ELogVerbosity::Type InVerbosity,
		const FName& InCategory) override;
	//~ End FOutputDevice

	/** How many warnings were raised while this capture was attached. */
	int32 WarningCount() const { return WarningEntries.Num(); }

	/** How many errors were raised while this capture was attached. */
	int32 ErrorCount() const { return ErrorCountValue; }

	/**
	 * How many warnings arrived while a labelled span was running.
	 *
	 * Used to attribute warnings to one asset: mark the start, save the asset,
	 * then ask how many arrived since. The engine's save paths do not tag their
	 * log lines with the asset, so a span is the only honest attribution.
	 */
	int32 WarningCountSince(int32 InMark) const
	{
		return FMath::Max(0, WarningEntries.Num() - InMark);
	}

	/** The current warning total, to pass back into WarningCountSince later. */
	int32 Mark() const { return WarningEntries.Num(); }

	/**
	 * A short phrase for the first warnings since a mark, with no log noise.
	 *
	 * Returns at most InMax entries, trimmed to their first line, because engine
	 * log lines can run long and multi-line and `P6` forbids flooding a result.
	 */
	FString SummariseSince(int32 InMark, int32 InMax = 3) const;

	/**
	 * How many errors arrived while a labelled span was running.
	 *
	 * `T04`: an ensure that fires while `GEnsuresAreErrors` is set is logged at
	 * `Error` rather than at `Warning` (`AssertionMacros.cpp:679`), so a span
	 * that can only answer for warnings is blind to exactly the case that task
	 * exists for. `Mark()` is not index-compatible with this count: marks index
	 * the warning store, while this is a running error total read as a pair.
	 */
	int32 ErrorCountSince(int32 InMarkErrorTotal) const;

	/**
	 * The first errors logged since an error mark, each on its own line.
	 *
	 * A compile that logged an engine error must list it rather than only
	 * counting it, which is what `T04`'s "lists each ensure and each error"
	 * asks for. Bounded by InMax and trimmed to one line each, as the warning
	 * summary is, so `P6` still holds.
	 */
	FString SummariseErrorsSince(int32 InMarkErrorTotal, int32 InMax = 3) const;

	/** The current error total, to pass back into the two methods above. */
	int32 ErrorMark() const { return ErrorCountValue; }

private:
	/** Detaches from GLog. Called by the destructor and safe to call twice. */
	void Detach();

	/**
	 * The stored warnings.
	 *
	 * Named distinctly from WarningCount() because a member and a member function
	 * sharing one name does not compile.
	 */
	TArray<FEntry> WarningEntries;

	/**
	 * The stored errors, capped exactly as the warnings are.
	 *
	 * Stored as well as counted because `T04` asks a compile to LIST each error
	 * the engine logged. The count stays exact past the cap, so a caller is
	 * never told a smaller number than was raised.
	 */
	TArray<FEntry> ErrorEntries;

	int32 ErrorCountValue = 0;

	/** The thread that created this capture; only its messages are recorded. */
	uint32 OwnerThreadId = 0;

	/** True while this device is registered on GLog. */
	bool bAttached = false;
};
