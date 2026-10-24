// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphRepoint.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPRepointFailureTest,
    "TurboMCP.Repoint.PostMutationFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPRepointFailureTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/RepointFailure_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("RepointFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPRepointTest"));
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
    FTurboMCPGraphRepoint::FPlan Plan;
    FString Error, Report;
    if (!TestTrue(TEXT("Plan succeeds"), FTurboMCPGraphRepoint::Plan(Blueprint->GetPathName(), Graph->GetName(),
        TurboMCPNodeId::MakeForNode(Source, true), TEXT("then"), TurboMCPNodeId::MakeForNode(Target, true),
        TEXT(""), TEXT("then"), TEXT("true"), Plan, Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Plan accepted"), Plan.bAcceptable())) return false;
    // Force final read-back failure after the production path has moved the link.
    ++Plan.NodeCountBefore;
    FTurboMCPGraphRepoint::FApplyContext Context;
    FTurboMCPGraphRepoint::FApplyOutcome Outcome;
    TestFalse(TEXT("Post-mutation mismatch fails"), FTurboMCPGraphRepoint::ApplyTransacted(Plan, Context, Outcome, Report, Error));
    TestEqual(TEXT("Failure reached final read-back"), Outcome.Code, FString(TEXT("graph_nodecount_mismatch")));
    TestEqual(TEXT("Restoration remains unknown in result"), Outcome.Changed, 2);
    TestFalse(TEXT("No complete rollback claim"), Error.Contains(TEXT("rollback=complete")));
    TestTrue(TEXT("Unverified restoration explained"), Error.Contains(TEXT("restoration was not verified")));
    TestEqual(TEXT("Node count restored"), Graph->Nodes.Num(), 3);
    auto Find = [&](const FGuid& Id)->UEdGraphNode*
    { for (UEdGraphNode* Node : Graph->Nodes) if (Node && Node->NodeGuid == Id) return Node; return nullptr; };
    UEdGraphNode* S = Find(SourceId); UEdGraphNode* C = Find(ConsumerId); UEdGraphNode* T = Find(TargetId);
    if (!TestNotNull(TEXT("Source retained"), S) || !TestNotNull(TEXT("Consumer retained"), C) || !TestNotNull(TEXT("Target retained"), T)) return false;
    UEdGraphPin* Input = C->FindPin(TEXT("execute"));
    TestTrue(TEXT("Original source link restored"), Input && Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(S->FindPin(TEXT("then"))));
    TestTrue(TEXT("Target link removed"), T->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("Default preserved"), C->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    TestEqual(TEXT("Consumer position preserved"), C->NodePosX, 300);
    return !HasAnyErrors();
}
#endif
