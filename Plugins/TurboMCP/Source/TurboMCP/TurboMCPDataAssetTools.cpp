// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPDataAssetValues.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPResult.h"
#include "Editor.h"
#include "Engine/DataAsset.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace
{
using TurboMCPDataAssetValues::FValue;
FString Export(const FProperty* Property, const void* Value, UObject* Owner)
{
    FString Text;
    Property->ExportTextItem_Direct(Text, Value, nullptr, Owner, PPF_None);
    return Text;
}
bool Editable(const FProperty* Property)
{
    return Property && Property->ArrayDim == 1 && Property->HasAnyPropertyFlags(CPF_Edit) &&
        !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_DisableEditOnTemplate);
}
struct FStoredField
{
    UObject* Owner;
    FProperty* Property;
    TUniquePtr<FValue> Value;
};
struct FSnapshot
{
    TArray<TStrongObjectPtr<UObject>> Retained;
    TArray<FStoredField> Fields;
    FString Revision;
    explicit FSnapshot(UObject* Asset)
    {
        TArray<UObject*> Objects;
        GetObjectsWithOuter(Asset, Objects, EGetObjectsFlags::IncludeNestedObjects);
        Objects.Add(Asset);
        Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
        FString Text;
        for (UObject* Object : Objects)
        {
            if (!IsValid(Object)) continue;
            Retained.Emplace(Object);
            // Snapshot buffers are not UPROPERTY storage. Retain references that a notification
            // could otherwise collect after replacing a live reference.
            TArray<UObject*> References;
            FReferenceFinder Finder(References, nullptr, false, true, false, false);
            Finder.FindReferences(Object);
            for (UObject* Reference : References) if (IsValid(Reference)) Retained.Emplace(Reference);
            Text += Object->GetPathName() + TEXT(":") + Object->GetClass()->GetPathName() + TEXT("\n");
            for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
            {
                if (It->HasAnyPropertyFlags(CPF_Transient)) continue;
                void* Address = It->ContainerPtrToValuePtr<void>(Object);
                const FString Encoded = Export(*It, Address, Object);
                Text += FString::Printf(TEXT("%s:%d:"), *It->GetName(), Encoded.Len()) + Encoded + TEXT("\n");
                Fields.Add({Object, *It, MakeUnique<FValue>(*It, Address)});
            }
        }
        Revision = FMD5::HashAnsiString(*Text);
    }
    bool Matches() const
    {
        for (const FStoredField& Field : Fields)
            if (!IsValid(Field.Owner) || !Field.Property->Identical(
                Field.Value->Get(), Field.Property->ContainerPtrToValuePtr<void>(Field.Owner), PPF_None)) return false;
        return true;
    }
    void Restore() const
    {
        for (const FStoredField& Field : Fields)
            if (IsValid(Field.Owner)) Field.Property->CopyCompleteValue(
                Field.Property->ContainerPtrToValuePtr<void>(Field.Owner), Field.Value->Get());
    }
};
struct FTarget
{
    UObject* Owner = nullptr;
    FProperty* Property = nullptr;
    FProperty* NotifyProperty = nullptr;
    void* Address = nullptr;
};
bool ResolveField(UDataAsset* Asset, const FString& Path, FTarget& Target, FString& Error)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("."), false);
    if (Parts.IsEmpty() || Parts.Num() > 12) { Error = TEXT("Property requires 1 to 12 exact reflected path segments."); return false; }
    void* Container = Asset;
    UStruct* Type = Asset->GetClass();
    Target.Owner = Asset;
    for (int32 Index = 0; Index < Parts.Num(); ++Index)
    {
        FProperty* Property = Parts[Index].IsEmpty() ? nullptr : FindFProperty<FProperty>(Type, *Parts[Index]);
        if (!Editable(Property)) { Error = TEXT("Missing or non-editable property: ") + Parts[Index]; return false; }
        if (!Target.NotifyProperty) Target.NotifyProperty = Property;
        void* Address = Property->ContainerPtrToValuePtr<void>(Container);
        if (Index == Parts.Num() - 1) { Target.Property = Property; Target.Address = Address; return true; }
        if (auto* Struct = CastField<FStructProperty>(Property)) { Type = Struct->Struct; Container = Address; continue; }
        if (auto* Object = CastField<FObjectProperty>(Property))
        {
            UObject* Child = Object->GetObjectPropertyValue(Address);
            if (Property->HasAnyPropertyFlags(CPF_InstancedReference) && IsValid(Child) && Child->IsIn(Asset))
            {
                Target.Owner = Child; Target.NotifyProperty = nullptr;
                Type = Child->GetClass(); Container = Child; continue;
            }
        }
        Error = TEXT("Paths may traverse structs and existing owned instanced children only. External objects and containers are not path segments.");
        return false;
    }
    return false;
}
FString Operate(const FString& AssetPath, const FString& Path, const FString& Operation,
    const FString& Key, const FString& Value, const FString& Revision, int32 Mode)
{
    FTurboMCPResult Result;
    Result.OperationId = FTurboMCPResult::NewOperationId();
    auto Refuse = [&](const FString& Message)
    {
        Result.Outcome = ETurboMCPResultOutcome::Refused; Result.Code = TEXT("invalid_request");
        return Result.AppendTo(TEXT("refused: ") + Message + TEXT(" No property write was applied."));
    };
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive() || (Mode && !GEditor->CanTransact()))
        return Refuse(TEXT("Use an idle editor outside play and active transactions."));
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString ResolveError = FTurboMCPAssetResolver::Resolve(AssetPath, Resolved);
    if (!ResolveError.IsEmpty()) return Refuse(ResolveError);
    UDataAsset* Asset = LoadObject<UDataAsset>(nullptr, *Resolved.ObjectPath);
    if (!Asset || Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return Refuse(TEXT("Select an existing data asset, not a class default or another asset type."));
    Result.Assets.Add(Asset->GetPathName());
    FSnapshot Baseline(Asset);
    const bool WasDirty = Asset->GetOutermost()->IsDirty();
    const FString Header = TEXT("asset: ") + Asset->GetPathName() + TEXT("\nclass: ") + Asset->GetClass()->GetPathName() + TEXT("\nrevision: ") + Baseline.Revision;
    if (!Mode && Path.IsEmpty())
    {
        FString Body = TEXT("read: Editable data-asset properties.\n") + Header;
        for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It)
            if (Editable(*It)) Body += TEXT("\n") + It->GetName() + TEXT(" [") + It->GetCPPType() + TEXT("]");
        return Result.AppendTo(Body + TEXT("\nRead an exact Property for its current value. Save and compile are separate."));
    }
    FTarget Target;
    FString Error;
    if (!ResolveField(Asset, Path, Target, Error)) return Refuse(Error);
    const FString Before = Export(Target.Property, Target.Address, Target.Owner);
    if (!Mode) return Result.AppendTo(TEXT("read: Data-asset property.\n") + Header + TEXT("\nproperty: ") + Path + TEXT("\ntype: ") + Target.Property->GetCPPType() + TEXT("\nvalue: ") + Before + TEXT("\nformat: Unreal property text for values; exact loaded object paths for references. Map operations take one key and value separately."));
    if (Revision.IsEmpty() || Revision != Baseline.Revision) return Refuse(TEXT("State revision is missing or stale. Read the asset again before previewing or editing."));
    FValue Planned(Target.Property, Target.Address);
    TStrongObjectPtr<UObject> HeldKey, HeldValue;
    auto Parse = [&](FProperty* Property, const FString& Input, bool IsKey, void* Out, TStrongObjectPtr<UObject>& Held)
    {
        if (CastField<FObjectPropertyBase>(Property)) return TurboMCPDataAssetValues::ResolveReference(Property, Input, Asset, IsKey, Out, Held, Error);
        return TurboMCPDataAssetValues::ParseValue(Property, Input, Out, Error);
    };
    if (Operation == TEXT("set"))
    {
        if (!Key.IsEmpty()) return Refuse(TEXT("Key must be empty for set."));
        if (!Parse(Target.Property, Value, false, Planned.Get(), HeldValue)) return Refuse(Error);
    }
    else if (Operation == TEXT("map_upsert") || Operation == TEXT("map_remove"))
    {
        auto* Map = CastField<FMapProperty>(Target.Property);
        if (!Map || Key.IsEmpty()) return Refuse(TEXT("Map operations require a map property and a nonempty Key."));
        const bool Remove = Operation == TEXT("map_remove");
        if (Remove && !Value.IsEmpty()) return Refuse(TEXT("Value must be empty for map_remove."));
        FValue ParsedKey(Map->KeyProp), ParsedValue(Map->ValueProp);
        if (!Parse(Map->KeyProp, Key, true, ParsedKey.Get(), HeldKey)) return Refuse(Error);
        if (!Remove && !Parse(Map->ValueProp, Value, false, ParsedValue.Get(), HeldValue)) return Refuse(Error);
        TurboMCPDataAssetValues::PlanMapEntry(Map, Target.Address, ParsedKey.Get(), ParsedValue.Get(), Remove, Planned.Get());
    }
    else return Refuse(TEXT("Operation must be set, map_upsert or map_remove."));
    const FString Proposed = Export(Target.Property, Planned.Get(), Target.Owner);
    const FString Detail = Header + TEXT("\nproperty: ") + Path + TEXT("\noperation: ") + Operation + TEXT("\nbefore: ") + Before + TEXT("\nafter: ") + Proposed;
    if (Mode == 1) { Result.Outcome = ETurboMCPResultOutcome::Planned; return Result.AppendTo(TEXT("preview: Validated temporary values; live properties unchanged.\n") + Detail); }
    if (Target.Property->Identical(Target.Address, Planned.Get(), PPF_None))
    { Result.Outcome = ETurboMCPResultOutcome::Unchanged; return Result.AppendTo(TEXT("unchanged: The requested value is already present.\n") + Detail); }
    FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "DataAssetProperty", "Edit data-asset property"));
    Asset->SetFlags(RF_Transactional); Asset->Modify();
    if (Target.Owner != Asset) { Target.Owner->SetFlags(RF_Transactional); Target.Owner->Modify(); }
    Target.Owner->PreEditChange(Target.NotifyProperty);
    // Pre-edit callbacks can change state. Do not install a plan derived from an obsolete baseline.
    bool Applied = false;
    bool Verified = false;
    if (Baseline.Matches())
    {
        Target.Property->CopyCompleteValue(Target.Address, Planned.Get());
        FSnapshot Expected(Asset);
        FPropertyChangedEvent Event(Target.NotifyProperty, EPropertyChangeType::ValueSet);
        Target.Owner->PostEditChangeProperty(Event);
        Applied = true;
        FSnapshot Actual(Asset);
        Verified = Expected.Matches() && Actual.Revision == Expected.Revision;
    }
    Result.Effects.Add(TEXT("edit_notifications_may_execute_project_code"));
    if (!Verified)
    {
        Baseline.Restore();
        FSnapshot Restored(Asset);
        const bool RestoredExactly = Baseline.Matches() && Restored.Revision == Baseline.Revision;
        Transaction.Cancel();
        if (RestoredExactly) Asset->GetOutermost()->SetDirtyFlag(WasDirty);
        Result.Outcome = ETurboMCPResultOutcome::Failed; Result.Code = TEXT("notification_state_mismatch");
        Result.Rollback = RestoredExactly ? ETurboMCPResultRollback::Complete : ETurboMCPResultRollback::Failed;
        Result.bComplete = false;
        if (!RestoredExactly) Result.SetChangedUnknown();
        return Result.AppendTo(FString(TEXT("failed: ")) + (Applied ? TEXT("Post-edit readback did not match the plan. ") : TEXT("Pre-edit notification changed the baseline. ")) +
            (RestoredExactly ? TEXT("Captured asset values were restored and verified. External callback effects are not reversible by this tool.") : TEXT("Restoration could not be verified. Inspect the asset; do not save or retry blindly.")));
    }
    Asset->MarkPackageDirty();
    FSnapshot Final(Asset);
    Result.Outcome = ETurboMCPResultOutcome::Changed; Result.SetChanged(true);
    return Result.AppendTo(TEXT("edited: Property updated and readback verified. Not saved.\n") + Detail + TEXT("\ncurrent_revision: ") + Final.Revision + TEXT("\ndirty: true\nProject edit notifications ran; external callback effects are outside this asset snapshot."));
}
}
FString UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(const FString& Asset, const FString& Property)
{ return Operate(Asset, Property, TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision)
{ return Operate(Asset, Property, Operation, Key, Value, Revision, 1); }
FString UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision)
{ return Operate(Asset, Property, Operation, Key, Value, Revision, 2); }
