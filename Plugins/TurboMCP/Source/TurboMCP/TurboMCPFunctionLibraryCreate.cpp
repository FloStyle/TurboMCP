// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFunctionLibraryFactory.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace
{
FString CreateLibrary(bool Apply, const FString& PackagePath)
{
    auto Refuse = [](const FString& Why) { return TEXT("unchanged\nrefused: ") + Why; };
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive() || !GEditor->CanTransact())
        return Refuse(TEXT("An idle editor game thread outside PIE and transactions is required."));
    FText Reason;
    if (!PackagePath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(PackagePath, false, &Reason))
        return Refuse(TEXT("PackagePath must be a new /Game package path without an object suffix or extension. ") + Reason.ToString());
    const FString Name = FPackageName::GetLongPackageAssetName(PackagePath);
    const FString ObjectPath = PackagePath + TEXT(".") + Name;
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Existing;
    Registry.Get().GetAssetsByPackageName(FName(*PackagePath), Existing);
    // Refuse even an empty loaded package: creating into it would reuse state
    // owned by another operation rather than create a fresh destination.
    if (FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath) || !Existing.IsEmpty())
        return Refuse(TEXT("The destination package already exists in memory, the registry or on disk: ") + PackagePath);
    if (!Apply)
        return TEXT("unchanged\npreview: create Blueprint function library ") + ObjectPath + TEXT("\nNo package or asset was created. Creation initializes a generated class; it does not save to disk.");
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) return Refuse(TEXT("The destination package could not be created."));
    UBlueprintFunctionLibraryFactory* Factory = NewObject<UBlueprintFunctionLibraryFactory>();
    UBlueprint* Blueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name), RF_Public | RF_Standalone, nullptr, GWarn, NAME_None));
    if (!Blueprint)
        return TEXT("changed\nerror: the factory did not return a Blueprint; the destination package may remain in memory. Nothing was saved. Inspect ") + PackagePath;
    FAssetRegistryModule::AssetCreated(Blueprint);
    Blueprint->MarkPackageDirty();
    if (Blueprint->BlueprintType != BPTYPE_FunctionLibrary || Blueprint->ParentClass != UBlueprintFunctionLibrary::StaticClass() || !Blueprint->GeneratedClass)
        return TEXT("changed\nerror: an asset was created but function-library validation failed. Nothing was saved. Inspect ") + ObjectPath;
    return TEXT("changed\nCreated Blueprint function library: ") + ObjectPath + TEXT("\nParent: /Script/Engine.BlueprintFunctionLibrary\nGenerated class: ") + Blueprint->GeneratedClass->GetPathName()
        + TEXT("\nUnsaved changes: yes. No disk save was requested. Inspect the library before saving; do not automatically retry creation.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPFunctionLibraryCreate(const FString& PackagePath)
{
    return CreateLibrary(false, PackagePath);
}
FString UTurboMCPEditToolset::EditTurboMCPFunctionLibraryCreate(const FString& PackagePath)
{
    return CreateLibrary(true, PackagePath);
}
