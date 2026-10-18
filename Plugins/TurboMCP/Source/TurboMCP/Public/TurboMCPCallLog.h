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
 *  - `FTurboMCPCallLog::FScopedCall` is used for this plugin's own tools, where the arguments are
 *    known, and writes the same line shape with full detail.
 *
 * *** AND "THIS PLUGIN'S OWN TOOLS" IS NOW TRUE OF THE CODE AND NOT ONLY OF THE DESIGN, WHICH IS A
 * CORRECTION RATHER THAN A DESCRIPTION. *** Nothing called `FScopedCall` for a long time - it was
 * defined and constructed nowhere - so every record in the log was `source=engine`, no result size
 * was ever measured, and `T02`'s start record did not exist on the agent's path. It is called now, by
 * `FTurboMCPRecordingToolset` (`TurboMCPCallRecorder.h`), which wraps each of the four toolset
 * handlers the engine's registry holds. THAT PLACEMENT IS THE POINT: the same capability written
 * inside the tools would have been 68 individually anchored insertions, and at the registry's door it
 * is four substitutions of one handler each.
 *
 * *** ONE CALL THEREFORE LEAVES MORE THAN ONE RECORD, AND THAT IS WHAT `FTurboMCPCallTimeline`
 * EXISTS FOR: it assembles the records back into calls, so a reader counting calls does not count the
 * plugin's start, the plugin's end and the engine's event as three of them.
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
		/** A size that was never measured. *** NOT ZERO: 0 IS A MEASUREMENT - "the result was empty" - AND AN UNMEASURED SIZE
	 * THAT READS AS 0 MAKES AN EMPTY RESULT AND AN UNKNOWN ONE INDISTINGUISHABLE. The engine's event carries no result
	 * text at all, so every agent-side call arrives with no size to report. *** */
	static constexpr int32 NotMeasured = -1;

	/** The size for a SUM: an unmeasured entry adds nothing, so a total is never corrupted by the sentinel. */
	int32 MeasuredChars() const { return ResultChars == NotMeasured ? 0 : ResultChars; }
	int32 MeasuredTokens() const { return TokenEstimate == NotMeasured ? 0 : TokenEstimate; }

	/** The size as text, for a report that must say `unknown` rather than a number it does not have. */
	static FString DescribeSize(int32 InValue) { return InValue == NotMeasured ? TEXT("unknown") : FString::FromInt(InValue); }

	/** A call at or above this duration is marked slow in the timeline. */
	static constexpr double SlowCallMs = 1000.0;

	/** A call at or above this many characters is marked large in the timeline. */
	static constexpr int32 LargeCallChars = 20000;

	bool IsSlow() const { return DurationMs >= SlowCallMs; }

	/** *** AN UNMEASURED SIZE IS NOT LARGE: `NotMeasured` IS NEGATIVE, SO THE COMPARISON WOULD BE FALSE ANYWAY, BUT IT
	 * IS STATED SO THAT A READER DOES NOT HAVE TO KNOW THE SENTINEL'S VALUE TO TRUST THE MARK. *** */
	bool IsLarge() const { return ResultChars != NotMeasured && ResultChars >= LargeCallChars; }

		int32 ResultChars = NotMeasured;

		/** The token estimate for the result. Zero on a start record. */
		int32 TokenEstimate = NotMeasured;

		/** True when the call reported an error. */
		bool bError = false;
	};

	/** One coherent read of the records and the revision that produced them. */
	struct FSnapshot
	{
		TArray<FEntry> Entries;
		uint64 Revision = 0;
	};

	/** The one log. */
	static FTurboMCPCallLog& Get();

	/** Public so an isolated test instance can be owned by a scoped fixture. */
	~FTurboMCPCallLog() = default;

#if WITH_DEV_AUTOMATION_TESTS
	/** A private per-test log with no disk sink and an explicit enabled state. */
	static FTurboMCPCallLog* CreateForTest(bool bEnabled = true);
#endif

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

	/** A coherent copy of entries and revision under one lock. */
	FSnapshot GetSnapshot() const;

	/** The monotonically increasing record revision, copied under the same lock. */
	uint64 GetRevision() const;

	/** The entries in memory, oldest first. */
	TArray<FEntry> GetEntries() const;

	/**
	 * How many entries are in memory, WITHOUT COPYING THEM.
	 *
	 * The panel no longer polls this to notice a call: an unchanged count cannot
	 * distinguish a capped write or a clear/refill. `GetRevision` is the refresh
	 * signal; this accessor remains for readers that need the bound itself.
	 */
	int32 NumEntries() const;

	/** Drops the in-memory entries. The file on disk is left alone. */
	void Clear();

	/** The file the log is written to. */
	FString GetLogFilePath() const;

	/**
	 * One line per call, oldest first: IN from the `start` record and OUT from the `end` record that follows it.
	 *
	 * *** A `start` WITH NO `end` MEANS COMPLETION WAS NOT OBSERVED, AND IS PRINTED THAT WAY RATHER THAN AS
	 * `running`, BECAUSE THE LOG CANNOT PROVE THE TOOL IS STILL ALIVE. *** A record with an `end` and
	 * no `start` (the engine's own events arrive that way) is a row of its own.
	 * *** TEXT RATHER THAN WIDGETS SO THE PANEL CAN RENDER IT AND A READER CAN DIFF IT; THE CALL IS ASSEMBLED BY
	 * `FTurboMCPCallTimeline`, WHICH IS ALSO WHAT `R-CALL-COST` COUNTS, SO THE ROW AND THE TOTAL CANNOT DISAGREE
	 * ABOUT WHAT ONE CALL IS. ***
	 *
	 * *** PUBLIC, AND THAT IS A CORRECTION RATHER THAN A CONVENIENCE: this method was written for the panel and left
	 * PRIVATE WITH NO CALLER AT ALL, so `T02`'s item 3 was recorded as done while the code that implements it could
	 * not be reached by anything. A reader of the card could not tell that from the card, which is the shape this
	 * project keeps having to look for. ***
	 */
	FString RenderTimeline() const;

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
	struct FTestOptions
	{
		bool bDiskSink = false;
		bool bEnabled = true;
	};

	FTurboMCPCallLog() = default;
	explicit FTurboMCPCallLog(const FTestOptions& InOptions)
		: bUseSettings(false)
		, bTestEnabled(InOptions.bEnabled)
		, bDiskSink(InOptions.bDiskSink)
	{
	}

	/** Serialises one entry as a JSON object on its own line. */
	static FString ToJsonLine(const FEntry& InEntry);

	/** Appends a line to the file and flushes it, so a crash cannot lose it. */
	void AppendToFile(const FString& InLine) const;

	mutable FCriticalSection Mutex;

	/** Kept bounded: a long session must not grow without limit. */
	TArray<FEntry> Entries;

	/** Incremented on every memory write and clear, under Mutex. */
	uint64 Revision = 0;

	/** Production reads the editor setting; the isolated test instance owns its state. */
	bool bUseSettings = true;
	bool bTestEnabled = false;
	bool bDiskSink = true;

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
