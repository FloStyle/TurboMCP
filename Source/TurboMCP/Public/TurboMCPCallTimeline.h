// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

#include "TurboMCPCallLog.h"

/**
 * ONE CALL, ASSEMBLED FROM THE RECORDS TWO WRITERS PRODUCE FOR IT.
 *
 * WHY THIS EXISTS
 * ---------------
 * The log has two writers on purpose (`TurboMCPCallLog.h:25-43`), and after the plugin's own tools
 * began recording, one agent call leaves MORE THAN ONE record in it:
 *
 *     plugin  start  TurboMCPReadToolset.ReadTurboMCPStatus   <- written before the call
 *     plugin  end    TurboMCPReadToolset.ReadTurboMCPStatus   <- written after it, with the size
 *     engine  end    TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus   <- the engine's own view
 *     engine  end    (top-level).call_tool                    <- the MCP layer's view of the dispatch
 *
 * *** THREE READERS COUNTED RECORDS AND CALLED THEM CALLS. *** `R-CALL-COST` totalled them, so one
 * call was counted twice the moment the start record existed; the timeline rendered a `start` with no
 * `end` beside it as `running` for the rest of the session; and a per-asset total printed `2 call(s)`
 * for one call because both records of the pair carry the arguments. NONE OF THAT IS A DEFECT IN ANY
 * ONE READER - they were all correct while every record was an engine `end` record, WHICH IS THE
 * MEASUREMENT THAT CHANGED.
 *
 * So the pairing lives here, ONCE, and every reader goes through it.
 *
 * WHAT A "CALL" IS
 * ----------------
 * A `plugin` `start` opens one. The `plugin` `end` with the same toolset and tool closes it, and the
 * call keeps THE START'S TIMESTAMP with the end's duration, size and status - because a row that says
 * when a call began is what a timeline is for. An `engine` `end` for a key that is already closed is
 * THE ENGINE'S OWN VIEW OF THAT SAME CALL and is merged rather than counted: that is the whole reason
 * one call does not become two rows. An `engine` `end` for a key that was never opened by this plugin
 * is a call of its own, which is what every engine tool still is.
 *
 * A `start` with no `end` is returned as it stands, so a reader can print it as `running` - the shape
 * a crash leaves, and the shape `T02`'s start record exists for.
 */
class FTurboMCPCallTimeline
{
public:
	/**
	 * The identity two records share if they describe the same call.
	 *
	 * THE LAST DOTTED SEGMENT, which is the rule `TurboMCPCallCost.cpp:114-119` already uses and for
	 * the same measured reason: the engine's event says `TurboMCP.TurboMCPReadToolset` and the plugin
	 * writes the bare `TurboMCPReadToolset`, and the engine's tool slot holds the FULLY QUALIFIED
	 * `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus` where the plugin writes the bare tool name.
	 * Without folding both fields, no pair would ever meet and every plugin start would render as
	 * `running` forever.
	 *
	 * Folded to lower case as well, because the engine resolves a hash to a name and the two writers
	 * do not have to agree on case for the same call.
	 */
	static FString Key(const FString& InToolset, const FString& InTool);

	/**
	 * ONE RECORD PER CALL, oldest first.
	 *
	 * The record returned for a finished call is the RICHEST one available - the plugin's own `end`
	 * record, which carries the arguments, the real size and the token estimate. An engine-only call
	 * keeps its engine record. A call that never ended keeps its `start` record, so a reader can tell
	 * `running` from `never happened` without a second pass.
	 *
	 * The returned record's `Timestamp` is WHEN THE CALL STARTED when a start record exists, and its
	 * `SessionId` is filled from the engine's own record for the same call when the plugin's record
	 * does not carry one - the plugin writes no session id, because the seam it records at (the
	 * engine's toolset registry) is below the MCP session and knows nothing about it.
	 */
	static TArray<FTurboMCPCallLog::FEntry> RepresentativeRecords(
		const TArray<FTurboMCPCallLog::FEntry>& InRecords);

	/**
	 * EVERY record, oldest first, with an empty `SessionId` replaced by the session in effect.
	 *
	 * A record with no session id belongs to the session of the records around it - the plugin's
	 * `start` and `end` sit between the engine's two events for the same call. `R-CALL-LOG` groups by
	 * session, and without this one call's own pair would split a batch script's group in two.
	 */
	static TArray<FTurboMCPCallLog::FEntry> WithSessionsInEffect(
		const TArray<FTurboMCPCallLog::FEntry>& InRecords);
};
