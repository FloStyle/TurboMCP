// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPFunctionDependencyScan.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "UObject/UObjectIterator.h"

namespace
{
bool DerivesFrom(UClass* Class, UBlueprint* Owner)
{
    for(UClass* Current=Class; Current; Current=Current->GetSuperClass())
        if(Current->ClassGeneratedBy==Owner) return true;
    return false;
}
FString RenameFunction(const FString& Asset,const FString& Function,const FString& NewName,bool Apply)
{
    if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("error: Function rename requires an idle editor outside play and active transactions. Nothing renamed.");
    FTurboMCPAssetResolver::FResolved Resolved; FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if(!Error.IsEmpty()) return Error;
    UBlueprint* Owner=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if(!Owner || Owner->BlueprintType==BPTYPE_Interface) return TEXT("error: Choose a Blueprint-owned function, not an interface. Nothing renamed.");
    UEdGraph* Graph=nullptr;
    for(UEdGraph* Candidate:Owner->FunctionGraphs) if(Candidate && Candidate->GetFName()==FName(*Function))
    { if(Graph) return TEXT("error: Function name is ambiguous. Nothing renamed."); Graph=Candidate; }
    if(!Graph) return TEXT("error: The named function is not owned by this Blueprint. Nothing renamed.");
    if(Owner->ParentClass && Owner->ParentClass->FindFunctionByName(Graph->GetFName()))
        return TEXT("error: Inherited overrides cannot be renamed independently. Rename the defining Blueprint function instead.");
    if(NewName.IsEmpty() || NewName.Len()>100 || FName(*NewName).IsNone()) return TEXT("error: NewName must be a valid function identifier of 1 to 100 characters.");
    if(Graph->GetFName()==FName(*NewName)) return TEXT("unchanged: The function already has that name. No callers changed.");
    FKismetNameValidator Validator(Owner,Graph->GetFName());
    if(Validator.IsValid(NewName)!=EValidatorResult::Ok) return TEXT("error: NewName is invalid or conflicts with an existing member. Nothing renamed.");
    if(!Graph->Rename(*NewName,Graph->GetOuter(),REN_Test)) return TEXT("error: NewName collides with an existing object. Nothing renamed.");
    if(!Owner->GeneratedClass || !Owner->SkeletonGeneratedClass || Owner->Status==BS_Error)
        return TEXT("error: Compile the defining Blueprint successfully before renaming its function.");

    // Collect only direct referencers of this class and its Blueprint children.
    // Ordinary consumers do not extend the frontier; their own consumers need
    // no inspection for a function call on this owner.
    const FTurboMCPFunctionDependencyScan Scan=FTurboMCPFunctionDependencyScan::Collect(Owner);
    if(!Scan.Error.IsEmpty()) return Scan.Error;
    TArray<UBlueprint*> Affected; Affected.Add(Owner);
    TArray<UK2Node_CallFunction*> Calls;
    TArray<UK2Node_CreateDelegate*> Delegates;
    for(TObjectIterator<UBlueprint> It;It;++It)
    {
        UBlueprint* BP=*It;
        if(!IsValid(BP) || BP->HasAnyFlags(RF_ClassDefaultObject|RF_Transient)) continue;
        const bool Child=BP!=Owner && DerivesFrom(BP->ParentClass,Owner);
        if(Child)
        {
            FKismetNameValidator ChildValidator(BP,FName(*Function));
            if(ChildValidator.IsValid(NewName)!=EValidatorResult::Ok)
                return TEXT("error: NewName conflicts in child Blueprint: ")+BP->GetPathName()+TEXT(". Nothing renamed.");
            Affected.AddUnique(BP);
        }
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for(UEdGraph* Page:Graphs) if(Page) for(UEdGraphNode* Node:Page->Nodes)
        {
            if(auto* Call=Cast<UK2Node_CallFunction>(Node))
            {
                UClass* Scope=Call->FunctionReference.IsSelfContext()?BP->GeneratedClass.Get():Call->FunctionReference.GetMemberParentClass();
                if(Call->FunctionReference.GetMemberName()==FName(*Function) && DerivesFrom(Scope,Owner))
                { Calls.Add(Call); Affected.AddUnique(BP); }
            }
            if(auto* Delegate=Cast<UK2Node_CreateDelegate>(Node))
                if(Delegate->GetFunctionName()==FName(*Function) && DerivesFrom(Delegate->GetScopeClass(),Owner))
                { Delegates.Add(Delegate); Affected.AddUnique(BP); }
        }
    }
    Affected.Sort([](const UBlueprint& A,const UBlueprint& B){return A.GetPathName()<B.GetPathName();});
    FString Summary=FString::Printf(TEXT("\nfunction: %s:%s -> %s\ndirect candidate packages inspected: %d\nnon-Blueprint assets skipped: %d\ncall nodes: %d\ndelegate nodes: %d"),*Owner->GetPathName(),*Function,*NewName,Scan.PackagesInspected,Scan.NonBlueprintAssetsSkipped,Calls.Num(),Delegates.Num());
    for(UBlueprint* BP:Affected) Summary+=TEXT("\naffected asset: ")+BP->GetPathName();
    for(auto* Call:Calls) Summary+=TEXT("\ncall node: ")+Call->GetPathName()+TEXT(" | ")+Call->NodeGuid.ToString();
    for(auto* Delegate:Delegates) Summary+=TEXT("\ndelegate node: ")+Delegate->GetPathName()+TEXT(" | ")+Delegate->NodeGuid.ToString();
    const FString Limits=TEXT("\nScope: direct registered referencers of the defining Blueprint and derived Blueprint classes, plus loaded Blueprint graphs. Unrelated transitive consumers are excluded. Dynamic strings, native code and unregistered/unavailable content are not verified. Preview loads candidates and may trigger engine load callbacks. Rename can refresh dependent classes; no automatic save or guaranteed rollback. Compile and inspect all listed assets before saving.");
    if(!Apply) return TEXT("preview: Rename function and update discovered Blueprint references. No rename performed.")+Summary+Limits;
    for(UBlueprint* BP:Affected) BP->Modify();
    // Delegate references use a stored function name; update them explicitly in
    // addition to the stock graph rename's call-node and local-scope handling.
    for(auto* Delegate:Delegates) {Delegate->Modify();Delegate->SetFunction(FName(*NewName));}
    FBlueprintEditorUtils::RenameGraph(Graph,NewName);
    if(Graph->GetFName()!=FName(*NewName)) return TEXT("error: Unreal did not rename the function. Earlier reference updates may remain; inspect affected assets. Nothing saved.")+Summary;
    for(UBlueprint* BP:Affected) FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    // Do not report a complete repair merely because the graph itself changed.
    for(auto* Call:Calls)
        if(!IsValid(Call) || Call->FunctionReference.GetMemberName()!=FName(*NewName))
            return TEXT("error: Function renamed, but a discovered call could not be verified. Inspect and compile the listed assets before saving. No rollback or save performed.")+Summary;
    for(auto* Delegate:Delegates)
        if(!IsValid(Delegate) || Delegate->GetFunctionName()!=FName(*NewName))
            return TEXT("error: Function renamed, but a discovered delegate reference could not be verified. Inspect the listed assets. No rollback or save performed.")+Summary;
    return TEXT("edited: Function renamed and discovered callers updated in memory. Nothing saved.")+Summary+Limits;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPFunctionRename(const FString& Asset,const FString& Function,const FString& NewName)
{return RenameFunction(Asset,Function,NewName,false);}
FString UTurboMCPEditToolset::EditTurboMCPFunctionRename(const FString& Asset,const FString& Function,const FString& NewName)
{return RenameFunction(Asset,Function,NewName,true);}
