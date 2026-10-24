// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphAppend.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Editor.h"
#include "ScopedTransaction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPAppendFailureTest,
    "TurboMCP.Append.PostMutationFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPAppendFailureTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/AppendFailure_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("AppendFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPAppendTest"));
    if (!TestNotNull(TEXT("Blueprint factory succeeds"), Blueprint)) return false;
    FAssetRegistryModule::AssetCreated(Blueprint);
    ON_SCOPE_EXIT
    {
        FAssetRegistryModule::AssetDeleted(Blueprint);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Disposable, unsaved fixture only.
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("FailureGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto MakeCall = [&](int32 X)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        return Node;
    };
    UK2Node_CallFunction* Source = MakeCall(0);
    UK2Node_CallFunction* Consumer = MakeCall(300);
    UK2Node_CallFunction* Target = MakeCall(600);
    Source->FindPinChecked(TEXT("then"))->MakeLinkTo(Consumer->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("preserve-me");
    Package->SetDirtyFlag(false);
    const FGuid SourceId = Source->NodeGuid, ConsumerId = Consumer->NodeGuid, TargetId = Target->NodeGuid;

    for (bool bJoined : {false, true})
    {
        FTurboMCPGraphAppend::FPlan Plan;
        FString Report, Error;
        if (!TestTrue(TEXT("Production planner accepts the node"), FTurboMCPGraphAppend::Plan(
            Blueprint->GetPathName(), Graph->GetName(), TEXT("(Development|PrintString\n InString \"new-node\"\n)"), Plan, Error)))
        { AddError(Error); return false; }
        if (!TestTrue(TEXT("Plan contains a new node"), Plan.bAcceptable() && Plan.Nodes.Num() == 1)) return false;
        TurboMCPGraphTextLex::FPinForm BadPin;
        BadPin.Name = TEXT("MissingPinForFailureTest");
        BadPin.Value = TEXT("42");
        BadPin.Line = 2;
        Plan.Nodes[0].Pins.Add(BadPin);
        // Bypass only scratch preflight, which would catch this deliberately malformed plan.
        // The production mutation core creates a node before looking up this missing pin.
        TUniquePtr<FScopedTransaction> Caller;
        if (bJoined) Caller = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "AppendFailureCaller", "Append failure test caller"));
        TestFalse(TEXT("Mutation core fails"), FTurboMCPGraphAppend::ExecuteGraph(Plan, Blueprint, Graph, Report, Error, false, bJoined));
        Caller.Reset();
        TestTrue(TEXT("Failure occurs after node creation at pin lookup"), Error.Contains(TEXT("has no pin named 'MissingPinForFailureTest'")));
        TestTrue(TEXT("Undo completion reported"), Error.Contains(TEXT("undo completed")));
        TestTrue(TEXT("Restoration uncertainty reported"), Error.Contains(TEXT("restoration is unverified")));
        TestFalse(TEXT("No false no-write claim"), Error.Contains(TEXT("NOTHING was written")));
        TestFalse(TEXT("No complete rollback claim"), Error.Contains(TEXT("rollback=complete")));
        TestFalse(TEXT("No transaction left active"), GEditor->IsTransactionActive());
        TestEqual(TEXT("Created node removed"), Graph->Nodes.Num(), 3);
        auto Find = [&](const FGuid& Id)->UEdGraphNode*
        { for (UEdGraphNode* Node : Graph->Nodes) if (Node && Node->NodeGuid == Id) return Node; return nullptr; };
        UEdGraphNode* S = Find(SourceId); UEdGraphNode* C = Find(ConsumerId); UEdGraphNode* T = Find(TargetId);
        if (!TestNotNull(TEXT("Source retained"), S) || !TestNotNull(TEXT("Consumer retained"), C)
            || !TestNotNull(TEXT("Unselected target retained"), T)) return false;
        UEdGraphPin* Input = C->FindPinChecked(TEXT("execute"));
        UEdGraphPin* Output = S->FindPinChecked(TEXT("then"));
        TestTrue(TEXT("Original reciprocal edge retained"), Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(Output)
            && Output->LinkedTo.Num() == 1 && Output->LinkedTo.Contains(Input));
        TestEqual(TEXT("Default retained"), C->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
        TestEqual(TEXT("Position retained"), C->NodePosX, 300);
    }
    return !HasAnyErrors();
}
#endif
