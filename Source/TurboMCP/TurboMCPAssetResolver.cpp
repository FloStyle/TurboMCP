// Copyright FloStyle. All Rights Reserved.

#include "TurboMCPAssetResolver.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	/**
	 * How many near-miss paths one refusal may list.
	 *
	 * `P6` caps every list so a result cannot flood an agent's context. Five is
	 * enough to recognise the right asset without turning an error into a folder
	 * listing.
	 */
	constexpr int32 MaxSuggestions = 5;

	/** Where to look for a short name that is not already loaded. */
	const TCHAR GameRootPath[] = TEXT("/Game");

	/** The `_C` suffix a generated class path carries. */
	const TCHAR ClassSuffix[] = TEXT("_C");
}

FString FTurboMCPAssetResolver::Resolve(const FString& InInput, FResolved& OutResolved)
{
	const FString Input = InInput.TrimStartAndEnd();

	// P1: never return nothing. An empty input says so and says what to pass.
	if (Input.IsEmpty())
	{
		return TEXT("error: no asset given. Pass one asset as a short name, a package path, ")
			TEXT("an object path or a generated class path. Example: WBP_ShopCard");
	}

	if (TryDirectForms(Input, OutResolved))
	{
		return FString();
	}

	// Nothing exact matched, so treat the input as a short name. This is the step
	// where guessing would be dangerous (P2, D-10), so it either finds exactly
	// one asset or refuses and lists what it found.
	TArray<FResolved> Matches;
	FindAllByName(Input, Matches);

	if (Matches.Num() == 1)
	{
		OutResolved = Matches[0];
		return FString();
	}

	if (Matches.Num() > 1)
	{
		FString Error = FString::Printf(
			TEXT("error: '%s' fits %d assets and so cannot be resolved. "),
			*Input,
			Matches.Num());

		for (int32 Index = 0; Index < Matches.Num() && Index < MaxSuggestions; ++Index)
		{
			Error += FString::Printf(TEXT("[%s] "), *Matches[Index].ObjectPath);
		}

		if (Matches.Num() > MaxSuggestions)
		{
			Error += FString::Printf(
				TEXT("(%d more, use the full path of the asset you want) "),
				Matches.Num() - MaxSuggestions);
		}

		Error += TEXT("Pass one full path from the list above instead.");
		return Error;
	}

	// Nothing at all matched. Offer names that contain the input so a typo costs
	// one call and not two (P7).
	FString Error = FString::Printf(TEXT("error: no asset named '%s'. "), *Input);

	TArray<FResolved> NearMisses;
	FindAllByName(Input, NearMisses, /*bContainsMatch=*/true);

	if (NearMisses.Num() > 0)
	{
		Error += TEXT("Closest names: ");
		for (int32 Index = 0; Index < NearMisses.Num() && Index < MaxSuggestions; ++Index)
		{
			Error += FString::Printf(TEXT("[%s] "), *NearMisses[Index].ObjectPath);
		}
	}
	else
	{
		// The short-name search covers /Game only, so an asset that ships inside a
		// mounted plugin cannot be found by its own name. Saying so is the
		// difference between "you typed it wrong" and "this search cannot see it":
		// `/AudioWidgets/AudioFader/AudioFader` is real and `AudioFader` alone
		// cannot reach it.
		//
		// Built by concatenation rather than FString::Printf. UE's compile-time
		// format checker refuses a TCHAR* passed for %s unless it can see it is a
		// literal, and GameRootPath is a member, so two attempts at Printf both
		// failed to compile. Concatenation has no checker to satisfy.
		Error += TEXT("Nothing with a similar name is loaded or on disk in this "
			"project. A short name is searched for in ");
		Error += GameRootPath;
		Error += TEXT(" only, so an asset that ships inside a mounted plugin or an "
			"engine folder is not found by its own name. Pass its full path, for "
			"example ");
		Error += GameRootPath;
		Error += TEXT("/YourAsset.");
	}

	return Error;
}

