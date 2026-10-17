// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPPlayCheck.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Engine/World.h"
#include "HAL/PlatformTLS.h"
#include "Logging/LogVerbosity.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDevice.h"
#include "PlayInEditorDataTypes.h"
#include "TurboMCPLogCapture.h"
#include "UObject/Package.h"

namespace
{
	/** How many records are kept. Past this only the counts grow, so a reported number stays true. */
	constexpr int32 MaxStoredRecords = 200;

	/** One log line the play session raised, with the category that raised it. */
	struct FRecord
	{
		FString Category;
		FString Message;
		bool bError = false;

		/**
		 * The thread that raised it.
		 *
		 * KEPT BECAUSE THE SESSION IS NOT ONE THREAD: the tool call creates its log device on one thread
		 * while PIE's own start-up lines arrive on a DIFFERENT one, so FILTERING ON A SINGLE THREAD ID
		 * WOULD DISCARD HALF THE SESSION.
		 */
		uint32 ThreadId = 0;
	};

	/**
	 * A log device that watches a PLAY SESSION rather than a single tool call.
	 *
	 * WHY THIS IS NOT `FTurboMCPLogCapture`, WHICH WAS THE FIRST THING TRIED. That class is the right shape
	 * - it is an `FOutputDevice` on `GLog`, it filters by owning thread, and its header is the model for
	 * this one - but it CANNOT SERVE THIS TOOL, for a reason stated in its own source rather than guessed:
	 *
	 *     // Errors are counted but not stored: the caller reports its own error for
	 *     // a failed save, and repeating the engine's would be the log noise P6 forbids.
	 *     ++ErrorCountValue;
	 *     return;
	 *                          - TurboMCPLogCapture.cpp:81-85
	 *
	 * `R-PLAY-CHECK`'s ENTIRE PURPOSE IS TO RETURN THE ERRORS, so a capture that counts them and throws the
	 * text away cannot answer it. The other difference is lifetime: that capture is documented for "the span
	 * of a single tool call", and a play session spans MANY calls and many frames. So the attachment
	 * MECHANISM is copied deliberately - `GLog->AddOutputDevice`, thread filter, bounded storage - and the
	 * storage differs because the question differs.
	 */
	class FPlayLogDevice : public FOutputDevice
	{
	public:
		FPlayLogDevice()
		{
			OwnerThreadId = FPlatformTLS::GetCurrentThreadId();
			if (GLog != nullptr)
			{
				GLog->AddOutputDevice(this);
				bAttached = true;
			}
		}

		virtual ~FPlayLogDevice() override { Detach(); }

		FPlayLogDevice(const FPlayLogDevice&) = delete;
		FPlayLogDevice& operator=(const FPlayLogDevice&) = delete;

