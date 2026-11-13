// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPDataAssetValues.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/TransactionObjectEvent.h"
#include "UObject/ObjectKey.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "SourceControlOperations.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
// Only assets created by this tool participate. Registry bookkeeping is not
// transactional in stock Unreal, although the UObject creation itself is.
class FCreationRegistryUndo
{
public:
    FCreationRegistryUndo()
    {
        Handle = FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FCreationRegistryUndo::OnTransacted);
    }
    ~FCreationRegistryUndo() { FCoreUObjectDelegates::OnObjectTransacted.Remove(Handle); }
    void Track(UObject* Object) { Created.Add(FObjectKey(Object), true); }
private:
    void OnTransacted(UObject* Object, const FTransactionObjectEvent& Event)
    {
        if (Event.GetEventType() != ETransactionObjectEventType::UndoRedo) return;
        bool* Registered = Created.Find(FObjectKey(Object));
        if (!Registered) return;
        const bool Live = IsValid(Object);
        if (Live == *Registered) return;
        if (Live)
            FAssetRegistryModule::AssetCreated(Object);
        else
        {
            // AssetDeleted requires IsAsset(), which rejects garbage objects. Undo has
            // just restored that flag; expose it only for synchronous registry removal.
            Object->ClearGarbage();
            FAssetRegistryModule::AssetDeleted(Object);
            Object->MarkAsGarbage();
        }
        *Registered = Live;
    }
    TMap<FObjectKey, bool> Created;
    FDelegateHandle Handle;
};
FCreationRegistryUndo& CreationRegistryUndo()
{
    static FCreationRegistryUndo Instance;
    return Instance;
}

struct FInitialField
{
    FProperty* Property;
    TUniquePtr<TurboMCPDataAssetValues::FValue> Value;
    TStrongObjectPtr<UObject> Reference;
};

