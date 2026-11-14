// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UBlueprint;

/** Direct candidate packages reached while expanding only Blueprint class inheritance. */
struct FTurboMCPFunctionPackageScan
{
    TArray<FName> Candidates;
    FString Error;
};

/** Registered Blueprint candidates that may call a function on the owner or an inherited child. */
struct FTurboMCPFunctionDependencyScan
{
    TArray<UBlueprint*> Blueprints;
    int32 PackagesInspected = 0;
    int32 NonBlueprintAssetsSkipped = 0;
    FString Error;

    /** Loads only direct referencers of the owner and its Blueprint children. Never loads maps. */
    static FTurboMCPFunctionDependencyScan Collect(UBlueprint* Owner);

    /** Shared traversal; injectable callbacks let the large transitive-chain case be checked without 300 disk fixtures. */
    static FTurboMCPFunctionPackageScan CollectPackages(const TArray<FName>& KnownClassPackages,
        TFunctionRef<void(FName, TArray<FName>&)> GetDirectReferencers,
        TFunctionRef<bool(FName, FString&)> InspectCandidate);
};
