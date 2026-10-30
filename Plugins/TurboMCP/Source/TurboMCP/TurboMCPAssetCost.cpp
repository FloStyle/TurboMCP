// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPAssetCost.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/AssetRegistryInterface.h"

#include "TurboMCPAssetResolver.h"

namespace
{
    // Missing or negative registry measurements are unknown, never zero cost.
	int64 DiskSizeOfPackage(IAssetRegistry* InRegistry, const FName InPackageName)
	{
		const TOptional<FAssetPackageData> Data = InRegistry->GetAssetPackageDataCopy(InPackageName);
		if (!Data.IsSet())
		{
			return INDEX_NONE;
		}
		return Data.GetValue().DiskSize >= 0 ? Data.GetValue().DiskSize : INDEX_NONE;
	}

	/** One dependency set, read with the readable query form. */
	void GatherDependencies(
		IAssetRegistry* InRegistry,
		const FName InPackageName,
		const UE::AssetRegistry::EDependencyQuery InKind,
		const FString& InReason,
		FTurboMCPAssetCost& Out)
	{
        // Query flags select dependency categories; categories can overlap.
		const UE::AssetRegistry::FDependencyQuery Query(InKind);

		TArray<FName> Dependencies;
		if (!InRegistry->GetDependencies(InPackageName, Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package, Query))
		{
			Out.QueryErrors.Add(InReason);
			return;
		}

		TArray<FTurboMCPAssetCost::FContributor>* Destination = &Out.Hard;
		if (InKind == UE::AssetRegistry::EDependencyQuery::Soft)
		{
			Destination = &Out.Soft;
		}
		else if (InKind == UE::AssetRegistry::EDependencyQuery::EditorOnly)
		{
			Destination = &Out.EditorOnly;
		}

		for (const FName& Dependency : Dependencies)
		{
			FTurboMCPAssetCost::FContributor Contributor;
			Contributor.Package = Dependency.ToString();
			Contributor.Reason = InReason;
			Contributor.Bytes = DiskSizeOfPackage(InRegistry, Dependency);

			Out.Examined++;

			if (Contributor.Bytes == INDEX_NONE)
			{
				// UNKNOWN IS ITS OWN LIST, WITH ITS REASON. It is NEVER added to a total as zero (`P1`).
				Contributor.Reason = InReason + TEXT(", size unavailable in registry");
				Out.Unknown.Add(Contributor);
			}
			// A missing size does not remove a dependency from its category count.
			Destination->Add(Contributor);
			Out.Largest.Add(Contributor);
		}
	}
}

