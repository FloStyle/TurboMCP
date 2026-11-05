// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

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
