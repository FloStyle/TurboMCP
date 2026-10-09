// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;

/**
 * The JSON output of `R-BP-LOAD`.
 *
 * WHY THIS IS A SECOND WRITER AND NOT A SECOND READER
 * ---------------------------------------------------
 * `04_FORMATS.md` warns that most bugs in this project came from two tools disagreeing
 * about one format. The rule that avoids it is one collection pass with two renderings,
 * never two collections.
 *
 * So this writer walks the Blueprint with the same accessors `FTurboMCPBlueprintRead`
 * uses, and `Tools/test-json.py` checks that both renderings carry the same facts. The
 * check is the guarantee: the two cannot silently drift, because a drift fails a test.
 *
 * GRAPH BODIES STAY STRINGS
 * -------------------------
 * `04_FORMATS.md` section 2 defines the graph text format as S-expressions, one top-level
 * form per entry point, with comments. Reshaping that into JSON objects would create a
 * second node model for the same nodes, which is exactly the disagreement the format
 * rules exist to prevent. A graph body is therefore carried as a **string** under its
 * graph's `body` key, in the one format that already exists.
 *
 * WHAT IS NOT YET IMPLEMENTED
 * ---------------------------
 * Only `manifest` is written. `signatures`, `bodies` and `full` are REFUSED by name
 * rather than answered with a partial document, because a caller that asked for
 * signatures and got a manifest-shaped document would read fields that are not there and
 * conclude the Blueprint has no functions.
 */
class FTurboMCPBlueprintJson
{
public:
	/**
	 * Writes one Blueprint as a JSON object.
	 *
	 * Returns an empty string and fills OutError when the level is not implemented, or when
	 * the document would not fit InBudgetChars, so a caller can tell "refused, and here is
	 * why" from "empty document".
	 *
	 * THE BUDGET IS A REFUSAL AND NOT A TRUNCATION. The text writer steps a graph's detail
	 * down when the budget is passed, because a text document that stops early is still
	 * readable. A JSON document that stops early is NOT: it does not parse at all, so a
	 * truncated one is worse than useless, because the caller cannot even find out what went
	 * wrong. The document is therefore built in full, measured, and refused with a parseable
	 * error naming what to ask for instead. Zero means no ceiling.
	 */
	static FString Write(const UBlueprint* InBlueprint, int32 InDetailLevel,
		int32 InBudgetChars, FString& OutError);

	/**
	 * Writes a machine-readable error object.
	 *
	 * A caller parsing JSON needs a document it can parse even when the answer is a
	 * refusal. A bare string that starts with "error:" is not JSON, and a parser handed
	 * one fails with a syntax error that says nothing about what went wrong.
	 */
	static FString WriteError(const FString& InMessage);
};
