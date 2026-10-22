// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPAssetResolver.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Self.h"
#include "K2Node_CreateDelegate.h"
#include "UObject/Class.h"
#include "K2Node_Literal.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "UObject/Package.h"

namespace
{
using FOperation=TFunction<bool(UEdGraph*,UEdGraph*,FString&,FString&)>;
UEdGraphNode* ResolveNode(UEdGraph* Graph,UEdGraph* Identity,const FString& Id)
{
    UEdGraphNode* Original=TurboMCPGraphScratch::Find(Identity,Id);
    return Original?TurboMCPGraphScratch::ByGuid(Graph,Original->NodeGuid):nullptr;
}
UEdGraphPin* Pin(UEdGraphNode* Node,const FString& Name,EEdGraphPinDirection Direction)
{
    UEdGraphPin* Found=nullptr;
    if(Node) for(UEdGraphPin* P:Node->Pins) if(P && !P->bOrphanedPin && P->Direction==Direction && P->PinName.ToString()==Name)
    { if(Found) return nullptr; Found=P; }
    return Found;
}
bool Connect(UEdGraph* Graph,UEdGraphPin* Output,UEdGraphPin* Input,FString& Error)
{
    if(!Output || !Input || Output->GetOwningNode()->GetGraph()!=Graph || Input->GetOwningNode()->GetGraph()!=Graph)
    { Error=TEXT("pins must resolve uniquely in the named graph"); return false; }
    const bool ExecOutput=Output->PinType.PinCategory==TEXT("exec");
    const bool ExecInput=Input->PinType.PinCategory==TEXT("exec");
    if(ExecOutput!=ExecInput) { Error=TEXT("cannot connect execution and data pins"); return false; }
    if(ExecOutput)
    {
        if(Output->LinkedTo.Num()==1 && Output->LinkedTo[0]==Input && Input->LinkedTo.Contains(Output)) return true;
        const FPinConnectionResponse Check=Graph->GetSchema()->CanCreateConnection(Output,Input);
        if(Check.Response!=CONNECT_RESPONSE_MAKE && Check.Response!=CONNECT_RESPONSE_BREAK_OTHERS_A)
        { Error=TEXT("execution connection refused without conversion or removal of other incoming paths: ")+Check.Message.ToString(); return false; }
        const TArray<UEdGraphPin*> Incoming=Input->LinkedTo;
        Output->BreakAllPinLinks();
        if(!Graph->GetSchema()->TryCreateConnection(Output,Input) || Output->LinkedTo.Num()!=1 || Output->LinkedTo[0]!=Input || !Input->LinkedTo.Contains(Output))
        { Error=TEXT("execution connection read-back failed"); return false; }
        for(UEdGraphPin* Prior:Incoming) if(Prior!=Output && (!Input->LinkedTo.Contains(Prior) || !Prior || !Prior->LinkedTo.Contains(Input)))
        { Error=TEXT("execution connection changed another incoming path"); return false; }
        return true;
    }
    if(Input->LinkedTo.Num()==1 && Input->LinkedTo[0]==Output && Output->LinkedTo.Contains(Input)) return true;
    const FPinConnectionResponse Check=Graph->GetSchema()->CanCreateConnection(Output,Input);
    if(Check.Response!=CONNECT_RESPONSE_MAKE && Check.Response!=CONNECT_RESPONSE_BREAK_OTHERS_B)
    { Error=TEXT("connection refused without conversion or output replacement: ")+Check.Message.ToString(); return false; }
    Input->BreakAllPinLinks();
    if(!Graph->GetSchema()->TryCreateConnection(Output,Input) || Input->LinkedTo.Num()!=1 || Input->LinkedTo[0]!=Output || !Output->LinkedTo.Contains(Input))
    { Error=TEXT("connection read-back failed: input must have exactly the requested source"); return false; }
    return true;
}
bool Trivial(UEdGraphNode* Node)
{
    UK2Node* K=Cast<UK2Node>(Node); if(!K || !K->IsNodePure()) return false;
    if(Node->GetClass()==UK2Node_Self::StaticClass() || Node->GetClass()==UK2Node_Literal::StaticClass() || Node->GetClass()==UK2Node_VariableGet::StaticClass()) return true;
    const UK2Node_CallFunction* Call=Cast<UK2Node_CallFunction>(Node);
    const UFunction* Fn=Call?Call->GetTargetFunction():nullptr;
    if(!Fn || Fn->GetOuterUClass()!=UKismetSystemLibrary::StaticClass()) return false;
    const FString Name=Fn->GetName();
    return Name==TEXT("MakeLiteralBool") || Name==TEXT("MakeLiteralInt") || Name==TEXT("MakeLiteralFloat") || Name==TEXT("MakeLiteralDouble") || Name==TEXT("MakeLiteralName") || Name==TEXT("MakeLiteralString") || Name==TEXT("MakeLiteralText") || Name==TEXT("MakeLiteralByte");
}
bool Split(UEdGraph* Graph,UEdGraphNode* Node,const TSet<UEdGraphNode*>* Selected,int32& Count,FString& Error,FString* Mapping=nullptr)
{
    if(!Trivial(Node)) { Error=TEXT("only pure self, variable-get, object-literal and whitelisted literal constructors can be split; never impure calls"); return false; }
    UEdGraphPin* Value=nullptr;
    for(UEdGraphPin* P:Node->Pins) if(P && P->Direction==EGPD_Output)
    { if(Value || P->PinType.PinCategory==TEXT("exec") || P->SubPins.Num()>0 || P->ParentPin) { Error=TEXT("split requires one unsplit data output"); return false; } Value=P; }
    if(!Value) { Error=TEXT("value output missing"); return false; }
    for(UEdGraphPin* P:Node->Pins) if(P && P->Direction==EGPD_Input && P->LinkedTo.Num()>1)
    { Error=TEXT("malformed multi-source input on value node; repair before splitting"); return false; }
    const int32 BeforeCopies=Count;
    const TArray<UEdGraphPin*> Readers=Value->LinkedTo;
    bool Kept=false;
    for(UEdGraphPin* Reader:Readers)
    {
        if(!Reader || Reader->Direction!=EGPD_Input || Reader->LinkedTo.Num()!=1 || Reader->LinkedTo[0]!=Value)
        { Error=TEXT("malformed value links must be repaired before splitting"); return false; }
        if(Selected && !Selected->Contains(Reader->GetOwningNode())) continue;
        if(!Kept && Readers.Num()==1) { Kept=true; continue; }
        UEdGraphNode* Copy=DuplicateObject<UEdGraphNode>(Node,Graph);
        if(!Copy) { Error=TEXT("value copy failed"); return false; }
        Copy->SetFlags(RF_Transactional); Copy->CreateNewGuid();
        // Clear only the copied link arrays, not their original peers' reciprocal links.
        for(UEdGraphPin* P:Copy->Pins) if(P) P->LinkedTo.Reset();
        Graph->AddNode(Copy,false,false);
        for(UEdGraphPin* P:Node->Pins) if(P && P->Direction==EGPD_Input)
        {
            UEdGraphPin* Q=Pin(Copy,P->PinName.ToString(),EGPD_Input);
            if(!Q) { Error=TEXT("copied input missing"); return false; }
            for(UEdGraphPin* Source:P->LinkedTo) if(!Connect(Graph,Source,Q,Error)) return false;
        }
        UEdGraphPin* NewValue=Pin(Copy,Value->PinName.ToString(),EGPD_Output);
        if(!Connect(Graph,NewValue,Reader,Error)) return false;
        Copy->NodePosX=Reader->GetOwningNode()->NodePosX-240; Copy->NodePosY=Reader->GetOwningNode()->NodePosY;
        if(Mapping)
        {
            *Mapping+=FString::Printf(TEXT("\ncopy %s output=%s -> consumer %s input=%s"),
                *TurboMCPNodeId::MakeForNode(Copy,true),*NewValue->PinName.ToString(),
                *TurboMCPNodeId::MakeForNode(Reader->GetOwningNode(),true),*Reader->PinName.ToString());
        }
        ++Count;
    }
    // Keep the original if anything outside this selection still consumes it.
    if(Value->LinkedTo.IsEmpty() && Count>BeforeCopies && Node->CanUserDeleteNode()) { Node->BreakAllNodeLinks(); Graph->RemoveNode(Node); }
    return true;
}
FString Fingerprint(UEdGraph* Graph)
{
    TArray<FString> Parts;
    for(UEdGraphNode* N:Graph->Nodes) if(N)
    {
        Parts.Add(N->NodeGuid.ToString()+FString::Printf(TEXT(":%d:%d"),N->NodePosX,N->NodePosY));
        for(UEdGraphPin* P:N->Pins) if(P) for(UEdGraphPin* Q:P->LinkedTo) if(Q && Q->GetOwningNodeUnchecked())
            Parts.Add(N->NodeGuid.ToString()+TEXT(":")+P->PinName.ToString()+TEXT("->")+Q->GetOwningNode()->NodeGuid.ToString()+TEXT(":")+Q->PinName.ToString());
    }
    Parts.Sort(); return FString::Join(Parts,TEXT("\n"));
}
FString Run(const FString& Asset,const FString& GraphName,bool Edit,const FOperation& Operation)
{
    if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive()) return TEXT("unchanged\nrefused: requires idle editor game thread outside PIE/transactions");
    FTurboMCPAssetResolver::FResolved R; FString Error=FTurboMCPAssetResolver::Resolve(Asset,R);
    if(!Error.IsEmpty()) return TEXT("unchanged\nrefused: ")+Error;
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); int32 Matches=0;
    UEdGraph* Graph=BP?TurboMCPNodeId::FindGraphByName(BP,GraphName,Matches):nullptr;
    if(!Graph || Matches!=1 || !Graph->GetSchema()) return TEXT("unchanged\nrefused: graph must resolve uniquely with a schema");
    TurboMCPGraphScratch::FCopy Copy;
    if(!Copy.Init(BP,Graph)) return TEXT("unchanged\nrefused: transient graph copy failed");
    FString Report; const FString BeforeState=Fingerprint(Copy.Graph);
    if(!Operation(Copy.Graph,Graph,Report,Error)) return TEXT("unchanged\nrefused: ")+Error;
    if(Fingerprint(Copy.Graph)==BeforeState) return TEXT("unchanged\nno edit needed\n")+Report;
    if(!Edit) return TEXT("unchanged\npreview (transient graph)\n")+Report+TEXT("\nNew node IDs in preview are provisional; use the edit result IDs for subsequent calls.\ncompile/save=deferred");
    UTransBuffer* Buffer=Cast<UTransBuffer>(GEditor->Trans);
    if(!Buffer || !GEditor->CanTransact()) return TEXT("unchanged\nrefused: undo unavailable");
    const bool Dirty=BP->GetOutermost()->IsDirty();
    TUniquePtr<FScopedTransaction> Tx=MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP","GraphRefactor","TurboMCP graph refactor"));
    if(!Tx->IsOutstanding() || Buffer->ActiveCount!=1 || Buffer->UndoBuffer.IsEmpty()) return TEXT("unchanged\nrefused: owned transaction unavailable");
    const FGuid Owned=Buffer->UndoBuffer.Last()->GetId();
    BP->Modify(); Graph->Modify(); for(UEdGraphNode* N:Graph->Nodes) if(N) N->Modify();
    Report.Reset();
    if(!Operation(Graph,Graph,Report,Error))
    {
        Tx.Reset(); const int32 Index=Buffer->GetQueueLength()-Buffer->GetUndoCount()-1;
        const FTransaction* Record=Index>=0?Buffer->GetTransaction(Index):nullptr;
        const bool Restored=Record && Record->GetId()==Owned && GEditor->UndoTransaction(false);
        if(Restored) BP->GetOutermost()->SetDirtyFlag(Dirty);
        return FString(Restored?TEXT("unchanged\nrollback=complete\n"):TEXT("partial\nrollback=failed\n"))+Error;
    }
    Graph->NotifyGraphChanged(); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    return TEXT("changed\n")+Report+TEXT("\nundo=one\ncompile/save=deferred\nRun layout after structural edits.");
}
bool BindFunction(UEdGraph* Graph,UEdGraph* Identity,const FString& TargetId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin,FString& Report,FString& Error)
{
    UEdGraphNode* Target=ResolveNode(Graph,Identity,TargetId);
    UEdGraphPin* Input=Pin(Target,TargetPin,EGPD_Input);
    if(!Input || Input->PinType.PinCategory!=TEXT("delegate") || FunctionName.TrimStartAndEnd().IsEmpty())
    { Error=TEXT("requires an exact delegate input pin and a function name"); return false; }
    const bool ExplicitObject=!ObjectNodeId.IsEmpty();
    if(ExplicitObject!=!ObjectPin.IsEmpty())
    { Error=TEXT("ObjectNodeId and ObjectPin must both be supplied, or both empty for self"); return false; }
    UEdGraphPin* ObjectOutput=ExplicitObject?Pin(ResolveNode(Graph,Identity,ObjectNodeId),ObjectPin,EGPD_Output):nullptr;
    if(ExplicitObject && (!ObjectOutput || ObjectOutput->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object))
    { Error=TEXT("object endpoint must resolve to an object-reference output pin"); return false; }
    UK2Node_CreateDelegate* Event=NewObject<UK2Node_CreateDelegate>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Event,false,false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    Event->NodePosX=Target->NodePosX-240; Event->NodePosY=Target->NodePosY+160;
    if(ExplicitObject && !Connect(Graph,ObjectOutput,Event->GetObjectInPin(),Error)) return false;
    if(!Connect(Graph,Event->GetDelegateOutPin(),Input,Error)) return false;
    UFunction* Signature=Event->GetDelegateSignature();
    UClass* Scope=Event->GetScopeClass();
    UFunction* Function=Scope?Scope->FindFunctionByName(FName(*FunctionName)):nullptr;
    if(!Signature || !Function || !Signature->IsSignatureCompatibleWith(Function)
        || !UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(Function))
    { Error=TEXT("function must exist on the chosen object class and match the target delegate signature exactly"); return false; }
    Event->SetFunction(Function->GetFName());
    Event->HandleAnyChangeWithoutNotifying();
    UEdGraphPin* Output=Event->GetDelegateOutPin();
    UEdGraphPin* CurrentInput=Pin(Target,TargetPin,EGPD_Input);
    if(Event->GetFunctionName()!=Function->GetFName() || !Output || !CurrentInput
        || !Output->LinkedTo.Contains(CurrentInput) || !CurrentInput->LinkedTo.Contains(Output))
    { Error=TEXT("delegate function selection or link did not survive read-back"); return false; }
    if(ExplicitObject)
    {
        UEdGraphPin* CurrentObject=Pin(ResolveNode(Graph,Identity,ObjectNodeId),ObjectPin,EGPD_Output);
        UEdGraphPin* ObjectInput=Event->GetObjectInPin();
        if(!CurrentObject || !ObjectInput || ObjectInput->LinkedTo.Num()!=1
            || ObjectInput->LinkedTo[0]!=CurrentObject || !CurrentObject->LinkedTo.Contains(ObjectInput))
        { Error=TEXT("delegate object context did not survive read-back"); return false; }
    }
    Report=TEXT("created function delegate ")+TurboMCPNodeId::MakeForNode(Event,true)+TEXT(" function=")+FunctionName
        +TEXT("; connected to ")+TargetId+TEXT(".")+TargetPin+TEXT("; existing input source replaced; object=")
        +(ExplicitObject?ObjectNodeId+TEXT(".")+ObjectPin:TEXT("self"))+TEXT("; no compile/save");
    return true;
}
bool Remove(UEdGraph* Graph,UEdGraph* Identity,const FString& Nodes,const FString& Links,FString& Report,FString& Error)
{
    TArray<TSharedPtr<FJsonValue>> NodeJson, LinkJson;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Nodes),NodeJson)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Links),LinkJson))
    { Error=TEXT("Nodes and Links must be JSON arrays; use [] for an unused selection"); return false; }
    TArray<UEdGraphNode*> Delete;
    struct FLink { UEdGraphPin* From; UEdGraphPin* To; };
    TArray<FLink> Break;
    for(const auto& Value:NodeJson)
    {
        FString Id;
        if(!Value.IsValid() || !Value->TryGetString(Id)) { Error=TEXT("Nodes must contain node ID strings"); return false; }
        UEdGraphNode* Node=ResolveNode(Graph,Identity,Id);
        if(!Node || !Node->CanUserDeleteNode() || Delete.Contains(Node)) { Error=TEXT("missing, repeated or non-deletable node: ")+Id; return false; }
        Delete.Add(Node); Report+=TEXT("\nremove node: ")+Id;
    }
    for(const auto& Value:LinkJson)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Object) { Error=TEXT("Links must contain endpoint objects"); return false; }
        const auto Object=Value->AsObject(); FString Source,SourcePin,Target,TargetPin;
        if(!Object->TryGetStringField(TEXT("sourceNodeId"),Source) || !Object->TryGetStringField(TEXT("sourcePin"),SourcePin)
            || !Object->TryGetStringField(TEXT("targetNodeId"),Target) || !Object->TryGetStringField(TEXT("targetPin"),TargetPin))
        { Error=TEXT("link needs sourceNodeId, sourcePin, targetNodeId, targetPin"); return false; }
        UEdGraphPin* From=Pin(ResolveNode(Graph,Identity,Source),SourcePin,EGPD_Output);
        UEdGraphPin* To=Pin(ResolveNode(Graph,Identity,Target),TargetPin,EGPD_Input);
        if(!From || !To || !From->LinkedTo.Contains(To) || !To->LinkedTo.Contains(From))
        { Error=TEXT("requested link must exist reciprocally between exact endpoints"); return false; }
        if(Delete.Contains(From->GetOwningNode()) || Delete.Contains(To->GetOwningNode()))
        { Error=TEXT("link is already removed by a selected node deletion; omit the redundant link"); return false; }
        if(Break.ContainsByPredicate([&](const FLink& L){return L.From==From && L.To==To;}))
        { Error=TEXT("duplicate link selection"); return false; }
        Break.Add({From,To}); Report+=TEXT("\nbreak link: ")+Source+TEXT(":")+SourcePin+TEXT(" -> ")+Target+TEXT(":")+TargetPin;
    }
    // Defer callbacks until all raw pin operations finish: K2 callbacks can reconstruct
    // pins, which would invalidate pointers for later links in this same batch.
    struct FNotifyPin { UEdGraphNode* Node; FGuid PinId; };
    TArray<FNotifyPin> NotifyPins;
    TArray<UEdGraphNode*> NotifyNodes;
    auto Remember = [&](UEdGraphPin* P)
    {
        if(!P || Delete.Contains(P->GetOwningNode())) return;
        UEdGraphNode* N=P->GetOwningNode();
        if(!NotifyPins.ContainsByPredicate([&](const FNotifyPin& R){return R.Node==N && R.PinId==P->PinId;}))
            NotifyPins.Add({N,P->PinId});
        NotifyNodes.AddUnique(N);
    };
    for(const FLink& Link:Break) { Remember(Link.From); Remember(Link.To); }
    for(UEdGraphNode* Node:Delete)
        for(UEdGraphPin* P:Node->Pins) if(P)
            for(UEdGraphPin* Other:P->LinkedTo) Remember(Other);
    // Exact reciprocal removal, followed by the editor's connection notifications.
    for(const FLink& Link:Break)
    {
        Link.From->BreakLinkTo(Link.To);
        if(Link.From->LinkedTo.Contains(Link.To) || Link.To->LinkedTo.Contains(Link.From))
        { Error=TEXT("link removal read-back failed"); return false; }
    }
    for(UEdGraphNode* Node:Delete)
    {
        for(UEdGraphPin* P:Node->Pins) if(P) P->BreakAllPinLinks(false);
        Graph->RemoveNode(Node);
        if(Graph->Nodes.Contains(Node)) { Error=TEXT("node removal read-back failed"); return false; }
    }
    for(const FNotifyPin& Ref:NotifyPins)
    {
        if(!Graph->Nodes.Contains(Ref.Node)) continue;
        UEdGraphPin* Current=nullptr;
        for(UEdGraphPin* P:Ref.Node->Pins) if(P && P->PinId==Ref.PinId) { Current=P; break; }
        if(Current) Ref.Node->PinConnectionListChanged(Current);
    }
    for(UEdGraphNode* Node:NotifyNodes)
        if(Graph->Nodes.Contains(Node)) Node->NodeConnectionListChanged();
    Report=FString::Printf(TEXT("removed nodes=%d links=%d; incident links of removed nodes are also removed; no automatic reconnection"),Delete.Num(),Break.Num())+Report;
    return true;
}
bool Sequence(UEdGraph* Graph,UEdGraph* Identity,const FString& Groups,bool SplitValues,FString& Report,FString& Error)
{
    TArray<TSharedPtr<FJsonValue>> Json;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Groups),Json) || Json.Num()<2 || Json.Num()>64)
    { Error=TEXT("Groups must be a JSON array of 2..64 ordered nonempty arrays of reader node IDs"); return false; }
    TArray<TArray<UEdGraphNode*>> Rows; TArray<UEdGraphNode*> Chain; TSet<UEdGraphNode*> Selected;
    TArray<UEdGraphPin*> Ins,Outs;
    for(const auto& Value:Json)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Array || Value->AsArray().IsEmpty()) { Error=TEXT("each group must be a nonempty node-ID array"); return false; }
        TArray<UEdGraphNode*> Row;
        for(const auto& Id:Value->AsArray())
        {
            FString Name; if(!Id.IsValid() || !Id->TryGetString(Name)) { Error=TEXT("group IDs must be strings"); return false; }
            UEdGraphNode* N=ResolveNode(Graph,Identity,Name);
            UK2Node_CallFunction* Call=Cast<UK2Node_CallFunction>(N); UFunction* Fn=Call?Call->GetTargetFunction():nullptr;
            if(!Call || Call->GetClass()!=UK2Node_CallFunction::StaticClass() || !Fn || Call->IsNodePure() || Fn->HasMetaData(TEXT("Latent")) || Selected.Contains(N))
            { Error=TEXT("groups require distinct synchronous single-path function calls; latent, branching, loop and unknown node types refuse"); return false; }
            UEdGraphPin* In=nullptr; UEdGraphPin* Out=nullptr;
            for(UEdGraphPin* P:N->Pins) if(P && P->PinType.PinCategory==TEXT("exec"))
            {
                if(P->Direction==EGPD_Input) { if(In) { Error=TEXT("multiple exec inputs refuse"); return false; } In=P; }
                else { if(Out) { Error=TEXT("multiple exec outputs refuse"); return false; } Out=P; }
            }
            if(!In || !Out || In->LinkedTo.Num()!=1 || Out->LinkedTo.Num()>1) { Error=TEXT("calls must form one connected linear execution chain"); return false; }
            Ins.Add(In); Outs.Add(Out); Selected.Add(N); Row.Add(N); Chain.Add(N);
        }
        Rows.Add(Row);
    }
    for(int32 I=1;I<Chain.Num();++I) if(Outs[I-1]->LinkedTo.Num()!=1 || Outs[I-1]->LinkedTo[0]!=Ins[I] || Ins[I]->LinkedTo[0]!=Outs[I-1])
    { Error=TEXT("groups must enumerate the original chain in execution order, without gaps or reordering"); return false; }
    UEdGraphPin* Before=Ins[0]->LinkedTo[0]; UEdGraphPin* After=Outs.Last()->LinkedTo.IsEmpty()?nullptr:Outs.Last()->LinkedTo[0];
    if(!Before || Selected.Contains(Before->GetOwningNode()) || (After && Selected.Contains(After->GetOwningNode()))) { Error=TEXT("execution cycle refuses"); return false; }
    if(Before->LinkedTo.Num()!=1 || Before->LinkedTo[0]!=Ins[0] || (After && (After->LinkedTo.Num()!=1 || After->LinkedTo[0]!=Outs.Last())))
    { Error=TEXT("entry and continuation links must be exclusive and reciprocal"); return false; }
    UK2Node_ExecutionSequence* Seq=NewObject<UK2Node_ExecutionSequence>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Seq,false,false); Seq->CreateNewGuid(); Seq->PostPlacedNewNode(); Seq->AllocateDefaultPins();
    const int32 Outputs=Rows.Num()+(After?1:0);
    for(int32 I=2;I<Outputs;++I) Seq->AddInputPin();
    Seq->NodePosX=Chain[0]->NodePosX-300; Seq->NodePosY=Chain[0]->NodePosY;
    UEdGraphPin* SeqIn=Seq->FindPin(TEXT("execute"),EGPD_Input);
    if(!SeqIn) { Error=TEXT("sequence input missing"); return false; }
    Before->BreakLinkTo(Ins[0]);
    if(!Graph->GetSchema()->TryCreateConnection(Before,SeqIn)) { Error=TEXT("sequence entry connection refused"); return false; }
    int32 Offset=0;
    for(int32 I=0;I<Rows.Num();++I)
    {
        UEdGraphPin* First=Ins[Offset]; UEdGraphPin* Last=Outs[Offset+Rows[I].Num()-1];
        First->BreakAllPinLinks(); Last->BreakAllPinLinks();
        if(!Graph->GetSchema()->TryCreateConnection(Seq->GetThenPinGivenIndex(I),First)) { Error=TEXT("sequence lane connection refused"); return false; }
        Offset+=Rows[I].Num();
    }
    if(After && !Graph->GetSchema()->TryCreateConnection(Seq->GetThenPinGivenIndex(Rows.Num()),After)) { Error=TEXT("sequence continuation refused"); return false; }
    Offset=0;
    for(int32 I=0;I<Rows.Num();++I)
    {
        UEdGraphPin* Output=Seq->GetThenPinGivenIndex(I);
        if(!Output || Output->LinkedTo.Num()!=1 || Output->LinkedTo[0]!=Ins[Offset] || Ins[Offset]->LinkedTo.Num()!=1 || Ins[Offset]->LinkedTo[0]!=Output)
        { Error=TEXT("sequence lane read-back failed"); return false; }
        Offset+=Rows[I].Num();
    }
    int32 Copies=0;
    if(SplitValues)
    {
        const TArray<TObjectPtr<UEdGraphNode>> Nodes=Graph->Nodes;
        for(UEdGraphNode* N:Nodes) if(Trivial(N) && !Split(Graph,N,&Selected,Copies,Error)) return false;
    }
    Report=FString::Printf(TEXT("sequence lanes=%d; ordered calls=%d; value copies=%d; continuation=%s; execution order preserved; no save"),Rows.Num(),Chain.Num(),Copies,After?TEXT("last output"):TEXT("none"));
    return true;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { const bool Ok=Connect(G,Pin(ResolveNode(G,I,SourceNodeId),SourcePin,EGPD_Output),Pin(ResolveNode(G,I,TargetNodeId),TargetPin,EGPD_Input),E); R=TEXT("exact named connection applied; data replaces the target input source, exec replaces only the named source output destination; other incoming exec paths preserved; no automatic continuation"); return Ok; }); }
