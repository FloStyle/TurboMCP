// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphReachability.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputAction.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"
#include "BlueprintActionDatabase.h"
#include "AssetRegistry/AssetRegistryModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPInputEntryTest,
    "TurboMCP.Graph.InputEntry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPInputEntryTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/InputEntry_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("InputEntry"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!BP) { AddError(TEXT("Could not create Blueprint fixture.")); return false; }
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("InputEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    UInputAction* Action = NewObject<UInputAction>(Package, TEXT("IA_EntryProbe"), RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Action);
    FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_EnhancedInputAction::StaticClass());
    UK2Node_EnhancedInputAction* Event = NewObject<UK2Node_EnhancedInputAction>(Graph);
    Event->InputAction = Action;
    Graph->AddNode(Event, false, false);
    Event->CreateNewGuid(); Event->AllocateDefaultPins();
    auto MakeCall = [&]()
    {
        auto* Call = NewObject<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Call, false, false);
        Call->CreateNewGuid(); Call->AllocateDefaultPins();
        return Call;
    };
    auto* Linked = MakeCall();
    auto* Orphan = MakeCall();
    UEdGraphPin* Triggered = Event->FindPin(TEXT("Triggered"));
    if (!Triggered) { AddError(TEXT("Real Enhanced Input event lacks Triggered output.")); return false; }
    Triggered->MakeLinkTo(Linked->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    TSet<const UEdGraphNode*> Reachable;
    TurboMCPGraphReachability::MarkReachable(Graph, Reachable);
    TestTrue(TEXT("Enhanced Input event is a recognized entry"), Reachable.Contains(Event));
    TestTrue(TEXT("Triggered call is reachable"), Reachable.Contains(Linked));
    TestFalse(TEXT("Disconnected ordinary call stays unreachable"), Reachable.Contains(Orphan));
    // Capture observable graph state, not merely its node count: a refusal must
    // not disconnect pins, change defaults or move existing nodes.
    auto Snapshot = [](const UEdGraph* InGraph)
    {
        FString State;
        for (const UEdGraphNode* Node : InGraph->Nodes)
        {
            State += FString::Printf(TEXT("%s:%d,%d\n"), *Node->NodeGuid.ToString(), Node->NodePosX, Node->NodePosY);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                State += Pin->PinId.ToString() + TEXT(":") + Pin->DefaultValue + TEXT(":")
                    + GetPathNameSafe(Pin->DefaultObject) + TEXT(":") + Pin->DefaultTextValue.ToString();
                for (const UEdGraphPin* Link : Pin->LinkedTo)
                {
                    State += TEXT("->") + Link->PinId.ToString();
                }
                State += TEXT("\n");
            }
        }
        return State;
    };
    const FString AssetPath = BP->GetPathName();
    const FString EventId = TurboMCPNodeId::MakeForNode(Event, true);
    const FString EventType = TurboMCPNodeId::GetNodeTypeId(Event);
    AddInfo(TEXT("Event type: ") + EventType);
    Package->SetDirtyFlag(false);
    const FString ExistingText = TEXT("(Development|PrintString\n InString \"existing input event\"\n execute (-> ") + EventId + TEXT(":Started))");
    const int32 BeforeCount = Graph->Nodes.Num();
    const FString BeforePreview = Snapshot(Graph);
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(AssetPath, Graph->GetName(), ExistingText);
    AddInfo(Preview);
    TestFalse(TEXT("Existing-event append preview accepted"), Preview.StartsWith(TEXT("error:")) || Preview.StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Preview preserves graph nodes"), Graph->Nodes.Num(), BeforeCount);
    TestFalse(TEXT("Preview preserves clean flag"), Package->IsDirty());
    TestEqual(TEXT("Preview preserves links, defaults and positions"), Snapshot(Graph), BeforePreview);
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPGraphAppend(AssetPath, Graph->GetName(), ExistingText);
    AddInfo(Edit);
    TestTrue(TEXT("Existing-event append creates one call"), Graph->Nodes.Num() == BeforeCount + 1 && !Edit.StartsWith(TEXT("error:")));
    const FString Compile = UTurboMCPEditToolset::EditTurboMCPCompile(AssetPath);
    AddInfo(Compile);
    TestTrue(TEXT("Compile reports only the genuine orphan"), Compile.Contains(TEXT("1 execution-input node(s) not reached")));
    // Enhanced Input permits one event per action in a Blueprint. Use a separate
    // Blueprint to test creation, and discover its creation ID instead of assuming
    // that a reader's graph-text ID is also an action-menu ID.
    UBlueprint* BatchBP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BatchInputEntry"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!BatchBP) { AddError(TEXT("Could not create same-batch Blueprint fixture.")); return false; }
    FKismetEditorUtilities::CompileBlueprint(BatchBP);
    UEdGraph* BatchGraph = FBlueprintEditorUtils::CreateNewGraph(BatchBP, TEXT("BatchInputEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BatchBP, BatchGraph);
    const FString BatchAssetPath = BatchBP->GetPathName();
    FString CreationType;
    for (const FString& Candidate : FTurboMCPNodeType::AvailableTypeIds(BatchBP, BatchGraph))
    {
        if (Candidate.Contains(TEXT("EntryProbe")))
        {
            AddInfo(TEXT("Available action type: ") + Candidate);
            if (Candidate.Contains(TEXT("EnhancedActionEvents"))) { CreationType = Candidate; }
        }
    }
    TestFalse(TEXT("Real input event has a discoverable creation ID"), CreationType.IsEmpty());
    const FString BatchText = TEXT("(") + CreationType + TEXT("\n ; label InputEntry\n)\n(Development|PrintString\n InString \"same batch input event\"\n execute (-> InputEntry:Triggered))");
    const FString BeforeBatchPreview = Snapshot(BatchGraph);
    const FString BatchPreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(BatchAssetPath, BatchGraph->GetName(), BatchText);
    AddInfo(BatchPreview);
    TestFalse(TEXT("Same-batch event preview accepted"), BatchPreview.StartsWith(TEXT("error:")) || BatchPreview.StartsWith(TEXT("unchanged:")));
    TestEqual(TEXT("Same-batch preview preserves graph"), Snapshot(BatchGraph), BeforeBatchPreview);
    const FString BatchEdit = UTurboMCPEditToolset::EditTurboMCPGraphAppend(BatchAssetPath, BatchGraph->GetName(), BatchText);
    AddInfo(BatchEdit);
    TestTrue(TEXT("Same-batch event and call created"), BatchGraph->Nodes.Num() == 2 && !BatchEdit.StartsWith(TEXT("error:")));
    const FString BatchCompile = UTurboMCPEditToolset::EditTurboMCPCompile(BatchAssetPath);
    AddInfo(BatchCompile);
    TestTrue(TEXT("Same-batch compile is clean with zero unreachable execution nodes"), BatchCompile.Contains(TEXT("[clean]")) && BatchCompile.Contains(TEXT("structural reachability: 0 execution-input node(s) not reached")) && !BatchCompile.StartsWith(TEXT("error:")));
    const FString BeforeRefusalState = Snapshot(BatchGraph);
    const int32 BeforeRefusal = BatchGraph->Nodes.Num();
    const bool DirtyBeforeRefusal = Package->IsDirty();
    const FString Refusal = UTurboMCPEditToolset::EditTurboMCPGraphAppend(BatchAssetPath, BatchGraph->GetName(), TEXT("(Development|PrintString InString \"orphan\")"));
    AddInfo(Refusal);
    TestTrue(TEXT("Orphan append refused"), Refusal.Contains(TEXT("no path from recognized entries")));
    TestEqual(TEXT("Refusal preserves node count"), BatchGraph->Nodes.Num(), BeforeRefusal);
    TestEqual(TEXT("Refusal preserves dirty flag"), Package->IsDirty(), DirtyBeforeRefusal);
    TestEqual(TEXT("Refusal preserves links, defaults and positions"), Snapshot(BatchGraph), BeforeRefusalState);
    FBlueprintActionDatabase::Get().ClearAssetActions(Action);
    FAssetRegistryModule::AssetDeleted(Action);
    Action->ClearFlags(RF_Public | RF_Standalone);
    Package->SetDirtyFlag(false);
    return !HasAnyErrors();
}
#endif
