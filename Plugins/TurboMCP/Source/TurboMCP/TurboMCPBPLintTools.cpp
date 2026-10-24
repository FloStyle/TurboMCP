// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPBPLint.h"

// Discover check names without loading assets, or report each requested Blueprint separately.
FString UTurboMCPReadToolset::ReadTurboMCPBPLint(
	const FString& Assets,
	const FString& Checks,
	const FString& TokenBudget)
{
	const FString Filter = Checks.TrimStartAndEnd();
	const FString AssetList = Assets.TrimStartAndEnd();

	// `list` BEFORE ANY ASSET IS REQUIRED, deliberately: a caller asking what the checks ARE should not have
	// to name an asset to find out, and requiring one would make the discovery path cost a load.
	if (Filter.Equals(TEXT("list"), ESearchCase::IgnoreCase))
	{
		FString Out = TEXT("Blueprint lint checks. Pass one or more in `checks`, comma separated;")
			TEXT("\npass `checks` empty to run every one. Each check reports how many items it examined,")
			TEXT("\nso a zero-finding answer can be told apart from a check that examined nothing.\n");

		for (const FTurboMCPBPLint::FCheckName& Name : FTurboMCPBPLint::KnownChecks())
		{
			Out += FString::Printf(TEXT("\n  %s\n      %s"), *Name.Name, *Name.Description);
		}

		Out += TEXT("\n\nThis listing does not load or edit assets.");
		return Out;
	}

	if (AssetList.IsEmpty())
	{
		FString Names;
		for (const FTurboMCPBPLint::FCheckName& Name : FTurboMCPBPLint::KnownChecks())
		{
			Names += Names.IsEmpty() ? Name.Name : FString::Printf(TEXT(", %s"), *Name.Name);
		}
		return FString::Printf(
			TEXT("error: no asset given. Pass one or more Blueprints in any path form, for example BP_NodeTest.")
			TEXT("\nThe checks are: %s.")
			TEXT("\nPass `checks=list` for what each one means."), *Names);
	}

	// Parse an integer budget without truncating fractions or overflowing.
	int32 Budget = 0;
	for (TCHAR Digit : TokenBudget.TrimStartAndEnd())
	{
		if (Digit < TEXT('0') || Digit > TEXT('9')
			|| Budget > (MAX_int32 - (Digit - TEXT('0'))) / 10)
			return TEXT("error: TokenBudget must be whole-number digits from 0 to 2147483647. Empty or 0 disables the finding-text budget; the unit is estimated tokens.");
		Budget = Budget * 10 + (Digit - TEXT('0'));
	}

	// SEVERAL ASSETS, EACH LINTED SEPARATELY, because a finding is about one asset and merging them would
	// lose which. A resolver refusal is REPORTED FOR THE ASSET IT HAPPENED TO rather than failing the call -
	// the same rule `T-QUAL-3` states for a batch, applied here for the same reason: five assets where one
	// is unresolvable should answer for four.
	TArray<FString> Requested;
	AssetList.ParseIntoArray(Requested, TEXT(","), /*bCullEmpty=*/true);
	if (Requested.Num() == 0)
	{
		Requested.Add(AssetList);
	}

	FString Out;
	int32 Linted = 0;
	TArray<FString> Refused;

	for (FString& One : Requested)
	{
		const FString Name = One.TrimStartAndEnd();
		if (Name.IsEmpty()) { continue; }

		FTurboMCPBPLint::FReport Report;
		FString Error;
		if (!FTurboMCPBPLint::Lint(Name, Filter, Report, Error))
		{
			// REFUSED BY NAME, AND THE REFUSAL IS KEPT rather than turning the whole call into an error:
			// an agent that asked about five Blueprints and got one wrong should still learn about the other
			// four, and `P1` says the empty answer states its reason.
			Refused.Add(FString::Printf(TEXT("%s: %s"), *Name, *Error));
			continue;
		}

		if (!Out.IsEmpty()) { Out += TEXT("\n\n---\n"); }
		Out += FTurboMCPBPLint::Render(Report, Budget);
		++Linted;
	}

	if (Refused.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n\n%d asset(s) could not be linted:"), Refused.Num());
		for (const FString& Line : Refused)
		{
			Out += FString::Printf(TEXT("\n  %s"), *Line);
		}
	}

	if (Linted == 0)
	{
		// NEVER RETURN NOTHING WITHOUT SAYING WHY (`P1`).
		return FString::Printf(
			TEXT("error: no asset could be linted.\n%s"), *Out.TrimStartAndEnd());
	}

	return Refused.Num() > 0 ? TEXT("partial: Some requested assets could not be linted.\n") + Out : Out;
}