		virtual void Serialize(const TCHAR* InMessage, ELogVerbosity::Type InVerbosity,
			const FName& InCategory) override
		{
			// NO THREAD FILTER HERE, AND THAT IS A CORRECTION RATHER THAN AN OVERSIGHT.
			//
			// `FTurboMCPLogCapture` filters to its owning thread, and copying that was the first thing
			// tried. IT SILENTLY DISCARDED EVERY RECORD THE TOOL EXISTS TO COLLECT. MEASURED with a
			// temporary diagnostic that printed the creating thread: the device was created on thread
			// 31392, and PIE's own start-up warning and error arrived on a DIFFERENT thread. So
			// `R-PLAY-CHECK` answered "0 warning(s) and 0 error(s) ... It ran and stopped cleanly" while
			// the log plainly contained both lines - the most misleading answer the tool could give, and
			// it survived until a deliberate warning was injected into the window to test the count.
			//
			// THE OWNING-THREAD RULE IS RIGHT FOR A SAVE AND WRONG FOR A SESSION. That capture attributes
			// engine noise to ONE asset being saved on one thread, so filtering is what stops an unrelated
			// worker's warning being blamed on the asset. A play session is not one asset and not one
			// thread: it is a running game, and a warning it raises is a warning about the session whoever
			// raised it. Filtering cannot separate "the session" from "the editor" here, because the
			// session's own start-up runs on the editor's threads.
			//
			// THE THREAD IS RECORDED INSTEAD, so a caller can still tell where a line came from.
			const bool bIsWarning = InVerbosity == ELogVerbosity::Warning;
			const bool bIsError = InVerbosity == ELogVerbosity::Error
				|| InVerbosity == ELogVerbosity::Fatal;
			if (!bIsWarning && !bIsError)
			{
				return;
			}

			if (bIsError) { ++ErrorCount; } else { ++WarningCount; }

			if (Records.Num() >= MaxStoredRecords)
			{
				// Past the cap only the counts grow. The report SAYS the list was capped rather than
				// letting a short list imply a quiet session.
				++DroppedCount;
				return;
			}

			FRecord Record;
			Record.Category = InCategory.ToString();
			// Trimmed to the first line: engine log lines run long and multi-line, and a report that
			// pastes them whole stops being readable at the moment it matters most.
			Record.Message = InMessage != nullptr ? FString(InMessage).TrimStartAndEnd() : FString();
			int32 Newline = INDEX_NONE;
			if (Record.Message.FindChar(TEXT('\n'), Newline))
			{
				Record.Message.LeftInline(Newline);
			}
			Record.bError = bIsError;
			Record.ThreadId = FPlatformTLS::GetCurrentThreadId();
			Records.Add(MoveTemp(Record));
		}

		void Detach()
		{
			if (bAttached && GLog != nullptr)
			{
				GLog->RemoveOutputDevice(this);
			}
			bAttached = false;
		}

		const TArray<FRecord>& GetRecords() const { return Records; }
		int32 GetWarningCount() const { return WarningCount; }
		int32 GetErrorCount() const { return ErrorCount; }
		int32 GetDroppedCount() const { return DroppedCount; }

	private:
		TArray<FRecord> Records;
		int32 WarningCount = 0;
		int32 ErrorCount = 0;
		int32 DroppedCount = 0;
		uint32 OwnerThreadId = 0;
		bool bAttached = false;
	};

	/**
	 * The one session this tool is watching, if any.
	 *
	 * A SINGLE SLOT RATHER THAN A LIST. Two concurrent play sessions cannot be told apart in one log, so
	 * offering to start a second would produce a report that mixes two runs. A second start is REFUSED BY
	 * NAME instead, which is the answer that cannot mislead.
	 */
	struct FWatch
	{
		TUniquePtr<FPlayLogDevice> Device;
		/**
		 * The ticker's handle, which is `FTSTicker::FDelegateHandle` and NOT the global `FDelegateHandle`.
		 *
		 * MEASURED: the ticker defines its own nested type - `using FDelegateHandle = TWeakPtr<FElement>;`
		 * at `Containers/Ticker.h:32` - so the two are unrelated types and the build refused to convert
		 * between them: `error C2664: cannot convert argument 1 from 'FDelegateHandle' to
		 * 'FTSTicker::FDelegateHandle'`. The engine's own users spell it out, e.g. `Ticker.h:162`.
		 */
		FTSTicker::FDelegateHandle Ticker;
		/**
		 * The handle of this watch's subscription to `PostPIEStarted`.
		 *
		 * KEPT SO IT CAN BE REMOVED. A delegate bound for every start and never removed would fire once per
		 * past session, so the tenth run would see ten callbacks and the counter would be meaningless. The
		 * engine's own users keep the handle for the same reason, e.g.
		 * `LevelEditorSequencerIntegration.cpp:232`.
		 */
		FDelegateHandle StartHandle;
		double ElapsedSeconds = 0.0;
		double BudgetSeconds = 0.0;
		FString Map;
		bool bEndRequested = false;
		/**
		 * True when the session never produced a play world within the grace period.
		 *
		 * Kept so the report can SAY that rather than claiming a clean window: an empty log from a session
		 * that never ran looks exactly like an empty log from a session that ran perfectly, and only one of
		 * those is good news.
		 */
		bool bNeverStarted = false;
	};

