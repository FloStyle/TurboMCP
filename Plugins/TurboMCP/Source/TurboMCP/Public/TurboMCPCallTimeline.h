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
 * `end` beside it as `completion not observed` for the rest of the session; and a per-asset total printed `2 call(s)`
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
 * A `start` with no `end` is returned as it stands, so a reader can print it as `completion not observed` - the shape
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
	 * `completion not observed` forever.
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
	 * `completion not observed` from `never happened` without a second pass.
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

	/**
	 * What a reader narrows the timeline down by, one field per thing the row shows.
	 *
	 * *** FOUR CRITERIA AND NOT ONE BLOB, WHICH IS A CORRECTION RATHER THAN A PREFERENCE. *** `T02`'s
	 * item 4b asks for rows "filtered by toolset, tool, asset and status", and one substring matched
	 * against everything cannot answer "which calls touched THIS asset" without also returning every
	 * call whose ARGUMENTS happen to mention it for another reason. Each field is matched against its
	 * own column, case-insensitively:
	 *
	 *     Toolset / Tool   the two name columns
	 *     Asset            the ARGUMENTS, which is the only place an asset is ever named - the engine's
	 *                      event carries no arguments at all, which is what `R-CALL-COST`'s per-asset
	 *                      section has always said
	 *     Status           `error` or `ok`, from the record's own `bError`
	 *
	 * An empty field is no constraint, so the default filter keeps every call.
	 */
	struct FFilter
	{
		FString Toolset;
		FString Tool;
		FString Asset;
		FString Status;

		bool IsEmpty() const
		{
			return Toolset.IsEmpty() && Tool.IsEmpty() && Asset.IsEmpty() && Status.IsEmpty();
		}
	};

	/**
	 * One row's text: the summary line the timeline prints for a call.
	 *
	 * *** IT LIVES HERE SO THE PANEL'S ROWS AND THE TIMELINE'S TEXT ARE ONE RENDERING. *** The panel
	 * renders a list of these and the text timeline prints them; a second formatter in the panel would
	 * be a second answer to "what does one row look like", which is the shape this project keeps
	 * finding - two places that agree until one of them changes.
	 */
	static FString RenderRow(const FTurboMCPCallLog::FEntry& InCall);

	/**
	 * One record per call, oldest first, KEEPING only the calls `InFilter` accepts.
	 *
	 * The pairing is `RepresentativeRecords`, so the filter is applied to CALLS and the count a caller
	 * shows beside its rows is a count of calls.
	 */
	static TArray<FTurboMCPCallLog::FEntry> Select(const TArray<FTurboMCPCallLog::FEntry>& InRecords,
		const FFilter& InFilter);

	/**
	 * One entry in the panel's list: a session HEADING, or a call.
	 *
	 * *** THE GROUPING IS A ROW RATHER THAN A COLUMN, WHICH IS WHAT `T02` ASKS FOR. *** "The calls of one
	 * batch script are grouped" is not satisfied by printing the session id on every row - that is a
	 * column, and it makes a reader compare twenty strings to find where one script ended. A heading
	 * appears once, when the session changes, exactly as the text timeline prints `; session <id>`.
	 */
	struct FListEntry
	{
		/** True on a heading row, which carries no call. */
		bool bHeading = false;

		/** The session the heading names, or the session the call belongs to. */
		FString SessionId;

		/** The call. Default on a heading row. */
		FTurboMCPCallLog::FEntry Call;
	};

	/**
	 * The rows a list should show: the filtered calls, with a heading before each new session.
	 *
	 * The heading is emitted only for the sessions the FILTER kept, so narrowing to one toolset does not
	 * leave a heading over an empty group.
	 */
	static TArray<FListEntry> ListEntries(const TArray<FTurboMCPCallLog::FEntry>& InRecords,
		const FFilter& InFilter);

	/**
	 * The expanded row: the FULL arguments and the first lines of the result.
	 *
	 * *** BOUNDED, PER `P6`, AND IT SAYS WHAT IT DROPPED. *** The unexpanded row shortens the arguments
	 * to 60 characters, which is enough to recognise a call and not enough to check one; this is what
	 * a reader opens the row for. The result is cut at a line boundary and the count of what is not
	 * shown is printed, so an expansion cannot read as a complete answer that happens to be short.
	 */
	static FString Expand(const FTurboMCPCallLog::FEntry& InCall, int32 InResultLines = 8);
};
