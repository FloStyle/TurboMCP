// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetIdentifier.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#include "TurboMCPAssetResolver.h"

/**
 * R-REFS: references in and out.
 *
 * `03_TOOLS.md` asks for the content assets this asset uses, and the assets that
 * use it, both by full path with their class.
 *
 * Both come from the asset registry rather than from a scan: the registry already
 * has the dependency edges, and walking every asset to find referencers would be
 * slower and would miss assets that are not loaded.
 *
 * The output is kept to content. Engine and engine-script packages are dropped,
 * because a Blueprint references `/Script/Engine.Actor` and dozens of others like
 * it, and listing those buries the handful of project assets a caller cares about.
 * `R-BP-LOAD`'s relation layer deliberately does not do this job; it reports what
 * the graphs touch, while this reports what the asset depends on.
 */
namespace
{
	/** How many references are printed in one direction before the list caps. */
	constexpr int32 MaxReferences = 60;

	/** How many characters one result may hold. */
	constexpr int32 MaxResultChars = 20000;

	/** One resolved reference: what to print and how to name it. */
	struct FReference
	{
		FString PackagePath;
		FString ObjectPath;
		FString ClassName;

		/** True when the package holds more than one asset, so the name is not the whole story. */
		bool bPackageHasSeveralAssets = false;
	};

	/**
	 * True for a package this tool should not list.
	 *
	 * `/Script/...` is engine code and is not content. `/Engine/...` is engine
	 * content that ships with the editor, so a project asset depending on it is
	 * normal and not worth a line each.
	 */
	bool IsNoise(const FString& InPackageName)
	{
		return InPackageName.StartsWith(TEXT("/Script/"))
			|| InPackageName.StartsWith(TEXT("/Engine/"))
			|| InPackageName.StartsWith(TEXT("/Temp/"))
			|| InPackageName.StartsWith(TEXT("/Transient"));
	}

