// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphScratch.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CreateDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

namespace
{
// Function selection is intentionally excluded: everything else must survive.
FString Structure(UEdGraph* Graph)
{
    TArray<FString> Rows;
    for (const UEdGraphNode* Node:Graph->Nodes) if (Node)
    {
        Rows.Add(Node->NodeGuid.ToString()+FString::Printf(TEXT(":%d:%d"),Node->NodePosX,Node->NodePosY));
        for (const UEdGraphPin* Pin:Node->Pins) if (Pin)
        {
            Rows.Add(Node->NodeGuid.ToString()+Pin->PinName.ToString()+Pin->DefaultValue+Pin->DefaultTextValue.ToString()+GetPathNameSafe(Pin->DefaultObject));
            for (const UEdGraphPin* Other:Pin->LinkedTo) if (Other && Other->GetOwningNodeUnchecked())
                Rows.Add(Node->NodeGuid.ToString()+Pin->PinName.ToString()+Other->GetOwningNode()->NodeGuid.ToString()+Other->PinName.ToString());
        }
    }
    Rows.Sort();return FString::Join(Rows,TEXT("\n"));
}
bool SelectFunction(UK2Node_CreateDelegate* Node,const FString& Name,FString& Error)
{
    UClass* Scope=Node->GetScopeClass();
    UFunction* Signature=Node->GetDelegateSignature();
    UFunction* Function=Scope?Scope->FindFunctionByName(FName(*Name)):nullptr;
    if (!Function || !Signature || !Signature->IsSignatureCompatibleWith(Function)
        || !UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(Function))
    { Error=TEXT("FunctionName must exist on the current object scope, be usable as a delegate and match the connected delegate signature.");return false; }
    for (const UEdGraphPin* Pin:Node->Pins) if (Pin)
    {
        if (Pin->bOrphanedPin) {Error=TEXT("The Create Event node has orphaned pins; repair them first.");return false;}
        for (const UEdGraphPin* Other:Pin->LinkedTo)
            if (!Other || !Other->GetOwningNodeUnchecked() || Other->GetOwningNode()->GetGraph()!=Node->GetGraph() || !Other->LinkedTo.Contains(Pin))
            {Error=TEXT("The Create Event node has invalid or nonreciprocal links; repair them first.");return false;}
    }
    Node->SetFunction(Function->GetFName());
    Node->HandleAnyChangeWithoutNotifying();
    if (Node->GetFunctionName()!=Function->GetFName())
    {Error=TEXT("Native function selection did not retain the requested function.");return false;}
    return true;
}
FString Retarget(bool Edit,const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& FunctionName)
{
    auto Refuse=[](const FString& Why){return TEXT("unchanged\nrefused: ")+Why;};
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return Refuse(TEXT("Requires an idle editor game thread outside play and active transactions."));
    if (GraphName.IsEmpty() || NodeId.IsEmpty() || FunctionName.IsEmpty())
        return Refuse(TEXT("GraphName, NodeId and FunctionName are required."));
    FTurboMCPAssetResolver::FResolved Resolved;const FString ResolveError=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!ResolveError.IsEmpty()) return Refuse(ResolveError);
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!BP) return Refuse(TEXT("Blueprint could not be loaded."));
    if (BP->bBeingCompiled || BP->bQueuedForCompilation) return Refuse(TEXT("Blueprint is compiling; wait and read it again."));
    int32 Matches=0;UEdGraph* Graph=TurboMCPNodeId::FindGraphByName(BP,GraphName,Matches);
    if (!Graph || Matches!=1 || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
        return Refuse(TEXT("GraphName must resolve to one K2 graph."));
    auto* Node=Cast<UK2Node_CreateDelegate>(TurboMCPGraphScratch::Find(Graph,NodeId));
    if (!Node) return Refuse(TEXT("NodeId must identify exactly one Create Event node in the named graph. Use its full reader ID."));
    int32 GuidMatches=0;for (const UEdGraphNode* Item:Graph->Nodes) if (Item && Item->NodeGuid==Node->NodeGuid) ++GuidMatches;
    if (!Node->NodeGuid.IsValid() || GuidMatches!=1) return Refuse(TEXT("Node GUID is missing or duplicated; repair its identity first."));
    TurboMCPGraphScratch::FCopy Copy;
    if (!Copy.Init(BP,Graph)) return Refuse(TEXT("Could not create a disposable preview graph."));
    auto* Probe=Cast<UK2Node_CreateDelegate>(TurboMCPGraphScratch::ByGuid(Copy.Graph,Node->NodeGuid));
    if (!Probe) return Refuse(TEXT("Create Event node is missing from the preview copy."));
    const FString Before=Structure(Copy.Graph);FString Error;
    if (!SelectFunction(Probe,FunctionName,Error)) return Refuse(Error);
    if (Structure(Copy.Graph)!=Before) return Refuse(TEXT("Retargeting would change graph structure or defaults."));
    const FString Report=TEXT("node: ")+TurboMCPNodeId::MakeForNode(Node,true)+TEXT("\nold function: ")+Node->GetFunctionName().ToString()+TEXT("\nnew function: ")+Probe->GetFunctionName().ToString();
    if (Node->GetFunctionName()==Probe->GetFunctionName()) return TEXT("unchanged\nThe compatible function is already selected.\n")+Report;
    if (!Edit) return TEXT("unchanged\npreview: retarget existing Create Event\n")+Report+TEXT("\nIdentity, position, object input and delegate consumers are preserved. No compile or save.");
    UTransBuffer* Buffer=Cast<UTransBuffer>(GEditor->Trans);
    if (!Buffer || !GEditor->CanTransact()) return Refuse(TEXT("Undo is unavailable."));
    auto Tx=MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP","DelegateRetarget","TurboMCP retarget Create Event"));
    if (!Tx->IsOutstanding() || Buffer->ActiveCount!=1 || Buffer->UndoBuffer.IsEmpty()) return Refuse(TEXT("Could not acquire an owned undo transaction."));
    const FGuid Owned=Buffer->UndoBuffer.Last()->GetId();
    const FString LiveBefore=Structure(Graph);
    BP->Modify();Graph->Modify();for (UEdGraphNode* Item:Graph->Nodes) if (Item) Item->Modify();
    if (!SelectFunction(Node,FunctionName,Error) || Structure(Graph)!=LiveBefore)
    {
        Tx.Reset();const int32 Index=Buffer->GetQueueLength()-Buffer->GetUndoCount()-1;
        const FTransaction* Record=Index>=0?Buffer->GetTransaction(Index):nullptr;
        const bool Undone=Record && Record->GetId()==Owned && GEditor->UndoTransaction(false);
        return FString(TEXT("partial\nretarget verification failed; rollback="))+(Undone?TEXT("applied; restoration unverified"):TEXT("failed"))+TEXT("\n")+Error+TEXT("\nInspect the asset before retrying. No compile or save.");
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Graph->NotifyGraphChanged();
    return TEXT("changed\n")+Report+TEXT("\nExisting node and connections preserved; undo is one transaction. This changes a delegate value, not runtime registration. Compile and save explicitly after review.");
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPDelegateRetarget(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& FunctionName)
{ return Retarget(false,Asset,GraphName,NodeId,FunctionName); }
FString UTurboMCPEditToolset::EditTurboMCPDelegateRetarget(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& FunctionName)
{ return Retarget(true,Asset,GraphName,NodeId,FunctionName); }
