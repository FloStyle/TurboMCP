// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphScratch.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

namespace
{
FString Pins(const UK2Node_ComponentBoundEvent* Node)
{
    FString Result;
    for (const UEdGraphPin* Pin:Node->Pins) if (Pin)
        Result+=TEXT("\n  ")+Pin->PinName.ToString()+TEXT(": ")+Pin->PinType.PinCategory.ToString()
            +(Pin->Direction==EGPD_Output?TEXT(" output"):TEXT(" input"));
    return Result;
}
UK2Node_ComponentBoundEvent* CreateEvent(UEdGraph* Graph,FObjectProperty* Property,FMulticastDelegateProperty* Delegate,int32 X,int32 Y)
{
    auto* Node=NewObject<UK2Node_ComponentBoundEvent>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Node,false,false);Node->CreateNewGuid();
    Node->InitializeComponentBoundEventParams(Property,Delegate);
    Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;
    return Node;
}
FString WidgetEvent(bool Edit,const FString& Asset,const FString& GraphName,const FString& WidgetName,const FString& EventName)
{
    auto Refuse=[](const FString& Why){return TEXT("unchanged\nrefused: ")+Why;};
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
        return Refuse(TEXT("Requires the idle editor game thread, no play session and no active transaction."));
    if (GraphName.IsEmpty() || WidgetName.IsEmpty() || EventName.IsEmpty()) return Refuse(TEXT("GraphName, WidgetName and EventName are required exact names."));
    FTurboMCPAssetResolver::FResolved Resolved;
    const FString Error=FTurboMCPAssetResolver::Resolve(Asset,Resolved);
    if (!Error.IsEmpty()) return Refuse(Error);
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*Resolved.ObjectPath);
    if (!BP || !BP->WidgetTree || BP->bBeingCompiled || BP->bQueuedForCompilation) return Refuse(TEXT("Requires an available Widget Blueprint that is not compiling."));
    UEdGraph* Graph=nullptr;
    for (UEdGraph* Candidate:BP->UbergraphPages) if (Candidate && Candidate->GetName()==GraphName)
    { if (Graph) return Refuse(TEXT("GraphName is ambiguous."));Graph=Candidate; }
    if (!Graph || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>()) return Refuse(TEXT("GraphName must name an owned K2 event graph, not a function or inherited graph."));
    UWidget* Widget=nullptr;TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Candidate:Widgets) if (Candidate && Candidate->GetName()==WidgetName && Candidate->GetOuter()==BP->WidgetTree)
    { if (Widget) return Refuse(TEXT("WidgetName is ambiguous."));Widget=Candidate; }
    if (!Widget || !Widget->bIsVariable) return Refuse(TEXT("WidgetName must name an owned designer widget with Is Variable enabled. Inherited widgets are unsupported."));
    auto* Property=FindFProperty<FObjectProperty>(BP->SkeletonGeneratedClass,*WidgetName);
    if (!Property || Property->GetName()!=WidgetName || Property->PropertyClass!=Widget->GetClass())
        return Refuse(TEXT("The skeleton widget property is missing or differs from the designer class. Compile explicitly, inspect diagnostics, then retry."));
    auto* Delegate=FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(),*EventName);
    if (!Delegate || Delegate->GetName()!=EventName || !Delegate->HasAnyPropertyFlags(CPF_BlueprintAssignable) || !Delegate->SignatureFunction)
        return Refuse(TEXT("EventName must exactly name a BlueprintAssignable multicast delegate on the widget's actual class, not its display label."));
    for (TFieldIterator<FProperty> It(Delegate->SignatureFunction);It;++It)
        if (It->HasAnyPropertyFlags(CPF_ReturnParm) || (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm)))
            return Refuse(TEXT("Delegate signatures with return or writable output parameters are unsupported."));
    TArray<UK2Node_ComponentBoundEvent*> Events;FBlueprintEditorUtils::GetAllNodesOfClass(BP,Events);
    UK2Node_ComponentBoundEvent* Existing=nullptr;
    for (auto* Node:Events) if (Node && Node->ComponentPropertyName==Property->GetFName() && Node->DelegatePropertyName==Delegate->GetFName())
    { if (Existing) return Refuse(TEXT("Multiple bindings already exist for this widget and delegate; inspect and resolve them first."));Existing=Node; }
    if (Existing)
    {
        if (Existing->GetGraph()!=Graph || Existing->DelegateOwnerClass!=Delegate->GetOwnerClass()) return Refuse(TEXT("An existing binding conflicts with the requested graph or delegate class."));
        return TEXT("unchanged\nBinding already exists. node: ")+TurboMCPNodeId::MakeForNode(Existing,true)+Pins(Existing);
    }
    int64 Right=0,Top=0;bool HasNode=false;
    for (const UEdGraphNode* Node:Graph->Nodes) if (Node)
    {
        const int64 Edge=int64(Node->NodePosX)+FMath::Max(Node->NodeWidth,640);
        Right=HasNode?FMath::Max(Right,Edge):Edge;Top=HasNode?FMath::Min(Top,int64(Node->NodePosY)):Node->NodePosY;HasNode=true;
    }
    const int64 X=HasNode?Right+160:0;
    if (X>MAX_int32-640 || X<MIN_int32 || Top>MAX_int32-320 || Top<MIN_int32) return Refuse(TEXT("No representable position beyond existing graph bounds."));
    TurboMCPGraphScratch::FCopy Scratch;
    if (!Scratch.Init(BP,Graph)) return Refuse(TEXT("Could not create a disposable preview graph."));
    auto* Probe=CreateEvent(Scratch.Graph,Property,Delegate,int32(X),int32(Top));
    if (!Probe->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output)) return Refuse(TEXT("Native event initialization did not produce an execution output."));
    const FString Guidance=TEXT("\nDesigner delegate binding; this is not a runtime Bind Event node. Connect the event body separately. Compile explicitly to generate the binding, inspect diagnostics, then save. No compile or save was requested.");
    if (!Edit) return TEXT("unchanged\npreview: create widget event ")+WidgetName+TEXT(".")+EventName+TEXT("\npins:")+Pins(Probe)+Guidance;
    if (!GEditor->CanTransact()) return Refuse(TEXT("Undo is unavailable."));
    FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP bind widget event")));
    BP->Modify();Graph->Modify();
    auto* Node=CreateEvent(Graph,Property,Delegate,int32(X),int32(Top));
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Graph->NotifyGraphChanged();
    return TEXT("changed\nnode: ")+TurboMCPNodeId::MakeForNode(Node,true)+TEXT("\npins:")+Pins(Node)+Guidance;
}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPWidgetEvent(const FString& Asset,const FString& GraphName,const FString& WidgetName,const FString& EventName)
{ return WidgetEvent(false,Asset,GraphName,WidgetName,EventName); }
FString UTurboMCPEditToolset::EditTurboMCPWidgetEvent(const FString& Asset,const FString& GraphName,const FString& WidgetName,const FString& EventName)
{ return WidgetEvent(true,Asset,GraphName,WidgetName,EventName); }