	/** Fills one reference from the registry, or leaves the class unknown. */
	FReference DescribePackage(IAssetRegistry* InRegistry, const FString& InPackageName)
	{
		FReference Reference;
		Reference.PackagePath = InPackageName;
		Reference.ClassName = TEXT("unknown");

		TArray<FAssetData> Found;
		InRegistry->GetAssetsByPackageName(FName(*InPackageName), Found,
			/*bIncludeOnlyOnDiskAssets=*/false);

		if (Found.Num() == 0)
		{
			// A dependency edge can name a package the registry has no asset for,
			// for example a package that was deleted. Saying so is better than
			// inventing a class.
			Reference.ObjectPath = InPackageName;
			Reference.ClassName = TEXT("not in the asset registry");
			return Reference;
		}

		Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetObjectPathString() < B.GetObjectPathString(); });
		Reference.bPackageHasSeveralAssets = Found.Num() > 1;

		// The registry's own object path and class, so this cannot disagree with
		// how every other tool prints an asset.
		Reference.ObjectPath = Found[0].GetObjectPathString();
		Reference.ClassName = Found[0].AssetClassPath.GetAssetName().ToString();
		if (Reference.ClassName.IsEmpty())
		{
			Reference.ClassName = TEXT("unknown");
		}
		return Reference;
	}

	/** Prints one direction, then the honest count of what was left. */
	FString PrintDirection(const TCHAR* InLabel, TArray<FReference>& InReferences,
		const TCHAR* InNextStep)
	{
		FString Out = FString::Printf(TEXT("\n%s: %d"), InLabel, InReferences.Num());

		if (InReferences.Num() == 0)
		{
			// `P1`: zero is a real answer and it says what it means.
			Out += FString::Printf(TEXT("\n  none. %s"), InNextStep);
			return Out;
		}

		// Sorted by path so two reads of an unchanged project give the same text.
		InReferences.Sort([](const FReference& A, const FReference& B)
		{
			return A.PackagePath < B.PackagePath;
		});

		int32 Shown = 0;
		for (const FReference& Reference : InReferences)
		{
			if (Shown >= MaxReferences)
			{
				break;
			}
			++Shown;

			Out += FString::Printf(TEXT("\n  %s  [%s]"),
				*Reference.ObjectPath, *Reference.ClassName);
			if (Reference.bPackageHasSeveralAssets)
			{
				// The package holds more than one asset, so the object path printed
				// is one of several. A caller acting on it has to know.
				Out += TEXT("  (package-level edge; first asset by path shown, not a proven object-level target)");
			}
		}

		if (InReferences.Num() > Shown)
		{
			// `04_FORMATS.md` section 1: a capped list says how many are left.
			Out += FString::Printf(
				TEXT("\n  (%d more references omitted by the 60-row limit; this tool has no pagination. Use Unreal Reference Viewer for the complete list.)"),
				InReferences.Num() - Shown);
		}

		return Out;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPRefs(
	const FString& Asset,
	const FString& Direction)
{
	if (Asset.TrimStartAndEnd().IsEmpty())
	{
		return TEXT("error: no asset given. Pass an asset in any path form, for ")
			TEXT("example BP_NodeTest or /Game/Tests/BP_NodeTest.");
	}

	// The direction is validated before anything is looked up, so a typo costs
	// nothing and the error names both accepted values.
	const FString Wanted = Direction.TrimStartAndEnd().ToLower();
	const bool bWantBoth = Wanted.IsEmpty() || Wanted == TEXT("both");
	const bool bWantOut = bWantBoth || Wanted == TEXT("out");
	const bool bWantIn = bWantBoth || Wanted == TEXT("in");

	if (!bWantOut && !bWantIn && !bWantBoth)
	{
		return FString::Printf(
			TEXT("error: '%s' is not a direction. Accepted: out, in, both. ")
			TEXT("Leave it empty for both."), *Direction);
	}

	// A non-null registry only means that its service exists. During the editor's
	// asynchronous initial scan it can resolve an asset before its dependency maps
	// contain the edges, which used to serialize a temporary empty map as "0".
	// Complete that scan before resolution too: the resolver consults the same
	// registry for package and short-name forms.
	IAssetRegistry* Registry = IAssetRegistry::Get();
	if (Registry == nullptr)
	{
		return TEXT("error: the asset registry is unavailable; the reference scan ")
			TEXT("cannot run. Try again after the editor has started.");
	}

	Registry->WaitForCompletion();
	if (Registry->IsGathering())
	{
		// IsLoadingAssets only describes the initial search. A mounted plugin can
		// start a later search after that initial scan, while this asset's dependency
		// and referencer edges are still absent. IsGathering covers both cases.
		// An incomplete scan is not an empty graph, so do not produce counts or a
		// `none` answer.
		return TEXT("error: the asset registry scan is incomplete; no reference ")
			TEXT("counts were produced. Try again after the editor has finished loading ")
			TEXT("mounted content.");
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	// The package, not the object: dependency edges are recorded between packages.
	const FName PackageName(*Resolved.PackagePath);

	FString Result = FString::Printf(TEXT("read: references of %s"), *Resolved.ObjectPath);

	int32 OutCount = 0;
	int32 InCount = 0;

	if (bWantOut)
	{
		// Read saved registry edges; failure is distinct from an empty result.
		TArray<FAssetDependency> Dependencies;
		if (!Registry->GetDependencies(FAssetIdentifier(PackageName), Dependencies))
			return TEXT("partial: outgoing registry references are unavailable; no complete counts were produced. The package may not have saved dependency data. No edit, compile or save was requested.");

		TArray<FReference> References;
		TArray<FString> Dropped;
		for (const FAssetDependency& Dependency : Dependencies)
		{
			const FString Name = Dependency.AssetId.PackageName.ToString();
			if (Name.IsEmpty() || IsNoise(Name))
			{
				// Counted, not listed, so the numbers stay honest while the output
				// stays readable.
				Dropped.Add(Name);
				continue;
			}

			// The dependency may name an object inside a package rather than the
			// package's main asset. A dependency recorded at package level gives the
			// object name `None`, which is not an object, so it is treated as absent:
			// printing it produced `Package.None`, which looks like an object path
			// and is not one.
			const FString ObjectName = Dependency.AssetId.ObjectName.ToString();
			FReference Reference = DescribePackage(Registry, Name);
			if (!ObjectName.IsEmpty()
				&& !ObjectName.Equals(TEXT("None"), ESearchCase::IgnoreCase)
				&& Reference.ObjectPath != Name)
			{
				Reference.ObjectPath = FString::Printf(TEXT("%s.%s"), *Name, *ObjectName);
				// The class is the object's own, which may differ from the package's
				// first asset. Unknown object classes must not inherit another asset's class.
				Reference.ClassName = TEXT("unknown (object class not loaded)");
				if (const UObject* Found = FindObject<UObject>(nullptr,
						*Reference.ObjectPath))
				{
					Reference.ClassName = Found->GetClass()->GetName();
				}
			}
			References.Add(MoveTemp(Reference));
		}

		OutCount = References.Num();
		Result += PrintDirection(TEXT("uses (out)"), References,
			TEXT("try detail signatures in ReadTurboMCPBlueprintLoad for what the ")
			TEXT("graphs touch"));

		if (Dropped.Num() > 0)
		{
			Result += FString::Printf(
				TEXT("\n  (%d registry entries omitted: engine, script, temporary or empty package identities)"), Dropped.Num());
		}
	}

	if (bWantIn)
	{
		TArray<FAssetDependency> Referencers;
		if (!Registry->GetReferencers(FAssetIdentifier(PackageName), Referencers))
			return TEXT("partial: incoming registry references are unavailable; no complete counts were produced. The package may not have saved dependency data. No edit, compile or save was requested.");

		TArray<FReference> References;
		TArray<FString> Dropped;
		for (const FAssetDependency& Referencer : Referencers)
		{
			const FString Name = Referencer.AssetId.PackageName.ToString();
			if (Name.IsEmpty() || IsNoise(Name))
			{
				Dropped.Add(Name);
				continue;
			}
			References.Add(DescribePackage(Registry, Name));
		}

		InCount = References.Num();
		Result += PrintDirection(TEXT("used by (in)"), References,
			TEXT("this asset may be unreferenced, or its users may not be saved yet"));

		if (Dropped.Num() > 0)
		{
			Result += FString::Printf(
				TEXT("\n  (%d registry entries omitted: engine, script, temporary or empty package identities)"), Dropped.Num());
		}
	}

	// The counts go on the status line, which is only possible now that both
	// directions have run. They are printed after the opening sentence rather than
	// in it, because the opening sentence is built before either query runs.
	Result += FString::Printf(TEXT("\ncounts: out=%s, in=%s"),
        bWantOut ? *FString::FromInt(OutCount) : TEXT("not queried"),
        bWantIn ? *FString::FromInt(InCount) : TEXT("not queried"));

	// The engine keeps dependency edges for packages it has seen. A package that
	// was never saved, or a project the registry has not scanned, has no edges,
	// and a caller seeing "0 in" must know which of the two it is looking at.
	Result += TEXT("\nScope: direct registry edges, not a recursive impact analysis. Engine, script and temporary packages are filtered. Unsaved or dynamic references may be missing; zero is not permission to delete an asset. No edit, compile or save was requested.");

	if (Result.Len() > MaxResultChars)
	{
		const int32 OriginalLength = Result.Len();
		int32 Boundary = MaxResultChars;
        while (Boundary > 0 && Result[Boundary - 1] != TEXT('\n')) --Boundary;
        Result = Result.Left(Boundary > 0 ? Boundary - 1 : 0);
		Result += FString::Printf(
			TEXT("\n\n; (%d more character(s) were dropped; use direction out or in ")
			TEXT("to read one side at a time. This truncated result cannot prove complete impact or safe deletion.)"), OriginalLength - Result.Len());
	}

	return Result;
}
