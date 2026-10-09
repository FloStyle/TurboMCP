// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

/**
 * The call log behind `E-CALL-LOG` and `R-CALL-LOG`.
 *
 * `03_TOOLS.md` asks for a JSON line per call holding the timestamp, the toolset and tool, the
 * arguments, the outcome or the error text, how long it took, the result size and a token estimate.
 * It also asks for a START record written and flushed BEFORE the call runs and an END record after
 * it, so the last call before an editor crash is in the log.
 *
 * TWO SOURCES, BECAUSE The ENGINE'S HOOK DOES NOT CARRY ENOUGH
 * -----------------------------------------------------------
 * Measured from the engine's own `ModelContextProtocolAnalytics.cpp`, a tool-call event carries
 * only:
 *
 *     SessionId, ToolsetNameHash, ToolNameHash, Duration, ResultStatus
 *
 * The names are **BLAKE3 hashes**, not names, and there are no arguments. `07_ENGINE_NOTES.md`
 * section 2.4 says so and gives the shape of the answer: record what the engine event has for
 * engine tools, and record full detail for this plugin's own tools from inside them.
 *
 * So there are two writers into one log:
 *
 *  - `FTurboMCPAnalyticsProvider` receives every engine tool call, RESOLVES the hashes back to real
 *    names by reproducing the engine's own `FBlake3` hash over the known names, and forwards the
 *    event to whichever provider was installed before it — never replacing a hook without passing
 *    its events on.
 *  - `FTurboMCPCallLog::ScopedCall` is used inside this plugin's own tools, where the arguments are
 *    known, and writes the same line shape with full detail.
 */
class FTurboMCPCallLog
{
public:
	/** One line of the log. */
	struct FEntry
	{
		/** When the call started, in UTC, as an ISO-8601 string. */
		FString Timestamp;

		/** `start` for the record written before the call, `end` for the one after. */
		FString Phase;

		/** `plugin` or `engine`, so a reader can tell where the detail comes from. */
		FString Source;

		FString SessionId;
		FString Toolset;
		FString Tool;

		/** The arguments, when they are known. Empty for an engine tool. */
		FString Arguments;

		/** The outcome or the error text. Empty on a start record. */
		FString Result;

		/** Milliseconds the call took. Zero on a start record. */
		double DurationMs = 0.0;

		/** Characters in the result. Zero on a start record. */
		int32 ResultChars = 0;

		/** The token estimate for the result. Zero on a start record. */
		int32 TokenEstimate = 0;

		/** True when the call reported an error. */
		bool bError = false;
	};

	/** The one log. */
	static FTurboMCPCallLog& Get();

	/**
	 * Whether logging is on.
	 *
	 * Read from the editor settings, where the panel's checkbox stores it. It STARTS OFF, which
	 * `03_TOOLS.md` requires: a log nobody asked for should not silently accumulate.
	 */
	bool IsEnabled() const;

	/** Turns logging on or off AND persists the choice in the editor settings. */
	void SetEnabled(bool bInEnabled);

	/** Writes a record. Does nothing when logging is off. */
	void Write(const FEntry& InEntry);

	/** The entries in memory, oldest first. */
	TArray<FEntry> GetEntries() const;

	/** Drops the in-memory entries. The file on disk is left alone. */
	void Clear();

	/** The file the log is written to. */
	FString GetLogFilePath() const;

	/** Turns one production of entries into a delta and back, for `ScopedCall`. */
	struct FScopedCall
	{
		FScopedCall(const FString& InToolset, const FString& InTool, const FString& InArguments);
		~FScopedCall();

		/** Records what came out. Call before the scope ends. */
		void Finish(const FString& InResult, bool bInError);

	private:
		FString Toolset;
		FString Tool;
		FString Arguments;
		FString StartStamp;
		double StartSeconds = 0.0;
		bool bFinished = false;
	};

private:
	FTurboMCPCallLog() = default;

	/** Serialises one entry as a JSON object on its own line. */
	static FString ToJsonLine(const FEntry& InEntry);

	/** Appends a line to the file and flushes it, so a crash cannot lose it. */
	void AppendToFile(const FString& InLine) const;

	mutable FCriticalSection Mutex;

	/** Kept bounded: a long session must not grow without limit. */
	TArray<FEntry> Entries;

	/** The cap, from `02_PRINCIPLES.md` `P6`: a list must not grow without bound. */
	static constexpr int32 MaxEntries = 1000;

	/**
	 * The file size at which the log rotates, in bytes.
	 *
	 * 4 MB holds roughly ten thousand lines at the size measured in phase 5 — about 400 bytes a
	 * line — which is many sessions of work. The exact figure is not the point: it is that a bound
	 * EXISTS. The file previously had none and reached 402 KB in a single round of testing, because
	 * every tool call writes a line and every READ of the log is itself a tool call.
	 */
	static constexpr int64 MaxFileBytes = 4 * 1024 * 1024;
};
