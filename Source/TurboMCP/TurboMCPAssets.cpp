// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Misc/PackageName.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "Logging.h"
#include "TurboMCPAssetResolver.h"

/**
 * Tools that find and describe assets.
 *
 * `R-ASSET-SEARCH` and `R-ASSET-DESCRIBE` live here together because they share
 * one question: which asset is this. Splitting them would mean two places that
 * decide what a short name means, which is how `04_FORMATS.md` says most bugs
 * appeared.
 */
namespace
{
	/**
	 * How many results one search may print.
	 *
	 * `P6` caps every list so a result cannot flood an agent's context. A query
	 * for a common word can match thousands of assets, and an agent needs to
	 * recognise the right one, not read the project.
	 */
	constexpr int32 MaxSearchResults = 40;

	/**
	 * How many assets one describe call may report.
	 *
	 * Smaller than the search cap because each line carries more, and because a
	 * caller naming assets one by one rarely needs many at once.
	 */
	constexpr int32 MaxDescribeAssets = 25;

	/** Characters a query may be split on, besides spaces. */
	const TCHAR* QuerySeparators = TEXT(" \t");

	/** A search filter prefix, so `type:Material` and `path:/Game/UI` work. */
	const TCHAR* TypeFilterPrefix = TEXT("type:");
	const TCHAR* PathFilterPrefix = TEXT("path:");

	/** One matched asset and why it matched, which decides its rank. */
	struct FSearchHit
	{
		FAssetData Asset;

		/** 0 exact name, 1 name starts with, 2 name contains, 3 folder only. */
		int32 Rank = 3;

		/** Shorter names sort first on a tie, per the spec's ranking rule. */
		int32 NameLength = 0;
	};

	/** The words and filters one query is made of. */
	struct FParsedQuery
	{
		TArray<FString> Words;
		FString TypeFilter;
		FString PathFilter;
	};

	/** Splits a query into words and pulls out the two filters. */
	FParsedQuery ParseQuery(const FString& InQuery)
	{
		FParsedQuery Parsed;

		TArray<FString> Tokens;
		InQuery.ParseIntoArray(Tokens, QuerySeparators, /*InCullEmpty=*/true);

		for (const FString& Token : Tokens)
		{
			if (Token.StartsWith(TypeFilterPrefix, ESearchCase::IgnoreCase))
			{
				// Filters may appear at any position, and a later one wins so a
				// caller can correct itself within one query.
				Parsed.TypeFilter = Token.RightChop(FCString::Strlen(TypeFilterPrefix));
			}
			else if (Token.StartsWith(PathFilterPrefix, ESearchCase::IgnoreCase))
			{
				Parsed.PathFilter = Token.RightChop(FCString::Strlen(PathFilterPrefix));
			}
			else
			{
				Parsed.Words.Add(Token);
			}
		}

		return Parsed;
	}

