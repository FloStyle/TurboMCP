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
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

#include "Logging.h"

/**
 * Tools that answer "what type is this" and "what is unsaved".
 *
 * `R-TYPE-FIND` exists because `P7` requires a result to name a thing in the
 * exact form another tool accepts, and the form a pin or variable needs is a
 * type path. Agents otherwise guess, and every guess is a failed call.
 */
namespace
{
	/** How many type matches one search may print (`P6`). */
	constexpr int32 MaxTypeResults = 30;

	/** Characters a type search is split on. */
	const TCHAR* WordSeparators = TEXT(" \t");

	/** One matched type. */
	struct FTypeHit
	{
		/** The exact path a pin or variable needs. */
		FString Path;

		/** `struct`, `enum`, `class`, `interface`, `function` or `delegate`. */
		FString Kind;

		/** The module for a native type, or the asset path for a user-made one. */
		FString Home;
	};

	/** True when a type lives in a user-made asset rather than in a module. */
	bool IsAssetType(const UObject* InType)
	{
		const UObject* Outer = InType != nullptr ? InType->GetOutermost() : nullptr;
		if (Outer == nullptr)
		{
			return false;
		}
		// A native type's outer package is `/Script/<Module>`. Anything else is
		// project content, which is what "user-made" means here.
		return !Outer->GetName().StartsWith(TEXT("/Script/"));
	}

	/** The module name for a native type, from `/Script/<Module>`. */
	FString ModuleOf(const UObject* InType)
	{
		const UObject* Outer = InType != nullptr ? InType->GetOutermost() : nullptr;
		if (Outer == nullptr)
		{
			return FString();
		}

		const FString PackageName = Outer->GetName();
		if (!PackageName.StartsWith(TEXT("/Script/")))
		{
			// A user-made type reports its asset path instead, which is the form
			// a caller needs to reopen it.
			return PackageName;
		}

		int32 LastSlash = INDEX_NONE;
		if (PackageName.FindLastChar(TEXT('/'), LastSlash))
		{
			return PackageName.Mid(LastSlash + 1);
		}
		return PackageName;
	}

	/** The kind a caller would ask for, from the type itself. */
	FString KindOfType(const UObject* InType)
	{
		if (InType == nullptr)
		{
			return TEXT("unknown");
		}

		if (const UClass* AsClass = Cast<UClass>(InType))
		{
			if (AsClass->HasAnyClassFlags(CLASS_Interface))
			{
				return TEXT("interface");
			}
			return TEXT("class");
		}
		if (InType->IsA<UScriptStruct>())
		{
			return TEXT("struct");
		}
		if (InType->IsA<UEnum>())
		{
			return TEXT("enum");
		}
		if (InType->IsA<UFunction>())
		{
			return TEXT("function");
		}
		return TEXT("type");
	}

