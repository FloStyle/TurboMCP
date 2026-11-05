// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

namespace
{
bool PrivateFlag(const FBPVariableDescription& Variable)
{
    for (const FBPVariableMetaDataEntry& Entry:Variable.MetaDataArray)
        if (Entry.DataKey==FBlueprintMetadata::MD_Private) return Entry.DataValue.Equals(TEXT("true"),ESearchCase::IgnoreCase);
    return false;
}
FString Flags(const FBPVariableDescription& Variable)
{
    return FString::Printf(TEXT("InstanceEditable=%s; ExposeOnSpawn=%s; Private=%s"),
        (Variable.PropertyFlags&CPF_DisableEditOnInstance)?TEXT("false"):TEXT("true"),
        (Variable.PropertyFlags&CPF_ExposeOnSpawn)?TEXT("true"):TEXT("false"),PrivateFlag(Variable)?TEXT("true"):TEXT("false"));
}
FString ChangeFlags(bool Edit,const FString& Asset,const FString& Name,const FString& InstanceEditable,const FString& ExposeOnSpawn,const FString& Private)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("unchanged\nrefused: requires the editor game thread, no play session and no active transaction.");
    for (const FString* Value:{&InstanceEditable,&ExposeOnSpawn,&Private})
        if (!Value->IsEmpty() && *Value!=TEXT("true") && *Value!=TEXT("false"))
            return TEXT("unchanged\nrefused: flags accept exact true, false or empty to preserve the current value.");
    if (InstanceEditable.IsEmpty() && ExposeOnSpawn.IsEmpty() && Private.IsEmpty())
        return TEXT("unchanged\nrefused: specify at least one flag; empty fields preserve current values.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!Error.IsEmpty()) return TEXT("unchanged\nrefused: ")+Error;
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!BP || BP->bBeingCompiled || BP->bQueuedForCompilation) return TEXT("unchanged\nrefused: Blueprint unavailable or compiling.");
    int32 Index=INDEX_NONE;
    for (int32 I=0;I<BP->NewVariables.Num();++I)
        if (BP->NewVariables[I].VarName.ToString().Equals(Name,ESearchCase::CaseSensitive))
        {
            if (Index!=INDEX_NONE) return TEXT("unchanged\nrefused: variable declaration is ambiguous.");
            Index=I;
        }
    if (Index==INDEX_NONE) return TEXT("unchanged\nrefused: Name must exactly match an owned member variable. Inherited, native, component and widget-generated variables are unsupported.");
    const FBPVariableDescription Before=BP->NewVariables[Index];
    if (!Before.VarGuid.IsValid()) return TEXT("unchanged\nrefused: variable has no stable declaration GUID.");
    FBPVariableDescription Planned=Before;
    if (!InstanceEditable.IsEmpty())
    {
        if (InstanceEditable==TEXT("true")) Planned.PropertyFlags&=~CPF_DisableEditOnInstance;
        else Planned.PropertyFlags|=CPF_DisableEditOnInstance;
    }
    if (!ExposeOnSpawn.IsEmpty())
    {
        if (ExposeOnSpawn==TEXT("true")) Planned.PropertyFlags|=CPF_ExposeOnSpawn;
        else Planned.PropertyFlags&=~CPF_ExposeOnSpawn;
    }
    if (!Private.IsEmpty() && PrivateFlag(Before)!=(Private==TEXT("true")))
    {
        Planned.MetaDataArray.RemoveAll([](const FBPVariableMetaDataEntry& Entry){return Entry.DataKey==FBlueprintMetadata::MD_Private;});
        if (Private==TEXT("true")) Planned.MetaDataArray.Emplace(FBlueprintMetadata::MD_Private,TEXT("true"));
    }
    const FString Report=TEXT("variable: ")+Name+TEXT("\nbefore: ")+Flags(Before)+TEXT("\nafter: ")+Flags(Planned)
        +TEXT("\nDeclaration flags only. Compile separately to update generated properties and spawn/access behavior; then inspect compiler diagnostics before saving. Existing external references are not validated. Private is Blueprint access metadata, not a security boundary. No compile or save was requested.");
    if (Flags(Before)==Flags(Planned)) return TEXT("unchanged\nRequested flags already match.\n")+Report;
    if (!Edit) return TEXT("unchanged\npreview: declaration flag changes\n")+Report;
    if (!GEditor->CanTransact()) return TEXT("unchanged\nrefused: undo unavailable.");
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP edit variable flags")));
    BP->Modify();
    // Edit the declaration directly: the engine metadata setters regenerate the
    // skeleton, which belongs to the caller's explicit compile step instead.
    BP->NewVariables[Index].PropertyFlags=Planned.PropertyFlags;
    BP->NewVariables[Index].MetaDataArray=Planned.MetaDataArray;
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    return TEXT("changed\n")+Report;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPVariableFlags(const FString& Asset,const FString& Name,const FString& InstanceEditable,const FString& ExposeOnSpawn,const FString& Private)
{ return ChangeFlags(false,Asset,Name,InstanceEditable,ExposeOnSpawn,Private); }
FString UTurboMCPEditToolset::EditTurboMCPVariableFlags(const FString& Asset,const FString& Name,const FString& InstanceEditable,const FString& ExposeOnSpawn,const FString& Private)
{ return ChangeFlags(true,Asset,Name,InstanceEditable,ExposeOnSpawn,Private); }