void FTurboMCPAssetResolver::ResolveMany(
	const TArray<FString>& InInputs,
	TArray<FResolved>& OutAssets,
	TArray<FString>& OutErrors)
{
	for (const FString& Raw : InInputs)
	{
		// One element may itself be a comma or pipe list, because agents send
		// both shapes for a list parameter (D-04).
		const TArray<FString> Parts = SplitList(Raw);

		if (Parts.Num() == 0)
		{
			// An empty element is a caller mistake worth naming, but it must not
			// stop the other elements from resolving.
			OutErrors.Add(TEXT("error: empty entry in the asset list, ignored."));
			continue;
		}

		for (const FString& Part : Parts)
		{
			FResolved Resolved;
			const FString Error = Resolve(Part, Resolved);

			if (Error.IsEmpty())
			{
				OutAssets.Add(Resolved);
			}
			else
			{
				OutErrors.Add(Error);
			}
		}
	}
}

TArray<FString> FTurboMCPAssetResolver::SplitList(const FString& InRaw)
{
	TArray<FString> CommaParts;
	InRaw.ParseIntoArray(CommaParts, TEXT(","), /*InCullEmpty=*/true);

	// `|` is the other separator agents reach for, so both must work (D-04).
	// Splitting the comma result keeps the caller's original order.
	TArray<FString> Result;
	for (const FString& CommaPart : CommaParts)
	{
		TArray<FString> PipeParts;
		CommaPart.ParseIntoArray(PipeParts, TEXT("|"), /*InCullEmpty=*/true);

		for (const FString& PipePart : PipeParts)
		{
			const FString Trimmed = PipePart.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Result.Add(Trimmed);
			}
		}
	}

	return Result;
}

bool FTurboMCPAssetResolver::TryDirectForms(const FString& InInput, FResolved& OutResolved)
{
	FString ObjectPath = InInput;

	// A generated class path names the class, not the asset, so drop the `_C`
	// suffix and keep looking. `04_FORMATS.md` section 4 lists that form and P5
	// requires it to work.
	const int32 DotBeforeSuffix = ObjectPath.Len() - (UE_ARRAY_COUNT(ClassSuffix) - 1) - 1;
	const bool bHadClassSuffix =
		ObjectPath.EndsWith(ClassSuffix)
		&& DotBeforeSuffix >= 0
		&& ObjectPath[DotBeforeSuffix] == TEXT('.');

	if (bHadClassSuffix)
	{
		ObjectPath.LeftChopInline(UE_ARRAY_COUNT(ClassSuffix) - 1);
	}

	FString PackageName;
	FString ObjectName;
	if (!FPackageName::TryConvertToMountedPath(
			ObjectPath,
			nullptr,
			&PackageName,
			&ObjectName,
			nullptr,
			nullptr)
		|| PackageName.IsEmpty())
	{
		return false;
	}

	// A path names an object when it carries an object name that is not just the
	// package: either `/Mount/Folder/Package.Object`, or `...Package.Object_C`
	// before the suffix was dropped. Without this check the package path form
	// `/Game/Tests/TA11_TestEnum` resolves to the UPackage, and the asset is then
	// reported as class `Package` with the package as its object path. That is
	// the wrong answer to "which asset is this", and it made the path forms
	// disagree, which `T-ASSET-2` forbids.
	const bool bLooksLikeObjectPath =
		!ObjectName.IsEmpty()
		&& (ObjectPath.Contains(TEXT(".") + ObjectName) || bHadClassSuffix);

	if (!bLooksLikeObjectPath)
	{
		// The input named a package. P5 requires this form to work, so it is
		// resolved through the registry, which finds the asset inside it.
		return CompleteFromPackagePath(PackageName, OutResolved);
	}

	// An asset with unsaved changes exists only in memory. Completing from the
	// package path alone would answer "not found" for the very asset the agent
	// just made, so prefer the loaded object (P5).
	if (UObject* Loaded = FindObject<UObject>(nullptr, *ObjectPath))
	{
		return CompleteFromObject(Loaded, OutResolved, /*bWasClassPath=*/bHadClassSuffix);
	}

	return CompleteFromPackagePath(PackageName, OutResolved);
}