	/** True when every word appears in the type's short name or its path. */
	bool TypeMatchesWords(const UObject* InType, const TArray<FString>& InWords)
	{
		const FString ShortName = InType->GetName();
		const FString FullPath = InType->GetPathName();

		for (const FString& Word : InWords)
		{
			if (!ShortName.Contains(Word, ESearchCase::IgnoreCase)
				&& !FullPath.Contains(Word, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * True when this object should be offered as a type.
	 *
	 * Skips the types that would only add noise: private implementation classes,
	 * the class default objects, and anything not flagged as usable by a pin.
	 */
	bool IsUsefulType(const UObject* InType)
	{
		if (InType == nullptr)
		{
			return false;
		}

		const FString Name = InType->GetName();
		if (Name.IsEmpty() || Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")))
		{
			return false;
		}

		// Only types a pin or variable can actually be given.
		if (!InType->HasAnyFlags(RF_Public) && !IsAssetType(InType))
		{
			return false;
		}

		if (const UClass* AsClass = Cast<UClass>(InType))
		{
			// Abstract and interface classes are still useful: an interface is a
			// legitimate pin type, and an abstract class can be a variable type.
			// A generated class is skipped because its Blueprint is the type a
			// caller wants, not the class UHT generated from it.
			if (AsClass->GetName().EndsWith(TEXT("_C")))
			{
				return false;
			}
		}
		return true;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPTypeFind(const FString& Words, const FString& Kind)
{
	// P1: an empty search says what to pass rather than listing every type in
	// the engine, which would be tens of thousands of lines.
	if (Words.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no words given. Pass part of a type name, and optionally a kind ")
			TEXT("of struct, enum, class or any. Example: Vector kind:struct");
	}

	TArray<FString> SearchWords;
	Words.ParseIntoArray(SearchWords, WordSeparators, /*InCullEmpty=*/true);

	FString WantedKind = Kind.TrimStartAndEnd().ToLower();
	if (WantedKind.IsEmpty())
	{
		WantedKind = TEXT("any");
	}

	static const TCHAR* KnownKinds[] = { TEXT("any"), TEXT("struct"), TEXT("enum"), TEXT("class"), TEXT("interface") };
	bool bKnownKind = false;
	for (const TCHAR* Candidate : KnownKinds)
	{
		if (WantedKind == Candidate)
		{
			bKnownKind = true;
			break;
		}
	}
	if (!bKnownKind)
	{
		return FString::Printf(
			TEXT("error: '%s' is not a kind. Accepted kinds: any, struct, enum, class, interface."),
			*Kind);
	}

	TArray<FTypeHit> Hits;

	// Native and user-made types both come from the object graph, which is the
	// only source that holds both. The asset registry is checked as well because
	// a user-made struct that is not loaded yet still exists on disk.
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Type = *It;
		if (Type == nullptr || Type->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		const bool bIsType =
			Type->IsA<UClass>() || Type->IsA<UScriptStruct>() || Type->IsA<UEnum>();
		if (!bIsType)
		{
			continue;
		}

		if (!IsUsefulType(Type))
		{
			continue;
		}

		const FString ThisKind = KindOfType(Type);
		if (WantedKind != TEXT("any") && !ThisKind.Equals(WantedKind, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!TypeMatchesWords(Type, SearchWords))
		{
			continue;
		}

		FTypeHit Hit;
		Hit.Path = Type->GetPathName();
		Hit.Kind = ThisKind;
		Hit.Home = ModuleOf(Type);
		Hits.Add(MoveTemp(Hit));
	}

	// A user-made struct or enum that is not loaded is not in the object graph
	// yet, so add anything the registry knows about that is not already listed.
	IAssetRegistry* Registry = IAssetRegistry::Get();
	int32 UnloadedCount = 0;
	if (Registry != nullptr && (WantedKind == TEXT("any") || WantedKind == TEXT("struct") || WantedKind == TEXT("enum")))
	{
		TArray<FAssetData> AllAssets;
		Registry->GetAllAssets(AllAssets, /*bIncludeOnlyOnDiskAssets=*/false);

		for (const FAssetData& Asset : AllAssets)
		{
			const FString ClassName = Asset.AssetClassPath.ToString();
			FString AssetKind;
			if (ClassName.Contains(TEXT("UserDefinedStruct")))
			{
				AssetKind = TEXT("struct");
			}
			else if (ClassName.Contains(TEXT("UserDefinedEnum")))
			{
				AssetKind = TEXT("enum");
			}
			else
			{
				continue;
			}

			if (WantedKind != TEXT("any") && !AssetKind.Equals(WantedKind, ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FString Path = Asset.GetObjectPathString();
			bool bAlreadyListed = false;
			for (const FTypeHit& Hit : Hits)
			{
				if (Hit.Path.Equals(Path, ESearchCase::IgnoreCase))
				{
					bAlreadyListed = true;
					break;
				}
			}
			if (bAlreadyListed)
			{
				continue;
			}

			const FString Name = Asset.AssetName.ToString();
			bool bMatches = true;
			for (const FString& Word : SearchWords)
			{
				if (!Name.Contains(Word, ESearchCase::IgnoreCase)
					&& !Path.Contains(Word, ESearchCase::IgnoreCase))
				{
					bMatches = false;
					break;
				}
			}
			if (!bMatches)
			{
				continue;
			}

			++UnloadedCount;

			FTypeHit Hit;
			Hit.Path = Path;
			Hit.Kind = AssetKind;
			Hit.Home = Asset.PackagePath.ToString() + TEXT(" (not loaded yet)");
			Hits.Add(MoveTemp(Hit));
		}
	}

	// Shortest path first, then by name, so the most likely answer leads and two
	// identical searches cannot disagree.
	Hits.Sort([](const FTypeHit& A, const FTypeHit& B)
	{
		if (A.Path.Len() != B.Path.Len())
		{
			return A.Path.Len() < B.Path.Len();
		}
		return A.Path < B.Path;
	});

	FString Result = FString::Printf(TEXT("read: type search for '%s', kind %s, %d match(es)"),
		*Words, *WantedKind, Hits.Num());

	if (Hits.Num() == 0)
	{
		Result += TEXT("\nNothing matched. Try fewer words or kind:any.");
		return Result;
	}

	const int32 Shown = FMath::Min(Hits.Num(), MaxTypeResults);
	if (Hits.Num() > Shown)
	{
		Result += FString::Printf(TEXT(", showing the first %d"), Shown);
	}

	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FTypeHit& Hit = Hits[Index];
		// The exact path comes first, because that is the form a pin or variable
		// needs and the reason this tool exists (P7).
		Result += FString::Printf(TEXT("\n%s  [%s] in %s"), *Hit.Path, *Hit.Kind, *Hit.Home);
	}

	if (Hits.Num() > Shown)
	{
		Result += FString::Printf(TEXT("\n(%d more, narrow the words or the kind)"), Hits.Num() - Shown);
	}

	if (UnloadedCount > 0)
	{
		Result += FString::Printf(
			TEXT("\n%d of these are not loaded yet; the path is still valid, but load the asset ")
			TEXT("before using it in a graph."),
			UnloadedCount);
	}

	return Result;
}

FString UTurboMCPReadToolset::ReadTurboMCPDirtyList(const FString& Limit)
{
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return TEXT("error: the asset registry is not available, so no unsaved asset could be listed.");
	}

	/**
	 * How many unsaved assets one result may list.
	 *
	 * `P6` caps every list. A caller that wants more can pass a higher limit, so
	 * the cap is a default and not a wall.
	 */
	constexpr int32 DefaultLimit = 50;
	int32 WantedLimit = DefaultLimit;
	if (!Limit.TrimStartAndEnd().IsEmpty())
	{
		// *** A LIMIT THAT IS NOT A NUMBER IS REFUSED RATHER THAN REPLACED BY THE DEFAULT. *** The default is
		// for a caller who LEFT IT EMPTY; a caller who typed `banana` or `0` made a mistake, and answering 50
		// hides it. The shape is `TurboMCPBlueprintLoad.cpp:100-114`, which does exactly this for the same kind
		// of parameter - and which refuses the two failures with DIFFERENT sentences, so a caller learns which
		// mistake they made.
		if (!Limit.TrimStartAndEnd().IsNumeric())
		{
			return FString::Printf(
				TEXT("error: '%s' is not a limit. Pass a whole number of assets, or leave it empty for the ")
				TEXT("default of %d."), *Limit, DefaultLimit);
		}
		WantedLimit = FCString::Atoi(*Limit);
		if (WantedLimit <= 0)
		{
			return FString::Printf(
				TEXT("error: a limit of %d is not usable. Pass a positive number of assets, or leave it empty ")
				TEXT("for the default of %d."), WantedLimit, DefaultLimit);
		}
	}

	// The dirty package list is the authoritative source, not a scan of every
	// asset: an asset with unsaved changes has a dirty package, and asking the
	// package is how `D-33` says to report the unsaved state. The content
	// variant is used so engine and transient packages do not appear as project
	// work a person could have edited.
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

	FString Result = FString::Printf(TEXT("read: unsaved assets, %d package(s) dirty"),
		DirtyPackages.Num());

	if (DirtyPackages.Num() == 0)
	{
		// P1: a zero says the check ran and found nothing, which is different
		// from a check that did not run.
		Result += TEXT("\nNothing has unsaved changes.");
		return Result;
	}

	int32 Listed = 0;
	FString Lines;
	for (UPackage* Package : DirtyPackages)
	{
		if (Package == nullptr)
		{
			continue;
		}

		if (Listed >= WantedLimit)
		{
			break;
		}

		const FString PackageName = Package->GetName();

		// The class comes from the registry rather than from loading the asset,
		// so listing unsaved work never forces it to load.
		FString ClassName = TEXT("unknown");
		TArray<FAssetData> Found;
		Registry->GetAssetsByPackageName(FName(*PackageName), Found, /*bIncludeOnlyOnDiskAssets=*/false);
		if (Found.Num() > 0 && Found[0].IsValid())
		{
			const FString Full = Found[0].AssetClassPath.ToString();
			int32 LastDot = INDEX_NONE;
			ClassName = Full.FindLastChar(TEXT('.'), LastDot) ? Full.Mid(LastDot + 1) : Full;
		}

		Lines += FString::Printf(TEXT("\n%s  [%s]"), *PackageName, *ClassName);
		++Listed;
	}

	Result += FString::Printf(TEXT(", listing %d"), Listed);
	Result += Lines;

	if (DirtyPackages.Num() > Listed)
	{
		Result += FString::Printf(
			TEXT("\n(%d more, pass a higher limit to see them)"), DirtyPackages.Num() - Listed);
	}

	return Result;
}
