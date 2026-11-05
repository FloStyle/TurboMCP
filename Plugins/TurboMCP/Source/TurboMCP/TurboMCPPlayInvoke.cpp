// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#include "TurboMCPLogCapture.h"

namespace
{
struct FInvocationTarget
{
    UObject* Object=nullptr;
    UFunction* Function=nullptr;
    UWorld* World=nullptr;
};
// Resolve only existing runtime identities. A stale path must never load an
// editor asset or accidentally select a different PIE client.
bool ResolveInvocation(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,FInvocationTarget& Out,FString& Error)
{
    if (!IsInGameThread() || !GEditor || !GEditor->IsPlaySessionInProgress())
    {Error=TEXT("A live in-process PIE session on the editor game thread is required.");return false;}
    if (WorldPath.IsEmpty() || ObjectPath.IsEmpty() || FunctionName.IsEmpty())
    {Error=TEXT("WorldPath, ObjectPath and FunctionName are required exact identities.");return false;}
    for (const FWorldContext& Context:GEditor->GetWorldContexts())
        if (Context.WorldType==EWorldType::PIE && Context.World() && Context.World()->GetPathName()==WorldPath)
        {if (Out.World) {Error=TEXT("WorldPath is ambiguous.");return false;} Out.World=Context.World();}
    if (!Out.World) {Error=TEXT("WorldPath does not identify a current PIE world. Read the current runtime identities again.");return false;}
    for (TObjectIterator<UObject> It;It;++It)
        if (It->GetPathName()==ObjectPath)
        {if (Out.Object) {Error=TEXT("ObjectPath is ambiguous.");return false;} Out.Object=*It;}
    if (!IsValid(Out.Object) || Out.Object->IsTemplate())
    {Error=TEXT("ObjectPath must identify an existing runtime instance, not an asset or class default object.");return false;}
    UWorld* ObjectWorld=Out.Object->GetWorld();
    // Local-player subsystems obtain their world through their owning player.
    if (const auto* Subsystem=Cast<ULocalPlayerSubsystem>(Out.Object))
        ObjectWorld=Subsystem->GetLocalPlayer()?Subsystem->GetLocalPlayer()->GetWorld():nullptr;
    if (ObjectWorld!=Out.World)
    {Error=TEXT("The object does not belong to the selected PIE world. No fallback target was used.");return false;}
    Out.Function=Out.Object->FindFunction(FName(*FunctionName));
    if (!Out.Function || Out.Function->GetName()!=FunctionName || !Out.Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
    {Error=TEXT("FunctionName must exactly name a Blueprint-callable function on this runtime object.");return false;}
    if (Out.Function->HasAnyFunctionFlags(FUNC_Net|FUNC_Delegate|FUNC_MulticastDelegate|FUNC_EditorOnly)
        || Out.Function->HasMetaData(TEXT("Latent")))
    {Error=TEXT("Network RPCs, delegates, editor-only and latent functions are not supported by synchronous runtime invocation.");return false;}
    return true;
}
FString DescribeInvocation(const FInvocationTarget& Target)
{
    FString Out=TEXT("world: ")+Target.World->GetPathName()+TEXT("\nobject: ")+Target.Object->GetPathName()+TEXT("\nfunction: ")+Target.Function->GetName();
    for (TFieldIterator<FProperty> It(Target.Function);It;++It)
    {
        const FProperty* Property=*It;
        if (!Property->HasAnyPropertyFlags(CPF_Parm)) continue;
        const TCHAR* Direction=Property->HasAnyPropertyFlags(CPF_ReturnParm)?TEXT("return"):
            Property->HasAnyPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ConstParm)?TEXT("out/ref"):TEXT("input");
        Out+=FString::Printf(TEXT("\n  %s: %s (%s)"),*Property->GetName(),*Property->GetCPPType(),Direction);
    }
    return Out+TEXT("\nSignature inspection does not invoke the function. Runtime calls can change gameplay state; they have no general undo guarantee.");
}
}
FString UTurboMCPReadToolset::ReadTurboMCPPlayFunction(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName)
{
    FInvocationTarget Target;FString Error;
    if (!ResolveInvocation(WorldPath,ObjectPath,FunctionName,Target,Error)) return TEXT("error: ")+Error;
    return TEXT("read: runtime function signature\n")+DescribeInvocation(Target);
}