	FWatch* GWatch = nullptr;

	/** True once the world has actually started, so a report is not read before the session ran. */
	bool GStartObserved = false;

	/**
	 * Counts frame time and asks the session to end once the budget is spent.
	 *
	 * THIS IS THE WHOLE TRICK. A tool call cannot wait, because waiting would block the thread that ticks
	 * PIE (see the header). So the waiting is done HERE, one frame at a time, by the engine's own ticker,
	 * while the game thread stays free to run the session.
	 *
	 * It returns true to keep ticking only while the session is still being watched.
	 */
	bool TickWatch(float DeltaSeconds)
	{
		if (GWatch == nullptr)
		{
			return false;
		}

		GWatch->ElapsedSeconds += static_cast<double>(DeltaSeconds);

		// A WATCH IS CLOSED IF THE SESSION NEVER ARRIVES, AND THIS GUARD EXISTS BECAUSE ONE DID NOT DURING
		// TESTING. `RequestEndPlayMap` does NOTHING when there is no `PlayWorld` to end - it sets a flag
		// only `if (PlayWorld)` (PlayLevel.cpp:1402) - so if the engine never produced a play world the end
		// request was a no-op and this ticker would have run FOREVER. The watch slot would stay taken, so
		// every later start would be refused with "already being watched" for the life of the process.
		//
		// The grace period is generous relative to the budget because a cold PIE start genuinely takes a
		// moment, and it is only reached when nothing has started AT ALL.
		constexpr double StartGraceSeconds = 15.0;
		if (!GStartObserved && !GWatch->bEndRequested
			&& GWatch->ElapsedSeconds >= GWatch->BudgetSeconds + StartGraceSeconds)
		{
			// Marked finished so the next report can say what happened rather than reading `pending`
			// forever, and the ticker stops.
			GWatch->bEndRequested = true;
			GWatch->bNeverStarted = true;
			return false;
		}

		if (!GWatch->bEndRequested && GWatch->ElapsedSeconds >= GWatch->BudgetSeconds)
		{
			GWatch->bEndRequested = true;
			if (GEditor != nullptr)
			{
				// The engine's own play control, and the only SAFE one from a tick callback: it queues the
				// request for the next tick rather than tearing the world down inside this stack frame
				// (EditorEngine.h: "should be used to end the game if it's not safe to directly call
				// EndPlayMap in your stack frame"). It is also what the Play menu calls.
				GEditor->RequestEndPlayMap();
			}
		}

		return true;
	}

	void CloseWatch()
	{
		if (GWatch == nullptr)
		{
			return;
		}
		if (GWatch->Ticker.IsValid())
		{
			FTSTicker::RemoveTicker(GWatch->Ticker);
			GWatch->Ticker.Reset();
		}
		if (GWatch->StartHandle.IsValid())
		{
			// Unbound, so a later session does not fire this one's callback. Without this the witness would
			// count every past start and could report a session that never ran.
			FEditorDelegates::PostPIEStarted.Remove(GWatch->StartHandle);
			GWatch->StartHandle.Reset();
		}
		if (GWatch->Device.IsValid())
		{
			GWatch->Device->Detach();
		}
		delete GWatch;
		GWatch = nullptr;
	}

