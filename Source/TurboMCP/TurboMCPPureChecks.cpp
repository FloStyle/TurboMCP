// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPPureChecks.h"

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PlatformString.h"

#include "TurboMCPCallCost.h"

namespace
{
	/**
	 * The text with line breaks and tabs folded to spaces, so ONE value stays on ONE line.
	 *
	 * THE ECHO IS FOLDED; THE MEASUREMENT IS NOT. `Report` echoes the arguments a caller passed, and
	 * `arguments` is free text written by a caller in the real log - so it can contain anything,
	 * including the line breaks that would cut the report's line shape in half. The string handed to
	 * `AssetFromArguments` is the caller's ORIGINAL string, so the answer this tool reports is the
	 * answer the real function gives; only what is printed back for a human to read is folded, and
	 * the caller is told so in the answer's own header.
	 */
	FString FoldedToLines(const FString& InText)
	{
		FString Out = InText;
		Out.ReplaceInline(TEXT("\r"), TEXT(" "));
		Out.ReplaceInline(TEXT("\n"), TEXT(" "));
		Out.ReplaceInline(TEXT("\t"), TEXT(" "));
		return Out;
	}

	/**
	 * A string that could be empty, written so an empty one is VISIBLE rather than absent.
	 *
	 * A line that ends after its separator is indistinguishable from a line that failed to print,
	 * and this project has recorded that confusion as its most expensive one. `(empty)` is a
	 * statement about the value; the length is printed beside it as a NUMBER, so a caller that does
	 * not want to trust the word does not have to.
	 */
	FString ShownOrEmpty(const FString& InValue, const TCHAR* InWhenEmpty)
	{
		return InValue.IsEmpty() ? FString(InWhenEmpty) : InValue;
	}
}

FString FTurboMCPPureChecks::Report(const FString& InToolsets, const FString& InArguments)
{
	FString Out = TEXT("read: the two PURE functions behind `R-CALL-COST`, called with literal inputs");
	Out += TEXT("\n; this tool computes strings only: it reads no log, loads no asset and writes nothing,");
	Out += TEXT("\n; so it needs no fixture, cannot race another suite, and has nothing to restore.");
	Out += TEXT("\n; the arguments echoed below have their line breaks written as spaces; the value the");
	Out += TEXT("\n; function was GIVEN is the caller's own string.");
	Out += TEXT("\nfunctions | FTurboMCPCallCost::NormaliseToolset, FTurboMCPCallCost::AssetFromArguments");

	// ---------------------------------------------------------------------------------------------
	// THE TOOLSET SPELLINGS. Commas and pipes both separate, because the project's list convention
	// admits either and a caller who uses the other one should not get a silent single spelling.
	// ---------------------------------------------------------------------------------------------
	TArray<FString> Pieces;
	InToolsets.ParseIntoArray(Pieces, TEXT(","), true);

	TArray<FString> Spellings;
	for (const FString& Piece : Pieces)
	{
		TArray<FString> Sub;
		Piece.ParseIntoArray(Sub, TEXT("|"), true);
		for (const FString& One : Sub)
		{
			// Trimmed around the separators only. `NormaliseToolset` trims its own argument already,
			// so this changes no key; it keeps the ECHO free of the whitespace a caller typed after
			// a comma, which would otherwise land in the middle of a value a reader parses.
			const FString Trimmed = One.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Spellings.Add(Trimmed);
			}
		}
	}

	Out += FString::Printf(TEXT("\ntoolset spelling(s) given | %d"), Spellings.Num());

	TSet<FString> DistinctKeys;
	for (const FString& Spelling : Spellings)
	{
		const FString Key = FTurboMCPCallCost::NormaliseToolset(Spelling);
		DistinctKeys.Add(Key);

		const FString KeyShown = ShownOrEmpty(Key, TEXT("(empty)"));
		Out += FString::Printf(TEXT("\n  in | %s"), *Spelling);
		Out += FString::Printf(TEXT("\n  key | %s"), *KeyShown);
	}

	if (Spellings.Num() == 0)
	{
		// `P1`: NO SPELLING IS NOT ONE KEY. A report that printed only `toolset distinct keys | 0`
		// would read as a normaliser that collapsed everything, which is the opposite of what an
		// empty input says. The empty input is the one shape this tool cannot reach through its
		// list argument, so it is named here rather than left to the count.
		Out += TEXT("\n  NO TOOLSET SPELLING WAS GIVEN, so no key was computed at all.");
		Out += TEXT("\n  THAT IS AN ABSENT MEASUREMENT, NOT ONE KEY AND NOT ZERO KEYS.");
	}

	// THE COUNT IS THE ANSWER TO "DO THE TWO SPELLINGS COLLAPSE": a caller reads a NUMBER rather
	// than deciding whether a sentence sounds like agreement.
	Out += FString::Printf(TEXT("\ntoolset distinct keys | %d"), DistinctKeys.Num());

	// ---------------------------------------------------------------------------------------------
	// THE ASSET EXTRACTION, which is the pure function that fails SILENTLY when it is wrong: an
	// asset name that comes back empty makes the per-asset totals quietly short, with no error
	// anywhere. So the answer and its length are both printed, and the length is a NUMBER.
	// ---------------------------------------------------------------------------------------------
	const FString ArgumentsShown = ShownOrEmpty(FoldedToLines(InArguments), TEXT("(none)"));
	Out += FString::Printf(TEXT("\narguments given | %s"), *ArgumentsShown);
	Out += FString::Printf(TEXT("\narguments length | %d"), InArguments.Len());

	const FString Asset = FTurboMCPCallCost::AssetFromArguments(InArguments);
	const FString AssetShown = ShownOrEmpty(Asset, TEXT("(empty)"));
	const FString NamesOne = Asset.IsEmpty() ? FString(TEXT("no")) : FString(TEXT("yes"));

	Out += FString::Printf(TEXT("\nasset key | %s"), *AssetShown);
	Out += FString::Printf(TEXT("\nasset length | %d"), Asset.Len());
	Out += FString::Printf(TEXT("\nasset names one | %s"), *NamesOne);

	return Out;
}