FString CreateDataAsset(bool Apply, const FString& ClassPath, const FString& PackagePath,
    const FString& InitialFieldsJSON, bool Save, bool AddToSourceControl)
{
    auto Refuse = [](const FString& Why) { return TEXT("unchanged\nrefused: ") + Why; };
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() ||
        GEditor->IsTransactionActive() || !GEditor->CanTransact())
        return Refuse(TEXT("An idle editor game thread outside PIE and active transactions is required."));
    if (AddToSourceControl && !Save)
        return Refuse(TEXT("AddToSourceControl requires Save=true. No asset was created."));
    UClass* Class = FindObject<UClass>(nullptr, *ClassPath);
    if (!IsValid(Class) || Class->GetPathName() != ClassPath || !Class->IsChildOf(UDataAsset::StaticClass()) ||
        Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Transient) ||
        Class->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
        return Refuse(TEXT("ClassPath must identify an already loaded, concrete, non-transient UDataAsset subclass. Native and Blueprint-generated classes are supported. No class was loaded."));
    if (!UPackage::StaticClass()->IsChildOf(Class->ClassWithin))
        return Refuse(TEXT("This class cannot be constructed directly inside a package."));
    // Requesting an existing default object avoids executing a constructor during preview.
    if (!Class->GetDefaultObject(false))
        return Refuse(TEXT("The class default object is not initialized. Initialize the class explicitly before creating an asset."));
    TStrongObjectPtr<UClass> HeldClass(Class);
    FText Reason;
    if (!PackagePath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(PackagePath, false, &Reason))
        return Refuse(TEXT("PackagePath must be a new /Game package path without an object suffix or extension. ") + Reason.ToString());
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, Filename, FPackageName::GetAssetPackageExtension()))
        return Refuse(TEXT("The destination has no mounted disk path."));
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    auto Occupied = [&]()
    {
        TArray<FAssetData> Assets;
        Registry.Get().GetAssetsByPackageName(FName(*PackagePath), Assets);
        return FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath) || !Assets.IsEmpty();
    };
    if (Occupied()) return Refuse(TEXT("The destination already exists in memory, the registry or on disk: ") + PackagePath);
    TSharedPtr<FJsonObject> FieldsJSON;
    const FString Input = InitialFieldsJSON.IsEmpty() ? TEXT("{}") : InitialFieldsJSON;
    if (Input.Len() > 65536 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), FieldsJSON) || !FieldsJSON)
        return Refuse(TEXT("InitialFieldsJSON must be a JSON object, at most 65536 characters, with reflected field names and Unreal property-text string values."));
    TArray<FInitialField> Fields;
    for (const auto& Item : FieldsJSON->Values)
    {
        FProperty* Property = FindFProperty<FProperty>(Class, *Item.Key);
        FString Value, Error;
        if (!Property || Property->ArrayDim != 1 || !Property->HasAnyPropertyFlags(CPF_Edit) ||
            Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated | CPF_DisableEditOnTemplate) ||
            !Item.Value->TryGetString(Value))
            return Refuse(TEXT("Each initial field must be editable and have a string value: ") + FString(*Item.Key));
        FInitialField Planned{Property, MakeUnique<TurboMCPDataAssetValues::FValue>(Property), TStrongObjectPtr<UObject>()};
        if (CastField<FObjectPropertyBase>(Property))
        {
            // A new asset cannot yet own an instanced reference. Never import external
            // objects into an owned field or run a constructor during validation.
            if (Property->HasAnyPropertyFlags(CPF_InstancedReference))
                return Refuse(TEXT("Initialize owned child objects after creation with create_child: ") + FString(*Item.Key));
            if (!TurboMCPDataAssetValues::ResolveReference(Property, Value, nullptr, true,
                Planned.Value->Get(), Planned.Reference, Error))
                return Refuse(FString(*Item.Key) + TEXT(": ") + Error);
        }
        else if (!TurboMCPDataAssetValues::ParseValue(Property, Value, Planned.Value->Get(), Error))
            return Refuse(FString(*Item.Key) + TEXT(": ") + Error);
        Fields.Add(MoveTemp(Planned));
    }
    const FString Name = FPackageName::GetLongPackageAssetName(PackagePath);
    const FString ObjectPath = PackagePath + TEXT(".") + Name;
    if (!Apply)
        return TEXT("unchanged\npreview: create ") + ObjectPath + TEXT("\nclass: ") + ClassPath +
            FString::Printf(TEXT("\ninitial_fields: %d\nsave_requested: %s\nsource_control_add_requested: %s"),
                Fields.Num(), Save ? TEXT("yes") : TEXT("no"), AddToSourceControl ? TEXT("yes") : TEXT("no")) +
            TEXT("\nNo package, asset or constructor was created or invoked. Disk permissions and source-control availability are not guaranteed by preview.");
    if (Occupied()) return Refuse(TEXT("The destination became occupied during preflight."));
    UPackage* Package = nullptr;
    UDataAsset* Asset = nullptr;
    {
        FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "DataAssetCreate", "Create Data Asset"));
        Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            Transaction.Cancel();
            return Refuse(TEXT("The destination package could not be allocated."));
        }
        Package->SetFlags(RF_Transactional);
        Package->Modify(false);
        // Stock Unreal records the new object's pre-existence state in this transaction.
        // Constructors can execute project code; this is not a project-code sandbox.
        Asset = NewObject<UDataAsset>(Package, Class, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
        if (!IsValid(Asset))
            return TEXT("changed\nerror: asset allocation failed after package creation. Nothing was saved. Inspect ") + PackagePath + TEXT("; do not retry blindly.");
        for (const FInitialField& Field : Fields)
            Field.Property->CopyCompleteValue(Field.Property->ContainerPtrToValuePtr<void>(Asset), Field.Value->Get());
        CreationRegistryUndo().Track(Asset);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
    }
    if (!IsValid(Asset) || !IsValid(Package) || Asset->GetClass() != Class || Asset->GetPathName() != ObjectPath)
        return TEXT("changed\nerror: creation callbacks changed the asset identity or validity. Nothing was saved. Inspect ") + PackagePath;
    TStrongObjectPtr<UDataAsset> HeldAsset(Asset);
    auto Result = [&]()
    {
        return TEXT("changed\nasset: ") + ObjectPath + TEXT("\nclass: ") + Asset->GetClass()->GetPathName() +
            TEXT("\nrevision: ") + TurboMCPDataAssetValues::Revision(Asset) +
            TEXT("\nunsaved_changes: ") + (Package->IsDirty() ? TEXT("yes") : TEXT("no"));
    };
    for (const FInitialField& Field : Fields)
        if (!Field.Property->Identical(Field.Property->ContainerPtrToValuePtr<void>(Asset), Field.Value->Get(), PPF_None))
            return Result() + TEXT("\nerror: initial field readback failed. Nothing was saved. Inspect the created asset; do not retry creation.");
    if (!Save)
        return Result() + TEXT("\nsave: not requested\nsource_control_add: not requested\nThe asset exists in editor memory only. Inspect it before saving.");
    // Persistence is deliberately outside the undo transaction. Undo cannot remove saved files.
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.bSlowTask = false;
    const bool Saved = UPackage::SavePackage(Package, Asset, *Filename, Args);
    const int64 FileBytes = IFileManager::Get().FileSize(*Filename);
    if (!Saved || Package->IsDirty() || FileBytes <= 0)
        return Result() + TEXT("\nsave: failed or unverified\nsource_control_add: not attempted\nThe created asset remains in memory; a partial file may exist. Inspect the destination before retrying.");
    FString Output = Result() + FString::Printf(TEXT("\nsave: written (%lld bytes)\nfile: %s\nSaved contents have not been reloaded; undo does not remove the file."), FileBytes, *Filename);
    if (!AddToSourceControl) return Output + TEXT("\nsource_control_add: not requested");
    ISourceControlModule& SourceControl = ISourceControlModule::Get();
    if (!SourceControl.IsEnabled() || !SourceControl.GetProvider().IsAvailable())
        return Output + TEXT("\nsource_control_add: not attempted (provider unavailable)\npartial: the asset was created and saved, but not added to source control.");
    ISourceControlProvider& Provider = SourceControl.GetProvider();
    const TSharedRef<FMarkForAdd, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FMarkForAdd>();
    const ECommandResult::Type AddResult = Provider.Execute(Operation, Filename);
    const FSourceControlStatePtr State = Provider.GetState(Filename, EStateCacheUsage::ForceUpdate);
    if (AddResult != ECommandResult::Succeeded || !State.IsValid() || !State->IsAdded())
        return Output + TEXT("\nsource_control_add: failed or unverified\npartial: the saved asset is preserved. Check the provider state; no checkout or submit was requested.");
    return Output + TEXT("\nsource_control_add: verified open for add\nNo checkout or submit was requested.");
}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetCreate(const FString& ClassPath,
    const FString& PackagePath, const FString& InitialFieldsJSON, bool Save, bool AddToSourceControl)
{
    return CreateDataAsset(false, ClassPath, PackagePath, InitialFieldsJSON, Save, AddToSourceControl);
}
FString UTurboMCPEditToolset::EditTurboMCPDataAssetCreate(const FString& ClassPath,
    const FString& PackagePath, const FString& InitialFieldsJSON, bool Save, bool AddToSourceControl)
{
    return CreateDataAsset(true, ClassPath, PackagePath, InitialFieldsJSON, Save, AddToSourceControl);
}
