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
#include "K2Node_CustomEvent.h"
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

// Disposable designer setup is a prerequisite, not widget-authoring tool evidence.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetVariableWorkflowTest,
    "TurboMCP.Workflow.WidgetVariable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPWidgetVariableWorkflowTest::RunTest(const FString& Parameters)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/WidgetVariable_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("widget-variable-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false);
    };
    auto Record=[&](const TCHAR* Name,const FString& Value){Transcript+=FString::Printf(TEXT("\n=== %s ===\n%s\n"),Name,*Value);return Value;};
    UWidgetBlueprint* BP=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Variable"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("WidgetVariableReview"),false);
    if(!BP)return false;
    UTextBlock* Label=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("PurchaseLabel"));
    UTextBlock* Other=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("OtherLabel"));
    for(UTextBlock* W:{Label,Other}) {
        W->bIsVariable=false;BP->WidgetVariableNameToGuidMap.Add(W->GetFName(),FGuid::NewGuid());
        Cast<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(W)->SetSize(FVector2D(300,80));
    }
    Label->SetText(FText::FromString(TEXT("Purchase")));Other->SetText(FText::FromString(TEXT("Unrelated")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    const FString Asset=BP->GetPathName();
    auto Flags=[&](){return FString::Printf(TEXT("%d,%d"),Label->bIsVariable,Other->bIsVariable);};
    auto Call=[&](const FString& Names,bool Value){return Record(TEXT("EditTurboMCPWidgetsAsVars"),UTurboMCPEditToolset::EditTurboMCPWidgetsAsVars(Asset,Names,Value));};
    const FString Before=Flags();const bool Dirty=Package->IsDirty();
    TestTrue(TEXT("Empty list refused"),Call(TEXT(""),true).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Missing batch refused"),Call(TEXT("PurchaseLabel,MissingLabel"),true).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Invalid requests preserve flags"),Flags(),Before);TestEqual(TEXT("Invalid requests preserve dirty state"),Package->IsDirty(),Dirty);
    const FString Enabled=Call(TEXT("PurchaseLabel,purchaselabel,OtherLabel"),true);
    TestTrue(TEXT("Case duplicates counted once"),Enabled.Contains(TEXT("2 of 2")));
    TestTrue(TEXT("Both flags enabled"),Label->bIsVariable&&Other->bIsVariable);
    const bool EnabledDirty=Package->IsDirty();
    TestTrue(TEXT("Repeated enable unchanged"),Call(TEXT("PurchaseLabel,OtherLabel"),true).StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("No-op preserves dirty flag"),Package->IsDirty(),EnabledDirty);
    Record(TEXT("Compile exposed widgets"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));
    if(!TestTrue(TEXT("Exposure compiles"),BP->Status!=BS_Error))return false;
    TestNotNull(TEXT("Compiled widget member exists"),FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("PurchaseLabel")));
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("VariableUse"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    // Use a custom named callback so the test controls execution explicitly.
    UK2Node_CustomEvent* Callback=NewObject<UK2Node_CustomEvent>(Graph);Callback->CustomFunctionName=TEXT("UpdateLabel");Graph->AddNode(Callback,false,false);Callback->CreateNewGuid();Callback->AllocateDefaultPins();
    UK2Node_VariableGet* Get=NewObject<UK2Node_VariableGet>(Graph);Get->VariableReference.SetSelfMember(TEXT("PurchaseLabel"));Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->AllocateDefaultPins();
    UK2Node_CallFunction* Set=NewObject<UK2Node_CallFunction>(Graph);Set->SetFromFunction(UTextBlock::StaticClass()->FindFunctionByName(TEXT("SetText")));Graph->AddNode(Set,false,false);Set->CreateNewGuid();Set->AllocateDefaultPins();Set->FindPinChecked(TEXT("InText"))->DefaultTextValue=FText::FromString(TEXT("Updated"));
    const FString GetId=TurboMCPNodeId::MakeForNode(Get,true),SetId=TurboMCPNodeId::MakeForNode(Set,true),CallbackId=TurboMCPNodeId::MakeForNode(Callback,true);
    Record(TEXT("Connect target"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset,Graph->GetName(),GetId,TEXT("PurchaseLabel"),SetId,TEXT("self")));
    Record(TEXT("Connect execution"),UTurboMCPEditToolset::EditTurboMCPGraphConnect(Asset,Graph->GetName(),CallbackId,TEXT("then"),SetId,TEXT("execute")));
    Record(TEXT("Compile access"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));if(!TestTrue(TEXT("Graph access compiles"),BP->Status!=BS_Error))return false;
    UUserWidget* Live=NewObject<UUserWidget>(GetTransientPackage(),BP->GeneratedClass);Live->Initialize();Live->TakeWidget();Live->ProcessEvent(Live->FindFunctionChecked(TEXT("UpdateLabel")),nullptr);
    TestEqual(TEXT("Exposed member works in compiled logic"),CastChecked<UTextBlock>(Live->WidgetTree->FindWidget(TEXT("PurchaseLabel")))->GetText().ToString(),FString(TEXT("Updated")));
    const FString Referenced=Call(TEXT("OtherLabel,PurchaseLabel"),false);
    TestTrue(TEXT("Referenced disable refused with exact ID"),Referenced.StartsWith(TEXT("unchanged:"))&&Referenced.Contains(GetId));
    TestTrue(TEXT("Whole disable batch preserved"),Label->bIsVariable&&Other->bIsVariable);
    const FString Nodes=FString::Printf(TEXT("[\"%s\",\"%s\",\"%s\"]"),*GetId,*SetId,*CallbackId);
    Record(TEXT("Explicit graph cleanup"),UTurboMCPEditToolset::EditTurboMCPGraphRemove(Asset,Graph->GetName(),Nodes,TEXT("[]")));
    TestEqual(TEXT("References removed explicitly"),Graph->Nodes.Num(),0);
    TestTrue(TEXT("Unused widgets can be disabled"),Call(TEXT("PurchaseLabel,OtherLabel"),false).StartsWith(TEXT("changed:")));
    TestFalse(TEXT("Label no longer variable"),Label->bIsVariable);TestFalse(TEXT("Other no longer variable"),Other->bIsVariable);
    Record(TEXT("Compile disabled widgets"),UTurboMCPEditToolset::EditTurboMCPCompile(Asset));TestTrue(TEXT("Disable compiles"),BP->Status!=BS_Error);
    TestNull(TEXT("Generated member removed"),FindFProperty<FObjectPropertyBase>(BP->GeneratedClass,TEXT("PurchaseLabel")));
    TestTrue(TEXT("Designer widgets retained"),BP->WidgetTree->FindWidget(TEXT("PurchaseLabel"))==Label&&BP->WidgetTree->FindWidget(TEXT("OtherLabel"))==Other);
    TestEqual(TEXT("Designer text preserved"),Label->GetText().ToString(),FString(TEXT("Purchase")));
    TestEqual(TEXT("Other text preserved"),Other->GetText().ToString(),FString(TEXT("Unrelated")));
    TestTrue(TEXT("Repeated disable unchanged"),Call(TEXT("PurchaseLabel,OtherLabel"),false).StartsWith(TEXT("unchanged:")));
    return !HasAnyErrors();
}
#endif
