// Copyright (c) 2026 FloStyle
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
		FString Error;
	};

	/** Splits a query into words and pulls out the two filters. */
	FParsedQuery ParseQuery(const FString& InQuery)
	{
		FParsedQuery Parsed;

		TArray<FString> Tokens;
		InQuery.ParseIntoArrayWS(Tokens);

		for (const FString& Token : Tokens)
		{
			if (Token.StartsWith(TypeFilterPrefix, ESearchCase::IgnoreCase) || Token.StartsWith(TEXT("class:"), ESearchCase::IgnoreCase))
			{
				// Filters may appear at any position, and a later one wins so a
				// caller can correct itself within one query.
				Parsed.TypeFilter = Token.RightChop(Token.StartsWith(TEXT("class:"), ESearchCase::IgnoreCase)?6:FCString::Strlen(TypeFilterPrefix));
				if (Parsed.TypeFilter.IsEmpty()) { Parsed.Error = TEXT("error: type: needs a class name, for example type:Blueprint."); return Parsed; }
			}
			else if (Token.StartsWith(PathFilterPrefix, ESearchCase::IgnoreCase))
			{
				Parsed.PathFilter = Token.RightChop(FCString::Strlen(PathFilterPrefix));
				if (Parsed.PathFilter.IsEmpty()) { Parsed.Error = TEXT("error: path: needs text to match in the folder path, for example path:UI."); return Parsed; }
			}
			else if (Token.Contains(TEXT(":")))
			{
				Parsed.Error = FString::Printf(TEXT("error: unsupported query filter '%s'. Use type:ClassName or path:FolderText. The class: filter is an alias for type:. Filters may appear before or after search words."), *Token);
				return Parsed;
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

	/** Read an existing Blueprint or its saved registry parent tag without loading it. */
	FString BlueprintParentOf(const FAssetData& InAsset)
	{
		if (const UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *InAsset.GetObjectPathString()))
		{
			return Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT("none (loaded Blueprint has no parent)");
		}
		FString ParentTag;
		if (InAsset.GetTagValue(FName(TEXT("ParentClass")), ParentTag) && !ParentTag.IsEmpty())
			return FPackageName::ExportTextPathToObjectPath(ParentTag) + TEXT(" (asset registry metadata)");
		return ClassNameOf(InAsset).Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase)
			? TEXT("unknown (not loaded; no parent metadata)") : FString();
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

		// Generated classes are commonly registry tags, not separate asset entries.
		// Missing metadata does not establish that the asset cannot generate a class.
		const UBlueprint* LoadedBlueprint = FindObject<UBlueprint>(nullptr, *InResolved.ObjectPath);
		FString GeneratedClassTag;
		if (LoadedBlueprint)
		{
			Line += LoadedBlueprint->GeneratedClass
				? FString::Printf(TEXT("\n  generated class: %s"), *LoadedBlueprint->GeneratedClass->GetPathName())
				: TEXT("\n  generated class: none currently assigned to the loaded Blueprint");
		}
		else if (Asset.GetTagValue(FName(TEXT("GeneratedClass")), GeneratedClassTag) && !GeneratedClassTag.IsEmpty())
		{
			Line += FString::Printf(TEXT("\n  generated class: %s (asset registry metadata; not loaded or validated)"),
				*FPackageName::ExportTextPathToObjectPath(GeneratedClassTag));
		}
		else
		{
			// Resolve only an already loaded class. Missing class metadata is not
			// proof that an unloaded custom asset cannot be a Blueprint subclass.
			const UClass* AssetClass = Asset.GetClass();
			Line += AssetClass && !AssetClass->IsChildOf(UBlueprint::StaticClass())
				? TEXT("\n  generated class: not applicable (non-Blueprint asset)")
				: TEXT("\n  generated class: unknown (no generated-class metadata available)");
		}

		const FString Parent = BlueprintParentOf(Asset);
		if (!Parent.IsEmpty())
		{
			Line += FString::Printf(TEXT("\n  parent: %s"), *Parent);
		}

		Line += FString::Printf(TEXT("\n  loaded: %s"), bLoaded ? TEXT("yes") : TEXT("no"));

		// Dirty state and package existence answer different questions.
		if (bLoaded)
		{
			const UPackage* Package = FindPackage(nullptr, *InResolved.PackagePath);
			const TCHAR* DirtyState = Package ? (Package->IsDirty() ? TEXT("yes") : TEXT("no")) : TEXT("unknown (package unavailable)");
			Line += FString::Printf(TEXT("\n  unsaved changes: %s"), DirtyState);
		}
		else
		{
			Line += TEXT("\n  unsaved changes: unknown, the asset is not loaded");
		}
		const bool bPackageExists = FPackageName::DoesPackageExist(InResolved.PackagePath);
		Line += bPackageExists
			? TEXT("\n  on disk: yes (package exists; in-memory changes may be unsaved)")
			: TEXT("\n  on disk: no (no saved package found)");

		return Line;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPAssetSearch(const FString& Query, const FString& Path, bool Recursive)
{
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return TEXT("error: the asset registry is not available, so no asset could be searched for.");
	}

	const FParsedQuery Parsed = ParseQuery(Query);
	if (!Parsed.Error.IsEmpty()) return Parsed.Error;

	FString Folder = Path == TEXT("*") ? FString() : Path;
	if (!Folder.IsEmpty())
	{
		Folder.RemoveFromEnd(TEXT("/"));
		// Validate the folder as a package parent so mount roots such as /Game also work.
		const FString ProbePackage = Folder + TEXT("/FolderProbe");
		if (Folder.IsEmpty() || Folder.Len() >= NAME_SIZE || FCString::Strlen(*Folder) != Folder.Len()
			|| !FPackageName::IsValidLongPackageName(ProbePackage, /*bIncludeReadOnlyRoots=*/true)
			|| FPackageName::IsScriptPackage(ProbePackage) || FPackageName::IsMemoryPackage(ProbePackage)
			|| FPackageName::IsTempPackage(ProbePackage))
		{
			return TEXT("error: Path must be a mounted Unreal content folder such as /Game/UI. ")
				TEXT("Pass a full folder path with forward slashes.");
		}
	}

	if (Folder.IsEmpty() && Parsed.Words.IsEmpty() && Parsed.TypeFilter.IsEmpty() && Parsed.PathFilter.IsEmpty())
	{
		return TEXT("error: no query given. Pass words, a filter such as type:Material or path:UI, ")
			TEXT("or an explicit Path such as /Game/UI.");
	}

	if (Registry->IsGathering())
	{
		return TEXT("partial: asset registry scanning is active. Wait for scanning to finish, then retry the same search. No assets were loaded or edited.");
	}

	// The registry folder filter matches package path segments and includes registered unsaved assets.
	TArray<FAssetData> Candidates;
	const bool bEnumerated = Folder.IsEmpty()
		? Registry->GetAllAssets(Candidates, /*bIncludeOnlyOnDiskAssets=*/false)
		: Registry->GetAssetsByPath(FName(*Folder), Candidates, Recursive, /*bIncludeOnlyOnDiskAssets=*/false);
	if (!bEnumerated)
	{
		return TEXT("error: the asset registry could not enumerate assets for this search.");
	}

	TArray<FSearchHit> Hits;
	for (const FAssetData& Asset : Candidates)
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

	// Sort by match rank, then name length, then name and full object path.
	// The final comparison makes same-name results independent of registry order.
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
        if (A.Asset.AssetName != B.Asset.AssetName)
            return A.Asset.AssetName.LexicalLess(B.Asset.AssetName);
        return A.Asset.GetObjectPathString() < B.Asset.GetObjectPathString();
	});

	FString Result = FString::Printf(TEXT("read: asset search for '%s', %d match(es)"),
		*Query, Hits.Num());

	if (!Parsed.TypeFilter.IsEmpty() || !Parsed.PathFilter.IsEmpty())
	{
		Result += FString::Printf(TEXT(" (type: %s, path: %s)"),
			Parsed.TypeFilter.IsEmpty() ? TEXT("any") : *Parsed.TypeFilter,
			Parsed.PathFilter.IsEmpty() ? TEXT("any") : *Parsed.PathFilter);
	}
	else if (Folder.IsEmpty())
	{
		Result += TEXT(" (no filters)");
	}

	if (!Folder.IsEmpty())
	{
		Result += FString::Printf(TEXT(" [folder scope: %s (%s)]"), *Folder,
			Recursive ? TEXT("includes subfolders") : TEXT("direct assets only"));
	}

	if (Hits.Num() == 0)
	{
		// P1: say it matched nothing and how many assets were searched, so the
		// reader can tell "no match" from "nothing to search".
		Result += FString::Printf(
			TEXT("\nNothing matched. %d assets were searched, including those in memory only."),
			Candidates.Num());
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
		Result += FString::Printf(
			TEXT("\n(%d more. Narrow the query or set Path to a smaller folder.)"),
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
