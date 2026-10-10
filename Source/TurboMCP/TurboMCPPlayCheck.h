// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"

/**
 * Runs a short Play-In-Editor session and reports the warnings and errors it raised.
 *
 * WHY THIS IS TWO CALLS AND NOT ONE SLEEP, WHICH IS THE WHOLE DESIGN.
 *
 * `R-PLAY-CHECK` is specified as: start PIE, wait `seconds`, stop it, return the log. THE NAIVE SHAPE IS
 * IMPOSSIBLE, and it is worth recording exactly why, because it looks obviously fine on paper.
 *
 * A TOOL CALL RUNS ON THE EDITOR'S GAME THREAD. So does PIE. MEASURED in the engine source:
 *
 *   - `UEditorEngine::Tick` ticks the play world DIRECTLY, in its own stack frame:
 *     `PieContext.World()->Tick(LEVELTICK_All, TickDeltaSeconds)` at
 *     `Engine/Source/Editor/UnrealEd/Private/EditorEngine.cpp:2169`.
 *   - `UEditorEngine::RequestPlaySession` does NOT start anything. It stores the request:
 *     `// Store our Request to be operated on next Tick.` at
 *     `Engine/Source/Editor/UnrealEd/Private/PlayLevel.cpp:987`.
 *   - `UEditorEngine::RequestEndPlayMap` does NOT end anything either. It only raises a flag,
 *     `bRequestEndPlayMapQueued = true`, at `PlayLevel.cpp:1404`, and the flag is consumed in
 *     `EditorEngine.cpp:2463`.
 *
 * SO A SLEEP INSIDE THE TOOL CALL BLOCKS THE VERY THREAD THAT WOULD START PIE, TICK IT, AND STOP IT. The
 * session would never begin, or would hang, and no amount of waiting would produce a log. Waiting is not a
 * timing problem here; it is a deadlock by construction.
 *
 * THE ACTUAL MECHANISM, therefore, is the engine's own deferred request plus a frame ticker:
 *
 *   CALL 1, `seconds` given:  request the session, attach a log capture, and register an
 *                             `FTSTicker` that requests the end once `seconds` of FRAME TIME have
 *                             elapsed. It returns immediately, because it must: the frames it is waiting
 *                             for can only happen once it has returned.
 *   CALL 2, `seconds` empty:  report whether the session is still running, and when it has finished, the
 *                             grouped warnings and errors from that window.
 *
 * A TOOL THAT STARTED PIE AND RETURNED AN EMPTY OR STALE LOG WOULD BE WORSE THAN A RECORDED GAP (P1), so
 * call 1 never pretends to have a result and call 2 says plainly whether it has one yet.
 *
 * THE ENGINE'S OWN CONTROLS ARE USED THROUGHOUT: `RequestPlaySession` and `RequestEndPlayMap` are the same
 * entry points the Play menu calls, and the session state is read from `IsPlaySessionInProgress` rather
 * than tracked independently.
 *
 * IT ONLY STOPS WHAT IT STARTED. A session already running when call 1 arrives is LEFT RUNNING and is
 * reported as such, because stopping a developer's own session would be this tool taking an action nobody
 * asked for.
 *
 * ONE SESSION AT A TIME. The capture and the ticker are held as a single process-wide slot. A second
 * `START` while one is pending is refused BY NAME rather than silently replacing it, because the first
 * session's log would otherwise be lost and the caller would never know.
 */
namespace TurboMCPPlayCheck
{
	/** What a call to the tool decided, so the caller can be told rather than left guessing. */
	enum class EOutcome : uint8
	{
		/** The session finished and the log below is the window that was asked for. */
		Reported,

		/** A session is running and has not finished. Ask again. */
		Pending,

		/** A session was already running that this tool did NOT start, so it was left alone. */
		AlreadyRunning,

		/** The caller asked for nothing sensible, and the reason says why. */
		Refused,
	};

	/** One captured warning or error, kept with the category that raised it. */
	struct FRecord
	{
		FString Category;
		FString Message;
		bool bError = false;
	};

	/** The answer of one call, rendered by the tool. */
	struct FAnswer
	{
		EOutcome Outcome = EOutcome::Refused;

		/** The records, grouped and counted, ready to print. Empty unless Reported. */
		FString Body;

		/** How many warns and errors the window held. Zero unless Reported. */
		int32 WarningCount = 0;
		int32 ErrorCount = 0;

		/** The map the session was asked for, echoed so the caller knows what ran. Empty means "the current one". */
		FString Map;

		/** True when this call started the session rather than joining one. */
		bool bStartedHere = false;

		/** Set on Refused and on AlreadyRunning: the sentence the caller reads. */
		FString Reason;

		/** True when the session has ended, whether or not it produced a report. */
		bool bFinished = false;
	};

	/**
	 * One call of the tool.
	 *
	 * `InSeconds` empty means "report", non-empty means "start". `InMap` is only meaningful on a start.
	 *
	 * THIS NEVER BLOCKS AND NEVER TICKS THE ENGINE. It only ever asks for a session to begin or to end.
	 */
	FAnswer Run(const FString& InSeconds, const FString& InMap);

	/** True while a session this tool started is still being watched. For the panel and for tests. */
	bool IsWatching();
}
