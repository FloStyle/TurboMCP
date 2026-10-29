// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "PackageTools.h"
#include "Misc/SecureHash.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPSaveUndoWorkflowTest,
    "TurboMCP.Workflow.SaveUndo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPSaveUndoWorkflowTest::RunTest(const FString& Parameters)
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

    const FString Name = TEXT("SaveUndo_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/") + Name));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, *Name,
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Owned Blueprint created"), BP)) return false;
    BP->BlueprintDescription = TEXT("original");
    const FString Asset = BP->GetPathName();
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP save undo fixture")));
        BP->Modify(); BP->BlueprintDescription = TEXT("edited");
    }
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset);
    AddInfo(Preview);
    FString Before, Confirm;
    if (!TestTrue(TEXT("Undo preview supplies exact confirmation"), Preview.Split(TEXT("Confirm="), &Before, &Confirm))) return false;
    TestEqual(TEXT("Preview leaves value unchanged"), BP->BlueprintDescription, FString(TEXT("edited")));
    TestTrue(TEXT("Missing confirmation refuses"), UTurboMCPEditToolset::EditTurboMCPAssetUndo(Asset,TEXT("")).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusal leaves value unchanged"), BP->BlueprintDescription, FString(TEXT("edited")));
    const FString Undo = UTurboMCPEditToolset::EditTurboMCPAssetUndo(Asset,Confirm);
    AddInfo(Undo);
    TestTrue(TEXT("Confirmed undo executes"), Undo.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Undo restores original value"), BP->BlueprintDescription, FString(TEXT("original")));
    TestTrue(TEXT("Replayed confirmation refuses"), UTurboMCPEditToolset::EditTurboMCPAssetUndo(Asset,Confirm).StartsWith(TEXT("error:")));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Human edit control")));
        BP->Modify(); BP->BlueprintDescription = TEXT("human change");
    }
    TestTrue(TEXT("Human transaction refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Human change preserved"), BP->BlueprintDescription, FString(TEXT("human change")));
    TestTrue(TEXT("Invalid dirty limit refuses"), UTurboMCPReadToolset::ReadTurboMCPDirtyList(TEXT("0")).StartsWith(TEXT("error:")));
    Package->SetDirtyFlag(true);
    const FString Dirty = UTurboMCPReadToolset::ReadTurboMCPDirtyList(TEXT("1000"));
    TestTrue(TEXT("Dirty list identifies target"), Dirty.Contains(Package->GetName()));
    const FString Saved = UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(Asset);
    AddInfo(Saved);
    TestTrue(TEXT("Direct package save reports write"), Saved.Contains(TEXT("package write: saved")));
    TestFalse(TEXT("Successful save clears dirty flag"), Package->IsDirty());
    TestTrue(TEXT("Saved file exists"), FPaths::FileExists(FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension())));
    const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    const FMD5Hash SavedHash = FMD5Hash::HashFile(*File);
    // Deliberately diverge memory, then use the stock package reload API, not
    // the save tool's status or another TurboMCP tool, to verify persisted data.
    BP->BlueprintDescription = TEXT("not saved");
    Package->SetDirtyFlag(true);
    TArray<UPackage*> Packages{Package};
    FText ReloadError;
    if (!TestTrue(TEXT("Independent stock reload succeeds"), UPackageTools::ReloadPackages(Packages,ReloadError,EReloadPackagesInteractionMode::AssumePositive)))
    { AddError(ReloadError.ToString()); return false; }
    BP = FindObject<UBlueprint>(nullptr,*Asset);
    if (!TestNotNull(TEXT("Reloaded target exists"),BP)) return false;
    TestEqual(TEXT("Disk contains saved value, not later memory value"),BP->BlueprintDescription,FString(TEXT("human change")));
    TestTrue(TEXT("Reload did not rewrite disk"),SavedHash == FMD5Hash::HashFile(*File));
    // Remaining acceptance checks include normal save, unloaded save targets,
    // unrelated dirty assets and cross-package/stale undo boundary controls.
    return !HasAnyErrors();
}
#endif