FString FTurboMCPAssetCost::Render(int32 Cap) const
{
    Cap = Cap > 0 ? Cap : 10;
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("read: asset package sizes and direct registry dependencies")));
	Lines.Add(FString::Printf(TEXT("asset: %s"), *Asset));
	Lines.Add(TEXT(""));

	// ---------------------------------------------------------------------------------------------
	// THE ASSET'S OWN SIZE, AND THE REASON WHEN IT CANNOT BE READ. `P1`: NEVER ZERO FOR UNKNOWN.
	// ---------------------------------------------------------------------------------------------
	if (OwnBytes == INDEX_NONE)
	{
		Lines.Add(FString::Printf(TEXT("OWN SIZE: unknown - %s"), *OwnReason));
		Lines.Add(TEXT("  Size is unknown, not zero. The registry has no usable disk-size measurement."));
		Lines.Add(TEXT("  Recorded package sizes do not measure runtime memory or cooked-build size."));
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("OWN SIZE: %lld byte(s)"), OwnBytes));
	}
	Lines.Add(TEXT(""));

	// ---------------------------------------------------------------------------------------------
	// THE SPLIT. THIS IS `D-52` AND IT IS THE REASON THE TOOL EXISTS.
	// ---------------------------------------------------------------------------------------------
	if (QueryErrors.Num() > 0)
	{
		Lines.Insert(TEXT("partial: Dependency counts are unknown because registry queries failed."), 0);
		for (const FString& Query : QueryErrors)
			Lines.Add(TEXT("unavailable query: ") + Query);
		Lines.Add(TEXT("Wait for the asset registry scan to finish, then retry. Unsaved packages may have no recorded dependencies."));
		return FString::Join(Lines, TEXT("\n"));
	}
	Lines.Add(FString::Printf(TEXT("DIRECT DEPENDENCIES BY KIND (%d dependency entr(ies) examined)"), Examined));
	Lines.Add(FString::Printf(TEXT("  hard references        : %d package(s)"), Hard.Num()));
	Lines.Add(FString::Printf(TEXT("  soft references        : %d package(s)"), Soft.Num()));
	Lines.Add(FString::Printf(TEXT("  editor-only references : %d package(s)"), EditorOnly.Num()));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("  These are direct registry dependencies, not a recursive load-cost total."));
	Lines.Add(TEXT("  Editor-only entries can overlap hard or soft entries; do not add the categories."));
	Lines.Add(TEXT("  Sizes are recorded package disk sizes, not runtime memory or cooked-build sizes; missing measurements remain unknown."));
	Lines.Add(TEXT(""));

	// ---------------------------------------------------------------------------------------------
	// UNKNOWN SIZES GET THEIR OWN SECTION RATHER THAN BEING ADDED AS ZERO.
	// ---------------------------------------------------------------------------------------------
	if (Unknown.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("SIZES THAT COULD NOT BE READ (%d) - AN ABSENT MEASUREMENT, NOT ZERO COST"),
			Unknown.Num()));
        const int32 UnknownShown = FMath::Min(Cap, Unknown.Num());
        for (int32 Index = 0; Index < UnknownShown; ++Index)
            Lines.Add(FString::Printf(TEXT("  %s"), *Unknown[Index].Package));
        if (UnknownShown < Unknown.Num())
            Lines.Add(FString::Printf(TEXT("  ... %d unknown-size entries omitted by Cap; increase Cap to see more."), Unknown.Num() - UnknownShown));
		Lines.Add(TEXT(""));
	}

	// ---------------------------------------------------------------------------------------------
	// THE LARGEST CONTRIBUTORS, CAPPED, EACH WITH ITS SIZE AND ITS REASON.
	// ---------------------------------------------------------------------------------------------
	TArray<FContributor> Ranked = Largest;
	Ranked.Sort([](const FContributor& A, const FContributor& B)
	{
		if (A.Bytes == B.Bytes)
		{
			return A.Package < B.Package;
		}
		return A.Bytes > B.Bytes;
	});

	const int32 Shown = FMath::Min(Cap, Ranked.Num());
	Lines.Add(FString::Printf(TEXT("LARGEST CONTRIBUTORS (%d of %d, capped at %d)"), Shown, Ranked.Num(), Cap));
	if (Ranked.Num() == 0)
	{
		Lines.Add(TEXT("  No dependencies were returned by the registry queries."));
		Lines.Add(TEXT("  This does not prove that the asset has no runtime or unsaved references."));
	}
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		const FContributor& Entry = Ranked[Index];
		if (Entry.Bytes == INDEX_NONE)
		{
			Lines.Add(FString::Printf(TEXT("  %s | size UNKNOWN | %s"), *Entry.Package, *Entry.Reason));
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("  %s | %lld byte(s) | %s"), *Entry.Package, Entry.Bytes, *Entry.Reason));
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString FTurboMCPAssetCostReader::Read(const FString& AssetName, int32 Cap)
{
	// THE INPUT IS PARSED BEFORE ANYTHING IS LOADED, WHICH IS THE ORDERING LESSON A PREVIOUS WORKSTREAM RECORDED:
	// *"a refusal about the REQUEST must not be masked by a refusal about the TARGET"*. Loading first made every
	// malformed input come back as "the asset did not resolve", and ten checks failed on one ordering mistake.
	if (AssetName.IsEmpty())
	{
		return TEXT("error: no asset was named. Pass the asset to report on, for example \"TA01_HandLaid\".");
	}

	const int32 EffectiveCap = (Cap > 0) ? Cap : 10;

	FAssetRegistryModule& Module =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry* Registry = &Module.Get();
	if (Registry == nullptr)
	{
		return TEXT("error: the asset registry is not available in this process, so no size can be reported "
		            "at all. That is an absent measurement rather than zero cost.");
	}

    Registry->WaitForCompletion();
    if (Registry->IsGathering())
        return TEXT("partial: Asset registry scanning is still active. Package sizes and dependencies are unknown; wait for scanning to finish and retry.");

	FTurboMCPAssetCost Report;

	// *** THE RESOLVER'S CONVENTION IS THE OPPOSITE OF WHAT A `bool` RETURN WOULD BE, AND I HAD IT BACKWARDS FIRST:
	// IT RETURNS AN EMPTY STRING ON SUCCESS AND A COMPLETE ERROR LINE ON FAILURE (`TurboMCPAssetResolver.h:61-71`).
	// READING THE HEADER RATHER THAN ASSUMING IS WHAT CAUGHT IT - the same rule that has caught ten recalled engine
	// names in this project. The resolver is the project's own path handling, so all accepted forms work. ***
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString Refusal = FTurboMCPAssetResolver::Resolve(AssetName, Resolved);
	if (!Refusal.IsEmpty())
	{
		// THE REFUSAL IS THE RESOLVER'S OWN LINE, NAMING WHAT WAS REFUSED AND WHAT WOULD WORK (`P2`, `P7`).
		// Reporting anything of my own here would be a second, worse explanation of the same failure.
		return Refusal;
	}

	// AND THE RESOLVER ALREADY KNOWS THE PACKAGE PATH, so it is taken from it rather than reconstructed by
	// slicing the object path at a colon and a dot - which is what I wrote first and which would have been a
	// second, weaker implementation of something the project already has.
	const FName PackageName(*Resolved.PackagePath);
	Report.Asset = Resolved.ObjectPath;

	Report.OwnBytes = DiskSizeOfPackage(Registry, PackageName);
	if (Report.OwnBytes == INDEX_NONE)
	{
		// THE REASON IS CARRIED, NOT INVENTED. `P1`: unknown with a reason beats a zero that reads as "free".
		Report.OwnReason = FString::Printf(
			TEXT("the registry has no usable disk-size measurement for '%s'"),
			*Resolved.PackagePath);
	}

	// *** THREE QUERIES RATHER THAN ONE, BECAUSE THE SPLIT IS THE CAPABILITY. `D-52` REJECTS A SINGLE TOTAL: *"the
	// total names no cause and cannot be acted on"*. The readable kinds come from `EDependencyQuery`, whose own
	// spellings are `Hard`, `Soft = NotHard` and `EditorOnly = NotGame`. ***
	GatherDependencies(Registry, PackageName, UE::AssetRegistry::EDependencyQuery::Hard,
		TEXT("HARD reference recorded by the asset registry"), Report);
	GatherDependencies(Registry, PackageName, UE::AssetRegistry::EDependencyQuery::Soft,
		TEXT("SOFT reference recorded by the asset registry"), Report);
	GatherDependencies(Registry, PackageName, UE::AssetRegistry::EDependencyQuery::EditorOnly,
		TEXT("EDITOR-ONLY reference recorded by the asset registry"), Report);

	if (Report.Examined == 0)
	{
		// NOT AN ERROR, BUT NOT SILENT EITHER: zero dependencies is a real answer and the caller is told which
		// answer this is. The same rule `R-BP-LINT` applies to its own empty result.
		Report.OwnReason = Report.OwnReason.IsEmpty()
			? TEXT("")
			: Report.OwnReason;
	}

	return Report.Render(EffectiveCap);
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPAssetCostRenderTest, "TurboMCP.Assets.CostOutput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPAssetCostRenderTest::RunTest(const FString& Parameters)
{
    FTurboMCPAssetCost Report; Report.Asset = TEXT("/Game/Tests/CostFixture"); Report.OwnBytes = 100;
    for (int32 I = 0; I < 5; ++I)
    {
        FTurboMCPAssetCost::FContributor Entry;
        Entry.Package = FString::Printf(TEXT("/Game/Unknown%d"), I); Entry.Reason = TEXT("hard reference, size unavailable");
        Report.Unknown.Add(Entry); Report.Hard.Add(Entry); Report.Largest.Add(Entry); ++Report.Examined;
    }
    const FString Text = Report.Render(2);
    TestTrue(TEXT("Unknown list cap disclosed"), Text.Contains(TEXT("3 unknown-size entries omitted by Cap")));
    TestFalse(TEXT("Unshown unknown package omitted"), Text.Contains(TEXT("/Game/Unknown4")));
    TestTrue(TEXT("Category retains full count"), Text.Contains(TEXT("hard references        : 5 package(s)")));
    TestTrue(TEXT("Known own size has memory disclaimer"), Text.Contains(TEXT("not runtime memory")));
    TestTrue(TEXT("Unknown never reported as zero"), Text.Contains(TEXT("size UNKNOWN")) && !Text.Contains(TEXT("0 byte(s) |")));
    TestFalse(TEXT("Nonpositive cap uses default"), Report.Render(0).Contains(TEXT("entries omitted by Cap")));
    Report.QueryErrors.Add(TEXT("hard references"));
    TestTrue(TEXT("Query failure is partial"), Report.Render(2).StartsWith(TEXT("partial:")));
    return !HasAnyErrors();
}
#endif
