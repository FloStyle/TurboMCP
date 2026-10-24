// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphStructure.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCopyFailureTest,
    "TurboMCP.Copy.PostMutationFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPCopyFailureTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/CopyFailure_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("CopyFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPCopyTest"));
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
    UEdGraph* Destination = NewObject<UEdGraph>(Blueprint, TEXT("CopyTarget"), RF_Transactional);
    Destination->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Destination);
    FTurboMCPGraphStructure::FPlan Plan;
    FString Error, Report;
    TArray<FString> Selection{TurboMCPNodeId::MakeForNode(Source, true), TurboMCPNodeId::MakeForNode(Consumer, true)};
    if (!TestTrue(TEXT("Copy plan succeeds"), FTurboMCPGraphStructure::PlanCopy(Blueprint->GetPathName(), Graph->GetName(),
        Selection, Blueprint->GetPathName(), Destination->GetName(), TEXT(""), Plan, Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Copy plan accepted"), Plan.bAcceptable()) || !TestTrue(TEXT("Internal link recorded"), !Plan.InternalLinks.IsEmpty())) return false;
    // The production copy creates nodes before restoring these planned links.
    Plan.InternalLinks[0].InsidePin = TEXT("MissingPinForFailureTest");
    TestFalse(TEXT("Copy fails after creation"), FTurboMCPGraphStructure::Apply(Plan, Report, Error));
    TestTrue(TEXT("Failure reaches preservation verification"), Error.Contains(TEXT("graph copy could not preserve")));
    TestTrue(TEXT("Restoration uncertainty stated"), Error.Contains(TEXT("restoration is unverified")));
    TestFalse(TEXT("No false unchanged result"), Error.StartsWith(TEXT("unchanged")));
    TestEqual(TEXT("Created copies removed"), Destination->Nodes.Num(), 0);
    TestEqual(TEXT("Source count retained"), Graph->Nodes.Num(), 3);
    auto Find = [&](const FGuid& Id)->UEdGraphNode*
    { for (UEdGraphNode* Node : Graph->Nodes) if (Node && Node->NodeGuid == Id) return Node; return nullptr; };
    UEdGraphNode* S = Find(SourceId); UEdGraphNode* C = Find(ConsumerId); UEdGraphNode* T = Find(TargetId);
    if (!TestNotNull(TEXT("Source retained"), S) || !TestNotNull(TEXT("Consumer retained"), C) || !TestNotNull(TEXT("Unselected node retained"), T)) return false;
    UEdGraphPin* Input = C->FindPin(TEXT("execute"));
    TestTrue(TEXT("Source link retained"), Input && Input->LinkedTo.Num() == 1 && Input->LinkedTo.Contains(S->FindPin(TEXT("then"))));
    TestEqual(TEXT("Source default retained"), C->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    TestEqual(TEXT("Source position retained"), C->NodePosX, 300);
    FTurboMCPGraphStructure::FPlan Valid;
    if (!TestTrue(TEXT("Fresh copy plan succeeds"), FTurboMCPGraphStructure::PlanCopy(Blueprint->GetPathName(), Graph->GetName(),
        Selection, Blueprint->GetPathName(), Destination->GetName(), TEXT(""), Valid, Error))) return false;
    if (!TestTrue(TEXT("Valid copy applies"), FTurboMCPGraphStructure::Apply(Valid, Report, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Exactly two copies created"), Destination->Nodes.Num(), 2);
    for (UEdGraphNode* Copy : Destination->Nodes)
    {
        TestEqual(TEXT("No duplicate allocated pins"), Copy->Pins.Num(), Source->Pins.Num());
        for (UEdGraphPin* Pin : Copy->Pins)
            if (Pin) for (UEdGraphPin* Linked : Pin->LinkedTo)
                TestTrue(TEXT("Copy links are reciprocal and stay in destination"), Linked && Linked->LinkedTo.Contains(Pin)
                    && Linked->GetOwningNode()->GetGraph() == Destination);
    }
    UEdGraphNode* CopiedSource = nullptr;
    UEdGraphNode* CopiedConsumer = nullptr;
    for (UEdGraphNode* Copy : Destination->Nodes)
    {
        if (Copy->NodePosX == 40) CopiedSource = Copy;
        if (Copy->NodePosX == 340) CopiedConsumer = Copy;
    }
    if (TestNotNull(TEXT("Translated source copy exists"), CopiedSource)
        && TestNotNull(TEXT("Translated consumer copy exists"), CopiedConsumer))
    {
        UEdGraphPin* CopyInput = CopiedConsumer->FindPinChecked(TEXT("execute"));
        TestTrue(TEXT("Expected internal edge recreated"), CopyInput->LinkedTo.Num() == 1
            && CopyInput->LinkedTo.Contains(CopiedSource->FindPinChecked(TEXT("then"))));
        TestEqual(TEXT("Copied default retained"), CopiedConsumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
        TestTrue(TEXT("Copied identities are new"), CopiedSource->NodeGuid != SourceId && CopiedConsumer->NodeGuid != ConsumerId);
    }
    TestEqual(TEXT("Successful copy leaves source graph count unchanged"), Graph->Nodes.Num(), 3);
    TestTrue(TEXT("Successful copy leaves original edge intact"), C->FindPinChecked(TEXT("execute"))->LinkedTo.Contains(S->FindPinChecked(TEXT("then"))));
    return !HasAnyErrors();
}
#endif
