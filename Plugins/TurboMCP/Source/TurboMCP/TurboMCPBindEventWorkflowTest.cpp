// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPGraphTidy.h"

// Parent widget setup is a test prerequisite, not evidence for widget creation tools.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBindEventWorkflowTest,
    "TurboMCP.Workflow.BindEvent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPBindEventWorkflowTest::RunTest(const FString& Parameters)
{
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    UClass* ButtonClass=LoadClass<UUserWidget>(nullptr,TEXT("/Script/CommonUI.CommonButtonBase"));
    if(!TestNotNull(TEXT("Common UI available"),ButtonClass))return false;
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/BindEvent_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("bind-event-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Only this unique, never-saved fixture package.
    };
    auto Record=[&](const TCHAR* Tool,const FString& Value){Transcript+=FString::Printf(TEXT("\n=== %s ===\n%s\n"),Tool,*Value);return Value;};
    UWidgetBlueprint* Child=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_ChildButton"),BPTYPE_Normal,ButtonClass,UCanvasPanel::StaticClass(),TEXT("BindEventReview"),false);
    if(!TestNotNull(TEXT("Child created"),Child))return false;
    FKismetEditorUtilities::CompileBlueprint(Child);
    UWidgetBlueprint* BP=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Parent"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("BindEventReview"),false);
    if(!TestNotNull(TEXT("Parent created"),BP))return false;
    UWidget* Button=BP->WidgetTree->ConstructWidget<UWidget>(Child->GeneratedClass.Get(),TEXT("PurchaseButton"));
    Button->bIsVariable=true;BP->WidgetVariableNameToGuidMap.Add(Button->GetFName(),FGuid::NewGuid());
    Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Button);
    FEdGraphPinType ReceivedType;ReceivedType.PinCategory=UEdGraphSchema_K2::PC_Object;ReceivedType.PinSubCategoryObject=ButtonClass;
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("ReceivedButton"),ReceivedType);
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ParentEvents"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    UK2Node_CustomEvent* Register=NewObject<UK2Node_CustomEvent>(Graph);Register->CustomFunctionName=TEXT("RegisterButton");Graph->AddNode(Register,false,false);Register->CreateNewGuid();Register->AllocateDefaultPins();
    UK2Node_VariableGet* Get=NewObject<UK2Node_VariableGet>(Graph);Get->VariableReference.SetSelfMember(TEXT("PurchaseButton"));Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->AllocateDefaultPins();Get->NodePosY=160;
    FMulticastDelegateProperty* DelegateProperty=FindFProperty<FMulticastDelegateProperty>(ButtonClass,TEXT("OnButtonBaseClicked"));
    if(!TestNotNull(TEXT("Blueprint-assignable click delegate"),DelegateProperty))return false;
    UK2Node_AddDelegate* Bind=NewObject<UK2Node_AddDelegate>(Graph);Bind->SetFromProperty(DelegateProperty,false,ButtonClass);Graph->AddNode(Bind,false,false);Bind->CreateNewGuid();Bind->AllocateDefaultPins();Bind->NodePosX=640;
    Graph->GetSchema()->TryCreateConnection(Register->FindPinChecked(TEXT("then")),Bind->FindPinChecked(TEXT("execute")));
    Graph->GetSchema()->TryCreateConnection(Get->FindPinChecked(TEXT("PurchaseButton")),Bind->FindPinChecked(TEXT("self")));
    const FString Asset=BP->GetPathName(),GraphName=Graph->GetName(),Target=TurboMCPNodeId::MakeForNode(Bind,true),PinName=Bind->GetDelegatePin()->PinName.ToString();
    auto Snapshot=[&](){TArray<FString> Rows;for(UEdGraphNode* N:Graph->Nodes){Rows.Add(N->NodeGuid.ToString()+FString::Printf(TEXT(":%d:%d"),N->NodePosX,N->NodePosY));for(UEdGraphPin* P:N->Pins)if(P){Rows.Add(N->NodeGuid.ToString()+P->PinName.ToString()+P->DefaultValue+P->DefaultTextValue.ToString()+GetPathNameSafe(P->DefaultObject));for(UEdGraphPin* Q:P->LinkedTo)Rows.Add(N->NodeGuid.ToString()+P->PinName.ToString()+Q->GetOwningNode()->NodeGuid.ToString()+Q->PinName.ToString());}}Rows.Sort();return FString::Join(Rows,TEXT("\n"));};
    TMap<UEdGraphNode*,FIntPoint> Positions;
    TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
    TMap<UEdGraphPin*,FString> Defaults;
    for(UEdGraphNode* N:Graph->Nodes){Positions.Add(N,FIntPoint(N->NodePosX,N->NodePosY));for(UEdGraphPin* P:N->Pins)if(P){Links.Add(P,P->LinkedTo);Defaults.Add(P,P->DefaultValue+P->DefaultTextValue.ToString()+GetPathNameSafe(P->DefaultObject));}}
    const FString Before=Snapshot(),TreeBefore=UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset);const bool Dirty=Package->IsDirty();const int32 Count=Graph->Nodes.Num();
    Record(TEXT("ReadTurboMCPGraphLoad"),UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset,GraphName,TEXT("read"),TEXT("full")));
    for(int32 Case=0;Case<3;++Case)for(bool Edit:{false,true})
    {
        const FString Id=Case==0?TEXT("missing"):Target,Pin=Case==1?TEXT("MissingPin"):PinName,Name=Case==2?TEXT("bad name"):TEXT("HandlePurchase");
        const FString Result=Record(TEXT("BindNewEvent refusal"),Edit?UTurboMCPEditToolset::EditTurboMCPBindNewEvent(Asset,GraphName,Id,Pin,Name):UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(Asset,GraphName,Id,Pin,Name));
        TestTrue(TEXT("Invalid request explicitly refused"),Result.Contains(TEXT("refused:")));TestEqual(TEXT("Refusal preserves state"),Snapshot(),Before);TestEqual(TEXT("Refusal preserves dirty flag"),Package->IsDirty(),Dirty);
    }
    const FString TargetKey = Bind->NodeGuid.ToString(EGuidFormats::DigitsLower).Left(6);
    const FGuid OriginalGetGuid = Get->NodeGuid;
    Get->NodeGuid = Bind->NodeGuid;
    const FString AmbiguousBefore = Snapshot();
    for (bool Apply : {false, true})
    {
        const FString Result = Apply
            ? UTurboMCPEditToolset::EditTurboMCPBindNewEvent(Asset, GraphName, TargetKey, PinName, TEXT("HandlePurchase"))
            : UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(Asset, GraphName, TargetKey, PinName, TEXT("HandlePurchase"));
        TestTrue(TEXT("Ambiguous bind ID explains identity problem"), Result.Contains(TEXT("ambiguous")));
        TestEqual(TEXT("Ambiguous bind preserves graph"), Snapshot(), AmbiguousBefore);
        TestEqual(TEXT("Ambiguous bind preserves dirty flag"), Package->IsDirty(), Dirty);
    }
    Get->NodeGuid = OriginalGetGuid;
    const FString Preview=Record(TEXT("PreviewTurboMCPBindNewEvent"),UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(Asset,GraphName,Target,PinName,TEXT("HandlePurchase")));
    TestFalse(TEXT("Valid preview not refused"),Preview.Contains(TEXT("refused:")));TestEqual(TEXT("Preview preserves state"),Snapshot(),Before);TestEqual(TEXT("Preview preserves dirty flag"),Package->IsDirty(),Dirty);
    const FString Edit=Record(TEXT("EditTurboMCPBindNewEvent"),UTurboMCPEditToolset::EditTurboMCPBindNewEvent(Asset,GraphName,TargetKey,PinName,TEXT("HandlePurchase")));
    if(!TestTrue(TEXT("Binding changed"),Edit.StartsWith(TEXT("changed"))))return false;
    TestEqual(TEXT("Exactly one new handler"),Graph->Nodes.Num(),Count+1);
    for(const auto& Pair:Positions) TestEqual(TEXT("Existing positions preserved"),FIntPoint(Pair.Key->NodePosX,Pair.Key->NodePosY),Pair.Value);
    for(const auto& Pair:Links) if(Pair.Key!=Bind->GetDelegatePin()) TestTrue(TEXT("Existing links preserved"),Pair.Key->LinkedTo==Pair.Value);
    for(const auto& Pair:Defaults) TestEqual(TEXT("Existing defaults preserved"),Pair.Key->DefaultValue+Pair.Key->DefaultTextValue.ToString()+GetPathNameSafe(Pair.Key->DefaultObject),Pair.Value);
    TestTrue(TEXT("Result names target and parameters"),Edit.Contains(Target+TEXT(":")+PinName)&&Edit.Contains(TEXT("Button: object /Script/CommonUI.CommonButtonBase")));
    TestTrue(TEXT("Initially empty input is not called replaced"),Edit.Contains(TEXT("replaced input sources: none")));
    TestTrue(TEXT("Runtime registration requirement explicit"),Edit.Contains(TEXT("must execute at runtime")));

    UK2Node_CustomEvent* Handler=nullptr;for(UEdGraphNode* N:Graph->Nodes)if(auto* E=Cast<UK2Node_CustomEvent>(N))if(E->CustomFunctionName==TEXT("HandlePurchase"))Handler=E;
    if(!TestNotNull(TEXT("Typed event exists"),Handler))return false;
    UEdGraphPin* Sender=Handler->FindPin(TEXT("Button"));if(!TestNotNull(TEXT("Delegate parameter retained"),Sender))return false;
    TestTrue(TEXT("Sender type matches Common Button"),Sender->PinType.PinSubCategoryObject==ButtonClass);
    TestTrue(TEXT("Handler connected to delegate input"),Bind->GetDelegatePin()->LinkedTo.Num()==1&&Bind->GetDelegatePin()->LinkedTo[0]->GetOwningNode()==Handler);
    TestTrue(TEXT("Result names live event ID"),Edit.Contains(TurboMCPNodeId::MakeForNode(Handler,true)));
    TestEqual(TEXT("Widget tree preserved"),UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset),TreeBefore);
    const FString After=Snapshot();
    for(bool IsEdit:{false,true})
    {
        const FString Duplicate=Record(TEXT("BindNewEvent duplicate"),IsEdit?UTurboMCPEditToolset::EditTurboMCPBindNewEvent(Asset,GraphName,Target,PinName,TEXT("HandlePurchase")):UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(Asset,GraphName,Target,PinName,TEXT("HandlePurchase")));
        TestTrue(TEXT("Duplicate refused"),Duplicate.Contains(TEXT("refused:")));TestEqual(TEXT("Duplicate preserves graph"),Snapshot(),After);
    }
    const FString ReplacementPreview=Record(TEXT("PreviewTurboMCPBindNewEvent replacement"),UTurboMCPPreviewToolset::PreviewTurboMCPBindNewEvent(Asset,GraphName,Target,PinName,TEXT("OtherHandler")));
    TestTrue(TEXT("Replacement preview identifies retained source"),ReplacementPreview.Contains(TurboMCPNodeId::MakeForNode(Handler,true))&&ReplacementPreview.Contains(TEXT("Previous source nodes were retained")));
    TestEqual(TEXT("Replacement preview preserves original handler"),Snapshot(),After);
    // A body is a separate operation. Use the existing Connect tool as support.
    UK2Node_VariableSet* Set=NewObject<UK2Node_VariableSet>(Graph);Set->VariableReference.SetSelfMember(TEXT("ReceivedButton"));Graph->AddNode(Set,false,false);Set->CreateNewGuid();Set->AllocateDefaultPins();Set->NodePosX=640;Set->NodePosY=320;
    const FString HandlerId=TurboMCPNodeId::MakeForNode(Handler,true),SetId=TurboMCPNodeId::MakeForNode(Set,true);
    Record(TEXT("EditTurboMCPGraphConnect body"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset,GraphName,HandlerId,TEXT("then"),SetId,TEXT("execute")));
    Record(TEXT("EditTurboMCPGraphConnect sender"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset,GraphName,HandlerId,TEXT("Button"),SetId,TEXT("ReceivedButton")));
    // Layout is an explicit workflow step after binding and body wiring.
    const TArray<FString> LayoutIds={TurboMCPNodeId::MakeForNode(Register,true),TurboMCPNodeId::MakeForNode(Get,true),Target,HandlerId,SetId};
    const FString BeforeLayout=Snapshot();
    TMap<UEdGraphPin*,TArray<UEdGraphPin*>> LayoutLinks;
    TMap<UEdGraphPin*,FString> LayoutDefaults;
    const int32 LayoutCount=Graph->Nodes.Num();
    for(UEdGraphNode* N:Graph->Nodes)for(UEdGraphPin* P:N->Pins)if(P){LayoutLinks.Add(P,P->LinkedTo);LayoutDefaults.Add(P,P->DefaultValue+P->DefaultTextValue.ToString()+GetPathNameSafe(P->DefaultObject));}
    Record(TEXT("PreviewTurboMCPGraphTidy"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset,GraphName,false,LayoutIds));
    TestEqual(TEXT("Layout preview preserves graph"),Snapshot(),BeforeLayout);
    const FString Layout=Record(TEXT("EditTurboMCPGraphTidy"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,GraphName,false,LayoutIds));
    TestFalse(TEXT("Layout succeeds"),Layout.Contains(TEXT("refused:"))||Layout.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Layout preserves node count"),Graph->Nodes.Num(),LayoutCount);
    for(const auto& Pair:LayoutLinks)TestTrue(TEXT("Layout preserves links"),Pair.Key->LinkedTo==Pair.Value);
    for(const auto& Pair:LayoutDefaults)TestEqual(TEXT("Layout preserves defaults"),Pair.Key->DefaultValue+Pair.Key->DefaultTextValue.ToString()+GetPathNameSafe(Pair.Key->DefaultObject),Pair.Value);
    // Re-measure the live graph after preview/capture changes the geometry cache.
    FString AlignmentError;
    if(!TestTrue(TEXT("Live layout geometry available"),FTurboMCPGraphTidy::PrepareAlignment(BP,Graph,AlignmentError)))return false;
    const FVector2D HandlerExec=FTurboMCPGraphTidy::AlignmentPinPosition(Handler->FindPinChecked(TEXT("then")));
    const FVector2D SetExec=FTurboMCPGraphTidy::AlignmentPinPosition(Set->FindPinChecked(TEXT("execute")));
    TestTrue(TEXT("Handler execution pins align"),FMath::Abs(HandlerExec.Y-SetExec.Y)<=2.0);
    const FVector2D GetterPin=FTurboMCPGraphTidy::AlignmentPinPosition(Get->FindPinChecked(TEXT("PurchaseButton")));
    const FVector2D BindTarget=FTurboMCPGraphTidy::AlignmentPinPosition(Bind->FindPinChecked(TEXT("self")));
    Record(TEXT("Layout measurements"),FString::Printf(TEXT("getter=(%.1f,%.1f); target=(%.1f,%.1f); exec drift=%.1f"),GetterPin.X,GetterPin.Y,BindTarget.X,BindTarget.Y,FMath::Abs(HandlerExec.Y-SetExec.Y)));
    // Slate pin offsets describe the pin widget origin, not the output socket.
    // Measure visible node-edge clearance rather than including the getter label.
    const double GetterGap=Bind->NodePosX-(Get->NodePosX+FTurboMCPGraphTidy::AlignmentNodeSize(Get).X);
    TestTrue(TEXT("Button getter is near and before consumer"),GetterGap>=0.0&&GetterGap<=160.0);
    TestTrue(TEXT("Button getter aligns with target row"),FMath::Abs(GetterPin.Y-BindTarget.Y)<=2.0);
    Record(TEXT("EditTurboMCPCompile"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if(!TestTrue(TEXT("Bound parent compiles"),BP->Status!=BS_Error))return false;
    UUserWidget* Instance=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);Instance->Initialize();
    UWidget* LiveButton=Instance->WidgetTree->FindWidget(TEXT("PurchaseButton"));if(!TestNotNull(TEXT("Live button exists"),LiveButton))return false;
    UFunction* RegisterFunction=Instance->FindFunction(TEXT("RegisterButton"));if(!TestNotNull(TEXT("Registration function exists"),RegisterFunction))return false;
    Instance->ProcessEvent(RegisterFunction,nullptr);
    const FMulticastScriptDelegate* Delegate=DelegateProperty->GetMulticastDelegate(DelegateProperty->ContainerPtrToValuePtr<void>(LiveButton));
    struct FClickParams{UObject* Button;};FClickParams Args{LiveButton};Delegate->ProcessDelegate<UObject>(&Args);
    FObjectPropertyBase* Received=FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("ReceivedButton"));
    TestTrue(TEXT("Broadcast reaches handler with exact sender"),Received&&Received->GetObjectPropertyValue_InContainer(Instance)==LiveButton);
    Record(TEXT("LookTurboMCPCapture"),UTurboMCPLookToolset::LookTurboMCPCapture(Asset,GraphName,TEXT("graph"),TEXT(""),TEXT("1200x700")));
    return !HasAnyErrors();
}
#endif
