// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPFunctionDependencyScan.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
constexpr int32 MaxCandidatePackages = 4096;

bool IsChildOfOwner(const UBlueprint* Candidate, const UBlueprint* Owner)
{
    if (!Candidate || Candidate == Owner) return false;
    for (const UClass* Class = Candidate->ParentClass; Class; Class = Class->GetSuperClass())
        if (Class->ClassGeneratedBy == Owner) return true;
    return false;
}
}

FTurboMCPFunctionPackageScan FTurboMCPFunctionDependencyScan::CollectPackages(
    const TArray<FName>& KnownClassPackages,
    TFunctionRef<void(FName, TArray<FName>&)> GetDirectReferencers,
    TFunctionRef<bool(FName, FString&)> InspectCandidate)
{
    FTurboMCPFunctionPackageScan Result;
    TSet<FName> Candidates;
    TSet<FName> SeenClasses;
    TArray<FName> ClassFrontier;
    for (FName Package : KnownClassPackages)
    {
        if (Package.IsNone() || SeenClasses.Contains(Package)) continue;
        SeenClasses.Add(Package);
        Candidates.Add(Package);
        ClassFrontier.Add(Package);
    }
    if (Candidates.IsEmpty() || Candidates.Num() > MaxCandidatePackages)
    {
        Result.Error = TEXT("error: The defining Blueprint has no usable package, or its loaded child classes exceed the 4096-package discovery limit. Nothing changed.");
        return Result;
    }
    ClassFrontier.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    for (int32 Index = 0; Index < ClassFrontier.Num(); ++Index)
    {
        TArray<FName> Referencers;
        GetDirectReferencers(ClassFrontier[Index], Referencers);
        Referencers.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
        for (FName Package : Referencers)
        {
            if (Candidates.Contains(Package)) continue;
            if (Candidates.Num() >= MaxCandidatePackages)
            {
                Result.Error = FString::Printf(TEXT("error: More than %d direct candidate packages refer to the defining Blueprint or its child classes. Nothing changed; use the editor for this project-wide change."), MaxCandidatePackages);
                return Result;
            }
            Candidates.Add(Package);
            FString InspectError;
            const bool IsChildClass = InspectCandidate(Package, InspectError);
            if (!InspectError.IsEmpty())
            {
                Result.Error = MoveTemp(InspectError);
                return Result;
            }
            if (IsChildClass && !SeenClasses.Contains(Package))
            {
                SeenClasses.Add(Package);
                ClassFrontier.Add(Package);
            }
        }
    }
    Result.Candidates = Candidates.Array();
    Result.Candidates.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
    return Result;
}

FTurboMCPFunctionDependencyScan FTurboMCPFunctionDependencyScan::Collect(UBlueprint* Owner)
{
    FTurboMCPFunctionDependencyScan Result;
    if (!Owner || !Owner->GetOutermost())
    {
        Result.Error = TEXT("error: The defining Blueprint has no package. Nothing changed.");
        return Result;
    }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets() || Registry.IsGathering())
    {
        Result.Error = TEXT("error: Asset discovery is still running. Wait for it to finish, then preview again. Nothing changed.");
        return Result;
    }

    // Every caller of a derived class directly references that class package.
    // Loaded unsaved children also seed discovery because registry inheritance
    // metadata may lag their current in-memory parent.
    TArray<FName> ClassPackages;
    ClassPackages.Add(Owner->GetOutermost()->GetFName());
    TSet<UBlueprint*> SeenBlueprints;
    SeenBlueprints.Add(Owner);
    Result.Blueprints.Add(Owner);

    // Include unsaved loaded children; the registry need not have indexed their
    // current inheritance yet. The caller inspection also scans loaded graphs.
    for (TObjectIterator<UBlueprint> It; It; ++It)
    {
        UBlueprint* Blueprint = *It;
        if (!IsValid(Blueprint) || Blueprint->HasAnyFlags(RF_ClassDefaultObject | RF_Transient)
            || !IsChildOfOwner(Blueprint, Owner)) continue;
        if (!SeenBlueprints.Contains(Blueprint))
        {
            SeenBlueprints.Add(Blueprint);
            Result.Blueprints.Add(Blueprint);
        }
        ClassPackages.AddUnique(Blueprint->GetOutermost()->GetFName());
    }

    const FTurboMCPFunctionPackageScan Packages = CollectPackages(ClassPackages,
        [&Registry](FName Package, TArray<FName>& Referencers)
        {
            Registry.GetReferencers(Package, Referencers);
        },
        [&Registry, Owner, &Result, &SeenBlueprints](FName Package, FString& InspectError)
    {
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(Package, Assets);
        if (Assets.IsEmpty())
        {
            InspectError = TEXT("error: A referenced package has no indexed asset: ") + Package.ToString()
                + TEXT(". Caller discovery is incomplete; nothing changed.");
            return false;
        }
        bool IsChildClass = false;
        for (const FAssetData& Data : Assets)
        {
            if (Data.AssetClassPath.GetAssetName() == FName(TEXT("World")))
            {
                InspectError = TEXT("error: A dependent map may contain a level-script call: ")
                    + Data.GetObjectPathString() + TEXT(". Map callers are unsupported; nothing changed.");
                return false;
            }
            UClass* Class = Data.GetClass();
            if (!Class)
            {
                InspectError = TEXT("error: Cannot classify candidate asset: ")
                    + Data.GetObjectPathString() + TEXT(". Nothing changed.");
                return false;
            }
            if (!Class->IsChildOf(UBlueprint::StaticClass()))
            {
                ++Result.NonBlueprintAssetsSkipped;
                continue;
            }
            UBlueprint* Blueprint = Cast<UBlueprint>(Data.GetAsset());
            if (!Blueprint)
            {
                InspectError = TEXT("error: Could not load candidate Blueprint: ")
                    + Data.GetObjectPathString() + TEXT(". Nothing changed.");
                return false;
            }
            if (!SeenBlueprints.Contains(Blueprint))
            {
                SeenBlueprints.Add(Blueprint);
                Result.Blueprints.Add(Blueprint);
            }
            IsChildClass |= IsChildOfOwner(Blueprint, Owner);
        }
        return IsChildClass;
    });
    if (!Packages.Error.IsEmpty())
    {
        Result.Error = Packages.Error;
        return Result;
    }
    Result.PackagesInspected = Packages.Candidates.Num();
    Result.Blueprints.Sort([](const UBlueprint& A, const UBlueprint& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    return Result;
}
