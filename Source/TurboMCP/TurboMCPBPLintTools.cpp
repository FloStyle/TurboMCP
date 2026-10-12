// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"

#include "TurboMCPBPLint.h"

/**
 * `R-BP-LINT`.
 *
 * ONE TOOL AND NO EDIT TWIN, WHICH IS THE POINT: the fix for a finding is an EXISTING tool, named on the
 * finding itself, and this one lives in the reader toolset that is structurally unable to write. The spec
 * says so in its own words - "This is a READ tool and it never edits: the fix for a finding is the existing
 * tool, named in the finding."
 *
 * `checks=list` IS A FIRST-CLASS ANSWER rather than a convenience: `P1` says a filter that matches nothing
 * must say so AND LIST WHAT EXISTS, and a caller has no other way to discover six check names. It answers
 * with the names and nothing else, so an agent that wants to know what it can ask for does not pay for a
 * whole lint.
 */
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
		FString Out = TEXT("R-BP-LINT checks, by name. Pass one or more in `checks`, comma separated;")
			TEXT("\npass `checks` empty to run every one. EVERY CHECK REPORTS HOW MANY THINGS IT LOOKED AT,")
			TEXT("\nso a zero-finding answer can be told apart from a check that examined nothing.\n");

		for (const FTurboMCPBPLint::FCheckName& Name : FTurboMCPBPLint::KnownChecks())
		{
			Out += FString::Printf(TEXT("\n  %s\n      %s"), *Name.Name, *Name.Description);
		}

		Out += TEXT("\n\nThis is a read tool and it never edits.");
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

	// `tokenBudget` of 0 means "no budget", which is how the other readers in this toolset spell it.
	// *** SO ZERO IS A VALUE HERE AND NOT A MISTAKE, WHICH IS WHY THIS SITE REFUSES ONLY WHAT IS NOT A NUMBER
	// AND WHAT IS NEGATIVE. *** Turning `banana` into "no budget" answers a question the caller did not ask;
	// the refusal names the parameter, states the alternative, and uses the shape of
	// `TurboMCPBlueprintLoad.cpp:100-114`, which does this for the same parameter name.
	//
	// `IsNumeric()` is the gate that catches a WORD; the sign test below is the gate that catches a SIGN.
	// WHICH OF THE TWO REFUSES `-5` DEPENDS ON `IsNumeric`'S OWN RULES RATHER THAN ON THIS FILE'S, so both are
	// present and either refusal is correct.
	int32 Budget = 0;
	if (!TokenBudget.TrimStartAndEnd().IsEmpty())
	{
		if (!TokenBudget.TrimStartAndEnd().IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a token budget. Pass a whole number of characters, or 0 for NO ")
				TEXT("BUDGET."), *TokenBudget);
		}
		Budget = FCString::Atoi(*TokenBudget);
		if (Budget < 0)
		{
			return FString::Printf(
				TEXT("error: a token budget of %d is not usable. Pass a whole number of characters, or 0 for ")
				TEXT("NO BUDGET."), Budget);
		}
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

	return Out;
}
