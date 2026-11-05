// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/LinkerLoad.h"
#include "InputAction.h"
#include "TurboMCPAssetDeleteFixture.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPAssetDeleteTest,
    "TurboMCP.DataAsset.CheckedDelete", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPAssetDeleteTest::RunTest(const FString& Parameters)
{
    // Use the same live-ancestor/token validation as the Python supervisor tools.
    // An environment flag alone is not sufficient admission for disk recovery.
    const FString Repo = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
    const FString Helper = Repo / TEXT("Tools/execution_runtime.py");
    FString Output, Error;
    int32 ExitCode = -1;
    const FString Args = FString::Printf(TEXT("-X utf8 \"%s\" --authorize \"%s\" \"%s\""),
        *Helper, *Repo, *FPaths::ConvertRelativePathToFull(FPaths::RootDir()));
    if (FPlatformMisc::GetEnvironmentVariable(TEXT("TURBOMCP_RUNNER_OWNS_FIXTURES")) != TEXT("1")
        || !FPaths::GetBaseFilename(FPaths::GetProjectFilePath()).Equals(TEXT("TurboMCPTest"))
        || !FPaths::FileExists(Helper)
        || !FPlatformProcess::ExecProcess(TEXT("python.exe"), *Args, &ExitCode, &Output, &Error)
        || ExitCode != 0)
    {
        AddError(TEXT("Recovery fixture admission refused: require a live supervisor-owned TestProject. ") + Error + Output);
        return false;
    }

    const FString Name = TEXT("Delete_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/Tests/") + Name;
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    if (FPaths::FileExists(Filename) || FindPackage(nullptr, *PackageName)) { AddError(TEXT("Owned fixture collision.")); return false; }
    UPackage* Package = CreatePackage(*PackageName);
    UInputAction* Asset = NewObject<UInputAction>(Package, *Name, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Asset);
    const FString AssetPath = Asset->GetPathName();
    FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs)) { AddError(TEXT("Cannot save owned deletion fixture.")); return false; }
    const FMD5Hash OriginalHash = FMD5Hash::HashFile(*Filename);
    const FString NeighborFile = Filename + TEXT(".neighbor");
    if (!FFileHelper::SaveStringToFile(TEXT("preserve neighbor"), *NeighborFile)) { AddError(TEXT("Cannot create owned neighbor.")); return false; }
    const FMD5Hash NeighborHash = FMD5Hash::HashFile(*NeighborFile);
    Package->SetDirtyFlag(true);
    TestTrue(TEXT("dirty asset refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath).Contains(TEXT("unsaved changes")));
    Package->SetDirtyFlag(false); // Restore the fixture's saved state, not a production recovery policy.
    TestTrue(TEXT("missing confirmation refuses"), UTurboMCPEditToolset::EditTurboMCPAssetDelete(AssetPath, TEXT("")).Contains(TEXT("refused:")));
    if (FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, true))
    {
        const FString ReadOnlyResult = UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath);
        FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false);
        TestTrue(TEXT("read-only asset refuses"), ReadOnlyResult.Contains(TEXT("read-only")));
    }
    else AddError(TEXT("Could not establish read-only fixture control."));
    {
        TStrongObjectPtr<UObject> LiveReference(Asset);
        TestTrue(TEXT("live reference refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath).Contains(TEXT("references")));
    }
    {
        UObject* Child = NewObject<UTurboMCPAssetDeleteFixture>(Asset);
        TStrongObjectPtr<UObject> ChildReference(Child);
        TestTrue(TEXT("strong subobject reference refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath).Contains(TEXT("references")));
    }
    UObject* UnregisteredAsset = NewObject<UInputAction>(Package, TEXT("UnregisteredNeighbor"), RF_Public | RF_Standalone);
    const FString MultipleAssetsResult = UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath);
    AddInfo(MultipleAssetsResult);
    TestTrue(TEXT("multiple loaded assets refuse"), MultipleAssetsResult.Contains(TEXT("Another loaded asset"))
        || MultipleAssetsResult.Contains(TEXT("exactly one asset")));
    TestTrue(TEXT("multiple-asset refusal preserves file"), OriginalHash == FMD5Hash::HashFile(*Filename));
    UnregisteredAsset->ClearFlags(RF_Public | RF_Standalone);
    UnregisteredAsset->MarkAsGarbage();
    const FString HolderName = Name + TEXT("_References");
    const FString HolderPackageName = TEXT("/Game/Tests/") + HolderName;
    UPackage* HolderPackage = CreatePackage(*HolderPackageName);
    UTurboMCPAssetDeleteFixture* Holder = NewObject<UTurboMCPAssetDeleteFixture>(HolderPackage, *HolderName, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Holder);
    const FString HolderFile = FPackageName::LongPackageNameToFilename(HolderPackageName, FPackageName::GetAssetPackageExtension());
    auto SaveHolder = [&]()
    {
        const bool Saved = UPackage::SavePackage(HolderPackage, Holder, *HolderFile, SaveArgs);
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanModifiedAssetFiles(TArray<FString>{HolderFile});
        return Saved;
    };
    Holder->HardReference = Asset;
    if (!SaveHolder()) { AddError(TEXT("Cannot save hard-reference control.")); return false; }
    TestTrue(TEXT("saved hard reference refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath).Contains(TEXT("references")));
    Holder->HardReference = nullptr;
    Holder->SoftReference = FSoftObjectPath(AssetPath);
    if (!SaveHolder()) { AddError(TEXT("Cannot save soft-reference control.")); return false; }
    TestTrue(TEXT("saved soft reference refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath).Contains(TEXT("references")));
    Holder->SoftReference.Reset();
    if (!SaveHolder()) { AddError(TEXT("Cannot clear owned reference controls.")); return false; }
    UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!Editors || !Editors->OpenEditorForAsset(Asset)) { AddError(TEXT("Cannot establish open-editor control.")); return false; }
    const FString OpenResult = UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath);
    Editors->CloseAllEditorsForAsset(Asset);
    TestTrue(TEXT("open editor refuses"), OpenResult.Contains(TEXT("Close the asset editor")));
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(AssetPath);
    AddInfo(Preview);
    FString Before, Tail, Token;
    if (!Preview.Split(TEXT("confirmation: "), &Before, &Tail)) { AddError(TEXT("No delete confirmation: ") + Preview); return false; }
    Tail.Split(TEXT("\n"), &Token, &Before);
    TestTrue(TEXT("preview preserves package bytes"), OriginalHash == FMD5Hash::HashFile(*Filename));
    TArray<uint8> SavedBytes;
    if (!FFileHelper::LoadFileToArray(SavedBytes, *Filename)) { AddError(TEXT("Cannot read owned fixture bytes.")); return false; }
    ResetLoaders(Package);
    TArray<uint8> ChangedBytes = SavedBytes; ChangedBytes.Add(0);
    if (!FFileHelper::SaveArrayToFile(ChangedBytes, *Filename)) { AddError(TEXT("Cannot prepare stale confirmation.")); return false; }
    const FString StaleResult = UTurboMCPEditToolset::EditTurboMCPAssetDelete(AssetPath, Token);
    if (!FFileHelper::SaveArrayToFile(SavedBytes, *Filename)) { AddError(TEXT("Owned fixture restoration failed; supervisor must restore.")); return false; }
    TestTrue(TEXT("changed bytes refuse old confirmation"), StaleResult.Contains(TEXT("stale")));
    TestTrue(TEXT("stale refusal retained file"), OriginalHash == FMD5Hash::HashFile(*Filename));

    const FString Result = UTurboMCPEditToolset::EditTurboMCPAssetDelete(AssetPath, Token);
    AddInfo(Result);
    TestTrue(TEXT("deletion verified"), Result.Contains(TEXT("Verified: registry entry and package file are absent")));
    TestFalse(TEXT("file independently absent"), FPaths::FileExists(Filename));
    TArray<FAssetData> RemainingAssets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPackageName(FName(*PackageName), RemainingAssets);
    TestEqual(TEXT("registry independently empty"), RemainingAssets.Num(), 0);
    TestTrue(TEXT("neighbor bytes preserved"), NeighborHash == FMD5Hash::HashFile(*NeighborFile));
    TestTrue(TEXT("replay refuses"), UTurboMCPEditToolset::EditTurboMCPAssetDelete(AssetPath, Token).Contains(TEXT("refused:")));
    for (const FString Tool : {TEXT("PreviewTurboMCPAssetDelete"), TEXT("EditTurboMCPAssetDelete")})
    {
        const FString Toolset = Tool.StartsWith(TEXT("Preview")) ? TEXT("TurboMCP.TurboMCPPreviewToolset") : TEXT("TurboMCP.TurboMCPEditToolset");
        const FString Help = UTurboMCPReadToolset::ReadTurboMCPToolHelp(Toolset, Tool);
        TestTrue(TEXT("delete help identifies exact target"), Help.Contains(TEXT("AssetPath")) && Help.Contains(TEXT("/Game/Tests/Unused.Unused")));
    }
    // Saving clears dirty state but must not authorize deletion of an object
    // still retained by undo. Leave cleanup to the owned supervisor.
    Holder->SetFlags(RF_Transactional);
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Deletion undo refusal fixture")));
        Holder->Modify();
        Holder->SoftReference = FSoftObjectPath(TEXT("/Game/Tests/Absent.Absent"));
    }
    if (!SaveHolder()) { AddError(TEXT("Cannot save undo-reference control.")); return false; }
    const FMD5Hash UndoHash = FMD5Hash::HashFile(*HolderFile);
    const FString UndoResult = UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(Holder->GetPathName());
    AddInfo(UndoResult);
    TestTrue(TEXT("saved asset retained by undo refuses"), UndoResult.Contains(TEXT("undo references prevent deletion")));
    TestTrue(TEXT("undo refusal preserves package bytes"), UndoHash == FMD5Hash::HashFile(*HolderFile));
    return true;
}
#endif
