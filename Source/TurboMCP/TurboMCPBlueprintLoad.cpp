// Copyright FloStyle. All Rights Reserved.
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
 * R-BP-LOAD: read whole Blueprints.
 *
 * `03_TOOLS.md` calls it the main Read tool. It adds only what is specific to
 * reading several assets in one call: resolving the list, validating the inputs
 * before loading anything, and reporting per asset so one bad name does not lose
 * the other answers.
 *
 * Every section is written by `FTurboMCPBlueprintRead`, which uses the same graph
 * text writer `R-GRAPH-LOAD` does.
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

	// JSON output. The accepted format names are validated before anything is loaded, so a
	// typo costs nothing.
	//
	// `json` is implemented for `manifest` only, and every other level is REFUSED BY NAME
	// with a machine-readable error object rather than answered with a partial document. A
	// caller that asked for signatures and got a manifest-shaped document would read
	// fields that are not there and conclude the Blueprint has no functions.
	const bool bJson = WantsJson(Format);
	if (!Format.TrimStartAndEnd().IsEmpty() && !bJson
		&& Format.TrimStartAndEnd().ToLower() != TEXT("read"))
	{
		return FString::Printf(
			TEXT("error: '%s' is not a format. Accepted: read, json."), *Format);
	}

	// The budget. Zero, empty or a non-number means the default, and the default
	// is generous rather than unlimited, so a huge Blueprint cannot flood a caller.
	int32 BudgetChars = DefaultBudgetChars;
	if (!TokenBudget.TrimStartAndEnd().IsEmpty())
	{
		if (!TokenBudget.TrimStartAndEnd().IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a token budget. Pass a whole number of ")
				TEXT("characters, or leave it empty for the default of %d."),
				*TokenBudget, DefaultBudgetChars);
		}
		BudgetChars = FCString::Atoi(*TokenBudget);
		if (BudgetChars <= 0)
		{
			return FString::Printf(
				TEXT("error: a token budget of %d is not usable. Pass a positive ")
				TEXT("number of characters, or leave it empty."),
				BudgetChars);
		}
	}

	FTurboMCPBlueprintRead::FRequest Request;
	Request.Detail = DetailLevel;
	Request.TokenBudgetChars = BudgetChars;
	Request.GraphFilter = FTurboMCPAssetResolver::SplitList(GraphFilter);

	// The counts go at the END, not in the opening line. The first version built
	// the opening line with the counts in it before the loop had run, so it read
	// `read: 1 asset(s): 1 read, 0 refused` followed by the body, which puts the
	// conclusion before the evidence and is wrong on its face.
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

		UBlueprint* Blueprint = Cast<UBlueprint>(
			LoadObject<UObject>(nullptr, *Resolved.ObjectPath));

		if (Blueprint == nullptr)
		{
			// One line per refusal, naming the class, so a caller does not retry
			// the same thing expecting a different answer.
			UObject* Loaded = FindObject<UObject>(nullptr, *Resolved.ObjectPath);
			Errors += FString::Printf(
				TEXT("\nerror: %s is a %s, and only Blueprints can be read by this tool."),
				*Resolved.ObjectPath,
				Loaded != nullptr ? *Loaded->GetClass()->GetName() : TEXT("missing object"));
			++RefusedCount;
			continue;
		}

		if (bJson)
		{
			// The JSON rendering, from the same accessors as the text above, so the two
			// cannot disagree about what the Blueprint contains.
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

		// The widget tree, at `signatures` and above, per `03_TOOLS.md`.
		//
		// A Widget Blueprint is a Blueprint, so this section is additive rather than a
		// separate tool: an agent reading a UI Blueprint wants the tree in the same answer
		// as its graphs, because the tree is what the graphs act on.
		if (DetailLevel == FTurboMCPBlueprintRead::EDetail::Signatures
			|| DetailLevel == FTurboMCPBlueprintRead::EDetail::Bodies
			|| DetailLevel == FTurboMCPBlueprintRead::EDetail::Full)
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
					Result += TEXT("\n\n") + TreeText;
				}
			}
		}

		if (Read.bReduced)
		{
			Result += FString::Printf(
				TEXT("\n; this asset was reduced to detail %s to fit the budget"),
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
		TEXT("read: %d of %d asset(s), detail %s, %d character(s)"),
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