	/** True when every word appears in the name or in the folder path. */
	bool MatchesAllWords(const FAssetData& InAsset, const TArray<FString>& InWords)
	{
		const FString Name = InAsset.AssetName.ToString();
		const FString Path = InAsset.PackagePath.ToString();

		for (const FString& Word : InWords)
		{
			if (!Name.Contains(Word, ESearchCase::IgnoreCase)
				&& !Path.Contains(Word, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return true;
	}

	/** Rank of the name match alone, ignoring the folder. */
	int32 NameRank(const FString& InName, const TArray<FString>& InWords)
	{
		if (InWords.Num() == 0)
		{
			return 3;
		}

		// Join the words to compare a multi word query as one run of text, which
		// is how a name like `WBP_ShopCard` answers the query `shop card`.
		FString Joined = FString::Join(InWords, TEXT(""));
		if (InName.Equals(Joined, ESearchCase::IgnoreCase))
		{
			return 0;
		}
		if (InName.StartsWith(Joined, ESearchCase::IgnoreCase))
		{
			return 1;
		}

		// Separately, every word must appear somewhere in the name for a
		// name-level match; otherwise the match came from the folder.
		bool bAllInName = true;
		for (const FString& Word : InWords)
		{
			if (!InName.Contains(Word, ESearchCase::IgnoreCase))
			{
				bAllInName = false;
				break;
			}
		}
		return bAllInName ? 2 : 3;
	}

	/** The class name alone, without the `/Script/...` prefix. */
	FString ClassNameOf(const FAssetData& InAsset)
	{
		const FString Full = InAsset.AssetClassPath.ToString();
		int32 LastDot = INDEX_NONE;
		if (Full.FindLastChar(TEXT('.'), LastDot))
		{
			return Full.Mid(LastDot + 1);
		}
		return Full;
	}

	/**
	 * The parent class of a Blueprint asset, or an empty string.
	 *
	 * Only asked for a Blueprint, because for anything else the parent is not a
	 * meaningful thing to print. The asset is loaded only when it is not already
	 * in memory, and a failure to load is reported as "parent unknown" rather
	 * than as no parent, because those are different facts (`P1`, `D-05`).
	 */
	FString BlueprintParentOf(const FAssetData& InAsset)
	{
		const FString ClassName = ClassNameOf(InAsset);
		if (!ClassName.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase))
		{
			return FString();
		}

		const FString ObjectPath = InAsset.GetObjectPathString();
		UObject* Asset = FindObject<UObject>(nullptr, *ObjectPath);
		if (Asset == nullptr)
		{
			Asset = LoadObject<UObject>(nullptr, *ObjectPath);
		}

		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (Blueprint == nullptr)
		{
			return Asset == nullptr ? TEXT("parent unknown (the asset could not be loaded)") : FString();
		}

		if (const UClass* Parent = Blueprint->ParentClass)
		{
			return Parent->GetName();
		}
		return TEXT("none");
	}

	/** One line describing an asset, in the form `R-ASSET-DESCRIBE` requires. */
	FString DescribeAssetLine(const FTurboMCPAssetResolver::FResolved& InResolved)
	{
		IAssetRegistry* Registry = IAssetRegistry::Get();
		if (Registry == nullptr)
		{
			return FString::Printf(TEXT("%s  (registry unavailable, so nothing further could be read)"),
				*InResolved.ObjectPath);
		}

		const FAssetData Asset = Registry->GetAssetByObjectPath(
			FSoftObjectPath(InResolved.ObjectPath), /*bIncludeOnlyOnDiskAssets=*/false);

		if (!Asset.IsValid())
		{
			return FString::Printf(
				TEXT("%s  class unknown (the asset registry has no entry for it)"), *InResolved.ObjectPath);
		}

		// Loaded or not is a real distinction: an asset in memory but never saved
		// is exactly what `P5` requires every tool to find, and saying so is how
		// a caller learns the asset is not on disk.
		const bool bLoaded = FindObject<UObject>(nullptr, *InResolved.ObjectPath) != nullptr;

		// The class name comes from the resolver, which fills it on both routes,
		// so every path form prints the same class.
		FString ClassShort = InResolved.AssetClass;
		int32 LastDot = INDEX_NONE;
		if (ClassShort.FindLastChar(TEXT('.'), LastDot))
		{
			ClassShort = ClassShort.Mid(LastDot + 1);
		}

		FString Line = FString::Printf(TEXT("package: %s\n  object: %s\n  class: %s"),
			*InResolved.PackagePath, *InResolved.ObjectPath,
			ClassShort.IsEmpty() ? TEXT("unknown") : *ClassShort);

		// The generated class is a separate, real thing that the engine's own
		// tools ask for by name, so it is printed only when it exists. Saying
		// "none" is a positive statement that the check ran (`P1`).
		const FString GeneratedClassPath = FString::Printf(
			TEXT("%s.%s_C"), *InResolved.PackagePath, *InResolved.AssetName);
		const FAssetData ClassAsset = Registry->GetAssetByObjectPath(
			FSoftObjectPath(GeneratedClassPath), /*bIncludeOnlyOnDiskAssets=*/false);

		Line += ClassAsset.IsValid()
			? FString::Printf(TEXT("\n  generated class: %s"), *ClassAsset.GetObjectPathString())
			: TEXT("\n  generated class: none, this asset kind has no generated class");

		const FString Parent = BlueprintParentOf(Asset);
		if (!Parent.IsEmpty())
		{
			Line += FString::Printf(TEXT("\n  parent: %s"), *Parent);
		}

		Line += FString::Printf(TEXT("\n  loaded: %s"), bLoaded ? TEXT("yes") : TEXT("no"));

		// The unsaved flag is only knowable for an asset that is loaded, so an
		// unloaded asset says so rather than claiming it is saved.
		if (bLoaded)
		{
			const UPackage* Package = FindPackage(nullptr, *InResolved.PackagePath);
			const bool bDirty = Package != nullptr && Package->IsDirty();
			Line += FString::Printf(TEXT("\n  unsaved changes: %s"), bDirty ? TEXT("yes") : TEXT("no"));
			Line += FString::Printf(TEXT("\n  on disk: %s"),
				Package != nullptr && Package->IsDirty() ? TEXT("not fully, it has unsaved changes") : TEXT("yes"));
		}
		else
		{
			Line += TEXT("\n  unsaved changes: unknown, the asset is not loaded");
			Line += TEXT("\n  on disk: yes (it was found without loading)");
		}

		return Line;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPAssetSearch(const FString& Query)
{
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return TEXT("error: the asset registry is not available, so no asset could be searched for.");
	}

	const FParsedQuery Parsed = ParseQuery(Query);

	// P1: a query that is empty says what to pass, rather than listing everything.
	if (Parsed.Words.Num() == 0 && Parsed.TypeFilter.IsEmpty() && Parsed.PathFilter.IsEmpty())
	{
		return TEXT("error: no query given. Pass words to match against asset names and folders, ")
			TEXT("or a filter on its own with type:<class words> or path:<folder words>. ")
			TEXT("Example: type:Material shop");
	}

	// GetAllAssets with include-only-on-disk false is the one query that also
	// sees assets which are in memory but never saved, which P5 requires.
	TArray<FAssetData> AllAssets;
	Registry->GetAllAssets(AllAssets, /*bIncludeOnlyOnDiskAssets=*/false);

	TArray<FSearchHit> Hits;
	for (const FAssetData& Asset : AllAssets)
	{
		if (!Asset.IsValid())
		{
			continue;
		}

		const FString ClassName = ClassNameOf(Asset);
		const FString Path = Asset.PackagePath.ToString();
		const FString Name = Asset.AssetName.ToString();

		if (!Parsed.TypeFilter.IsEmpty()
			&& !ClassName.Contains(Parsed.TypeFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!Parsed.PathFilter.IsEmpty()
			&& !Path.Contains(Parsed.PathFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!MatchesAllWords(Asset, Parsed.Words))
		{
			continue;
		}

		FSearchHit Hit;
		Hit.Asset = Asset;
		Hit.Rank = NameRank(Name, Parsed.Words);
		Hit.NameLength = Name.Len();
		Hits.Add(MoveTemp(Hit));
	}

	// Ranking is the spec's: exact name, then name starts with, then name
	// contains, then folder only; shorter names first on a tie. The name is a
	// final tie-break so the order is stable between calls rather than depending
	// on registry order, which would make two identical queries disagree.
	Hits.Sort([](const FSearchHit& A, const FSearchHit& B)
	{
		if (A.Rank != B.Rank)
		{
			return A.Rank < B.Rank;
		}
		if (A.NameLength != B.NameLength)
		{
			return A.NameLength < B.NameLength;
		}
		return A.Asset.AssetName.LexicalLess(B.Asset.AssetName);
	});

	FString Result = FString::Printf(TEXT("read: asset search for '%s', %d match(es)"),
		*Query, Hits.Num());

	if (Parsed.TypeFilter.IsEmpty() && Parsed.PathFilter.IsEmpty())
	{
		Result += TEXT(" (no filters)");
	}
	else
	{
		Result += FString::Printf(TEXT(" (type: %s, path: %s)"),
			Parsed.TypeFilter.IsEmpty() ? TEXT("any") : *Parsed.TypeFilter,
			Parsed.PathFilter.IsEmpty() ? TEXT("any") : *Parsed.PathFilter);
	}

	if (Hits.Num() == 0)
	{
		// P1: say it matched nothing and how many assets were searched, so the
		// reader can tell "no match" from "nothing to search".
		Result += FString::Printf(
			TEXT("\nNothing matched. %d assets were searched, including those in memory only."),
			AllAssets.Num());
		Result += TEXT("\nTry fewer words, or a filter on its own such as type:Material.");
		return Result;
	}

	const int32 Shown = FMath::Min(Hits.Num(), MaxSearchResults);

	// Print how many of the matches are listed, so a capped result is never
	// mistaken for the whole answer.
	if (Hits.Num() > Shown)
	{
		Result += FString::Printf(TEXT(", showing the first %d"), Shown);
	}

	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FSearchHit& Hit = Hits[Index];
		const FString ClassName = ClassNameOf(Hit.Asset);

		Result += FString::Printf(TEXT("\n%s  [%s]"), *Hit.Asset.GetObjectPathString(), *ClassName);

		// The parent is only meaningful for a Blueprint, so it is only printed
		// for one, and only when it could be read.
		const FString Parent = BlueprintParentOf(Hit.Asset);
		if (!Parent.IsEmpty())
		{
			Result += FString::Printf(TEXT(" parent: %s"), *Parent);
		}
	}

	if (Hits.Num() > Shown)
	{
		// P6: say how many were left out and the exact call that gets them.
		Result += FString::Printf(
			TEXT("\n(%d more, narrow the query or add type: or path: to see them)"),
			Hits.Num() - Shown);
	}

	return Result;
}

FString UTurboMCPReadToolset::ReadTurboMCPAssetDescribe(const FString& Assets)
{
	// The comma and pipe list form is accepted here as everywhere else (D-04).
	const TArray<FString> Parts = FTurboMCPAssetResolver::SplitList(Assets);

	if (Parts.Num() == 0)
	{
		return TEXT("error: no asset given. Pass one or more assets separated by commas or pipes. ")
			TEXT("Every path form is accepted: a short name, a package path, an object path, ")
			TEXT("or a generated class path.");
	}

	// The summary must count what was actually described, not what was asked
	// for, or a failed call still reads as a success (`P1`).
	FString Result = FString::Printf(TEXT("read: describing %d asset(s)"), Parts.Num());

	int32 Described = 0;
	int32 OverCap = 0;
	int32 Failed = 0;
	FString Lines;
	FString Errors;

	for (const FString& Part : Parts)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString Error = FTurboMCPAssetResolver::Resolve(Part, Resolved);

		if (!Error.IsEmpty())
		{
			Errors += TEXT("\n") + Error;
			++Failed;
			continue;
		}

		if (Described >= MaxDescribeAssets)
		{
			// P6: say what was left out rather than dropping it silently.
			OverCap += 1;
			continue;
		}

		Lines += TEXT("\n") + DescribeAssetLine(Resolved);
		++Described;
	}

	// Report the real outcome: N described, N refused. An error is never hidden
	// behind a count that only mentions the successes.
	if (Failed == 0 && OverCap == 0)
	{
		Result += FString::Printf(TEXT(", all %d described"), Described);
	}
	else
	{
		Result += FString::Printf(TEXT(", %d described, %d refused"), Described, Failed);
		if (OverCap > 0)
		{
			Result += FString::Printf(TEXT(", %d left out by the %d asset cap"), OverCap, MaxDescribeAssets);
		}
	}

	Result += Lines;
	Result += Errors;

	return Result;
}
