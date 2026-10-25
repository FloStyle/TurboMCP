// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
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

// Fixture setup uses engine APIs. Only recorded tool calls count as tool evidence.
// Common UI is enabled in TestProject, not made a TurboMCP dependency.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPOverrideRemoveWorkflowTest,
    "TurboMCP.Workflow.OverrideRemove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPOverrideRemoveWorkflowTest::RunTest(const FString& Parameters)
{
    FModuleManager::Get().LoadModule(TEXT("CommonUI"));
    UClass* ButtonClass = LoadClass<UUserWidget>(nullptr, TEXT("/Script/CommonUI.CommonButtonBase"));
    UClass* StyleClass = LoadClass<UObject>(nullptr, TEXT("/Script/CommonUI.CommonButtonStyle"));
    if (!TestNotNull(TEXT("Stock Common Button class available"), ButtonClass) || !TestNotNull(TEXT("Stock style class available"), StyleClass)) return false;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/CommonButton_") + Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TurboMCP/Workflow"), Suffix);
        IFileManager::Get().MakeDirectory(*Folder, true);
        FFileHelper::SaveStringToFile(Transcript, *FPaths::Combine(Folder, TEXT("override-remove-review.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Only the unique, never-saved fixture package.
    };
    auto Record = [&](const TCHAR* Name, const FString& Value) { Transcript += FString::Printf(TEXT("\n=== %s ===\n%s\n"), Name, *Value); return Value; };
    UBlueprint* Style = FKismetEditorUtilities::CreateBlueprint(StyleClass, Package, TEXT("BS_Review"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("CommonButtonReview"));
    if (!TestNotNull(TEXT("Style fixture created"), Style)) return false;
    FKismetEditorUtilities::CompileBlueprint(Style);
    UWidgetBlueprint* BP = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, TEXT("WBP_CommonReview"), BPTYPE_Normal, ButtonClass, UCanvasPanel::StaticClass(), TEXT("CommonButtonReview"), false);
    if (!TestNotNull(TEXT("Button fixture created"), BP)) return false;
    UTextBlock* Label = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
    Label->bIsVariable = true; BP->WidgetVariableNameToGuidMap.Add(Label->GetFName(), FGuid::NewGuid()); Label->SetText(FText::FromString(TEXT("Purchase")));
    UCanvasPanelSlot* Slot = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Label);
    Slot->SetPosition(FVector2D(40,40)); Slot->SetSize(FVector2D(300,80));
    FKismetEditorUtilities::CompileBlueprint(BP);
    // Remove only the disposable template's unused default events before the scenario.
    // This is fixture preparation, not evidence for a removal tool.
    for(UEdGraph* G:BP->UbergraphPages) {
        const TArray<TObjectPtr<UEdGraphNode>> Defaults=G->Nodes;
        for(UEdGraphNode* N:Defaults)G->RemoveNode(N);
    }
    const FString OverrideAsset=BP->GetPathName();
    auto Snapshot=[&]() {
        TArray<FString> Rows;
        for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* N:G->Nodes) {
            FString Row=N->NodeGuid.ToString()+FString::Printf(TEXT(":%d,%d"),N->NodePosX,N->NodePosY);
            for(UEdGraphPin* P:N->Pins)if(P){Row+=P->PinName.ToString()+P->DefaultValue+P->DefaultTextValue.ToString();for(UEdGraphPin* L:P->LinkedTo)Row+=L->PinId.ToString();}
            Rows.Add(Row);
        }
        Rows.Sort();return FString::Join(Rows,TEXT("\n"));
    };
    UEdGraph* Graph=nullptr;
    for (const auto& Pair : {TPair<FName,FString>(TEXT("BP_OnHovered"),TEXT("Ready to purchase")), {TEXT("BP_OnUnhovered"),TEXT("Purchase")}})
    {
        const FString Created=Record(TEXT("EditTurboMCPOverrideAdd"),UTurboMCPEditToolset::EditTurboMCPOverrideAdd(OverrideAsset,Pair.Key.ToString()));
        UK2Node_Event* Event=nullptr;
        for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* N:G->Nodes)if(UK2Node_Event* E=Cast<UK2Node_Event>(N))if(E->GetFunctionName()==Pair.Key){Event=E;Graph=G;}
        if(!TestNotNull(TEXT("Override event created"),Event))return false;
        TestTrue(TEXT("Result provides live node identity"),Created.Contains(TurboMCPNodeId::MakeForNode(Event,true)));
        UK2Node_VariableGet* Get=NewObject<UK2Node_VariableGet>(Graph);Get->VariableReference.SetSelfMember(TEXT("Label"));Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->AllocateDefaultPins();
        UK2Node_CallFunction* Call=NewObject<UK2Node_CallFunction>(Graph);Call->SetFromFunction(UTextBlock::StaticClass()->FindFunctionByName(TEXT("SetText")));Graph->AddNode(Call,false,false);Call->CreateNewGuid();Call->AllocateDefaultPins();
        Call->FindPinChecked(TEXT("InText"))->DefaultTextValue=FText::FromString(Pair.Value);
        const FString EventId=TurboMCPNodeId::MakeForNode(Event,true),CallId=TurboMCPNodeId::MakeForNode(Call,true),GetId=TurboMCPNodeId::MakeForNode(Get,true);
        Record(TEXT("Connect body"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(OverrideAsset,Graph->GetName(),EventId,TEXT("then"),CallId,TEXT("execute")));
        Record(TEXT("Connect label"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(OverrideAsset,Graph->GetName(),GetId,TEXT("Label"),CallId,TEXT("self")));
    }
    // Establish a working baseline before the focused removal operation.
    Record(TEXT("Compile baseline"),UTurboMCPEditToolset::EditTurboMCPCompile(OverrideAsset));
    if(!TestTrue(TEXT("Baseline compiles"),BP->Status!=BS_Error))return false;
    UK2Node_Event* Hover=nullptr;UK2Node_Event* Unhover=nullptr;
    for(UEdGraphNode* N:Graph->Nodes)if(UK2Node_Event* E=Cast<UK2Node_Event>(N)) {
        if(E->GetFunctionName()==TEXT("BP_OnHovered"))Hover=E;
        if(E->GetFunctionName()==TEXT("BP_OnUnhovered"))Unhover=E;
    }
    if(!Hover||!Unhover)return false;
    UEdGraphNode* HoverBody=Hover->FindPinChecked(TEXT("then"))->LinkedTo[0]->GetOwningNode();
    UEdGraphNode* HoverGetter=HoverBody->FindPinChecked(TEXT("self"))->LinkedTo[0]->GetOwningNode();
    const FString HoverId=TurboMCPNodeId::MakeForNode(Hover,true),BodyId=TurboMCPNodeId::MakeForNode(HoverBody,true),GetterId=TurboMCPNodeId::MakeForNode(HoverGetter,true);
    FClassProperty* StyleProperty=FindFProperty<FClassProperty>(ButtonClass,TEXT("Style"));
    if(!TestNotNull(TEXT("Style property available"),StyleProperty))return false;
    StyleProperty->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Style->GeneratedClass);
    // Compare hierarchy separately from reflected override owners, which must
    // change when the local hover implementation is removed.
    auto Hierarchy=[&]() { FString Tree,Context; const FString Read=UTurboMCPReadToolset::ReadTurboMCPWidgetTree(OverrideAsset); return Read.Split(TEXT("; Common Button context:"),&Tree,&Context)?Tree:Read; };
    const FString TreeBefore=Hierarchy();
    const FString Before=Snapshot();
    for(const FString& Name:{FString(),FString(TEXT("MissingOverride"))}) {
        const FString Result=Record(TEXT("OverrideRemove invalid or missing"),UTurboMCPEditToolset::EditTurboMCPOverrideRemove(OverrideAsset,Name));
        TestTrue(TEXT("Invalid or missing removal does not claim a change"),Result.StartsWith(TEXT("error:"))||Result.StartsWith(TEXT("unchanged:")));
        TestEqual(TEXT("Invalid removal preserves graph"),Snapshot(),Before);
    }
    TMap<UEdGraphPin*,TArray<UEdGraphPin*>> OtherLinks;
    TMap<UEdGraphNode*,FIntPoint> Positions;
    for(UEdGraphNode* N:Graph->Nodes)if(N!=Hover) {
        Positions.Add(N,FIntPoint(N->NodePosX,N->NodePosY));
        for(UEdGraphPin* P:N->Pins)if(P){TArray<UEdGraphPin*> Links=P->LinkedTo;Links.RemoveAll([&](UEdGraphPin* L){return L->GetOwningNode()==Hover;});OtherLinks.Add(P,Links);}
    }
    const int32 BeforeCount=Graph->Nodes.Num();
    const FString Removed=Record(TEXT("EditTurboMCPOverrideRemove"),UTurboMCPEditToolset::EditTurboMCPOverrideRemove(OverrideAsset,TEXT("BP_OnHovered")));
    TestTrue(TEXT("Removal succeeds"),Removed.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Result identifies event and detached body"),Removed.Contains(HoverId)&&Removed.Contains(BodyId));
    TestEqual(TEXT("Only event removed"),Graph->Nodes.Num(),BeforeCount-1);
    TestFalse(TEXT("Hover event absent"),Graph->Nodes.Contains(Hover));
    TestTrue(TEXT("Unhover and hover body retained"),Graph->Nodes.Contains(Unhover)&&Graph->Nodes.Contains(HoverBody)&&Graph->Nodes.Contains(HoverGetter));
    for(const auto& Pair:OtherLinks)TestTrue(TEXT("All other links preserved"),Pair.Key->LinkedTo==Pair.Value);
    for(const auto& Pair:Positions)TestTrue(TEXT("Other positions preserved"),FIntPoint(Pair.Key->NodePosX,Pair.Key->NodePosY)==Pair.Value);
    const FString After=Snapshot();
    TestTrue(TEXT("Repeated removal unchanged"),Record(TEXT("OverrideRemove repeat"),UTurboMCPEditToolset::EditTurboMCPOverrideRemove(OverrideAsset,TEXT("BP_OnHovered"))).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Repeat preserves graph"),Snapshot(),After);
    const FString Available=Record(TEXT("Read overrides after removal"),UTurboMCPReadToolset::ReadTurboMCPOverrides(OverrideAsset,TEXT("available")));
    TestTrue(TEXT("Removed override available again"),Available.Contains(TEXT("BP_OnHovered :")));
    // This fixture's disconnected body has no other execution entry and its getter
    // is used only by that body. Cleanup is a separate explicit operation.
    const FString CleanupIds=FString::Printf(TEXT("[\"%s\",\"%s\"]"),*BodyId,*GetterId);
    Record(TEXT("Preview cleanup"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphRemove(OverrideAsset,Graph->GetName(),CleanupIds,TEXT("[]")));
    TestEqual(TEXT("Cleanup preview preserves graph"),Snapshot(),After);
    Record(TEXT("Edit cleanup"),UTurboMCPEditToolset::EditTurboMCPGraphRemove(OverrideAsset,Graph->GetName(),CleanupIds,TEXT("[]")));
    TestEqual(TEXT("Only unhover flow remains"),Graph->Nodes.Num(),3);
    Record(TEXT("Tidy remaining flow"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(OverrideAsset,Graph->GetName(),false,{}));
    Record(TEXT("Compile remaining flow"),UTurboMCPEditToolset::EditTurboMCPCompile(OverrideAsset));
    if(!TestTrue(TEXT("Remaining graph compiles"),BP->Status!=BS_Error))return false;
    TestTrue(TEXT("Style reference preserved"),StyleProperty->GetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject())==Style->GeneratedClass);
    TestEqual(TEXT("Widget tree preserved"),Hierarchy(),TreeBefore);
    UUserWidget* Live=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);Live->Initialize();Live->TakeWidget();
    UTextBlock* LiveLabel=Cast<UTextBlock>(Live->WidgetTree->FindWidget(TEXT("Label")));if(!LiveLabel)return false;
    LiveLabel->SetText(FText::FromString(TEXT("Sentinel")));
    Live->ProcessEvent(Live->FindFunctionChecked(TEXT("BP_OnHovered")),nullptr);
    TestEqual(TEXT("Removed hover no longer changes label"),LiveLabel->GetText().ToString(),FString(TEXT("Sentinel")));
    Live->ProcessEvent(Live->FindFunctionChecked(TEXT("BP_OnUnhovered")),nullptr);
    TestEqual(TEXT("Unhover still works"),LiveLabel->GetText().ToString(),FString(TEXT("Purchase")));
    const FString Capture=Record(TEXT("Final capture"),UTurboMCPLookToolset::LookTurboMCPCapture(OverrideAsset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1200x700")));
    TestTrue(TEXT("Capture succeeds"),Capture.Contains(TEXT("image: ")));
    return !HasAnyErrors();
}
#endif