	/** Groups records by category, counted, errors first within each category. */
	FString RenderRecords(const FPlayLogDevice& InDevice, int32& OutWarnings, int32& OutErrors)
	{
		OutWarnings = InDevice.GetWarningCount();
		OutErrors = InDevice.GetErrorCount();

		const TArray<FRecord>& Records = InDevice.GetRecords();

		// Counted per category. The counters are built from the RECORDS, so they describe the list that is
		// printed; the totals above describe the whole window including anything the cap dropped, and the
		// report says so rather than letting the two disagree silently.
		TMap<FString, TPair<int32, int32>> ByCategory; // category -> (warnings, errors)
		for (const FRecord& Record : Records)
		{
			TPair<int32, int32>& Counts = ByCategory.FindOrAdd(Record.Category);
			if (Record.bError) { ++Counts.Value; } else { ++Counts.Key; }
		}

		TArray<FString> Categories;
		ByCategory.GetKeys(Categories);
		Categories.Sort();

		FString Out;
		for (const FString& Category : Categories)
		{
			const TPair<int32, int32>& Counts = ByCategory[Category];
			Out += FString::Printf(TEXT("\n  %s: %d warning(s), %d error(s)"),
				*Category, Counts.Key, Counts.Value);
			for (const FRecord& Record : Records)
			{
				if (Record.Category == Category)
				{
					Out += FString::Printf(TEXT("\n      %s %s"),
						Record.bError ? TEXT("ERROR") : TEXT("warn "), *Record.Message);
				}
			}
		}

		if (InDevice.GetDroppedCount() > 0)
		{
			Out += FString::Printf(
				TEXT("\n  %d further record(s) were raised and counted but not listed, "
					"because the list is capped at %d."),
				InDevice.GetDroppedCount(), MaxStoredRecords);
		}

		return Out;
	}
}

namespace TurboMCPPlayCheck
{
	bool IsWatching()
	{
		return GWatch != nullptr;
	}

