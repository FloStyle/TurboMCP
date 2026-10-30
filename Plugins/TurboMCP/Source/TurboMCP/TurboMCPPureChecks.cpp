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

// Fold display values only; normalization and extraction use the original inputs.
namespace
{
	
	FString FoldedToLines(const FString& InText)
	{
		FString Out = InText;
		Out.ReplaceInline(TEXT("\r"), TEXT(" "));
		Out.ReplaceInline(TEXT("\n"), TEXT(" "));
		Out.ReplaceInline(TEXT("\t"), TEXT(" "));
		return Out;
	}

	
	FString ShownOrEmpty(const FString& InValue, const TCHAR* InWhenEmpty)
	{
		return InValue.IsEmpty() ? FString(InWhenEmpty) : InValue;
	}
}

FString FTurboMCPPureChecks::Report(const FString& InToolsets, const FString& InArguments)
{
	FString Out = TEXT("read: call-cost string diagnostics for literal inputs");
	Out += TEXT("\nThis does not invoke tools, resolve assets or read the call log. Tool-call recording may record this invocation.");
	Out += TEXT("\nEchoed line breaks and tabs are shown as spaces; extraction uses the original input.");
	Out += TEXT("\nAn extracted key is a heuristic string, not proof that an asset exists or that the input is valid JSON.");
	Out += TEXT("\nfunctions | FTurboMCPCallCost::NormaliseToolset, FTurboMCPCallCost::AssetFromArguments");

	TArray<FString> Pieces;
	InToolsets.ParseIntoArray(Pieces, TEXT(","), true);

	TArray<FString> Spellings;
	for (const FString& Piece : Pieces)
	{
		TArray<FString> Sub;
		Piece.ParseIntoArray(Sub, TEXT("|"), true);
		for (const FString& One : Sub)
		{
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

		const FString KeyShown = ShownOrEmpty(FoldedToLines(Key), TEXT("(empty)"));
		Out += FString::Printf(TEXT("\n  in | %s"), *FoldedToLines(Spelling));
		Out += FString::Printf(TEXT("\n  key | %s"), *KeyShown);
	}

	if (Spellings.Num() == 0)
	{
		Out += TEXT("\n  No toolset spelling was supplied; no normalization was performed.");
		Out += TEXT("\n  The count below describes the empty input, not a normalization result.");
	}

	Out += FString::Printf(TEXT("\ntoolset distinct keys | %d"), DistinctKeys.Num());

	const FString ArgumentsShown = ShownOrEmpty(FoldedToLines(InArguments), TEXT("(none)"));
	Out += FString::Printf(TEXT("\narguments given | %s"), *ArgumentsShown);
	Out += FString::Printf(TEXT("\narguments length | %d"), InArguments.Len());

	const FString Asset = FTurboMCPCallCost::AssetFromArguments(InArguments);
	const FString AssetShown = ShownOrEmpty(FoldedToLines(Asset), TEXT("(empty)"));
	const FString NamesOne = Asset.IsEmpty() ? FString(TEXT("no")) : FString(TEXT("yes"));

	Out += FString::Printf(TEXT("\nasset key | %s"), *AssetShown);
	Out += FString::Printf(TEXT("\nasset length | %d"), Asset.Len());
	Out += FString::Printf(TEXT("\nasset names one | %s"), *NamesOne);

	return Out;
}
