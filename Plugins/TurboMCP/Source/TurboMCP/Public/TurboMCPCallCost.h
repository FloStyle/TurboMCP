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

#include "TurboMCPCallLog.h"

/**
 * The cost view over the call log, behind `R-CALL-COST`.
 *
 * `R-CALL-LOG` answers "what was called"; this answers "what did it cost", which the spec frames as
 * four questions rather than one: **which calls cost the most TOKENS, which are SLOWEST, which
 * FAILED, and what the totals are PER TOOL and PER ASSET.**
 *
 * WHY THE FIRST TWO ARE KEPT SEPARATE, AND IT IS NOT PEDANTRY. A call that returns a huge payload
 * and a call that blocks the game thread for two seconds are different problems with different
 * fixes, and a single "most expensive" ranking would answer whichever question the author happened
 * to have in mind. So the two are reported as SEPARATE LINES, each naming what it ranked by.
 *
 * WHY IT SHARES THE LOG'S PARSER. The spec requires it in its own words - "it reads the same log
 * `R-CALL-LOG` reads and shares its parser, so the two cannot disagree". It does that BY READING THE
 * SAME OBJECT: `FTurboMCPCallLog::GetEntries()` hands over `FEntry` structs that the log itself
 * parsed, so there is no second JSON reader to drift. A SECOND PARSER IS A SECOND DIALECT.
 *
 * AND WHY AN EMPTY ANSWER NAMES ITS CAUSE. `P1` is "never return nothing", and the spec lists three
 * distinct emptinesses that must not be confused: logging OFF, on with NO ENTRIES, and on with
 * entries that the FILTER did not match. A report that prints zeros for all three would tell an
 * agent its tools are cheap when in fact nothing was recorded.
 */
class FTurboMCPCallCost
{
public:
	/** The three emptinesses, so a caller is never left guessing which one it has. */
	enum class EEmptiness : uint8
	{
		/** There are no entries at all and logging is off, so nothing was recorded. */
		LoggingOff,

		/** Logging is on and the log is genuinely empty: no tool has been called yet. */
		LogOnAndEmpty,

		/** There are entries, but none matched the caller's filter. */
		FilterMatchedNothing,
	};

	/** One aggregate row: a tool, or an asset, with the totals computed for it. */
	struct FTotal
	{
		/** The tool or asset this row is about, already normalised. */
		FString Key;

		/** How many entries were counted into this row. THE DENOMINATOR, and it is never omitted. */
		int32 Calls = 0;

		int32 Errors = 0;

		int32 TotalTokens = 0;

		int32 TotalChars = 0;

		double TotalMs = 0.0;

		/** The slowest single entry in this row, and the time it took. */
		double SlowestMs = 0.0;
		FString SlowestStamp;

		/** The entry with the largest token estimate, and how many. */
		int32 MostTokens = 0;
		FString MostTokensStamp;

		/** True when no entry in this row reported a token estimate above zero. */
		bool bAllTokensZero = true;
	};

	/**
	 * The toolset name reduced to the toolset's own name, so two spellings become one key.
	 *
	 * *** THE RULE IS "LAST DOTTED SEGMENT", BECAUSE THE QUALIFICATION IS NOT A PREFIX. THE TOOL'S OWN
	 * OUTPUT SHOWS THE SHAPE: ***
	 *
	 *     TOTALS PER TOOL (3 row(s), each with its call count)
	 *       TurboMCPEditToolset.TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog | 1 call(s) | ...
	 *
	 * The qualification is not a PREFIX, IT IS A WHOLE PATH APPEARING TWICE. The analytics provider
	 * resolves BOTH the toolset and the tool hashes against a registry of FULLY QUALIFIED names
	 * (`TurboMCPAnalyticsProvider.cpp:55` stores each identifier exactly as given, and the engine's
	 * own list gives paths), so `Toolset` arrives as `TurboMCP.TurboMCPEditToolset` and `Tool` as
	 * `TurboMCP.TurboMCPEditToolset.EditTurboMCPCallLog`. Appending one to the other therefore
	 * produces the doubling, AND NO PREFIX STRIP CAN UNDO IT.
	 *
	 * The LAST segment is stable against BOTH writers, which is what makes one rule enough: this
	 * plugin's `FScopedCall` records the bare `TurboMCPEditToolset`, whose last segment is itself.
	 * **A PREFIX LIST WOULD HAVE TO BE KEPT IN STEP WITH EVERY TOOLSET AND WOULD FAIL SILENTLY WHEN
	 * IT WAS NOT; THE LAST SEGMENT CANNOT DRIFT FROM THE NAME IT IS DERIVED FROM.**
	 *
	 * The engine's `(top-level)` placeholder is returned unchanged, because it is not a toolset name
	 * and inventing one would be worse than reporting the one the engine gave.
	 */
	static FString NormaliseToolset(const FString& InToolset);

	/**
	 * The asset name from an entry's arguments, or empty when it names none.
	 *
	 * `arguments` is free text written by the caller, so this looks for the asset keys the
	 * conventions in `04_FORMATS.md` use rather than pretending to parse a schema. An entry that
	 * names no asset is REPORTED as unattributed rather than dropped, because silently omitting it
	 * would make the per-asset totals look complete when they are not.
	 */
	static FString AssetFromArguments(const FString& InArguments);

	/** True when an asset total can be computed at all: at least one entry names an asset. */
	static bool AnyEntryNamesAnAsset(const TArray<FTurboMCPCallLog::FEntry>& Entries);

	/** The word for an emptiness, so callers do not re-derive the mapping. */
	static FString DescribeEmptiness(EEmptiness InEmptiness);
};