	FAnswer Run(const FString& InSeconds, const FString& InMap)
	{
		FAnswer Answer;
		Answer.Map = InMap;

		const FString Seconds = InSeconds.TrimStartAndEnd();
		if (GEditor == nullptr)
		{
			Answer.Outcome = EOutcome::Refused;
			Answer.Reason = TEXT("the editor engine is not available, so there is no Play-In-Editor to "
				"start or observe.");
			return Answer;
		}

		const bool bSessionUp = GEditor->IsPlaySessionInProgress();

		// ---------------------------------------------------------------------------------------------
		// THE REPORT HALF. `seconds` empty asks "what have you got".
		// ---------------------------------------------------------------------------------------------
		if (Seconds.IsEmpty())
		{
			if (GWatch == nullptr)
			{
				if (bSessionUp)
				{
					// A session is running that this tool did not start and is not watching, so its log is
					// not being captured and cannot honestly be reported.
					Answer.Outcome = EOutcome::AlreadyRunning;
					Answer.Reason = TEXT("a Play-In-Editor session is running that this tool did NOT start, "
						"so nothing is being captured for it. Stop it, or start one with a `seconds` value "
						"so the window is recorded.");
					Answer.bFinished = false;
					return Answer;
				}
				Answer.Outcome = EOutcome::Refused;
				Answer.Reason = TEXT("nothing is being watched and no session is running. Pass a `seconds` "
					"value to start one.");
				return Answer;
			}

			// A watch exists. It is finished when the end was requested AND the engine has actually torn
			// the session down, which is the only honest signal: `RequestEndPlayMap` only queues the work.
			const bool bTornDown = !GEditor->IsPlaySessionInProgress();
			if (!GWatch->bEndRequested || !bTornDown)
			{
				Answer.Outcome = EOutcome::Pending;
				Answer.bStartedHere = true;
				Answer.bFinished = false;
				Answer.Reason = FString::Printf(
					TEXT("the session is still running: %.1f of %.1f second(s) elapsed%s. Ask again with an "
						"empty `seconds` once it has stopped."),
					GWatch->ElapsedSeconds, GWatch->BudgetSeconds,
					GWatch->bEndRequested ? TEXT(", and the stop has been requested") : TEXT(""));
				return Answer;
			}

			// A SESSION THAT NEVER STARTED IS ITS OWN ANSWER, NOT A CLEAN WINDOW. This is checked BEFORE
			// the report is assembled, because an empty log from a session that never ran and an empty log
			// from a session that ran perfectly look identical, and reporting the first as the second would
			// be the most misleading thing this tool could do.
			if (GWatch->bNeverStarted)
			{
				Answer.Outcome = EOutcome::Refused;
				Answer.bStartedHere = true;
				Answer.bFinished = true;
				Answer.Reason = FString::Printf(
					TEXT("the play session was requested but NEVER REACHED A RUNNING WORLD within %.0f "
						"second(s) of the window ending, so there is no result to report. The editor did not "
						"raise PostPIEStarted and no session was in progress. Nothing is being watched now. "
						"This is NOT a clean run."),
					GWatch->ElapsedSeconds - GWatch->BudgetSeconds);
				CloseWatch();
				GStartObserved = false;
				return Answer;
			}

			Answer.Outcome = EOutcome::Reported;
			Answer.bStartedHere = true;
			Answer.bFinished = true;
			Answer.Map = GWatch->Map;
			if (GWatch->Device.IsValid())
			{
				Answer.Body = RenderRecords(*GWatch->Device, Answer.WarningCount, Answer.ErrorCount);
			}
			// THE REASON IS FILLED HERE RATHER THAN LEFT FOR THE RENDERER TO INVENT. The renderer must not
			// decide anything, and "what was actually played" is a fact only this side knows.
			Answer.Reason = FString::Printf(
				TEXT("played for %.1f second(s)%s, then stopped because this tool started it."),
				GWatch->BudgetSeconds,
				GWatch->Map.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" on %s"), *GWatch->Map));
			// The watch is closed HERE because its report has been handed over. A second report would
			// otherwise print the same window twice and read as a second session.
			CloseWatch();
			GStartObserved = false;
			return Answer;
		}

		// ---------------------------------------------------------------------------------------------
		// THE START HALF.
		// ---------------------------------------------------------------------------------------------
		if (GWatch != nullptr)
		{
			Answer.Outcome = EOutcome::Refused;
			Answer.Reason = FString::Printf(
				TEXT("a session started by this tool is already being watched (%.1f of %.1f second(s)). "
					"Report it with an empty `seconds` before starting another, so the two windows are not "
					"mixed into one log."),
				GWatch->ElapsedSeconds, GWatch->BudgetSeconds);
			return Answer;
		}

		if (bSessionUp)
		{
			Answer.Outcome = EOutcome::AlreadyRunning;
			Answer.Reason = TEXT("a Play-In-Editor session is ALREADY running, so it was left alone rather "
				"than restarted: stopping someone else's session is not this tool's decision. Report on it "
				"by asking with an empty `seconds` after it stops, or run this again when nothing is "
				"playing.");
			Answer.bFinished = false;
			return Answer;
		}

		double Budget = 0.0;
		if (!Seconds.IsNumeric())
		{
			Answer.Outcome = EOutcome::Refused;
			Answer.Reason = FString::Printf(
				TEXT("`seconds` must be a number and %s is not. Pass a positive number of seconds to play "
					"for."), *Seconds);
			return Answer;
		}
		Budget = FCString::Atod(*Seconds);
		if (Budget <= 0.0)
		{
			Answer.Outcome = EOutcome::Refused;
			Answer.Reason = FString::Printf(
				TEXT("`seconds` must be greater than zero and %s is not, so the session would end the "
					"instant it started and report a window nothing had a chance to write to."), *Seconds);
			return Answer;
		}
		// A ceiling, because the ticker only advances while the editor ticks. An accidentally huge value
		// would hold the watch open indefinitely and every later start would be refused by the single-slot
		// rule, which is a bad failure mode to leave reachable.
		constexpr double MaxBudgetSeconds = 600.0;
		if (Budget > MaxBudgetSeconds)
		{
			Answer.Outcome = EOutcome::Refused;
			Answer.Reason = FString::Printf(
				TEXT("`seconds` is capped at %.0f and %s is larger. A longer window would hold the single "
					"watch slot open and refuse every start until it elapsed."), MaxBudgetSeconds, *Seconds);
			return Answer;
		}
		// THE MAP IS VALIDATED BEFORE ANYTHING IS ARMED, AND THAT GUARD EXISTS BECAUSE AN INVALID MAP
		// WEDGED THE EDITOR DURING TESTING.
		//
		// MEASURED: asking for `/Game/DoesNotExist/NoMap` does not fail politely. The request is accepted,
		// the engine logs `Warning: Failed to find object 'Object /Game/DoesNotExist/NoMap'`, and the game
		// thread then stops servicing requests altogether - the editor stayed alive, port 8000 stayed open,
		// and every later MCP call timed out. THE TOOL HAD NO DEFENCE, so one bad argument took the whole
		// server down with it, which is far worse than refusing.
		//
		// A package that cannot be found is now refused BY NAME while nothing is at risk. `LoadPackage`
		// with `LOAD_NoWarn | LOAD_Quiet` does not fault on a missing package; it returns null, which is
		// the answer being asked for.
		const FString MapPath = InMap.TrimStartAndEnd();
		if (!MapPath.IsEmpty())
		{
			UPackage* Package = LoadPackage(nullptr, *MapPath, LOAD_NoWarn | LOAD_Quiet);
			if (Package == nullptr)
			{
				Answer.Outcome = EOutcome::Refused;
				Answer.Reason = FString::Printf(
					TEXT("no map exists at %s, so the session was NOT requested. The path must be a package "
						"the editor can load, for example /Game/Maps/MyMap. Refusing here rather than asking "
						"the engine for a map that is not there is deliberate: measured, an unfindable map "
						"leaves the editor unable to answer any further request."), *MapPath);
				return Answer;
			}
		}

		// The device is attached BEFORE the session starts, so a warning raised while the world is coming
		// up is inside the window rather than before it.
		GWatch = new FWatch();
		GWatch->Device = MakeUnique<FPlayLogDevice>();
		GWatch->BudgetSeconds = Budget;
		GWatch->Map = MapPath;

		// THE DELEGATE IS THE WITNESS THAT THE SESSION ACTUALLY BEGAN. Without it a report could be read
		// from a session that never started, which would look like a clean run.
		GStartObserved = false;
		//
		// A LAMBDA AND NOT `AddRaw(nullptr, &OnPostPIEStarted)`, WHICH THE COMPILER REFUSED:
		// `error C2672: 'AddRaw': no matching overloaded function found`. `AddRaw` binds a POINTER TO A
		// MEMBER FUNCTION, so it needs an object to call it on and a free function is not that. The engine's
		// own `AddRaw` uses are all member functions - `AutomationEditorCommon.cpp:1182`,
		// `LevelEditorSequencerIntegration.cpp:232`. `AddLambda` takes a callable and hands back a handle
		// that can be removed, which is exactly what is needed here.
		GWatch->StartHandle = FEditorDelegates::PostPIEStarted.AddLambda(
			[](const bool /*bIsSimulatingInEditor*/)
			{
				GStartObserved = true;
			});

		GWatch->Ticker = FTSTicker::GetCoreTicker().AddTicker(
			TEXT("TurboMCPPlayCheckWatch"), 0.0f, &TickWatch);

		// THE ENGINE'S OWN PLAY CONTROL, and it does NOT start the session: it stores the request for the
		// next tick (PlayLevel.cpp:987). Handing it the map override is how a specific map is asked for.
		FRequestPlaySessionParams Params;
		Params.WorldType = EPlaySessionWorldType::PlayInEditor;
		Params.SessionDestination = EPlaySessionDestinationType::InProcess;
		if (!GWatch->Map.IsEmpty())
		{
			Params.GlobalMapOverride = GWatch->Map;
		}
		GEditor->RequestPlaySession(Params);

		Answer.Outcome = EOutcome::Pending;
		Answer.bStartedHere = true;
		Answer.bFinished = false;
		Answer.Reason = FString::Printf(
			TEXT("the play session has been REQUESTED for %.1f second(s)%s. It starts on the next editor "
				"tick, which is why this call cannot wait for it: a tool call occupies the very thread the "
				"session needs in order to run. Call this again with an empty `seconds` to get the warnings "
				"and errors once it has stopped."),
			Budget, GWatch->Map.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" on %s"), *GWatch->Map));
		return Answer;
	}
}
