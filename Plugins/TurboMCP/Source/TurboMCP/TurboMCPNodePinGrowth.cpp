// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPRollback.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeArray.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Misc/ScopeExit.h"

namespace
{
int32 OperandCount(UEdGraphNode* Node)
{
    if (auto* Array=Cast<UK2Node_MakeArray>(Node)) return Array->NumInputs;
    if (auto* Operator=Cast<UK2Node_CommutativeAssociativeBinaryOperator>(Node)) return 2+Operator->GetNumberOfAdditionalInputs();
    if (Node->IsA<UK2Node_ExecutionSequence>())
    {
        int32 Count=0;
        for (const UEdGraphPin* Pin:Node->Pins) if (Pin && Pin->Direction==EGPD_Output && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec) ++Count;
        return Count;
    }
    return INDEX_NONE;
}
struct FPinBefore
{
    UEdGraphPin* Pin;
    FName Name;
    FEdGraphPinType Type;
    FString Default;
    UObject* Object;
    FText Text;
    TArray<UEdGraphPin*> Links;
};
bool Grow(UBlueprint* BP,UEdGraphNode* Node,int32 Desired,FString& Added)
{
    TArray<FPinBefore> Before;
    for (UEdGraphPin* Pin:Node->Pins)
    {
        if (!Pin) return false;
        Before.Add({Pin,Pin->PinName,Pin->PinType,Pin->DefaultValue,Pin->DefaultObject,Pin->DefaultTextValue,Pin->LinkedTo});
    }
    // These are graph-only additions. Native helpers otherwise request skeleton
    // regeneration, which can reinstance unrelated objects during the transaction.
    const bool Suppressed=BP->bSuppressStructurallyModified;
    BP->bSuppressStructurallyModified=true;
    ON_SCOPE_EXIT { BP->bSuppressStructurallyModified=Suppressed; };
    while (OperandCount(Node)<Desired)
    {
        const int32 Previous=OperandCount(Node);
        if (auto* Array=Cast<UK2Node_MakeArray>(Node)) Array->AddInputPin();
        else if (auto* Sequence=Cast<UK2Node_ExecutionSequence>(Node)) Sequence->AddInputPin();
        else if (auto* Operator=Cast<UK2Node_CommutativeAssociativeBinaryOperator>(Node)) Operator->AddInputPin();
        if (OperandCount(Node)!=Previous+1) return false;
    }
    for (const FPinBefore& Original:Before)
        if (!Node->Pins.Contains(Original.Pin) || Original.Pin->PinName!=Original.Name || Original.Pin->PinType!=Original.Type
            || Original.Pin->DefaultValue!=Original.Default || Original.Pin->DefaultObject!=Original.Object
            || !Original.Pin->DefaultTextValue.EqualTo(Original.Text) || Original.Pin->LinkedTo!=Original.Links) return false;
    TArray<FString> Names;
    for (UEdGraphPin* Pin:Node->Pins)
        if (!Before.ContainsByPredicate([&](const FPinBefore& Entry){return Entry.Pin==Pin;})) Names.Add(Pin->PinName.ToString());
    Added=FString::Join(Names,TEXT(", "));
    return OperandCount(Node)==Desired;
}
FString Execute(bool Edit,const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& Count)
{
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return TEXT("unchanged\nrefused: requires the editor game thread, no play session and no active transaction.");
    int32 Desired=0;
    if (Count.IsEmpty() || Count.Len()>2) return TEXT("unchanged\nrefused: Count must be an integer from 1 to 32.");
    for (TCHAR Character:Count) if (Character<TEXT('0') || Character>TEXT('9')) return TEXT("unchanged\nrefused: Count must contain decimal digits only.");
    if (!LexTryParseString(Desired,*Count) || Desired<1 || Desired>32) return TEXT("unchanged\nrefused: Count must be from 1 to 32.");
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!Error.IsEmpty()) return TEXT("unchanged\nrefused: ")+Error;
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!BP || BP->bBeingCompiled || BP->bQueuedForCompilation) return TEXT("unchanged\nrefused: Blueprint unavailable or compiling.");
    int32 Matches=0;
    UEdGraph* Graph=TurboMCPNodeId::FindGraphByName(BP,GraphName,Matches);
    if (!Graph || Matches!=1) return TEXT("unchanged\nrefused: GraphName must identify one graph.");
    UEdGraphNode* Node=TurboMCPGraphScratch::Find(Graph,NodeId);
    if (!Node) return TEXT("unchanged\nrefused: NodeId must be an exact reader node ID in this graph.");
    const int32 Current=OperandCount(Node);
    if (Current==INDEX_NONE) return TEXT("unchanged\nrefused: only Make Array, Sequence and associative operator nodes support this tool.");
    if (Desired<Current) return TEXT("unchanged\nrefused: shrinking pins is not supported; existing links and values are preserved.");
    if (Desired==Current) return TEXT("unchanged\nThe node already has the requested pin count. No compile or save.");
    TurboMCPGraphScratch::FCopy Copy;
    if (!Copy.Init(BP,Graph)) return TEXT("unchanged\nrefused: could not create a transient preview graph.");
    UEdGraphNode* Scratch=TurboMCPGraphScratch::ByGuid(Copy.Graph,Node->NodeGuid);
    FString Added;
    if (!Scratch || !Grow(Copy.Owner.Get(),Scratch,Desired,Added)) return TEXT("unchanged\nrefused: native pin growth failed or changed existing pins in preview.");
    const FString Report=FString::Printf(TEXT("pin count: %d -> %d\nadded pins: %s\nSequence counts output branches; arrays/operators count inputs. Existing pins, links and values are preserved. Compile separately before saving; no compile or save was requested."),Current,Desired,*Added);
    if (!Edit) return TEXT("unchanged\npreview (transient graph)\n")+Report;
    if (!GEditor->CanTransact()) return TEXT("unchanged\nrefused: undo is unavailable.");
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP grow node pins")));
    BP->Modify(); Graph->Modify(); Node->Modify();
    if (!Grow(BP,Node,Desired,Added))
    {
        const bool Undone=FTurboMCPRollback::EndAndUndoActiveTransaction();
        return Undone?TEXT("error: pin growth failed; undo requested. Inspect the graph before retrying; restoration is not verified."):TEXT("partial: pin growth failed and undo failed. Inspect the graph; do not save blindly.");
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    Graph->NotifyGraphChanged();
    return TEXT("changed\n")+Report;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPNodePinGrowth(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& Count)
{ return Execute(false,Asset,GraphName,NodeId,Count); }
FString UTurboMCPEditToolset::EditTurboMCPNodePinGrowth(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& Count)
{ return Execute(true,Asset,GraphName,NodeId,Count); }
