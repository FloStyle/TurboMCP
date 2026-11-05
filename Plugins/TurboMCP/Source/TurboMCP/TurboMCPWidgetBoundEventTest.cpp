// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/ComponentDelegateBinding.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetBoundEventTest,"TurboMCP.GraphAppend.WidgetBoundEvent",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPWidgetBoundEventTest::RunTest(const FString&)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/WidgetEvent_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    auto* BP=FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package,TEXT("WBP_Events"),BPTYPE_Normal,UUserWidget::StaticClass(),UCanvasPanel::StaticClass(),TEXT("WidgetEventTest"),false);
    if (!TestNotNull(TEXT("Widget fixture"),BP)) return false;
    auto* Button=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("Purchase"));Button->bIsVariable=true;
    CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget)->AddChildToCanvas(Button);
    if (BP->UbergraphPages.IsEmpty())
    {
        UEdGraph* NewGraph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(BP,NewGraph);
    }
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=BP->UbergraphPages[0];const FString Asset=BP->GetPathName(),Name=Graph->GetName();
    const int32 BeforeCount=Graph->Nodes.Num();
    const FString Tree=UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset);
    Package->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPWidgetEvent(Asset,Name,TEXT("Purchase"),TEXT("OnClicked"));AddInfo(Preview);
    TestTrue(TEXT("Preview identifies event and execution pin"),Preview.Contains(TEXT("preview: create widget event Purchase.OnClicked")) && Preview.Contains(TEXT("then: exec output")));
    TestFalse(TEXT("Preview leaves package clean"),Package->IsDirty());TestEqual(TEXT("Preview preserves graph"),Graph->Nodes.Num(),BeforeCount);
    const FString Edited=UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,Name,TEXT("Purchase"),TEXT("OnClicked"));AddInfo(Edited);
    TestTrue(TEXT("Edit reports changed"),Edited.StartsWith(TEXT("changed")));TestEqual(TEXT("One event created"),Graph->Nodes.Num(),BeforeCount+1);
    TArray<UK2Node_ComponentBoundEvent*> Nodes;FBlueprintEditorUtils::GetAllNodesOfClass(BP,Nodes);
    if (!TestEqual(TEXT("One bound event"),Nodes.Num(),1)) return false;
    TestTrue(TEXT("Exact created ID returned"),Edited.Contains(TurboMCPNodeId::MakeForNode(Nodes[0],true)));
    TestEqual(TEXT("Designer tree unchanged"),UTurboMCPReadToolset::ReadTurboMCPWidgetTree(Asset),Tree);
    Package->SetDirtyFlag(false);
    const FString Duplicate=UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,Name,TEXT("Purchase"),TEXT("OnClicked"));
    TestTrue(TEXT("Duplicate returns existing ID"),Duplicate.StartsWith(TEXT("unchanged")) && Duplicate.Contains(TurboMCPNodeId::MakeForNode(Nodes[0],true)));
    TestFalse(TEXT("Duplicate clean"),Package->IsDirty());
    TestTrue(TEXT("Invalid delegate refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,Name,TEXT("Purchase"),TEXT("Missing")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Invalid widget refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,Name,TEXT("Missing"),TEXT("OnClicked")).Contains(TEXT("refused:")));
    TestTrue(TEXT("Invalid graph refuses"),UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,TEXT("Missing"),TEXT("Purchase"),TEXT("OnClicked")).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty());
    TestTrue(TEXT("Undo creation"),GEditor->UndoTransaction(false));TestEqual(TEXT("Undo restores graph count"),Graph->Nodes.Num(),BeforeCount);
    TestTrue(TEXT("Recreate"),UTurboMCPEditToolset::EditTurboMCPWidgetEvent(Asset,Name,TEXT("Purchase"),TEXT("OnClicked")).StartsWith(TEXT("changed")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    int32 Bindings=0;
    for (UDynamicBlueprintBinding* Entry:CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)->DynamicBindingObjects)
        if (auto* Bound=Cast<UComponentDelegateBinding>(Entry))
            for (const auto& Binding:Bound->ComponentDelegateBindings)
                if (Binding.ComponentPropertyName==TEXT("Purchase") && Binding.DelegatePropertyName==TEXT("OnClicked"))
                { ++Bindings;TestNotNull(TEXT("Compiled handler exists"),BP->GeneratedClass->FindFunctionByName(Binding.FunctionNameToBind)); }
    TestEqual(TEXT("Compiled designer delegate binding"),Bindings,1);
    for (const TCHAR* Tool:{TEXT("PreviewTurboMCPWidgetEvent"),TEXT("EditTurboMCPWidgetEvent")})
    {
        const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Tool).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Tool);AddInfo(Help);
        TestTrue(TEXT("Help explains widget and delegate names"),Help.Contains(TEXT("WidgetName")) && Help.Contains(TEXT("BlueprintAssignable")) && Help.Contains(TEXT("compile")));
    }
    return true;
}
#endif
