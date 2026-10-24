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
            bReferenceSource ? TEXT("execute") : TEXT("then"), *KeptKey,
            bReferenceSource ? TEXT("then") : TEXT("execute"));
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
    TestEqual(TEXT("Planning preserves node count"), Graph->Nodes.Num(), 3);
    TestTrue(TEXT("Planning preserves original edge"), Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Consumer->FindPinChecked(TEXT("execute"))));
    TestEqual(TEXT("Planning preserves default"), Consumer->FindPinChecked(TEXT("InString"))->DefaultValue, FString(TEXT("preserve-me")));
    return !HasAnyErrors();
}
#endif
