#include "Tools/TurboMCPToolsets.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPAssetResolver.h"
#include "TurboMCPFunction.h"
#include "TurboMCPTypeParser.h"
#include "TurboMCPNodeId.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
struct FEventParameter { FString Name; FEdGraphPinType Type; };
bool EventIdentifier(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len()>128 || (!FChar::IsAlpha(Name[0]) && Name[0]!=TCHAR('_'))) return false;
    for (TCHAR C:Name) if (!FChar::IsAlnum(C) && C!=TCHAR('_')) return false;
    return true;
}
FString CustomEvent(const FString& Asset, const FString& GraphName, const FString& Name, const FString& Inputs, bool Apply)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("unchanged: custom-event creation requires an idle editor outside PIE and active transactions.");
    if (!EventIdentifier(Name)) return TEXT("unchanged: event name must be an identifier of 1..128 characters.");
    if (Inputs.Len()>4096) return TEXT("unchanged: input signature exceeds 4096 characters.");
    TArray<FString> Parts;
    int32 Depth=0, Start=0;
    for (int32 I=0; I<Inputs.Len(); ++I)
    {
        if (Inputs[I]==TCHAR('<')) ++Depth;
        else if (Inputs[I]==TCHAR('>')) { if (--Depth<0) return TEXT("unchanged: unmatched type bracket."); }
        else if (Inputs[I]==TCHAR(',') && Depth==0) { Parts.Add(Inputs.Mid(Start,I-Start)); Start=I+1; }
    }
    if (Depth!=0) return TEXT("unchanged: unmatched type bracket.");
    if (!Inputs.TrimStartAndEnd().IsEmpty()) Parts.Add(Inputs.Mid(Start));
    if (Parts.Num()>32) return TEXT("unchanged: at most 32 event parameters are supported.");
    TArray<FEventParameter> Parameters;
    TSet<FName> Used;
    Used.Add(UEdGraphSchema_K2::PN_Then);
    Used.Add(TEXT("OutputDelegate"));
    for (const FString& Part:Parts)
    {
        FString PinName, TypeName, Why;
        if (!Part.Split(TEXT(":"),&PinName,&TypeName)) return TEXT("unchanged: each input must be Name:type.");
        PinName.TrimStartAndEndInline(); TypeName.TrimStartAndEndInline();
        if (!EventIdentifier(PinName) || Used.Contains(FName(*PinName)))
            return TEXT("unchanged: parameter name is invalid, reserved or duplicated: ")+PinName;
        FEventParameter Parameter; Parameter.Name=PinName;
        if (!FTurboMCPTypeParser::Resolve(TypeName,Parameter.Type,Why))
            return TEXT("unchanged: invalid parameter type: ")+Why;
        const FName Category=Parameter.Type.PinCategory;
        if (Category==UEdGraphSchema_K2::PC_Exec || Category==UEdGraphSchema_K2::PC_Wildcard
            || Category==UEdGraphSchema_K2::PC_Delegate || Category==UEdGraphSchema_K2::PC_MCDelegate)
            return TEXT("unchanged: event inputs require concrete data types, not execution or delegate types.");
        Used.Add(FName(*PinName)); Parameters.Add(Parameter);
    }
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!Error.IsEmpty()) return Error;
    UBlueprint* Blueprint=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!Blueprint) return TEXT("unchanged: target is not a Blueprint.");
    UEdGraph* Graph=nullptr; int32 Matches=0;
    for (UEdGraph* Candidate:Blueprint->UbergraphPages)
        if (Candidate && Candidate->GetName()==GraphName) { Graph=Candidate; ++Matches; }
    if (Matches!=1 || !Graph || !Cast<UEdGraphSchema_K2>(Graph->GetSchema()))
        return TEXT("unchanged: GraphName must identify exactly one existing K2 event graph.");
    const auto Check=FTurboMCPFunction::CheckName(Blueprint,Name);
    if (!Check.bCanAdd) return TEXT("unchanged: ")+Check.Reason;
    if (!Apply) return FString::Printf(TEXT("preview: create local custom event %s in %s with %d input(s). Existing nodes stay fixed. No graph edits, compile or save."),*Name,*GraphName,Parameters.Num());
    const bool WasDirty=Blueprint->GetOutermost()->IsDirty();
    FScopedTransaction Transaction(NSLOCTEXT("TurboMCP","CustomEventAdd","Add custom event"));
    Blueprint->Modify(); Graph->Modify();
    UK2Node_CustomEvent* Event=NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);
    Event->CustomFunctionName=FName(*Name);
    Graph->AddNode(Event,false,false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    for (const FEventParameter& Parameter:Parameters)
    {
        UEdGraphPin* Pin=Event->CreateUserDefinedPin(FName(*Parameter.Name),Parameter.Type,EGPD_Output,false);
        if (!Pin || Pin->Direction!=EGPD_Output || Pin->PinType!=Parameter.Type)
        {
            Event->DestroyNode(); Transaction.Cancel(); Blueprint->GetOutermost()->SetDirtyFlag(WasDirty);
            return TEXT("unchanged: event pin creation failed; the new node was removed.");
        }
    }
    // Place beyond existing node rectangles; never move or tidy unrelated logic.
    int64 Right=0;
    for (UEdGraphNode* Node:Graph->Nodes)
        if (Node && Node!=Event) Right=FMath::Max(Right,int64(Node->NodePosX)+FMath::Max(512,Node->NodeWidth)+128);
    if (Right>MAX_int32-1024)
    {
        Event->DestroyNode(); Transaction.Cancel(); Blueprint->GetOutermost()->SetDirtyFlag(WasDirty);
        return TEXT("unchanged: no safe horizontal placement range remains.");
    }
    Event->NodePosX=int32(Right); Event->NodePosY=0;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Graph->NotifyGraphChanged();
    FString Result=TEXT("created custom event: ")+Name+TEXT("\nnode: ")+TurboMCPNodeId::MakeForNode(Event,true)+TEXT("\npins:");
    for (UEdGraphPin* Pin:Event->Pins) if (Pin) Result+=TEXT("\n  ")+Pin->PinName.ToString();
    return Result+TEXT("\nUse this node ID with GraphAppend link syntax to append its body. Local event; no replication, delegate binding, explicit compile or save requested.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPCustomEventAdd(const FString& Asset,const FString& GraphName,const FString& Name,const FString& Inputs)
{ return CustomEvent(Asset,GraphName,Name,Inputs,false); }
FString UTurboMCPEditToolset::EditTurboMCPCustomEventAdd(const FString& Asset,const FString& GraphName,const FString& Name,const FString& Inputs)
{ return CustomEvent(Asset,GraphName,Name,Inputs,true); }
