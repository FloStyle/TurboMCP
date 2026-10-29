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
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
    const FString ControlName = Name + TEXT("_Control");
    UPackage* ControlPackage = CreatePackage(*(TEXT("/Game/Tests/") + ControlName));
    UBlueprint* Control = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), ControlPackage, *ControlName,
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Unrelated control created"), Control)) return false;
    TStrongObjectPtr<UBlueprint> KeepControl(Control);
    Control->BlueprintDescription = TEXT("control saved");
    FKismetEditorUtilities::CompileBlueprint(Control);
    const FString ControlFile = FPackageName::LongPackageNameToFilename(ControlPackage->GetName(),FPackageName::GetAssetPackageExtension());
    FSavePackageArgs ControlSaveArgs; ControlSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!TestTrue(TEXT("Control baseline saved"),UPackage::SavePackage(ControlPackage,Control,*ControlFile,ControlSaveArgs))) return false;
    const FMD5Hash ControlHash = FMD5Hash::HashFile(*ControlFile);
    Control->BlueprintDescription = TEXT("control unsaved"); ControlPackage->SetDirtyFlag(true);
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP save undo fixture")));
        BP->Modify(); BP->BlueprintDescription = TEXT("edited");
    }
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset);
    AddInfo(Preview);
    FString Before, Confirm;
    if (!TestTrue(TEXT("Undo preview supplies exact confirmation"), Preview.Split(TEXT("Confirm="), &Before, &Confirm))) return false;
    TestTrue(TEXT("Wrong asset refuses undo"),UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Control->GetPathName()).StartsWith(TEXT("error:")));
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
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP multi-package control")));
        BP->Modify(); Control->Modify();
        BP->BlueprintDescription = TEXT("cross target"); Control->BlueprintDescription = TEXT("control unsaved");
    }
    TestTrue(TEXT("Cross-package undo refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset).Contains(TEXT("another package")));
    TestEqual(TEXT("Refusal preserves control value"),Control->BlueprintDescription,FString(TEXT("control unsaved")));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP stale preview fixture")));
        BP->Modify(); BP->BlueprintDescription = TEXT("first newer edit");
    }
    FString Prefix, StaleConfirm;
    TestTrue(TEXT("Stale-preview setup"),UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset).Split(TEXT("Confirm="),&Prefix,&StaleConfirm));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP newest edit fixture")));
        BP->Modify(); BP->BlueprintDescription = TEXT("human change");
        TestTrue(TEXT("Active transaction refuses undo"),UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(Asset).StartsWith(TEXT("error:")));
        TestTrue(TEXT("Active transaction refuses save"),UTurboMCPEditToolset::EditTurboMCPSave(Asset).StartsWith(TEXT("error:")));
    }
    TestTrue(TEXT("New transaction invalidates old confirmation"),UTurboMCPEditToolset::EditTurboMCPAssetUndo(Asset,StaleConfirm).StartsWith(TEXT("error:")));
    TestEqual(TEXT("Stale refusal preserves target"),BP->BlueprintDescription,FString(TEXT("human change")));
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
    BP->BlueprintDescription = TEXT("normal save value"); BP->GetOutermost()->SetDirtyFlag(true);
    const FString NormalSave = UTurboMCPEditToolset::EditTurboMCPSave(Asset);
    AddInfo(NormalSave);
    TestTrue(TEXT("Normal save reports package write"),NormalSave.Contains(TEXT("package write: saved")));
    BP->BlueprintDescription = TEXT("later unsaved value"); BP->GetOutermost()->SetDirtyFlag(true);
    Packages = {BP->GetOutermost()};
    if (!TestTrue(TEXT("Normal-save stock reload succeeds"),UPackageTools::ReloadPackages(Packages,ReloadError,EReloadPackagesInteractionMode::AssumePositive))) return false;
    BP = FindObject<UBlueprint>(nullptr,*Asset);
    if (!TestNotNull(TEXT("Normal-save reloaded Blueprint exists"),BP)) return false;
    TestEqual(TEXT("Normal-save disk value verified"),BP->BlueprintDescription,FString(TEXT("normal save value")));
    TestEqual(TEXT("Unrelated memory preserved"),Control->BlueprintDescription,FString(TEXT("control unsaved")));
    TestTrue(TEXT("Unrelated dirty flag preserved"),ControlPackage->IsDirty());
    TestTrue(TEXT("Unrelated disk bytes preserved"),ControlHash == FMD5Hash::HashFile(*ControlFile));
    BP->BlueprintDescription = TEXT("partial batch saved value"); BP->GetOutermost()->SetDirtyFlag(true);
    const FString Partial = UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(Asset + TEXT("|/Game/Tests/DefinitelyMissingSaveUndoAsset"));
    AddInfo(Partial);
    TestTrue(TEXT("Mixed batch reports successful item"),Partial.Contains(TEXT("saved 1 of 2")));
    TestTrue(TEXT("Mixed batch reports failed item"),Partial.Contains(TEXT("1 failed")));
    TestEqual(TEXT("Mixed batch preserves unrelated memory"),Control->BlueprintDescription,FString(TEXT("control unsaved")));
    TestTrue(TEXT("Mixed batch preserves unrelated disk"),ControlHash == FMD5Hash::HashFile(*ControlFile));
    FAssetRegistryModule::AssetCreated(BP);
    const FString PackageName = BP->GetOutermost()->GetName();
    Packages = {BP->GetOutermost()};
    BP = nullptr; Package = nullptr;
    FText UnloadError;
    if (!TestTrue(TEXT("Owned saved target unloads"),UPackageTools::UnloadPackages(Packages,UnloadError)))
    { AddError(UnloadError.ToString()); return false; }
    Packages.Reset();
    TestNull(TEXT("Target really is unloaded before save"),FindObject<UBlueprint>(nullptr,*Asset));
    const FString UnloadedSave = UTurboMCPEditToolset::EditTurboMCPSaveWithoutThumbnail(Asset);
    AddInfo(UnloadedSave);
    TestTrue(TEXT("Registry-known unloaded asset saves"),UnloadedSave.Contains(TEXT("package write: saved")));
    BP = FindObject<UBlueprint>(nullptr,*Asset);
    if (TestNotNull(TEXT("Save loaded the requested target"),BP))
        TestEqual(TEXT("Loaded target retains persisted data"),BP->BlueprintDescription,FString(TEXT("partial batch saved value")));
    TestTrue(TEXT("Final control disk unchanged"),ControlHash == FMD5Hash::HashFile(*ControlFile));
    return !HasAnyErrors();
}
#endif
