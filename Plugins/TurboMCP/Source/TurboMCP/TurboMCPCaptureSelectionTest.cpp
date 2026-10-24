// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPCapture.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "GameFramework/Actor.h"
#include "GraphEditor.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCaptureSelectionTest,
    "TurboMCP.Capture.SelectionState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPCaptureSelectionTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/Selection_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("SelectionFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPSelectionTest"));
    if (!TestNotNull(TEXT("Blueprint factory succeeds"), Blueprint)) return false;
    ON_SCOPE_EXIT
    {
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Only this unsaved disposable fixture.
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("SelectionGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto AddEvent = [Graph](const TCHAR* Name)
    {
        UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
        Node->CustomFunctionName = FName(Name);
        Node->CreateNewGuid();
        Graph->AddNode(Node, false, false);
        Node->AllocateDefaultPins();
        return Node;
    };
    UK2Node_CustomEvent* Original = AddEvent(TEXT("Original"));
    UK2Node_CustomEvent* Target = AddEvent(TEXT("Target"));
    TSharedRef<SGraphEditor> Panel = SNew(SGraphEditor).GraphToEdit(Graph).IsEditable(false);
    Panel->SetNodeSelection(Original, true);
    FString Description, Why;
    const FString Key = Target->NodeGuid.ToString(EGuidFormats::Digits);
    TestTrue(TEXT("Probe succeeds with duplicate target IDs"), TurboMCPCaptureSelector::ProbeSelection(
        &Panel.Get(), Graph, Key + TEXT(",") + Key, Blueprint->GetPathName(), Description, Why));
    TestTrue(TEXT("Original node remains selected"), Panel->GetSelectedNodes().Contains(Original));
    TestFalse(TEXT("Temporary target is no longer selected"), Panel->GetSelectedNodes().Contains(Target));
    TestEqual(TEXT("Exact original count restored"), Panel->GetSelectedNodes().Num(), 1);
    TestTrue(TEXT("Result reports restoration"), Description.Contains(TEXT("original selection restored: 1 object(s)")));

    Description.Empty();
    TestFalse(TEXT("Missing target refuses"), TurboMCPCaptureSelector::ProbeSelection(
        &Panel.Get(), Graph, TEXT("ffffffffffffffffffffffffffffffff"), Blueprint->GetPathName(), Description, Why));
    TestTrue(TEXT("Refusal preserves original node"), Panel->GetSelectedNodes().Contains(Original));
    TestEqual(TEXT("Refusal preserves original count"), Panel->GetSelectedNodes().Num(), 1);
    Panel->SetNodeSelection(Target, true);
    Description.Empty();
    TestTrue(TEXT("Probe restores multiple original nodes"), TurboMCPCaptureSelector::ProbeSelection(
        &Panel.Get(), Graph, Key, Blueprint->GetPathName(), Description, Why));
    TestEqual(TEXT("Both original selections restored"), Panel->GetSelectedNodes().Num(), 2);
    TestTrue(TEXT("Original membership restored"), Panel->GetSelectedNodes().Contains(Original));
    TestTrue(TEXT("Target membership restored"), Panel->GetSelectedNodes().Contains(Target));
    return true;
}
#endif