namespace
{
// Reference-bearing values are deliberately rejected before conversion: the
// generic JSON importer can load objects, which is inappropriate for PIE calls.
bool SupportedValue(const FProperty* P,int32 Depth=0)
{
    if (!P || Depth>16 || P->ArrayDim!=1) return false;
    if (CastField<FNumericProperty>(P) || CastField<FBoolProperty>(P) || CastField<FStrProperty>(P)
        || CastField<FNameProperty>(P) || CastField<FTextProperty>(P) || CastField<FEnumProperty>(P)) return true;
    if (const auto* A=CastField<FArrayProperty>(P)) return SupportedValue(A->Inner,Depth+1);
    if (const auto* A=CastField<FSetProperty>(P)) return SupportedValue(A->ElementProp,Depth+1);
    if (const auto* A=CastField<FMapProperty>(P)) return SupportedValue(A->KeyProp,Depth+1) && SupportedValue(A->ValueProp,Depth+1);
    if (const auto* S=CastField<FStructProperty>(P))
    {
        // Native custom importers can execute arbitrary parsing code. Only
        // reflected fields are admitted through the standard JSON converter.
        if (S->Struct->StructFlags & STRUCT_ImportTextItemNative) return false;
        for (TFieldIterator<FProperty> It(S->Struct);It;++It) if (!SupportedValue(*It,Depth+1)) return false;
        return true;
    }
    return false;
}
bool IsInput(const FProperty* P)
{
    return !P->HasAnyPropertyFlags(CPF_ReturnParm) &&
        (!P->HasAnyPropertyFlags(CPF_OutParm) || P->HasAnyPropertyFlags(CPF_ConstParm|CPF_ReferenceParm));
}
TMap<FString,double> PendingInvocations;
FString Invoke(bool Edit,const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,const FString& Arguments,const FString& Confirmation)
{
    auto Refuse=[](const FString& E){return TEXT("unchanged\nrefused: ")+E;};
    FInvocationTarget Target;FString Error;
    if (!ResolveInvocation(WorldPath,ObjectPath,FunctionName,Target,Error)) return Refuse(Error);
    if (Arguments.Len()>65536) return Refuse(TEXT("Arguments exceeds 65536 characters."));
    TSharedPtr<FJsonObject> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Arguments),Values) || !Values.IsValid())
        return Refuse(TEXT("Arguments must be a JSON object with exact parameter names; use {} for no inputs."));
    FStructOnScope Storage(Target.Function);
    TSet<FString> Inputs;
    for (TFieldIterator<FProperty> It(Target.Function);It;++It)
    {
        FProperty* P=*It;
        if (!P->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (!SupportedValue(P)) return Refuse(TEXT("Unsupported parameter type (object references, delegates and custom-imported structs are not accepted): ")+P->GetName()+TEXT(" (")+P->GetCPPType()+TEXT(")"));
        if (!IsInput(P)) continue;
        Inputs.Add(P->GetName());
        const TSharedPtr<FJsonValue>* Value=Values->Values.Find(P->GetName());
        if (!Value) return Refuse(TEXT("Missing required input: ")+P->GetName());
        FText Reason;
        if (!FJsonObjectConverter::JsonValueToUProperty(*Value,P,P->ContainerPtrToValuePtr<void>(Storage.GetStructMemory()),0,0,true,&Reason))
            return Refuse(TEXT("Invalid input ")+P->GetName()+TEXT(": ")+Reason.ToString());
    }
    for (const auto& Pair:Values->Values) if (!Inputs.Contains(Pair.Key)) return Refuse(TEXT("Unknown or output-only parameter: ")+Pair.Key);
    const FString Signature=DescribeInvocation(Target);
    const FString Intent=Signature+Arguments+FString::Printf(TEXT("/%u/%u"),Target.World->GetUniqueID(),Target.Object->GetUniqueID());
    FSHAHash Digest;FSHA1::HashBuffer(*Intent,Intent.Len()*sizeof(TCHAR),Digest.Hash);
    const FString Token=TEXT("invoke ")+Digest.ToString();
    const double Now=FPlatformTime::Seconds();
    for (auto It=PendingInvocations.CreateIterator();It;++It) if (It.Value()<Now) It.RemoveCurrent();
    if (!Edit)
    {
        if (PendingInvocations.Num()>=64) PendingInvocations.Empty();
        PendingInvocations.Add(Token,Now+120.0);
        return TEXT("unchanged\npreview: runtime call arguments validated\n")+Signature+TEXT("\nconfirmation: ")+Token+TEXT("\nConfirmation expires after 120 seconds and is consumed once. Invoke with identical arguments. No function was called.");
    }
    const double* Expiry=PendingInvocations.Find(Token);
    if (Confirmation!=Token || !Expiry) return Refuse(TEXT("Run preview again and copy its confirmation; target and arguments must be unchanged."));
    PendingInvocations.Remove(Token); // Consume before project code runs; never silently retry.
    TStrongObjectPtr<UObject> KeepAlive(Target.Object);
    FTurboMCPLogCapture Capture;
    Target.Object->ProcessEvent(Target.Function,Storage.GetStructMemory());
    FString Out=TEXT("dispatched: runtime function returned; this does not prove gameplay success. Do not automatically retry.\n")+Signature;
    for (TFieldIterator<FProperty> It(Target.Function);It;++It)
    {
        FProperty* P=*It;
        if (!P->HasAnyPropertyFlags(CPF_Parm) || !P->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)) continue;
        FString Value;P->ExportTextItem_Direct(Value,P->ContainerPtrToValuePtr<void>(Storage.GetStructMemory()),nullptr,nullptr,PPF_None);
        Out+=TEXT("\nresult ")+P->GetName()+TEXT(": ")+Value.Left(4096)+(Value.Len()>4096?TEXT(" [truncated]"):TEXT(""));
    }
    Out+=FString::Printf(TEXT("\nSynchronous game-thread log errors: %d; warnings: %d. No automatic save or rollback."),Capture.ErrorCount(),Capture.WarningCount());
    return Out;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPPlayInvoke(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,const FString& Arguments)
{return Invoke(false,WorldPath,ObjectPath,FunctionName,Arguments,TEXT(""));}
FString UTurboMCPEditToolset::EditTurboMCPPlayInvoke(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,const FString& Arguments,const FString& Confirmation)
{return Invoke(true,WorldPath,ObjectPath,FunctionName,Arguments,Confirmation);}