FString UTurboMCPEditToolset::EditTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { const bool Ok=Connect(G,Pin(ResolveNode(G,I,SourceNodeId),SourcePin,EGPD_Output),Pin(ResolveNode(G,I,TargetNodeId),TargetPin,EGPD_Input),E); R=TEXT("exact named connection applied; data replaces the target input source, exec replaces only the named source output destination; other incoming exec paths preserved; no automatic continuation"); return Ok; }); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { int32 Count=0; FString Mapping; const bool Ok=Split(G,ResolveNode(G,I,NodeId),nullptr,Count,E,&Mapping); R=FString::Printf(TEXT("value copies=%d; one source per consumer; layout required"),Count)+Mapping; return Ok; }); }
FString UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { int32 Count=0; FString Mapping; const bool Ok=Split(G,ResolveNode(G,I,NodeId),nullptr,Count,E,&Mapping); R=FString::Printf(TEXT("value copies=%d; one source per consumer; layout required"),Count)+Mapping; return Ok; }); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Sequence(G,I,Groups,bSplitValues,R,E); }); }
FString UTurboMCPEditToolset::EditTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Sequence(G,I,Groups,bSplitValues,R,E); }); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Remove(G,I,Nodes,Links,R,E); }); }

FString UTurboMCPEditToolset::EditTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Remove(G,I,Nodes,Links,R,E); }); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindFunction(G,I,TargetNodeId,TargetPin,FunctionName,ObjectNodeId,ObjectPin,R,E); }); }

FString UTurboMCPEditToolset::EditTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindFunction(G,I,TargetNodeId,TargetPin,FunctionName,ObjectNodeId,ObjectPin,R,E); }); }