bool FTurboMCPAssetResolver::CompleteFromObject(
	const UObject* InObject,
	FResolved& OutResolved,
	bool bWasClassPath)
{
	if (InObject == nullptr)
	{
		return false;
	}

	// A UPackage is not an asset. Reporting one names the container instead of
	// the thing inside it, so it is refused and the caller resolves through the
	// registry instead.
	if (InObject->IsA<UPackage>())
	{
		return false;
	}

	OutResolved.ObjectPath = InObject->GetPathName();
	OutResolved.AssetName = InObject->GetName();

	if (const UPackage* Package = InObject->GetOutermost())
	{
		OutResolved.PackagePath = Package->GetName();
	}

	// The class path is filled only for a generated class, which is the one form
	// the engine's own tools ask for. Inventing `Asset._C` for every asset made
	// this field disagree between path forms, so it stays empty when the asset
	// has no generated class. The asset's own class is printed separately.
	if (bWasClassPath)
	{
		if (const UClass* Class = InObject->GetClass())
		{
			OutResolved.ClassPath = Class->GetPathName();
		}
	}

	// The asset's own class, set on this route too so the loaded-object route and
	// the registry route cannot disagree (`T-ASSET-2`).
	if (const UClass* AssetClass = InObject->GetClass())
	{
		OutResolved.AssetClass = AssetClass->GetPathName();
	}

	return true;
}

bool FTurboMCPAssetResolver::CompleteFromPackagePath(
	const FString& InPackagePath,
	FResolved& OutResolved)
{
	// The registry is the only source that sees assets which are in memory and
	// never saved, which P5 requires. It is null only during shutdown.
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr || InPackagePath.IsEmpty())
	{
		return false;
	}

	TArray<FAssetData> Found;
	Registry->GetAssetsByPackageName(FName(*InPackagePath), Found, /*bIncludeOnlyOnDiskAssets=*/false);

	if (Found.Num() != 1)
	{
		// Zero means the path is wrong. More than one means the path names a
		// package holding several assets, which is not one asset either, so P2
		// says refuse rather than pick the first.
		return false;
	}

	const FAssetData& Asset = Found[0];
	OutResolved.ObjectPath = Asset.GetObjectPathString();
	OutResolved.PackagePath = Asset.PackageName.ToString();
	OutResolved.AssetName = Asset.AssetName.ToString();

	// The asset's own class, taken from the registry so this route and the
	// loaded-object route cannot disagree. `T-ASSET-2` requires every path form
	// of one asset to produce the same line.
	OutResolved.AssetClass = Asset.AssetClassPath.ToString();

	// ClassPath stays empty on this route. It is only meaningful when the caller
	// actually named a generated class, and `R-ASSET-DESCRIBE` prints the
	// generated class under its own label by asking the registry for it.

	return true;
}

void FTurboMCPAssetResolver::FindAllByName(
	const FString& InName,
	TArray<FResolved>& OutMatches,
	bool bContainsMatch)
{
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr || InName.IsEmpty())
	{
		return;
	}

	// Scanning /Game recursively is the only query that covers the whole project,
	// including assets that exist in memory and were never saved. It is only run
	// after the exact forms have already failed, so a valid full path never pays
	// for it.
	TArray<FAssetData> AllGameAssets;
	Registry->GetAssetsByPath(FName(GameRootPath), AllGameAssets, /*bRecursive=*/true,
		/*bIncludeOnlyOnDiskAssets=*/false);

	for (const FAssetData& Asset : AllGameAssets)
	{
		const FString AssetName = Asset.AssetName.ToString();

		// Exact by default. A contains match is a guess, so it is only ever used
		// to build an error message and never to resolve a tool input.
		const bool bMatches = bContainsMatch
			? AssetName.Contains(InName, ESearchCase::IgnoreCase)
			: AssetName.Equals(InName, ESearchCase::IgnoreCase);

		if (!bMatches)
		{
			continue;
		}

		FResolved Match;
		Match.ObjectPath = Asset.GetObjectPathString();
		Match.PackagePath = Asset.PackageName.ToString();
		Match.AssetName = AssetName;
		// Set here as well as on the other two routes, or the short name form
		// prints "class: unknown" while the package and object forms print the
		// real class, and `T-ASSET-2` requires all forms to agree.
		Match.AssetClass = Asset.AssetClassPath.ToString();
		OutMatches.Add(MoveTemp(Match));
	}
}
