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
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
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
                for (int32 ArrayIndex = 0; ArrayIndex < It->ArrayDim; ++ArrayIndex)
                {
                    const FString Encoded = Export(*It, static_cast<const uint8*>(Address) + ArrayIndex * It->ElementSize, Object);
                    Text += FString::Printf(TEXT("%s[%d]:%d:"), *It->GetName(), ArrayIndex, Encoded.Len()) + Encoded + TEXT("\n");
                }
                Fields.Add({Object, *It, MakeUnique<FValue>(*It, Address)});
            }
        }
        Revision = FMD5::HashAnsiString(*Text);
    }
    bool Matches() const
    {
        for (const FStoredField& Field : Fields)
        {
            if (!IsValid(Field.Owner)) return false;
            for (int32 ArrayIndex = 0; ArrayIndex < Field.Property->ArrayDim; ++ArrayIndex)
                if (!Field.Property->Identical(
                    static_cast<const uint8*>(Field.Value->Get()) + ArrayIndex * Field.Property->ElementSize,
                    Field.Property->ContainerPtrToValuePtr<void>(Field.Owner, ArrayIndex), PPF_None)) return false;
        }
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
    TStrongObjectPtr<UObject> HeldKey, HeldValue, TemporaryOwner, ProposedChild, CreatedChild;
    FObjectProperty* ChildProperty = nullptr;
    FMapProperty* ChildMap = nullptr;
    TUniquePtr<FValue> ChildKey;
    FString ChildDescription;
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
    else if (Operation == TEXT("create_child"))
    {
        ChildMap = CastField<FMapProperty>(Target.Property);
        ChildProperty = CastField<FObjectProperty>(ChildMap ? ChildMap->ValueProp : Target.Property);
        if (!ChildProperty || !ChildProperty->HasAnyPropertyFlags(CPF_InstancedReference) || CastField<FClassProperty>(ChildProperty))
            return Refuse(TEXT("create_child requires an instanced object property or an instanced map value."));
        if (ChildMap)
        {
            if (Key.IsEmpty()) return Refuse(TEXT("Key is required when creating a map value."));
            ChildKey = MakeUnique<FValue>(ChildMap->KeyProp);
            if (!Parse(ChildMap->KeyProp, Key, true, ChildKey->Get(), HeldKey)) return Refuse(Error);
            FScriptMapHelper Existing(ChildMap, Target.Address);
            const int32 Index = Existing.FindMapIndexWithKey(ChildKey->Get());
            if (Index != INDEX_NONE && ChildProperty->GetObjectPropertyValue(Existing.GetValuePtr(Index)))
                return Refuse(TEXT("This map entry already has an object. Edit its owned fields or explicitly remove the entry; create_child never overwrites it."));
        }
        else
        {
            if (!Key.IsEmpty()) return Refuse(TEXT("Key must be empty for a direct child property."));
            if (ChildProperty->GetObjectPropertyValue(Target.Address))
                return Refuse(TEXT("The property already has an object. Edit its fields; create_child never overwrites it."));
        }
        TSharedPtr<FJsonObject> Spec;
        if (Value.Len() > 65536 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value), Spec) || !Spec)
            return Refuse(TEXT("create_child Value must be a JSON object with class and optional fields; maximum 65536 characters."));
        for (const auto& Item : Spec->Values)
            if (Item.Key != TEXT("class") && Item.Key != TEXT("fields")) return Refuse(TEXT("Unknown create_child option: ") + Item.Key);
        FString ClassPath;
        if (!Spec->TryGetStringField(TEXT("class"), ClassPath)) return Refuse(TEXT("create_child requires a class path string."));
        UClass* Class = FindObject<UClass>(nullptr, *ClassPath);
        if (!Class || Class->GetPathName() != ClassPath || !Class->IsChildOf(ChildProperty->PropertyClass) ||
            !Class->HasAnyClassFlags(CLASS_EditInlineNew) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
            return Refuse(TEXT("Class must be loaded, exact, concrete, EditInlineNew and compatible with the property."));
        if (Target.Owner->GetClass()->HasAnyClassFlags(CLASS_Abstract) || !Target.Owner->IsA(Class->ClassWithin))
            return Refuse(TEXT("The child class cannot be constructed under this property's owner."));
        // Construct under an isolated owner of the same class. Constructors may execute project
        // code, but no temporary reference is ever installed into the live asset.
        TemporaryOwner.Reset(NewObject<UObject>(GetTransientPackage(), Target.Owner->GetClass(), NAME_None, RF_Transient));
        ProposedChild.Reset(NewObject<UObject>(TemporaryOwner.Get(), Class, NAME_None, RF_Transient));
        if (!ProposedChild.IsValid()) return Refuse(TEXT("The temporary child could not be constructed."));
        Result.Effects.Add(TEXT("temporary_object_constructors_executed"));
        if (Spec->HasField(TEXT("fields")))
        {
            const TSharedPtr<FJsonObject>* Fields = nullptr;
            if (!Spec->TryGetObjectField(TEXT("fields"), Fields)) return Refuse(TEXT("fields must be a JSON object whose values are Unreal property-text strings."));
            for (const auto& Item : (*Fields)->Values)
            {
                FProperty* Field = FindFProperty<FProperty>(Class, *Item.Key);
                FString Input;
                if (!Editable(Field) || !Item.Value->TryGetString(Input)) return Refuse(TEXT("Each initial field must be an editable reflected field with a string value: ") + Item.Key);
                FValue Parsed(Field, Field->ContainerPtrToValuePtr<void>(ProposedChild.Get()));
                if (!TurboMCPDataAssetValues::ParseValue(Field, Input, Parsed.Get(), Error)) return Refuse(Item.Key + TEXT(": ") + Error);
                Field->CopyCompleteValue(Field->ContainerPtrToValuePtr<void>(ProposedChild.Get()), Parsed.Get());
            }
        }
        FValue ChildValue(ChildProperty);
        ChildProperty->SetObjectPropertyValue(ChildValue.Get(), ProposedChild.Get());
        if (ChildMap) TurboMCPDataAssetValues::PlanMapEntry(ChildMap, Target.Address, ChildKey->Get(), ChildValue.Get(), false, Planned.Get());
        else ChildProperty->CopyCompleteValue(Planned.Get(), ChildValue.Get());
        ChildDescription = TEXT("\nchild_class: ") + ClassPath + TEXT("\nchild_owner: ") + Target.Owner->GetPathName() + TEXT("\nThe preview child path is temporary; edit creates a new owned object.");
        FSnapshot AfterConstruction(Asset);
        if (!Baseline.Matches() || AfterConstruction.Revision != Baseline.Revision || Asset->GetOutermost()->IsDirty() != WasDirty)
        {
            Result.Outcome = ETurboMCPResultOutcome::Failed; Result.Code = TEXT("constructor_changed_live_state");
            Result.SetChangedUnknown(); Result.bComplete = false;
            return Result.AppendTo(TEXT("failed: A project constructor changed live state while preparing the child. No planned write was applied. Inspect the asset; do not retry blindly."));
        }
    }
    else return Refuse(TEXT("Operation must be set, map_upsert, map_remove or create_child."));
    const FString Proposed = Export(Target.Property, Planned.Get(), Target.Owner);
    const FString Detail = Header + TEXT("\nproperty: ") + Path + TEXT("\noperation: ") + Operation + TEXT("\nbefore: ") + Before + TEXT("\nafter: ") + Proposed + ChildDescription;
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
        if (ProposedChild.IsValid())
        {
            CreatedChild.Reset(DuplicateObject(ProposedChild.Get(), Target.Owner));
            if (CreatedChild.IsValid())
            {
                CreatedChild->ClearFlags(RF_Transient);
                CreatedChild->SetFlags(RF_Transactional);
                CreatedChild->Modify();
                FValue FinalChild(ChildProperty);
                ChildProperty->SetObjectPropertyValue(FinalChild.Get(), CreatedChild.Get());
                if (ChildMap) TurboMCPDataAssetValues::PlanMapEntry(ChildMap, Target.Address, ChildKey->Get(), FinalChild.Get(), false, Planned.Get());
                else ChildProperty->CopyCompleteValue(Planned.Get(), FinalChild.Get());
            }
        }
        if (!ProposedChild.IsValid() || (CreatedChild.IsValid() && CreatedChild->GetOuter() == Target.Owner))
        {
        Target.Property->CopyCompleteValue(Target.Address, Planned.Get());
        FSnapshot Expected(Asset);
        FPropertyChangedEvent Event(Target.NotifyProperty, EPropertyChangeType::ValueSet);
        Target.Owner->PostEditChangeProperty(Event);
        Applied = true;
        FSnapshot Actual(Asset);
        Verified = Expected.Matches() && Actual.Revision == Expected.Revision;
        }
    }
    Result.Effects.Add(TEXT("edit_notifications_may_execute_project_code"));
    if (!Verified)
    {
        Baseline.Restore();
        if (CreatedChild.IsValid()) CreatedChild->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
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
    return Result.AppendTo(TEXT("edited: Property updated and readback verified. Not saved.\n") + Header + TEXT("\nproperty: ") + Path + TEXT("\noperation: ") + Operation + TEXT("\nbefore: ") + Before + TEXT("\nafter: ") + Export(Target.Property, Target.Address, Target.Owner) + (CreatedChild.IsValid() ? TEXT("\ncreated_object: ") + CreatedChild->GetPathName() : FString()) + TEXT("\ncurrent_revision: ") + Final.Revision + TEXT("\ndirty: ") + (Asset->GetOutermost()->IsDirty() ? TEXT("true") : TEXT("false")) + TEXT("\nProject edit notifications ran; external callback effects are outside this asset snapshot."));
}
}
FString UTurboMCPReadToolset::ReadTurboMCPDataAssetProperty(const FString& Asset, const FString& Property)
{ return Operate(Asset, Property, TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision)
{ return Operate(Asset, Property, Operation, Key, Value, Revision, 1); }
FString UTurboMCPEditToolset::EditTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision)
{ return Operate(Asset, Property, Operation, Key, Value, Revision, 2); }
