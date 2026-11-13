// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "TurboMCPDataAssetValues.h"
#include "Misc/OutputDevice.h"
#include "Misc/SecureHash.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace TurboMCPDataAssetValues
{
FString Revision(UObject* Asset)
{
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Asset, Objects, EGetObjectsFlags::IncludeNestedObjects);
    Objects.Add(Asset);
    Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
    FString Text;
    for (UObject* Object : Objects)
    {
        if (!IsValid(Object)) continue;
        Text += Object->GetPathName() + TEXT(":") + Object->GetClass()->GetPathName() + TEXT("\n");
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Transient)) continue;
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
            {
                FString Encoded;
                It->ExportTextItem_Direct(Encoded, It->ContainerPtrToValuePtr<void>(Object, Index), nullptr, Object, PPF_None);
                Text += FString::Printf(TEXT("%s[%d]:%d:"), *It->GetName(), Index, Encoded.Len()) + Encoded + TEXT("\n");
            }
        }
    }
    const FTCHARToUTF8 Utf8(*Text);
    return FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

FValue::FValue(const FProperty* InProperty, const void* Source) : Property(InProperty)
{
    check(Property);
    Data = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
    Property->InitializeValue(Data);
    if (Source) Property->CopyCompleteValue(Data, Source);
}
FValue::~FValue()
{
    Property->DestroyValue(Data);
    FMemory::Free(Data);
}

namespace
{
bool IsValueType(const FProperty* Property, int32 Depth = 0)
{
    if (!Property || Property->ArrayDim != 1 || Depth > 12) return false;
    if (const auto* Struct = CastField<FStructProperty>(Property))
    {
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            if (!IsValueType(*It, Depth + 1)) return false;
        return true;
    }
    return CastField<FNumericProperty>(Property) || CastField<FBoolProperty>(Property) ||
        CastField<FEnumProperty>(Property) || CastField<FStrProperty>(Property) || CastField<FNameProperty>(Property);
}
bool IsFinite(const FProperty* Property, const void* Value)
{
    if (const auto* Numeric = CastField<FNumericProperty>(Property))
        if (Numeric->IsFloatingPoint()) return FMath::IsFinite(Numeric->GetFloatingPointPropertyValue(Value));
    if (const auto* Struct = CastField<FStructProperty>(Property))
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            if (!IsFinite(*It, It->ContainerPtrToValuePtr<void>(Value))) return false;
    return true;
}
struct FImportMessages : FOutputDevice
{
    FString Text;
    void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
    {
        if (!Text.IsEmpty()) Text += TEXT("; ");
        Text += Message;
    }
};
}

bool ParseValue(const FProperty* Property, const FString& Text, void* Temporary, FString& Error)
{
    Error.Reset();
    if (!IsValueType(Property) || Text.Len() > 65536)
    {
        Error = TEXT("Select a supported scalar or value-struct field and provide at most 65536 characters. References require an exact object path.");
        return false;
    }
    FImportMessages Messages;
    const TCHAR* End = Property->ImportText_Direct(*Text, Temporary, nullptr, PPF_None, &Messages);
    if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || !Messages.Text.IsEmpty() || !IsFinite(Property, Temporary))
    {
        Error = TEXT("Invalid property text: the value is incomplete, contains trailing text, produces an import warning, or contains a non-finite number. ") + Messages.Text;
        return false;
    }
    return true;
}

bool ResolveReference(const FProperty* Property, const FString& Path, UObject* Owner,
    bool bMapKey, void* Temporary, TStrongObjectPtr<UObject>& Retained, FString& Error)
{
    Error.Reset();
    Retained.Reset();
    // Class/soft/weak references need different semantics and must not silently enter this route.
    const auto* ObjectProperty = CastField<FObjectProperty>(Property);
    if (!ObjectProperty || CastField<FClassProperty>(Property) || Property->ArrayDim != 1 || Path.IsEmpty())
    {
        Error = TEXT("This operation requires a hard object reference and an exact object path.");
        return false;
    }
    UObject* Object = FindObject<UObject>(nullptr, *Path);
    if (!IsValid(Object) || Object->GetPathName() != Path || !Object->IsA(ObjectProperty->PropertyClass))
    {
        Error = TEXT("The object path is not loaded, is not exact, or has an incompatible class. Nothing was loaded or created.");
        return false;
    }
    if (!bMapKey && Property->HasAnyPropertyFlags(CPF_InstancedReference) && (!Owner || !Object->IsIn(Owner)))
    {
        Error = TEXT("An instanced value must be owned by the selected asset. Use create_child for a new owned object.");
        return false;
    }
    Retained.Reset(Object);
    ObjectProperty->SetObjectPropertyValue(Temporary, Object);
    return true;
}

void PlanMapEntry(const FMapProperty* Property, const void* LiveMap, const void* Key,
    const void* Value, bool bRemove, void* TemporaryMap)
{
    check(Property && LiveMap && Key && TemporaryMap && LiveMap != TemporaryMap);
    check(bRemove || Value);
    Property->CopyCompleteValue(TemporaryMap, LiveMap);
    FScriptMapHelper Map(Property, TemporaryMap);
    if (bRemove) Map.RemovePair(Key);
    else Map.AddPair(Key, Value);
}
}
