// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPWidgetRetarget.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPWidgetRetargetQualityTest,
    "TurboMCP.Widget.RetargetQuality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPWidgetRetargetQualityTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/Retarget_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UWidgetBlueprint* BP = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, TEXT("WBP_Retarget"),
        BPTYPE_Normal, UUserWidget::StaticClass(), UCanvasPanel::StaticClass(), TEXT("RetargetReview"), false);
    if (!TestNotNull(TEXT("Fixture created"), BP)) return false;
    UCanvasPanel* Root = CastChecked<UCanvasPanel>(BP->WidgetTree->RootWidget);
    Root->AddChild(BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label")));
    Root->AddChild(BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button")));
    // Deliberately malformed serialized bindings represent a widget replacement.
    // Public binding creation correctly refuses these inputs, so setup uses native data.
    auto AddBinding = [&](const TCHAR* Widget, const TCHAR* Property) {
        FDelegateEditorBinding Binding;
        Binding.ObjectName = Widget; Binding.PropertyName = FName(Property);
        Binding.FunctionName = TEXT("UnverifiedHandler"); BP->Bindings.Add(Binding);
    };
    AddBinding(TEXT("Label"), TEXT("Text"));
    AddBinding(TEXT("Button"), TEXT("OnClicked"));
    AddBinding(TEXT("Label"), TEXT("RemovedDelegate"));
    AddBinding(TEXT("MissingWidget"), TEXT("Text"));
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("RetargetCalls"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    UK2Node_CallFunction* External = NewObject<UK2Node_CallFunction>(Graph);
    External->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Abs")));
    Graph->AddNode(External, false, false);
    UK2Node_CallFunction* Missing = NewObject<UK2Node_CallFunction>(Graph);
    Missing->FunctionReference.SetSelfMember(TEXT("MissingHandler"));
    Graph->AddNode(Missing, false, false);
    Package->SetDirtyFlag(false);
    FTurboMCPWidgetRetarget::FPlan Plan; FString Error;
    if (!TestTrue(TEXT("Plan succeeds"), FTurboMCPWidgetRetarget::Plan(BP->GetPathName(), Plan, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Two broken bindings"), Plan.NumBroken(), 2);
    TestEqual(TEXT("Only missing self call reported"), Plan.StaleCalls.Num(), 1);
    if (Plan.StaleCalls.Num() == 1) TestTrue(TEXT("Missing function identified"), Plan.StaleCalls[0].Contains(TEXT("MissingHandler")));
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRetarget(BP->GetPathName());
    AddInfo(Preview);
    TestEqual(TEXT("Preview preserves bindings"), BP->Bindings.Num(), 4);
    TestFalse(TEXT("Preview leaves clean package clean"), Package->IsDirty());
    TestTrue(TEXT("Preview states validation limits"), Preview.Contains(TEXT("not bound-function existence")));
    TestTrue(TEXT("Plan can be reused"), FTurboMCPWidgetRetarget::Plan(BP->GetPathName(), Plan, Error));
    TestEqual(TEXT("Reused plan does not accumulate bindings"), Plan.Bindings.Num(), 4);
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPWidgetRetarget(BP->GetPathName()); AddInfo(Edit);
    TestTrue(TEXT("Edit reports two removals"), Edit.Contains(TEXT("removed 2 binding(s)")));
    TestEqual(TEXT("Two bindings remain"), BP->Bindings.Num(), 2);
    if (BP->Bindings.Num() == 2) {
        TestEqual(TEXT("Single delegate retained"), BP->Bindings[0].ObjectName, FString(TEXT("Label")));
        TestEqual(TEXT("Multicast delegate retained"), BP->Bindings[1].ObjectName, FString(TEXT("Button")));
        TestEqual(TEXT("Unverified function untouched"), BP->Bindings[0].FunctionName, FName(TEXT("UnverifiedHandler")));
    }
    TestEqual(TEXT("Graph nodes retained"), Graph->Nodes.Num(), 2);
    TestTrue(TEXT("Edit marks package dirty"), Package->IsDirty());
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Repeated edit is unchanged"), UTurboMCPEditToolset::EditTurboMCPWidgetRetarget(BP->GetPathName()).StartsWith(TEXT("unchanged:")));
    TestFalse(TEXT("No-op does not dirty package"), Package->IsDirty());
    TestTrue(TEXT("Empty asset refused"), UTurboMCPPreviewToolset::PreviewTurboMCPWidgetRetarget(TEXT("")).StartsWith(TEXT("error:")));
    return true;
}
#endif
