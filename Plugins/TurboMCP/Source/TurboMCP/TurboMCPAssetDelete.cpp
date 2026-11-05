// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Editor.h"
#include "AssetDeleteModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/World.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"

namespace
{
struct FDeleteConfirmation
{
    FString Identity;
    double Expires = 0;
};
TMap<FString, FDeleteConfirmation> Confirmations;
FString DeleteAsset(bool Apply, const FString& AssetPath, const FString& Confirmation)
{
    auto Refuse = [](const FString& Why) { return TEXT("unchanged\nrefused: ") + Why; };
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive() || !GEditor->CanTransact())
        return Refuse(TEXT("Requires an idle editor outside PIE and active transactions."));
    const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    if (!PackageName.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(PackageName)
        || AssetPath != PackageName + TEXT(".") + AssetName)
        return Refuse(TEXT("Use one exact /Game asset object path, including its object suffix."));
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets()) return Refuse(TEXT("The asset registry is still scanning. Retry after discovery completes."));
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPackageName(FName(*PackageName), Assets);
    if (Assets.Num() != 1 || Assets[0].GetSoftObjectPath().ToString() != AssetPath)
        return Refuse(TEXT("The registry must identify exactly one asset in the named package."));
    FString Filename;
    if (!FPackageName::DoesPackageExist(PackageName, &Filename)) return Refuse(TEXT("Only saved assets with an existing package file can be deleted."));
    if (IFileManager::Get().IsReadOnly(*Filename)) return Refuse(TEXT("The package file is read-only; no permissions were changed."));
    if (ISourceControlModule::Get().IsEnabled())
    {
        auto& Provider = ISourceControlModule::Get().GetProvider();
        if (!Provider.IsAvailable()) return Refuse(TEXT("Source control is unavailable."));
        const auto State = Provider.GetState(Filename, EStateCacheUsage::ForceUpdate);
        if (!State.IsValid() || State->IsUnknown() || State->IsConflicted() || State->IsCheckedOutOther()
            || (State->IsSourceControlled() && (!State->IsCurrent() || !State->CanDelete())))
            return Refuse(TEXT("Source-control state does not permit a checked deletion. Resolve it before previewing again."));
    }
    UObject* Target = Assets[0].GetAsset();
    if (!IsValid(Target) || Target->IsA<UWorld>() || Target->IsA<UObjectRedirector>() || !Target->IsAsset())
        return Refuse(TEXT("Maps, redirectors and invalid/non-asset targets are unsupported."));
    // StrongObjectPtr keeps an object alive through the UObject ref count,
    // which is not necessarily represented by a UObject referencer edge.
    if (Target->IsRooted() || Target->GetRefCount() > 0) return Refuse(TEXT("Live strong references or rooting prevent deletion."));
    if (Target->GetOutermost()->IsDirty()) return Refuse(TEXT("The asset has unsaved changes. Save or discard them separately."));
    UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!Editors || Editors->FindEditorForAsset(Target, false)) return Refuse(TEXT("Close the asset editor before previewing deletion."));
    TArray<FName> References;
    Registry.GetReferencers(FName(*PackageName), References);
    if (!References.IsEmpty()) return Refuse(TEXT("Registry references remain; deletion was not attempted."));
    const FMD5Hash FileHash = FMD5Hash::HashFile(*Filename);
    if (!FileHash.IsValid()) return Refuse(TEXT("The package file could not be fingerprinted; no deletion was attempted."));
    const FString Identity = AssetPath + TEXT("|") + LexToString(Target->GetUniqueID()) + TEXT("|") + LexToString(FileHash);
    const double Now = FPlatformTime::Seconds();
    for (auto It = Confirmations.CreateIterator(); It; ++It) if (It.Value().Expires < Now) It.RemoveCurrent();
    if (Apply)
    {
        const FDeleteConfirmation* Pending = Confirmations.Find(Confirmation);
        if (!Pending || Pending->Identity != Identity) return Refuse(TEXT("Preview again: confirmation is missing, expired or stale."));
    }
    // Never reuse a preview model: construction collects garbage and the scan
    // must observe current live, disk and undo references before mutation.
    FAssetDeleteModel Model(TArray<UObject*>{Target});
    const double Deadline = FPlatformTime::Seconds() + 10.0;
    while (Model.GetState() != FAssetDeleteModel::Finished && FPlatformTime::Seconds() < Deadline) Model.Tick(0.0f);
    if (Model.GetState() != FAssetDeleteModel::Finished) return Refuse(TEXT("Reference scanning did not finish; no deletion was attempted."));
    if (!Model.CanDelete() || Model.IsAnythingReferencedInMemoryByUndo())
        return Refuse(TEXT("Live, disk or undo references prevent deletion. No references were replaced and undo history was not cleared."));
    if (!IsValid(Target) || Target->IsRooted() || Target->GetRefCount() > 0 || Target->GetOutermost()->IsDirty() || IFileManager::Get().IsReadOnly(*Filename)
        || Identity != AssetPath + TEXT("|") + LexToString(Target->GetUniqueID()) + TEXT("|") + LexToString(FMD5Hash::HashFile(*Filename)))
        return Refuse(TEXT("The target changed during reference scanning. Preview again."));
    if (!Apply)
    {
        if (Confirmations.Num() >= 64) Confirmations.Empty();
        const FString Token = TEXT("delete ") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Confirmations.Add(Token, {Identity, FPlatformTime::Seconds() + 60.0});
        return TEXT("unchanged\npreview: checked deletion of ") + AssetPath + TEXT("\nLoading and garbage collection ran; no asset or source file was deleted. Deletion is permanent and cannot be undone.\nconfirmation: ") + Token + TEXT("\nExpires in 60 seconds; edit repeats all checks.");
    }
    // Scanning may take time; confirmation must still be valid at mutation.
    const FDeleteConfirmation* Pending = Confirmations.Find(Confirmation);
    if (!Pending || Pending->Expires < FPlatformTime::Seconds())
        return Refuse(TEXT("Confirmation expired during reference scanning. Preview again."));
    Confirmations.Remove(Confirmation);
    Model.DoDelete();
    // DoDelete's boolean is not a success verdict. It can return true even
    // when an editor veto or filesystem failure prevented actual deletion.
    TArray<FAssetData> Remaining;
    Registry.GetAssetsByPackageName(FName(*PackageName), Remaining);
    const bool FileRemains = IFileManager::Get().FileExists(*Filename);
    const int32 Deleted = Model.GetDeletedObjectCount();
    if (Deleted < 1 || !Remaining.IsEmpty() || FileRemains)
        return FString::Printf(TEXT("changed\nerror: deletion outcome is incomplete or unverified; deleted objects=%d, registry assets=%d, file remains=%s. Inspect the asset and source control; do not retry automatically. No rollback is claimed."), Deleted, Remaining.Num(), FileRemains ? TEXT("yes") : TEXT("no"));
    return TEXT("changed\nDeleted asset: ") + AssetPath + TEXT("\nVerified: registry entry and package file are absent. Imported source files were not requested for deletion. No undo or rollback is available.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPAssetDelete(const FString& AssetPath) { return DeleteAsset(false, AssetPath, FString()); }
FString UTurboMCPEditToolset::EditTurboMCPAssetDelete(const FString& AssetPath, const FString& Confirmation) { return DeleteAsset(true, AssetPath, Confirmation); }
