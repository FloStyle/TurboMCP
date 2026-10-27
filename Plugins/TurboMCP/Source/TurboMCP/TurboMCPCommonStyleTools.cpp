// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

namespace
{
// Traverse reflected value structs only. Object references never become implicit
// edit targets: style assets are shared, and their identity must stay explicit.
bool StyleValue(const FProperty* P, int32 Depth=0)
{
    if (!P || P->ArrayDim!=1 || Depth>12) return false;
    if (const auto* S=CastField<FStructProperty>(P))
    {
        for (TFieldIterator<FProperty> It(S->Struct);It;++It) if (!StyleValue(*It,Depth+1)) return false;
        return true;
    }
    return CastField<FNumericProperty>(P) || CastField<FBoolProperty>(P) || CastField<FEnumProperty>(P) || CastField<FStrProperty>(P) || CastField<FNameProperty>(P);
}
bool FiniteStyleValue(const FProperty* P,const void* V)
{
    if (const auto* N=CastField<FNumericProperty>(P)) if (N->IsFloatingPoint()) return FMath::IsFinite(N->GetFloatingPointPropertyValue(V));
    if (const auto* S=CastField<FStructProperty>(P))
        for (TFieldIterator<FProperty> It(S->Struct);It;++It)
            if (!FiniteStyleValue(*It,It->ContainerPtrToValuePtr<void>(V))) return false;
    return true;
}
bool EditableStyleField(const FProperty* P)
{
    return P && P->ArrayDim==1 && P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_EditConst|CPF_Transient|CPF_DisableEditOnTemplate);
}
struct FStyleImportErrors : FOutputDevice
{
    FString Text;
    void Serialize(const TCHAR* Message,ELogVerbosity::Type,const FName&) override {Text+=Message;}
};
FString StyleOperation(const FString& Asset,const FString& Path,const FString* Input,bool Apply)
{
    if (!IsInGameThread() || !GEditor || (Input && (GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive() || !GEditor->CanTransact())))
        return TEXT("error: Style preview/edit requires an idle editor outside play and active transactions. Nothing changed.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!Error.IsEmpty()) return Error;
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!BP || !BP->GeneratedClass || BP->Status==BS_Error) return TEXT("error: Select a successfully compiled CommonButtonStyle Blueprint asset.");
    bool IsStyle=false;
    for (UClass* Class=BP->ParentClass;Class;Class=Class->GetSuperClass())
        if (Class->GetPathName()==TEXT("/Script/CommonUI.CommonButtonStyle")) {IsStyle=true;break;}
    if (!IsStyle) return TEXT("error: Asset must derive from CommonUI CommonButtonStyle. Widget instances and native classes are not style edit targets.");
    UObject* Defaults=BP->GeneratedClass->GetDefaultObject();
    const FString Header=TEXT("asset: ")+BP->GetPathName()+TEXT("\nsource: compiled shared style defaults; not a live widget");
    const FString Limits=TEXT("\nShared asset: all buttons using this style may be affected. No compile, save or runtime refresh is performed. Read back, compile and recreate/refresh affected widgets to inspect the result. Edit conditions such as bSingleMaterial and color-use rules still determine which values are visible.");
    auto Export=[Defaults](const FProperty* P,const void* V){FString Text;P->ExportTextItem_Direct(Text,V,nullptr,Defaults,PPF_None);return Text;};
    if (Path.IsEmpty())
    {
        if (Input) return TEXT("error: Property is required for a style preview/edit. Empty never means all fields.");
        TArray<FString> Lines;
        for (TFieldIterator<FProperty> It(Defaults->GetClass());It;++It) if (EditableStyleField(*It))
        {
            const FString Mode=StyleValue(*It)?TEXT("editable value"):CastField<FStructProperty>(*It)?TEXT("struct; inspect nested fields"):TEXT("read-only reference or unsupported value");
            Lines.Add(It->GetName()+TEXT(" [")+Mode+TEXT("] = ")+Export(*It,It->ContainerPtrToValuePtr<void>(Defaults)));
        }
        Lines.Sort();
        return TEXT("read: Common button style properties.\n")+Header+TEXT("\n")+FString::Join(Lines,TEXT("\n"))+Limits;
    }
    TArray<FString> Parts;Path.ParseIntoArray(Parts,TEXT("."),false);
    if (Parts.IsEmpty() || Parts.Num()>12) return TEXT("error: Property must be an exact reflected path with at most 12 segments.");
    UStruct* Type=Defaults->GetClass();void* Container=Defaults;FProperty* Property=nullptr;FProperty* Root=nullptr;
    for (int32 Index=0;Index<Parts.Num();++Index)
    {
        if (Parts[Index].IsEmpty()) return TEXT("error: Property path contains an empty segment. Nothing changed.");
        Property=FindFProperty<FProperty>(Type,FName(*Parts[Index]));
        if (!EditableStyleField(Property)) return TEXT("error: Property path is missing or not editable on style defaults: ")+Path+TEXT(". Nothing changed.");
        if (!Root) Root=Property;
        if (Index+1<Parts.Num())
        {
            auto* Struct=CastField<FStructProperty>(Property);
            if (!Struct) return TEXT("error: Property paths may descend structs only, not objects, arrays or maps. Nothing changed.");
            Container=Struct->ContainerPtrToValuePtr<void>(Container);Type=Struct->Struct;
        }
    }
    void* Live=Property->ContainerPtrToValuePtr<void>(Container);
    const FString Before=Export(Property,Live);
    const FString Detail=Header+TEXT("\nproperty: ")+Path+TEXT("\ntype: ")+Property->GetCPPType();
    if (!Input) return TEXT("read: Common button style value.\n")+Detail+TEXT("\nwritable: ")+(StyleValue(Property)?TEXT("yes"):TEXT("no; objects, containers, delegates and localized text are read-only"))+TEXT("\nvalue: ")+Before+Limits;
    if (!StyleValue(Property)) return TEXT("error: This field contains references or unsupported values. Select a scalar/value-struct child path; nothing changed.");
    if (Input->Len()>65536) return TEXT("error: Value exceeds 65536 characters. Nothing changed.");
    void* Temporary=FMemory::Malloc(Property->GetSize(),Property->GetMinAlignment());Property->InitializeValue(Temporary);
    ON_SCOPE_EXIT {Property->DestroyValue(Temporary);FMemory::Free(Temporary);};
    Property->CopyCompleteValue(Temporary,Live);
    FStyleImportErrors Errors;
    const TCHAR* End=Property->ImportText_Direct(**Input,Temporary,Defaults,PPF_None,&Errors);
    if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || !Errors.Text.IsEmpty() || !FiniteStyleValue(Property,Temporary))
        return TEXT("error: Value is invalid, incomplete, has trailing text or contains a non-finite number. Nothing changed. Use the reader's Unreal property text format. ")+Errors.Text;
    const FString After=Export(Property,Temporary);
    if (!Apply) return TEXT("preview: Shared style value only; nothing changed.\n")+Detail+TEXT("\nbefore: ")+Before+TEXT("\nafter: ")+After+Limits;
    if (Property->Identical(Live,Temporary)) return TEXT("unchanged: Style already has this value.\n")+Detail+TEXT("\nvalue: ")+Before+Limits;
    const FScopedTransaction Transaction(NSLOCTEXT("TurboMCP","StyleValue","Set CommonUI style value"));
    BP->Modify();Defaults->SetFlags(RF_Transactional);Defaults->Modify();Defaults->PreEditChange(Root);
    Property->CopyCompleteValue(Live,Temporary);
    FPropertyChangedEvent Changed(Root,EPropertyChangeType::ValueSet);Defaults->PostEditChangeProperty(Changed);
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    // Native hooks run synchronously. Report their actual resulting value, not
    // merely the requested input. No structural compile is requested here.
    return TEXT("edited: Shared style value updated in memory.\n")+Detail+TEXT("\nbefore: ")+Before+TEXT("\nafter: ")+Export(Property,Live)+Limits;
}
}
FString UTurboMCPReadToolset::ReadTurboMCPCommonButtonStyle(const FString& Asset,const FString& Property) {return StyleOperation(Asset,Property,nullptr,false);}
FString UTurboMCPPreviewToolset::PreviewTurboMCPCommonButtonStyle(const FString& Asset,const FString& Property,const FString& Value) {return StyleOperation(Asset,Property,&Value,false);}
FString UTurboMCPEditToolset::EditTurboMCPCommonButtonStyle(const FString& Asset,const FString& Property,const FString& Value) {return StyleOperation(Asset,Property,&Value,true);}
