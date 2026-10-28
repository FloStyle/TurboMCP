// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/LinkerLoad.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPAssetRecoveryTest,
    "TurboMCP.DataAsset.SavedRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPAssetRecoveryTest::RunTest(const FString& Parameters)
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
    const FString Name = TEXT("Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/Tests/") + Name;
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    if (FPaths::FileExists(Filename) || FindPackage(nullptr, *PackageName))
    {
        AddError(TEXT("Unique recovery fixture already exists; refusing to overwrite it."));
        return false;
    }
    UPackage* Package = CreatePackage(*PackageName);
    UInputAction* Asset = NewObject<UInputAction>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
    const FString AssetPath = Asset->GetPathName();
    Asset->bConsumeInput = true;
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
    {
        AddError(TEXT("Owned fixture could not be saved."));
        return false;
    }
    const FMD5Hash SavedHash = FMD5Hash::HashFile(*Filename);
    TestTrue(TEXT("Saved fixture has a disk hash"), SavedHash.IsValid());
    Asset->bConsumeInput = false;
    Package->SetDirtyFlag(true);
    auto PreviewToken = [&]()
    {
        const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPAssetRevert(AssetPath);
        AddInfo(Preview);
        FString Before, Token;
        TestTrue(TEXT("Preview returns confirmation"), Preview.Split(TEXT("Confirm="), &Before, &Token));
        TestTrue(TEXT("Preview reports dirty state"), Preview.Contains(TEXT("dirty=true")));
        return Token;
    };
    const FString FirstToken = PreviewToken();
    TestFalse(TEXT("Preview preserves unsaved value"), Asset->bConsumeInput);
    TestTrue(TEXT("Preview preserves dirty flag"), Package->IsDirty());
    TestTrue(TEXT("Wrong token refuses"), UTurboMCPEditToolset::EditTurboMCPAssetRevert(AssetPath, TEXT("wrong")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Empty token refuses"), UTurboMCPEditToolset::EditTurboMCPAssetRevert(AssetPath, TEXT("")).StartsWith(TEXT("error:")));
    Asset->bConsumeInput = true; // State changes while the dirty flag remains true.
    TestTrue(TEXT("Changed memory invalidates approval"), UTurboMCPEditToolset::EditTurboMCPAssetRevert(AssetPath, FirstToken).Contains(TEXT("changed after preview")));
    Asset->bConsumeInput = false;
    const FString Token = PreviewToken();
    const FString Result = UTurboMCPEditToolset::EditTurboMCPAssetRevert(AssetPath, Token);
    AddInfo(Result);
    TestTrue(TEXT("Saved data asset reload succeeds"), Result.StartsWith(TEXT("changed:")));
    // Never dereference the original asset/package after stock reload.
    UInputAction* Fresh = FindObject<UInputAction>(nullptr, *AssetPath);
    TestNotNull(TEXT("Asset resolves after reload"), Fresh);
    if (Fresh)
    {
        TestTrue(TEXT("Saved field value restored"), Fresh->bConsumeInput);
        TestFalse(TEXT("Restored package is clean"), Fresh->GetOutermost()->IsDirty());
    }
    TestTrue(TEXT("Disk was not rewritten"), SavedHash == FMD5Hash::HashFile(*Filename));
    TestTrue(TEXT("Result explains undo effects"), Result.Contains(TEXT("undo history")));
    TestTrue(TEXT("Consumed token refuses"), UTurboMCPEditToolset::EditTurboMCPAssetRevert(AssetPath, Token).StartsWith(TEXT("error:")));
    // Close the reloaded fixture linker before deleting its owned file on Windows.
    if (Fresh) ResetLoaders(Fresh->GetOutermost());
    // Delete only the uniquely named file created by this admitted fixture.
    TestTrue(TEXT("Owned fixture file removed"), IFileManager::Get().Delete(*Filename, true, false));
    return !HasAnyErrors();
}
#endif
