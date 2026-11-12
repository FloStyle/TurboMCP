// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "BaseWidgetBlueprint.h"
#include "Logging.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPBlueprintJson.h"
#include "TurboMCPBlueprintRead.h"
#include "TurboMCPWidgetTree.h"

/**
 * Read Blueprint context and optionally graph bodies. Validate shared inputs
 * before loading. Per-asset refusals preserve successful sibling results.
 * Text and JSON share accessors but have separate renderers and budget behavior.
 */
namespace
{
	/** How many assets one call may read. */
	constexpr int32 MaxAssetsPerCall = 8;

	/** How many characters one result may hold before the budget logic is told. */
	constexpr int32 DefaultBudgetChars = 60000;

	/** True when the caller asked for JSON rather than graph text. */
	bool WantsJson(const FString& InFormat)
	{
		const FString Lower = InFormat.TrimStartAndEnd().ToLower();
		return Lower == TEXT("json");
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPBlueprintLoad(
	const FString& Assets,
	const FString& Detail,
	const FString& GraphFilter,
	const FString& Format,
	const FString& TokenBudget)
{
	const TArray<FString> Parts = FTurboMCPAssetResolver::SplitList(Assets);
	if (Parts.Num() == 0)
	{
		return TEXT("error: no asset given. Pass one or more Blueprints separated by ")
			TEXT("commas or pipes, for example BP_NodeTest, or ")
			TEXT("/Game/Tests/BP_NodeTest in any path form.");
	}
	if (Parts.Num() > MaxAssetsPerCall)
	{
		return FString::Printf(
			TEXT("error: %d assets given, and one call reads at most %d. Split the list."),
			Parts.Num(), MaxAssetsPerCall);
	}

	// The detail level is validated before anything is loaded, so a typo costs
	// nothing and the error names every accepted value.
	FTurboMCPBlueprintRead::EDetail DetailLevel;
	if (!FTurboMCPBlueprintRead::ParseDetail(Detail, DetailLevel))
	{
		return FString::Printf(
			TEXT("error: '%s' is not a detail level. Accepted: %s."),
			*Detail, *FTurboMCPBlueprintRead::DetailNames());
	}

	// Validate the requested format before loading assets. JSON sections retain the text envelope.
	const bool bJson = WantsJson(Format);
	if (!Format.TrimStartAndEnd().IsEmpty() && !bJson
		&& Format.TrimStartAndEnd().ToLower() != TEXT("read"))
	{
		return FString::Printf(
			TEXT("error: '%s' is not a format. Accepted: read, json."), *Format);
	}

	// TokenBudget is a legacy name for a per-asset character budget. Text and JSON
	// refuse an oversized document; response headings remain outside that budget.
	const FString NumericText = TokenBudget.TrimStartAndEnd();
	int32 BudgetChars = NumericText.IsEmpty() ? DefaultBudgetChars : 0;
	for (TCHAR Digit : NumericText)
	{
		if (Digit < TEXT('0') || Digit > TEXT('9')
			|| BudgetChars > (MAX_int32 - (Digit - TEXT('0'))) / 10)
			return TEXT("error: character budget must be a positive whole number no larger than 2147483647; leave empty for the default.");
		BudgetChars = BudgetChars * 10 + (Digit - TEXT('0'));
	}
	if (BudgetChars == 0)
		return TEXT("error: character budget must be positive; leave empty for the default.");

	FTurboMCPBlueprintRead::FRequest Request;
	Request.Detail = DetailLevel;
	Request.TokenBudgetChars = BudgetChars;
	Request.GraphFilter = FTurboMCPAssetResolver::SplitList(GraphFilter == TEXT("*") ? FString() : GraphFilter);

	// Accumulate per-asset results before formatting the summary counts.
	FString Result;

	int32 ReadCount = 0;
	int32 RefusedCount = 0;
	FString Errors;

	for (const FString& Part : Parts)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(Part, Resolved);
		if (!ResolveError.IsEmpty())
		{
			// The resolver's own message already names what would work, so it is
			// passed through rather than wrapped in a second explanation.
			Errors += TEXT("\n") + ResolveError;
			++RefusedCount;
			continue;
		}

		UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
		if (Loaded == nullptr)
		{
			Errors += FString::Printf(TEXT("\nerror: could not load %s. Check asset availability and the editor log; its asset type could not be verified."), *Resolved.ObjectPath);
			++RefusedCount;
			continue;
		}
		UBlueprint* Blueprint = Cast<UBlueprint>(Loaded);

		if (Blueprint == nullptr)
		{
			// One line per refusal, naming the class, so a caller does not retry
			// the same thing expecting a different answer.
			Errors += FString::Printf(
				TEXT("\nerror: %s is a %s, and only Blueprints can be read by this tool."),
				*Resolved.ObjectPath,
				*Loaded->GetClass()->GetName());
			++RefusedCount;
			continue;
		}

		if (bJson)
		{
			// JSON is a per-asset section within the surrounding text response.
			FString JsonError;
			FTurboMCPBlueprintJson::FRequest JsonRequest;
			JsonRequest.DetailLevel = static_cast<int32>(DetailLevel);
			JsonRequest.GraphFilter = Request.GraphFilter;
			JsonRequest.BudgetChars = BudgetChars;
			const FString Json = FTurboMCPBlueprintJson::Write(
				Blueprint, JsonRequest, JsonError);

			if (!JsonError.IsEmpty())
			{
				// A refusal is still a parseable document, so a caller reading JSON never
				// has to guess whether a syntax error means "no" or "broken".
				Result += TEXT("\n") + FTurboMCPBlueprintJson::WriteError(JsonError);
				++RefusedCount;
				continue;
			}

			Result += FString::Printf(TEXT("\n\n===== %s ====="), *Resolved.ObjectPath);
			Result += TEXT("\n") + Json;
			++ReadCount;
			continue;
		}

		const FTurboMCPBlueprintRead::FResult Read =
			FTurboMCPBlueprintRead::Read(Blueprint, Request);

		Result += FString::Printf(TEXT("\n\n===== %s ====="), *Resolved.ObjectPath);
		Result += TEXT("\n") + Read.Text;
		if (Read.Text.StartsWith(TEXT("error:")))
		{
			++RefusedCount;
			continue;
		}


		// A graph-focused read excludes unrelated widget data. Respect the effective
		// detail of the successful read.
		if (Request.GraphFilter.Num() == 0
			&& Read.DetailUsed != FTurboMCPBlueprintRead::EDetail::Manifest)
		{
			if (UBaseWidgetBlueprint* WidgetBlueprint = Cast<UBaseWidgetBlueprint>(Blueprint))
			{
				FString TreeError;
				const FString TreeText =
					FTurboMCPWidgetTree::Render(WidgetBlueprint, TreeError);

				if (!TreeError.IsEmpty())
				{
					Result += FString::Printf(
						TEXT("\n\n  widget tree: could not be read (%s)"), *TreeError);
				}
				else if (!TreeText.IsEmpty())
				{
					if (static_cast<int64>(Read.Text.Len()) + TreeText.Len() + 2 <= BudgetChars)
						Result += TEXT("\n\n") + TreeText;
					else
						Result += TEXT("\n; widget tree omitted to fit the character budget; raise TokenBudget to include it.");
				}
			}
		}

		if (Read.bReduced)
		{
			Result += FString::Printf(
				TEXT("\n; character-budget handling used detail %s; see budget notes above"),
				Read.DetailUsed == FTurboMCPBlueprintRead::EDetail::Full ? TEXT("full")
					: Read.DetailUsed == FTurboMCPBlueprintRead::EDetail::Bodies ? TEXT("bodies")
					: Read.DetailUsed == FTurboMCPBlueprintRead::EDetail::Signatures ? TEXT("signatures")
					: TEXT("manifest"));
		}

		++ReadCount;
	}

	// The opening line, now that the counts are known. `04_FORMATS.md` section 1
	// wants a status line first, and a Read status says what was read and how big
	// it is. It is PREPENDED to the body built above, not assigned over it.
	const FString Status = FString::Printf(
		TEXT("read: %d of %d asset(s), requested detail %s, body %d character(s)"),
		ReadCount, Parts.Num(),
		DetailLevel == FTurboMCPBlueprintRead::EDetail::Full ? TEXT("full")
			: DetailLevel == FTurboMCPBlueprintRead::EDetail::Bodies ? TEXT("bodies")
			: DetailLevel == FTurboMCPBlueprintRead::EDetail::Signatures ? TEXT("signatures")
			: TEXT("manifest"),
		Result.Len());

	Result = Status + Result;

	Result += Errors;

	if (RefusedCount > 0)
	{
		Result += FString::Printf(TEXT("\n; %d asset(s) were refused"), RefusedCount);
	}

	if (ReadCount == 0)
	{
		// `P1`: a call that produced nothing says why rather than ending on a count.
		Result += TEXT("\nNothing was read. Check the asset names above; each refusal ")
			TEXT("names the form that would work.");
	}

	return Result;
}
