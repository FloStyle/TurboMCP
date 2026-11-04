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
#include "K2Node_CustomEvent.h"
#include "UObject/UnrealType.h"
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
    UEdGraphNode* Original = nullptr;
    if (Identity) for (UEdGraphNode* Node : Identity->Nodes)
        if (TurboMCPNodeId::MatchesNode(Node, Id)) { if (Original) return nullptr; Original = Node; }
    UEdGraphNode* Found = nullptr;
    if (Original && Graph) for (UEdGraphNode* Node : Graph->Nodes)
        if (Node && Node->NodeGuid == Original->NodeGuid) { if (Found) return nullptr; Found = Node; }
    return Found;
}
/** Resolve edit endpoints without dropping asset/graph qualifiers or guessing a match.
 * Bare hexadecimal keys are scoped by the already resolved graph. Exact reader
 * IDs retain their full identity; a failed qualified ID is never treated as a key.
 */
UEdGraphNode* ResolveEditNode(UEdGraph* Graph, UEdGraph* Identity, const FString& Id, FString& Error)
{
    const FString Requested = Id.TrimStartAndEnd();
    TArray<UEdGraphNode*> Matches;
    const FString Key = TurboMCPNodeId::NormaliseKey(Requested);
    if (!Key.IsEmpty())
        TurboMCPNodeId::FindNodesByKey(Identity, Key, Matches);
    else if (Identity)
        for (UEdGraphNode* Node : Identity->Nodes)
            if (Node && (TurboMCPNodeId::MakeForNode(Node, true) == Requested
                || TurboMCPNodeId::MakeForNode(Node, false) == Requested))
                Matches.Add(Node);
    if (Matches.Num() != 1)
    {
        Error = Matches.IsEmpty()
            ? TEXT("Node ID was not found in the named graph. Use an exact ID from the reader or a unique hexadecimal node key of at least six digits: ") + Requested
            : TEXT("Node ID is ambiguous in the named graph. Use a longer unique key or an exact reader ID: ") + Requested;
        return nullptr;
    }
    // Scratch copies preserve GUIDs, but a malformed duplicate must not select
    // the first node silently when mapping back to the editable graph.
    UEdGraphNode* Found = nullptr;
    if (Graph) for (UEdGraphNode* Node : Graph->Nodes)
        if (Node && Node->NodeGuid == Matches[0]->NodeGuid)
        {
            if (Found) { Error = TEXT("Node GUID is duplicated in the graph; repair the duplicate identity before editing."); return nullptr; }
            Found = Node;
        }
    if (!Found) Error = TEXT("Resolved node is unavailable in the editable graph. Read the graph again before retrying.");
    return Found;
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
bool Split(UEdGraph* Graph, UEdGraphNode* Node, const TSet<UEdGraphNode*>* Selected,
    int32& Count, FString& Error, FString* Mapping=nullptr)
{
    if (!Graph || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>()
        || !Node || Node->GetGraph()!=Graph || !Trivial(Node))
    {
        Error=TEXT("Select a supported pure self, variable-get, object-literal or literal-constructor node in the named K2 graph; arbitrary pure functions and impure calls are not supported.");
        return false;
    }
    UEdGraphPin* Value=nullptr;
    for (UEdGraphPin* P:Node->Pins)
    {
        if (!P || P->bOrphanedPin) { Error=TEXT("The source contains missing or orphaned pins; repair it before splitting."); return false; }
        if (P->Direction==EGPD_Output)
        {
            if (Value || P->PinType.PinCategory==TEXT("exec") || !P->SubPins.IsEmpty() || P->ParentPin)
            { Error=TEXT("The source must have exactly one unsplit data output."); return false; }
            Value=P;
        }
        else
        {
            if (P->LinkedTo.Num()>1) { Error=TEXT("A source input has multiple connections; repair it before splitting."); return false; }
            for (UEdGraphPin* Source:P->LinkedTo)
                if (!Source || Source->Direction!=EGPD_Output || Source->GetOwningNode()->GetGraph()!=Graph || !Source->LinkedTo.Contains(P))
                { Error=TEXT("A source input link is invalid or belongs to another graph; repair it before splitting."); return false; }
        }
    }
    if (!Value) { Error=TEXT("The source has no data output."); return false; }
    const TArray<UEdGraphPin*> Readers=Value->LinkedTo;
    if (Readers.Num()>4096) { Error=TEXT("A source may have at most 4096 connected consumer inputs for this operation."); return false; }
    // Validate every affected endpoint before cloning or reconnecting anything.
    // Each connected input receives its own copy, even on the same consumer node.
    for (UEdGraphPin* Reader:Readers)
    {
        if (!Reader || Reader->bOrphanedPin || Reader->Direction!=EGPD_Input
            || Reader->GetOwningNode()->GetGraph()!=Graph || Reader->GetOwningNode()==Node
            || Reader->LinkedTo.Num()!=1 || Reader->LinkedTo[0]!=Value)
        { Error=TEXT("A consumer link is invalid, cyclic or belongs to another graph; repair it before splitting."); return false; }
        if ((!Selected || Selected->Contains(Reader->GetOwningNode())) && Readers.Num()>1
            && static_cast<int64>(Reader->GetOwningNode()->NodePosX)-240 < MIN_int32)
        { Error=TEXT("A proposed value-copy position exceeds the supported coordinate range."); return false; }
    }
    if (Readers.Num()<=1) return true;
    if (!Node->CanUserDeleteNode())
    { Error=TEXT("This source cannot be deleted safely; splitting it would leave an unused original node."); return false; }
    const int32 BeforeCopies=Count;
    for (UEdGraphPin* Reader:Readers)
    {
        if (Selected && !Selected->Contains(Reader->GetOwningNode())) continue;
        UEdGraphNode* Copy=DuplicateObject<UEdGraphNode>(Node,Graph);
        if (!Copy) { Error=TEXT("The engine could not copy the source node."); return false; }
        Copy->SetFlags(RF_Transactional); Copy->CreateNewGuid();
        // Clear only the copied arrays. The original peers still own their links.
        for (UEdGraphPin* P:Copy->Pins) if (P) P->LinkedTo.Reset();
        Graph->AddNode(Copy,false,false);
        for (UEdGraphPin* P:Node->Pins) if (P->Direction==EGPD_Input)
        {
            UEdGraphPin* Q=Pin(Copy,P->PinName.ToString(),EGPD_Input);
            if (!Q) { Error=TEXT("A copied input pin is missing or ambiguous."); return false; }
            for (UEdGraphPin* Source:P->LinkedTo) if (!Connect(Graph,Source,Q,Error)) return false;
        }
        UEdGraphPin* NewValue=Pin(Copy,Value->PinName.ToString(),EGPD_Output);
        if (!Connect(Graph,NewValue,Reader,Error)) return false;
        Copy->NodePosX=Reader->GetOwningNode()->NodePosX-240;
        Copy->NodePosY=Reader->GetOwningNode()->NodePosY;
        if (Mapping)
            *Mapping+=FString::Printf(TEXT("\nCopy %s output=%s -> consumer %s input=%s"),
                *TurboMCPNodeId::MakeForNode(Copy,true),*NewValue->PinName.ToString(),
                *TurboMCPNodeId::MakeForNode(Reader->GetOwningNode(),true),*Reader->PinName.ToString());
        ++Count;
    }
    // Sequence may select only some consumers. Keep the source for all others.
    if (Value->LinkedTo.IsEmpty() && Count>BeforeCopies)
    { Node->BreakAllNodeLinks(); Graph->RemoveNode(Node); }
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
FString Run(const FString& Asset,const FString& GraphName,bool Edit,const FOperation& Operation,bool bCanCreateNodes=true)
{
    if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive()) return TEXT("unchanged\nrefused: requires idle editor game thread outside PIE/transactions");
    FTurboMCPAssetResolver::FResolved R; FString Error=FTurboMCPAssetResolver::Resolve(Asset,R);
    if(!Error.IsEmpty()) return TEXT("unchanged\nrefused: ")+Error;
    UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); int32 Matches=0;
    if (!BP) return TEXT("unchanged\nrefused: Blueprint could not be loaded");
    UEdGraph* Graph=BP?TurboMCPNodeId::FindGraphByName(BP,GraphName,Matches):nullptr;
    if(!Graph || Matches!=1 || !Graph->GetSchema()) return TEXT("unchanged\nrefused: graph must resolve uniquely with a schema");
    TurboMCPGraphScratch::FCopy Copy;
    if(!Copy.Init(BP,Graph)) return TEXT("unchanged\nrefused: transient graph copy failed");
    FString Report; const FString BeforeState=Fingerprint(Copy.Graph);
    if(!Operation(Copy.Graph,Graph,Report,Error)) return TEXT("unchanged\nrefused: ")+Error;
    if(Fingerprint(Copy.Graph)==BeforeState) return TEXT("unchanged\nno edit needed\n")+Report;
    if(!Edit) return TEXT("unchanged\npreview (transient graph)\n")+Report
        +(bCanCreateNodes ? TEXT("\nNew node IDs in preview are provisional; use the edit result IDs for subsequent calls.") : TEXT(""))
        +TEXT("\ncompile/save=deferred");
    UTransBuffer* Buffer=Cast<UTransBuffer>(GEditor->Trans);
    if(!Buffer || !GEditor->CanTransact()) return TEXT("unchanged\nrefused: undo unavailable");
    TUniquePtr<FScopedTransaction> Tx=MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP","GraphRefactor","TurboMCP graph refactor"));
    if(!Tx->IsOutstanding() || Buffer->ActiveCount!=1 || Buffer->UndoBuffer.IsEmpty()) return TEXT("unchanged\nrefused: owned transaction unavailable");
    const FGuid Owned=Buffer->UndoBuffer.Last()->GetId();
    BP->Modify(); Graph->Modify(); for(UEdGraphNode* N:Graph->Nodes) if(N) N->Modify();
    Report.Reset();
    if(!Operation(Graph,Graph,Report,Error))
    {
        Tx.Reset(); const int32 Index=Buffer->GetQueueLength()-Buffer->GetUndoCount()-1;
        const FTransaction* Record=Index>=0?Buffer->GetTransaction(Index):nullptr;
        const bool UndoApplied=Record && Record->GetId()==Owned && GEditor->UndoTransaction(false);
        // Undo success does not prove complete restoration of properties or callback effects.
        // Do not clear dirty state or report unchanged without a complete comparison.
        return FString(UndoApplied
            ? TEXT("partial\nrollback=applied; restoration=unverified\n")
            : TEXT("partial\nrollback=failed; restoration=unverified\n"))
            + Error + TEXT("\ncompile=not_run; save=not_requested\nInspect the asset before retrying or saving.");
    }
    Graph->NotifyGraphChanged(); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    return TEXT("changed\n")+Report+TEXT("\nundo=one\ncompile/save=deferred\nInspect layout after structural edits; tidy only the affected nodes if needed.");
}
bool BindNewEvent(UEdGraph* Graph,UEdGraph* Identity,const FString& TargetId,const FString& TargetPin,const FString& EventName,FString& Report,FString& Error)
{
    UBlueprint* BP=FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if(!BP || !BP->UbergraphPages.Contains(Graph))
    { Error=TEXT("custom events require an event graph"); return false; }
    if(EventName.IsEmpty() || FChar::IsDigit(EventName[0]))
    { Error=TEXT("event name must be a nonempty identifier starting with a letter or underscore"); return false; }
    for(TCHAR C:EventName) if(!FChar::IsAlnum(C) && C!=TEXT('_'))
    { Error=TEXT("event name may contain only letters, digits and underscores"); return false; }
    UBlueprint* OriginalBP=FBlueprintEditorUtils::FindBlueprintForGraph(Identity);
    const FName Name(*EventName);
    if(OriginalBP && ((OriginalBP->SkeletonGeneratedClass && OriginalBP->SkeletonGeneratedClass->FindFunctionByName(Name))
        || (OriginalBP->GeneratedClass && OriginalBP->GeneratedClass->FindFunctionByName(Name))))
    { Error=TEXT("event name already exists as a function"); return false; }
    TArray<UEdGraph*> AllGraphs;
    if(OriginalBP) OriginalBP->GetAllGraphs(AllGraphs);
    for(UEdGraph* G:AllGraphs) if(G) for(UEdGraphNode* N:G->Nodes)
        if(const UK2Node_CustomEvent* E=Cast<UK2Node_CustomEvent>(N)) if(E->CustomFunctionName==Name)
        { Error=TEXT("event name already exists"); return false; }
    UEdGraphNode* Target=ResolveEditNode(Graph,Identity,TargetId,Error);
    if (!Target) return false;
    UEdGraphPin* Input=Pin(Target,TargetPin,EGPD_Input);
    if(!Input || Input->PinType.PinCategory!=TEXT("delegate"))
    { Error=TEXT("target must be an exact delegate input pin"); return false; }
    // Record displaced sources before the signature probe temporarily replaces them.
    // Use live identities even when the operation runs on a scratch graph.
    TArray<FString> Displaced;
    for (UEdGraphPin* Source : Input->LinkedTo) if (Source)
        if (UEdGraphNode* Original = TurboMCPGraphScratch::ByGuid(Identity, Source->GetOwningNode()->NodeGuid))
            Displaced.Add(TurboMCPNodeId::MakeForNode(Original, true) + TEXT(":") + Source->PinName.ToString());
    // Let the same engine delegate node used by the editor resolve the signature.
    UK2Node_CreateDelegate* Probe=NewObject<UK2Node_CreateDelegate>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Probe,false,false); Probe->CreateNewGuid(); Probe->AllocateDefaultPins();
    if(!Connect(Graph,Probe->GetDelegateOutPin(),Input,Error)) return false;
    UFunction* Signature=Probe->GetDelegateSignature();
    if(!Signature) { Error=TEXT("target delegate signature could not be resolved"); return false; }
    for(TFieldIterator<FProperty> It(Signature);It && It->HasAnyPropertyFlags(CPF_Parm);++It)
        if(It->HasAnyPropertyFlags(CPF_ReturnParm) || (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ReferenceParm)))
        { Error=TEXT("custom events cannot implement a return/output-value signature; bind a compatible function instead"); return false; }
    Probe->BreakAllNodeLinks(); Graph->RemoveNode(Probe);
    UK2Node_CustomEvent* Event=UK2Node_CustomEvent::CreateFromFunction(
        FVector2D(Target->NodePosX-320,Target->NodePosY+240),Graph,EventName,Signature,false);
    if(!Event) { Error=TEXT("engine could not create the typed custom event"); return false; }
    UEdGraphPin* Output=nullptr;
    for(UEdGraphPin* P:Event->Pins) if(P && P->Direction==EGPD_Output && P->PinType.PinCategory==TEXT("delegate")) Output=P;
    Input=Pin(Target,TargetPin,EGPD_Input);
    if(!Connect(Graph,Output,Input,Error)) return false;
    TArray<FString> Parameters;
    for (const UEdGraphPin* Parameter : Event->Pins)
        if (Parameter && Parameter->Direction == EGPD_Output
            && Parameter->PinType.PinCategory != TEXT("exec") && Parameter->PinType.PinCategory != TEXT("delegate"))
        {
            FString Type = Parameter->PinType.PinCategory.ToString();
            if (Parameter->PinType.PinSubCategoryObject.IsValid())
                Type += TEXT(" ") + Parameter->PinType.PinSubCategoryObject->GetPathName();
            Parameters.Add(Parameter->PinName.ToString() + TEXT(": ") + Type);
        }
    Report = TEXT("handler: ") + TurboMCPNodeId::MakeForNode(Event, true) + TEXT("; name=") + EventName
        + TEXT("\ntarget: ") + TurboMCPNodeId::MakeForNode(TurboMCPGraphScratch::ByGuid(Identity, Target->NodeGuid), true) + TEXT(":") + TargetPin
        + TEXT("\nsignature: ") + Signature->GetPathName()
        + TEXT("\nhandler parameters: ") + (Parameters.IsEmpty() ? FString(TEXT("none")) : FString::Join(Parameters, TEXT(", ")))
        + TEXT("\nreplaced input sources: ") + (Displaced.IsEmpty() ? FString(TEXT("none")) : FString::Join(Displaced, TEXT(", ")))
        + TEXT("\nThe handler body is empty. Connect its execution output to the intended body. The target Bind node must execute at runtime to register the listener; this tool does not add that execution path.")
        + TEXT("\nInspect the graph, then compile and test the callback before saving. Compilation and runtime behavior were not verified.");
    if (!Displaced.IsEmpty())
        Report += TEXT("\nPrevious source nodes were retained. Review their remaining uses before removing any obsolete handler or delegate node.");
    return true;
}
bool BindFunction(UEdGraph* Graph,UEdGraph* Identity,const FString& TargetId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin,FString& Report,FString& Error)
{
    if (!Graph || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    { Error=TEXT("Function binding requires a Blueprint K2 graph."); return false; }
    UEdGraphNode* Target=ResolveEditNode(Graph,Identity,TargetId,Error);
    if (!Target) return false;
    UEdGraphPin* Input=Pin(Target,TargetPin,EGPD_Input);
    if(!Input || Input->PinType.PinCategory!=TEXT("delegate") || FunctionName.TrimStartAndEnd().IsEmpty())
    { Error=TEXT("Provide an exact delegate input pin and an existing function name."); return false; }
    const bool ExplicitObject=!ObjectNodeId.IsEmpty();
    if(ExplicitObject!=!ObjectPin.IsEmpty())
    { Error=TEXT("Supply both ObjectNodeId and ObjectPin, or leave both empty to use self."); return false; }
    UEdGraphNode* ObjectNode = ExplicitObject ? ResolveEditNode(Graph, Identity, ObjectNodeId, Error) : nullptr;
    if (ExplicitObject && !ObjectNode) return false;
    UEdGraphPin* ObjectOutput=ExplicitObject?Pin(ObjectNode,ObjectPin,EGPD_Output):nullptr;
    if(ExplicitObject && (!ObjectOutput || ObjectOutput->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object))
    { Error=TEXT("The object endpoint must be an object-reference output pin."); return false; }
    TArray<FString> Displaced;
    if (Input->LinkedTo.Num()>1) { Error=TEXT("The delegate input has multiple sources; repair it before binding a function."); return false; }
    for (UEdGraphPin* Prior:Input->LinkedTo)
    {
        if (!Prior || Prior->GetOwningNode()->GetGraph()!=Graph || !Prior->LinkedTo.Contains(Input))
        { Error=TEXT("The existing delegate connection is invalid; repair it before replacing it."); return false; }
        UEdGraphNode* Original=TurboMCPGraphScratch::ByGuid(Identity,Prior->GetOwningNode()->NodeGuid);
        Displaced.Add(TurboMCPNodeId::MakeForNode(Original?Original:Prior->GetOwningNode(),true)+TEXT(" pin=")+Prior->PinName.ToString());
        // An exact, still-compatible binding is a no-op. Do this before creating
        // a probe, which would otherwise displace the existing connection.
        if (auto* Existing=Cast<UK2Node_CreateDelegate>(Prior->GetOwningNode()))
        {
            UFunction* Signature=Existing->GetDelegateSignature();
            UClass* Scope=Existing->GetScopeClass();
            UFunction* Function=Scope?Scope->FindFunctionByName(FName(*FunctionName)):nullptr;
            UEdGraphPin* ObjectInput=Existing->GetObjectInPin();
            const bool SameObject=ObjectInput && (ExplicitObject
                ? ObjectInput->LinkedTo.Num()==1 && ObjectInput->LinkedTo[0]==ObjectOutput && ObjectOutput->LinkedTo.Contains(ObjectInput)
                : ObjectInput->LinkedTo.IsEmpty() && !ObjectInput->DefaultObject);
            if (Prior==Existing->GetDelegateOutPin() && Function && Signature
                && Existing->GetFunctionName()==Function->GetFName() && SameObject
                && Signature->IsSignatureCompatibleWith(Function) && UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(Function))
            {
                Report=TEXT("The requested function and object are already connected; no node was created. Existing delegate: ")
                    +TurboMCPNodeId::MakeForNode(Original?Original:Existing,true);
                return true;
            }
        }
    }
    if (static_cast<int64>(Target->NodePosX)-240 < MIN_int32 || static_cast<int64>(Target->NodePosY)+160 > MAX_int32)
    { Error=TEXT("The proposed delegate-node position exceeds the supported coordinate range."); return false; }
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
    { Error=TEXT("The function must exist on the chosen object class, be usable as a delegate and match the target signature."); return false; }
    Event->SetFunction(Function->GetFName());
    Event->HandleAnyChangeWithoutNotifying();
    UEdGraphPin* Output=Event->GetDelegateOutPin();
    UEdGraphPin* CurrentInput=Pin(Target,TargetPin,EGPD_Input);
    if(Event->GetFunctionName()!=Function->GetFName() || !Output || !CurrentInput
        || !Output->LinkedTo.Contains(CurrentInput) || !CurrentInput->LinkedTo.Contains(Output))
    { Error=TEXT("The delegate function selection or connection failed verification."); return false; }
    if(ExplicitObject)
    {
        UEdGraphPin* CurrentObject=Pin(ObjectNode,ObjectPin,EGPD_Output);
        UEdGraphPin* ObjectInput=Event->GetObjectInPin();
        if(!CurrentObject || !ObjectInput || ObjectInput->LinkedTo.Num()!=1
            || ObjectInput->LinkedTo[0]!=CurrentObject || !CurrentObject->LinkedTo.Contains(ObjectInput))
        { Error=TEXT("The delegate object connection failed verification."); return false; }
    }
    Report=TEXT("Created function delegate ")+TurboMCPNodeId::MakeForNode(Event,true)+TEXT("; function=")+Function->GetName()
        +TEXT("; target=")+TargetId+TEXT(" pin=")+TargetPin+TEXT("; object=")
        +(ExplicitObject?ObjectNodeId+TEXT(" pin=")+ObjectPin:TEXT("self"));
    Report+=Displaced.IsEmpty() ? TEXT("\nThe target input was previously unconnected.")
        : TEXT("\nReplaced input sources (nodes retained; inspect remaining uses before cleanup):\n")+FString::Join(Displaced,TEXT("\n"));
    Report+=TEXT("\nFunction selection and requested links were verified. This creates a delegate value, not a runtime registration; the owning Bind or Timer node must execute. Read back, arrange new nodes, then compile and save explicitly.");
    return true;
}
bool Remove(UEdGraph* Graph,UEdGraph* Identity,const FString& Nodes,const FString& Links,FString& Report,FString& Error)
{
    TArray<TSharedPtr<FJsonValue>> NodeJson, LinkJson;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Nodes),NodeJson)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Links),LinkJson))
    { Error=TEXT("Nodes and Links must be JSON arrays. Nodes example: [\"unique node key or reader ID\"]. Links example: [{\"sourceNodeId\":\"source ID\",\"sourcePin\":\"then\",\"targetNodeId\":\"target ID\",\"targetPin\":\"execute\"}]. Use [] for an unused selection"); return false; }
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
    // Preview and edit share the same preflight; layout and compilation are separate.
    if (!Graph || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    { Error=TEXT("Sequence refactoring requires a Blueprint K2 graph."); return false; }
    TArray<TSharedPtr<FJsonValue>> Json;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Groups),Json) || Json.Num()<2 || Json.Num()>64)
    { Error=TEXT("Groups must be a JSON array of 2 to 64 ordered, nonempty arrays of reader node IDs."); return false; }
    TArray<TArray<UEdGraphNode*>> Rows; TArray<UEdGraphNode*> Chain; TSet<UEdGraphNode*> Selected;
    TArray<UEdGraphPin*> Ins,Outs;
    for(const auto& Value:Json)
    {
        if(!Value.IsValid() || Value->Type!=EJson::Array || Value->AsArray().IsEmpty()) { Error=TEXT("Each group must be a nonempty array of node IDs."); return false; }
        TArray<UEdGraphNode*> Row;
        for(const auto& Id:Value->AsArray())
        {
            FString Name; if(!Id.IsValid() || Id->Type!=EJson::String || !Id->TryGetString(Name)) { Error=TEXT("Each node ID must be a JSON string."); return false; }
            if (Chain.Num()>=4096) { Error=TEXT("Groups may contain at most 4096 calls in total."); return false; }
            UEdGraphNode* N=ResolveEditNode(Graph,Identity,Name,Error);
            if (!N) return false;
            UK2Node_CallFunction* Call=Cast<UK2Node_CallFunction>(N); UFunction* Fn=Call?Call->GetTargetFunction():nullptr;
            if(!Call || Call->GetClass()!=UK2Node_CallFunction::StaticClass() || !Fn || Call->IsNodePure() || Fn->HasMetaData(TEXT("Latent")) || Selected.Contains(N))
            { Error=TEXT("Groups require distinct, synchronous function calls with one execution input and output; latent, branching and unsupported nodes are refused."); return false; }
            UEdGraphPin* In=nullptr; UEdGraphPin* Out=nullptr;
            for(UEdGraphPin* P:N->Pins) if(P && !P->bOrphanedPin && P->PinType.PinCategory==TEXT("exec"))
            {
                if(P->Direction==EGPD_Input) { if(In) { Error=TEXT("Calls with multiple execution inputs are not supported."); return false; } In=P; }
                else { if(Out) { Error=TEXT("Calls with multiple execution outputs are not supported."); return false; } Out=P; }
            }
            if(!In || !Out || In->LinkedTo.Num()!=1 || Out->LinkedTo.Num()>1) { Error=TEXT("Calls must form one connected, linear execution chain."); return false; }
            Ins.Add(In); Outs.Add(Out); Selected.Add(N); Row.Add(N); Chain.Add(N);
        }
        Rows.Add(Row);
    }
    for(int32 I=1;I<Chain.Num();++I) if(Outs[I-1]->LinkedTo.Num()!=1 || Outs[I-1]->LinkedTo[0]!=Ins[I] || Ins[I]->LinkedTo[0]!=Outs[I-1])
    { Error=TEXT("Groups must list the original chain in execution order, without gaps or reordering."); return false; }
    UEdGraphPin* Before=Ins[0]->LinkedTo[0]; UEdGraphPin* After=Outs.Last()->LinkedTo.IsEmpty()?nullptr:Outs.Last()->LinkedTo[0];
    if(!Before || Selected.Contains(Before->GetOwningNode()) || (After && Selected.Contains(After->GetOwningNode()))) { Error=TEXT("The selected execution chain contains a cycle."); return false; }
    if(Before->LinkedTo.Num()!=1 || Before->LinkedTo[0]!=Ins[0] || (After && (After->LinkedTo.Num()!=1 || After->LinkedTo[0]!=Outs.Last())))
    { Error=TEXT("Entry and continuation links must be exclusive and reciprocal."); return false; }
    // A continuation must not re-enter the selected chain through intermediate nodes.
    TArray<UEdGraphNode*> Pending; TSet<UEdGraphNode*> Seen;
    if (After) Pending.Add(After->GetOwningNode());
    while (!Pending.IsEmpty())
    {
        UEdGraphNode* N=Pending.Pop(EAllowShrinking::No);
        if (!N || Seen.Contains(N)) continue;
        if (Selected.Contains(N)) { Error=TEXT("The continuation returns to a selected call; cyclic chains are not supported."); return false; }
        Seen.Add(N);
        for (UEdGraphPin* P:N->Pins) if (P && P->Direction==EGPD_Output && P->PinType.PinCategory==TEXT("exec"))
            for (UEdGraphPin* Q:P->LinkedTo) if (Q) Pending.Add(Q->GetOwningNode());
    }
    if (static_cast<int64>(Chain[0]->NodePosX)-300 < MIN_int32)
    { Error=TEXT("The Sequence position would exceed the supported coordinate range."); return false; }
    UK2Node_ExecutionSequence* Seq=NewObject<UK2Node_ExecutionSequence>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Seq,false,false); Seq->CreateNewGuid(); Seq->PostPlacedNewNode(); Seq->AllocateDefaultPins();
    const int32 Outputs=Rows.Num()+(After?1:0);
    for(int32 I=2;I<Outputs;++I) Seq->AddInputPin();
    Seq->NodePosX=Chain[0]->NodePosX-300; Seq->NodePosY=Chain[0]->NodePosY;
    UEdGraphPin* SeqIn=Seq->FindPin(TEXT("execute"),EGPD_Input);
    if(!SeqIn) { Error=TEXT("The new Sequence input is missing."); return false; }
    Before->BreakLinkTo(Ins[0]);
    if(!Graph->GetSchema()->TryCreateConnection(Before,SeqIn)) { Error=TEXT("The schema refused the Sequence entry connection."); return false; }
    int32 Offset=0;
    for(int32 I=0;I<Rows.Num();++I)
    {
        UEdGraphPin* First=Ins[Offset]; UEdGraphPin* Last=Outs[Offset+Rows[I].Num()-1];
        First->BreakAllPinLinks(); Last->BreakAllPinLinks();
        if(!Graph->GetSchema()->TryCreateConnection(Seq->GetThenPinGivenIndex(I),First)) { Error=TEXT("The schema refused a Sequence row connection."); return false; }
        Offset+=Rows[I].Num();
    }
    if(After && !Graph->GetSchema()->TryCreateConnection(Seq->GetThenPinGivenIndex(Rows.Num()),After)) { Error=TEXT("The schema refused the Sequence continuation connection."); return false; }
    Offset=0;
    for(int32 I=0;I<Rows.Num();++I)
    {
        UEdGraphPin* Output=Seq->GetThenPinGivenIndex(I);
        if(!Output || Output->LinkedTo.Num()!=1 || Output->LinkedTo[0]!=Ins[Offset] || Ins[Offset]->LinkedTo.Num()!=1 || Ins[Offset]->LinkedTo[0]!=Output)
        { Error=TEXT("Sequence row connection verification failed."); return false; }
        Offset+=Rows[I].Num();
    }
    if (Before->LinkedTo.Num()!=1 || Before->LinkedTo[0]!=SeqIn || !SeqIn->LinkedTo.Contains(Before)
        || (After && (After->LinkedTo.Num()!=1 || After->LinkedTo[0]!=Seq->GetThenPinGivenIndex(Rows.Num())
            || !Seq->GetThenPinGivenIndex(Rows.Num())->LinkedTo.Contains(After))))
    { Error=TEXT("Sequence entry or continuation verification failed."); return false; }
    int32 Copies=0;
    FString CopyMapping;
    if(SplitValues)
    {
        const TArray<TObjectPtr<UEdGraphNode>> Nodes=Graph->Nodes;
        for (UEdGraphNode* N:Nodes) if (Trivial(N))
        {
            bool UsedBySelection=false;
            for (UEdGraphPin* P:N->Pins) if (P && P->Direction==EGPD_Output)
                for (UEdGraphPin* Q:P->LinkedTo) if (Q && Selected.Contains(Q->GetOwningNode())) UsedBySelection=true;
            // Do not validate or rewrite unrelated values elsewhere in the graph.
            if (UsedBySelection)
            {
                for (UEdGraphNode* Consumer:Selected) if (static_cast<int64>(Consumer->NodePosX)-240 < MIN_int32)
                { Error=TEXT("A value-copy position would exceed the supported coordinate range."); return false; }
                if (!Split(Graph,N,&Selected,Copies,Error,&CopyMapping)) return false;
            }
        }
    }
    Report=FString::Printf(TEXT("sequence lanes=%d; ordered calls=%d; value copies=%d; continuation=%s; execution order preserved; no save"),Rows.Num(),Chain.Num(),Copies,After?TEXT("last output"):TEXT("none"));
    Report+=TEXT("\nSequence node: ")+TurboMCPNodeId::MakeForNode(Seq,true);
    for (int32 I=0;I<Rows.Num();++I)
        Report+=FString::Printf(TEXT("\nThen %d: %d call(s), first=%s, last=%s"),I,Rows[I].Num(),
            *TurboMCPNodeId::MakeForNode(Rows[I][0],true),*TurboMCPNodeId::MakeForNode(Rows[I].Last(),true));
    if (After) Report+=FString::Printf(TEXT("\nThen %d: continuation=%s pin=%s"),Rows.Num(),
        *TurboMCPNodeId::MakeForNode(After->GetOwningNode(),true),*After->PinName.ToString());
    Report+=CopyMapping+TEXT("\nLayout is not tidied automatically. Read the graph, then tidy the affected nodes and inspect a capture.");
    return true;
}
}
namespace
{
// Report the exact operation rather than claiming that a scratch preview edited
// the asset. Existing data sources and exec destinations are different contracts.
FString ConnectNamed(const FString& Asset, const FString& GraphName, bool bEdit,
    const FString& SourceNodeId, const FString& SourcePin, const FString& TargetNodeId, const FString& TargetPin)
{
    return Run(Asset, GraphName, bEdit, [&](UEdGraph* Graph, UEdGraph* Identity, FString& Report, FString& Error)
    {
        UEdGraphNode* Source = ResolveEditNode(Graph, Identity, SourceNodeId, Error);
        if (!Source) return false;
        UEdGraphNode* Target = ResolveEditNode(Graph, Identity, TargetNodeId, Error);
        if (!Target) return false;
        UEdGraphPin* Output = Pin(Source, SourcePin, EGPD_Output);
        UEdGraphPin* Input = Pin(Target, TargetPin, EGPD_Input);
        const FString Endpoints = TurboMCPNodeId::MakeForNode(TurboMCPGraphScratch::ByGuid(Identity, Source->NodeGuid), true) + TEXT(":") + SourcePin
            + TEXT(" -> ") + TurboMCPNodeId::MakeForNode(TurboMCPGraphScratch::ByGuid(Identity, Target->NodeGuid), true) + TEXT(":") + TargetPin;
        const bool bExec = Output && Output->PinType.PinCategory == TEXT("exec");
        TArray<FString> Displaced;
        TArray<FGuid> DisplacedSourceNodes;
        UEdGraphPin* ReplacedPin = bExec ? Output : Input;
        if (ReplacedPin)
            for (UEdGraphPin* Old : ReplacedPin->LinkedTo)
                if (Old && Old != (bExec ? Input : Output))
                {
                    if (!bExec) DisplacedSourceNodes.AddUnique(Old->GetOwningNode()->NodeGuid);
                    UEdGraphNode* Original = TurboMCPGraphScratch::ByGuid(Identity, Old->GetOwningNode()->NodeGuid);
                    if (Original) Displaced.Add(TurboMCPNodeId::MakeForNode(Original, true) + TEXT(":") + Old->PinName.ToString());
                }
        if (!Connect(Graph, Output, Input, Error))
        { Error += TEXT("\nconnection: ") + Endpoints; return false; }
        Report = TEXT("connection: ") + Endpoints
            + (bExec ? TEXT("\nreplaced source-output destinations: ") : TEXT("\nreplaced target-input sources: "))
            + (Displaced.IsEmpty() ? FString(TEXT("none")) : FString::Join(Displaced, TEXT(", ")))
            + TEXT("\nNo automatic continuation, conversion nodes, compile or save.");
        // Inspect every output of displaced data producers. One disconnected
        // pin is not enough: shared values and other output consumers must survive.
        TArray<FString> Unconsumed;
        for (const FGuid& Guid : DisplacedSourceNodes)
        {
            UK2Node* Node = Cast<UK2Node>(TurboMCPGraphScratch::ByGuid(Graph, Guid));
            UEdGraphNode* Original = TurboMCPGraphScratch::ByGuid(Identity, Guid);
            if (!Node || !Original || !Node->IsNodePure()) continue;
            bool bHasOutput = false, bHasConsumer = false, bHasExec = false;
            for (const UEdGraphPin* P : Node->Pins) if (P)
            {
                bHasExec |= P->PinType.PinCategory == TEXT("exec");
                if (P->Direction == EGPD_Output)
                { bHasOutput = true; bHasConsumer |= !P->LinkedTo.IsEmpty(); }
            }
            if (bHasOutput && !bHasConsumer && !bHasExec)
                Unconsumed.Add(TurboMCPNodeId::MakeForNode(Original, true));
        }
        if (!Unconsumed.IsEmpty())
            Report += TEXT("\ncleanup review: displaced pure node(s) have no output consumers after this connection: ")
                + FString::Join(Unconsumed, TEXT(", "))
                + TEXT("\nNo nodes were deleted. Inspect these nodes and their dependencies; if obsolete, use PreviewTurboMCPGraphRemove then EditTurboMCPGraphRemove with these exact IDs. This is not proof that arbitrary pure calls are safe to delete.");
        return true;
    }, false);
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin)
{ return ConnectNamed(Asset, GraphName, false, SourceNodeId, SourcePin, TargetNodeId, TargetPin); }
FString UTurboMCPEditToolset::EditTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin)
{ return ConnectNamed(Asset, GraphName, true, SourceNodeId, SourcePin, TargetNodeId, TargetPin); }
namespace
{
FString SplitNamed(const FString& Asset, const FString& GraphName, const FString& NodeId, bool bEdit)
{
    if (GraphName.IsEmpty()) return TEXT("unchanged\nrefused: GraphName is required; use an explicit graph name from the reader.");
    return Run(Asset,GraphName,bEdit,[&](UEdGraph* Graph,UEdGraph* Identity,FString& Report,FString& Error)
    {
        UEdGraphNode* Source=ResolveEditNode(Graph,Identity,NodeId,Error);
        if (!Source) return false;
        int32 Count=0; FString Mapping;
        if (!Split(Graph,Source,nullptr,Count,Error,&Mapping)) return false;
        Report=Count==0
            ? TEXT("No copies needed: the source has zero or one connected consumer input.")
            : FString::Printf(TEXT("Created %d copies, one per connected consumer input; removed the original source. Defaults and existing input sources are preserved."),Count)
                +Mapping+TEXT("\nExisting nodes were not moved. New copies need layout; tidy the affected nodes and inspect a capture before compiling and saving explicitly.");
        return true;
    });
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId)
{ return SplitNamed(Asset,GraphName,NodeId,false); }
FString UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId)
{ return SplitNamed(Asset,GraphName,NodeId,true); }
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues)
{ if (GraphName.IsEmpty()) return TEXT("unchanged\nrefused: GraphName is required; use an explicit graph name from the reader."); return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Sequence(G,I,Groups,bSplitValues,R,E); }); }
FString UTurboMCPEditToolset::EditTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues)
{ if (GraphName.IsEmpty()) return TEXT("unchanged\nrefused: GraphName is required; use an explicit graph name from the reader."); return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Sequence(G,I,Groups,bSplitValues,R,E); }); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Remove(G,I,Nodes,Links,R,E); }, false); }

FString UTurboMCPEditToolset::EditTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return Remove(G,I,Nodes,Links,R,E); }, false); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin)
{ if (GraphName.IsEmpty()) return TEXT("unchanged\nrefused: GraphName is required; use an explicit graph name from the reader."); return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindFunction(G,I,TargetNodeId,TargetPin,FunctionName,ObjectNodeId,ObjectPin,R,E); }); }

FString UTurboMCPEditToolset::EditTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId,const FString& ObjectPin)
{ if (GraphName.IsEmpty()) return TEXT("unchanged\nrefused: GraphName is required; use an explicit graph name from the reader."); return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindFunction(G,I,TargetNodeId,TargetPin,FunctionName,ObjectNodeId,ObjectPin,R,E); }); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName)
{ return Run(Asset,GraphName,false,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindNewEvent(G,I,TargetNodeId,TargetPin,EventName,R,E); }); }

FString UTurboMCPEditToolset::EditTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName)
{ return Run(Asset,GraphName,true,[&](UEdGraph* G,UEdGraph* I,FString& R,FString& E) { return BindNewEvent(G,I,TargetNodeId,TargetPin,EventName,R,E); }); }
