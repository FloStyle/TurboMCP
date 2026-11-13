// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "TurboMCPDataAssetValues.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "InputAction.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "HAL/FileManager.h"
#include "PackageTools.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDataAssetCreateTest,
    "TurboMCP.DataAsset.Creation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPDataAssetCreateTest::RunTest(const FString& Parameters)
{
    const FString ClassPath = UInputAction::StaticClass()->GetPathName();
    UInputAction::StaticClass()->GetDefaultObject();
    const FString PackagePath = TEXT("/Game/Tests/TurboMCPCreate_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
    auto Preview = [&](const FString& Fields)
    {
        return UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetCreate(ClassPath, PackagePath, Fields, false, false);
    };
    TestTrue(TEXT("preview accepts an initialized native Data Asset class"), Preview(TEXT("{}")).Contains(TEXT("preview: create")));
    TestNull(TEXT("preview allocates no package"), FindPackage(nullptr, *PackagePath));
    TestTrue(TEXT("invalid field refused before creation"), Preview(TEXT("{\"NotAProperty\":\"1\"}")).Contains(TEXT("refused:")));
    TestNull(TEXT("refusal allocates no package"), FindPackage(nullptr, *PackagePath));
    const FString Created = UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
        ClassPath, PackagePath, TEXT("{\"bConsumeInput\":\"False\"}"), false, false);
    AddInfo(Created);
    UInputAction* Asset = FindObject<UInputAction>(nullptr, *ObjectPath);
    if (!TestNotNull(TEXT("created typed asset"), Asset)) return false;
    TestFalse(TEXT("initial value applied"), Asset->bConsumeInput);
    TestEqual(TEXT("unmentioned default preserved"), Asset->ValueType, GetDefault<UInputAction>()->ValueType);
    TestTrue(TEXT("reader-compatible revision returned"), Created.Contains(TurboMCPDataAssetValues::Revision(Asset)));
    TestTrue(TEXT("created package dirty"), Asset->GetPackage()->IsDirty());
    TestFalse(TEXT("memory creation does not save"), FPackageName::DoesPackageExist(PackagePath));
    TestTrue(TEXT("occupied destination refused"), UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
        ClassPath, PackagePath, TEXT("{}"), false, false).Contains(TEXT("refused:")));
    UPackage* CreatedPackage = Asset->GetPackage();
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    GEditor->UndoTransaction();
    TestFalse(TEXT("undo removes live created object"), IsValid(Asset));
    TestFalse(TEXT("undo restores clean package state"), CreatedPackage->IsDirty());
    // Exact-path lookup in stock 5.8.4 uses FindObject without an IsValid gate,
    // so it can return metadata for an undo tombstone. Enumerate live assets instead.
    TArray<FAssetData> AfterUndo;
    Registry.Get().GetAssetsByPackageName(FName(*PackagePath), AfterUndo);
    TestTrue(TEXT("undo removes enumerated asset"), AfterUndo.IsEmpty());
    GEditor->RedoTransaction();
    TestTrue(TEXT("redo restores created object"), IsValid(Asset));
    TestTrue(TEXT("redo restores registry-visible asset"), Registry.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid());
    if (IsValid(Asset))
    {
        TestFalse(TEXT("redo restores initialized value"), Asset->bConsumeInput);
        // Only this disposable memory fixture is removed; no saved asset is touched.
        FAssetRegistryModule::AssetDeleted(Asset);
        Asset->GetPackage()->SetDirtyFlag(false);
        Asset->ClearFlags(RF_Public | RF_Standalone);
        Asset->MarkAsGarbage();
    }
    // A Blueprint-derived type must also be usable without loading or compiling during the tool call.
    UPackage* BlueprintPackage = CreatePackage(*(PackagePath + TEXT("_Type")));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UPrimaryDataAsset::StaticClass(),
        BlueprintPackage, TEXT("BP_CreateType"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (TestNotNull(TEXT("Blueprint Data Asset type fixture"), Blueprint) &&
        TestNotNull(TEXT("generated class fixture"), Blueprint->GeneratedClass.Get()))
    {
        Blueprint->GeneratedClass->GetDefaultObject();
        const FString DerivedPackage = PackagePath + TEXT("_Derived");
        const FString DerivedResult = UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
            Blueprint->GeneratedClass->GetPathName(), DerivedPackage, TEXT("{}"), false, false);
        TestTrue(TEXT("loaded Blueprint-derived creation"), DerivedResult.Contains(TEXT("save: not requested")));
        const FString DerivedPath = DerivedPackage + TEXT(".") + FPackageName::GetLongPackageAssetName(DerivedPackage);
        if (UDataAsset* Derived = FindObject<UDataAsset>(nullptr, *DerivedPath))
        {
            TestEqual(TEXT("exact generated class retained"), Derived->GetClass(), Blueprint->GeneratedClass.Get());
            FAssetRegistryModule::AssetDeleted(Derived);
            Derived->GetPackage()->SetDirtyFlag(false);
            Derived->ClearFlags(RF_Public | RF_Standalone);
            Derived->MarkAsGarbage();
        }
        BlueprintPackage->SetDirtyFlag(false);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
    }
    const FString SavedPackage = PackagePath + TEXT("_Saved");
    const FString SavedObject = SavedPackage + TEXT(".") + FPackageName::GetLongPackageAssetName(SavedPackage);
    // Do not add fixture files to a real provider. The unavailable-provider case is checked
    // only when this supervised editor already has no usable provider.
    const bool AddUnavailable = !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable();
    const FString SavedResult = UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(
        ClassPath, SavedPackage, TEXT("{\"bConsumeInput\":\"False\"}"), true, AddUnavailable);
    AddInfo(SavedResult);
    TestTrue(TEXT("explicit save reports successful write"), SavedResult.Contains(TEXT("save: written")));
    if (AddUnavailable)
        TestTrue(TEXT("unavailable add reports partial completion"), SavedResult.Contains(TEXT("partial:")));
    FString SavedFilename;
    FPackageName::TryConvertLongPackageNameToFilename(SavedPackage, SavedFilename, FPackageName::GetAssetPackageExtension());
    UInputAction* SavedAsset = FindObject<UInputAction>(nullptr, *SavedObject);
    if (SavedAsset)
    {
        TWeakObjectPtr<UInputAction> OldAsset(SavedAsset);
        TArray<UPackage*> Packages{SavedAsset->GetPackage()};
        SavedAsset = nullptr;
        UPackageTools::FUnloadPackageParams Unload(Packages);
        Unload.bUnloadDirtyPackages = false;
        Unload.bResetTransBuffer = true; // This test runs only in the supervisor-owned disposable editor.
        TestTrue(TEXT("saved fixture unloads"), UPackageTools::UnloadPackages(Unload));
        TestFalse(TEXT("old fixture identity is gone"), OldAsset.IsValid());
        // Stock unload marks the package deleted in the registry; the deliberate
        // disk reload reports that it has become visible again.
        AddExpectedError(TEXT("package was marked as deleted in editor, but has been modified on disk"), EAutomationExpectedErrorFlags::Contains, 1);
        UInputAction* Reloaded = LoadObject<UInputAction>(nullptr, *SavedObject);
        if (TestNotNull(TEXT("saved fixture reloads"), Reloaded))
        {
            TestFalse(TEXT("initial value survives fresh disk reload"), Reloaded->bConsumeInput);
            ResetLoaders(Reloaded->GetPackage());
            TestTrue(TEXT("owned saved fixture removed before registry notification"), IFileManager::Get().Delete(*SavedFilename, false, true));
            FAssetRegistryModule::AssetDeleted(Reloaded);
            Reloaded->ClearFlags(RF_Public | RF_Standalone);
            Reloaded->MarkAsGarbage();
        }
    }
    TestTrue(TEXT("owned saved fixture removed"), IFileManager::Get().Delete(*SavedFilename, false, true));
    return true;
}
#endif

