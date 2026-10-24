// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphReplace.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPReplacePlanTest,
    "TurboMCP.Replace.LinkedRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPReplacePlanTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/ReplacePlan_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("ReplaceFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPReplaceTest"));
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

    UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph, NAME_None, RF_Transactional);
    Event->CustomFunctionName = TEXT("ReplacementStart");
    Graph->AddNode(Event, false, false);
    Event->CreateNewGuid();
    Event->AllocateDefaultPins();
    Event->FindPinChecked(TEXT("then"))->MakeLinkTo(Source->FindPinChecked(TEXT("execute")));
    const FString SourceKey = TurboMCPNodeId::MakeForNode(Source, true);
    const FString ConsumerKey = TurboMCPNodeId::MakeForNode(Consumer, true);
    FString Error;
    FTurboMCPGraphReplace::FPlan Baseline;
    if (!TestTrue(TEXT("Unreferenced replacement plans"), FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(),
        Graph->GetName(), TEXT("(Development|PrintString\n InString \"new\"\n)"), Baseline, Error)))
    { AddError(Error); return false; }
    if (!TestTrue(TEXT("Baseline accepted"), Baseline.bAcceptable())) { AddError(FTurboMCPGraphReplace::RenderPreview(Baseline)); return false; }
    TestEqual(TEXT("Unreferenced body nodes still scheduled for removal"), Baseline.Removals.Num(), 3);
    for (bool bReferenceSource : {true, false})
    {
        const FString KeptKey = bReferenceSource ? SourceKey : ConsumerKey;
        const FString Text = FString::Printf(TEXT("(Development|PrintString\n InString \"new\"\n %s (-> %s:%s)\n)"),
            TEXT("execute"), *KeptKey, TEXT("then"));
        FTurboMCPGraphReplace::FPlan Plan;
        if (!TestTrue(TEXT("Linked replacement plans"), FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(), Graph->GetName(), Text, Plan, Error)))
        { AddError(Error); return false; }
        if (!TestTrue(TEXT("Linked replacement accepted"), Plan.bAcceptable()))
        { AddError(FTurboMCPGraphReplace::RenderPreview(Plan)); return false; }
        TestEqual(TEXT("One explicit link recorded"), Plan.Append.Links.Num(), 1);
        TestFalse(TEXT("Explicit endpoint is not deleted"), Plan.Removals.ContainsByPredicate(
            [&](const FTurboMCPGraphReplace::FRemoval& Removal) { return Removal.Key == KeptKey; }));
        TestEqual(TEXT("Only unreferenced body nodes removed"), Plan.Removals.Num(), 2);
        TestEqual(TEXT("Referenced endpoint counted as retained"), Plan.DescribedKeysKept, 1);
    }
    TestEqual(TEXT("Planning preserves node count"), Graph->Nodes.Num(), 4);
    TestTrue(TEXT("Planning preserves original edge"), Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Consumer->FindPinChecked(TEXT("execute"))));
    TestEqual(TEXT("Planning preserves default"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    FTurboMCPGraphReplace::FPlan LivePlan;
    const FString LiveText = FString::Printf(TEXT("(Development|PrintString\n InString \"replacement\"\n execute (-> %s:then)\n)"), *SourceKey);
    if (!TestTrue(TEXT("Live replacement plans"), FTurboMCPGraphReplace::Plan(Blueprint->GetPathName(), Graph->GetName(), LiveText, LivePlan, Error)))
    { AddError(Error); return false; }
    if (!TestTrue(TEXT("Live replacement accepted"), LivePlan.bAcceptable())) return false;
    FString Report;
    if (!TestTrue(TEXT("Live replacement applies"), FTurboMCPGraphReplace::Apply(LivePlan, Report, Error)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Only event, referenced source and new call remain"), Graph->Nodes.Num(), 3);
    TestTrue(TEXT("Event retained"), Graph->Nodes.Contains(Event));
    TestTrue(TEXT("Referenced source retained"), Graph->Nodes.Contains(Source));
    TestFalse(TEXT("Old unreferenced consumer removed"), Graph->Nodes.Contains(Consumer));
    TestFalse(TEXT("Unreferenced target removed"), Graph->Nodes.Contains(Target));
    UEdGraphNode* Added = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes) if (Node != Event && Node != Source) Added = Node;
    if (TestNotNull(TEXT("New call exists"), Added))
    {
        UEdGraphPin* Input = Added->FindPinChecked(TEXT("execute"));
        UEdGraphPin* Output = Source->FindPinChecked(TEXT("then"));
        TestTrue(TEXT("Requested wire survives deletion"), Input->LinkedTo.Contains(Output) && Output->LinkedTo.Contains(Input));
        TestEqual(TEXT("New literal retained"), Added->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("replacement")));
    }
    TestTrue(TEXT("Entry connection survives"), Event->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Source->FindPinChecked(TEXT("execute"))));
    TestEqual(TEXT("Retained source position unchanged"), Source->NodePosX, 0);
    return !HasAnyErrors();
}
#endif
