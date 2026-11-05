// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPNodeType.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "BlueprintActionDatabase.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Variable.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_AddDelegate.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPNodeDiscoveryTest,
    "TurboMCP.GraphAppend.NodeDiscovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPNodeDiscoveryTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/NodeDiscovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_Discovery"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int;
    if (!TestTrue(TEXT("Member created"), FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("AvailableFunds"), Type))) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("Discovery"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    FKismetEditorUtilities::CompileBlueprint(BP);
    FBlueprintActionDatabase::Get().RefreshAssetActions(BP);
    for (const FString& Query : {FString(TEXT("AvailableFunds")), FString(TEXT("get Available Funds")), FString(TEXT("getter AvailableFunds")), FString(TEXT("setter AvailableFunds"))})
    {
        FTurboMCPNodeType::FPlan Plan; FString Error;
        const int32 Before = Graph->Nodes.Num();
        Package->SetDirtyFlag(false);
        const bool Found = FTurboMCPNodeType::Plan(BP->GetPathName(), Graph->GetName(), Query, TEXT("50"), Plan, Error);
        TestTrue(*FString::Printf(TEXT("Member search: %s"), *Query), Found);
        AddInfo(Query + TEXT("\n") + Error + FTurboMCPNodeType::Render(Plan));
        TestEqual(TEXT("Discovery adds no nodes"), Graph->Nodes.Num(), Before);
        TestFalse(TEXT("Discovery preserves clean package"), Package->IsDirty());
        bool Verified = false;
        for (const auto& Match : Plan.Matches)
        {
            const FString Access = Query.StartsWith(TEXT("setter")) ? TEXT("Set") : TEXT("Get");
            if (!Match.TypeId.Contains(TEXT("|") + Access + TEXT("|AvailableFunds")) || !Match.bOwnedByThisBlueprint) continue;
            FString Note;
            UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(BP, Graph, Match.TypeId, Note);
            UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Probe);
            Verified = Variable && Variable->VariableReference.GetMemberName() == FName(TEXT("AvailableFunds")) && Variable->VariableReference.IsSelfContext();
            if (Probe) Probe->DestroyNode();
            TestTrue(TEXT("Returned variable ID creates the intended self member"), Verified);
            break;
        }
        TestTrue(TEXT("Intended member was found and resolved"), Verified);
    }
    for (const FString& Query : {FString(TEXT("add")), FString(TEXT("subtract")), FString(TEXT("plus")), FString(TEXT("minus"))})
    {
        FTurboMCPNodeType::FPlan Plan; FString Error;
        const bool Found = FTurboMCPNodeType::Plan(BP->GetPathName(), Graph->GetName(), Query, TEXT("100"), Plan, Error);
        bool OperatorFound = false;
        for (const auto& Match : Plan.Matches)
        {
            FString Note;
            UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(BP, Graph, Match.TypeId, Note);
            OperatorFound |= Probe && Probe->IsA<UK2Node_PromotableOperator>();
            if (Probe) Probe->DestroyNode();
            if (OperatorFound) break;
        }
        TestTrue(*FString::Printf(TEXT("Arithmetic query finds a promotable operator: %s"), *Query), Found && OperatorFound);
        AddInfo(Query + TEXT("\n") + Error + FTurboMCPNodeType::Render(Plan));
    }
    // Search by reflected dispatcher identity, not its potentially different
    // menu display text. The returned bind node must target the button owner.
    UClass* ButtonClass = LoadObject<UClass>(nullptr, TEXT("/Script/CommonUI.CommonButtonBase"));
    if (ButtonClass == nullptr) { AddError(TEXT("Stock CommonUI button class is unavailable.")); return false; }
    FBlueprintActionDatabase::Get().RefreshClassActions(ButtonClass);
    FTurboMCPNodeType::FPlan DelegatePlan; FString DelegateError;
    const bool DelegateFound = FTurboMCPNodeType::Plan(BP->GetPathName(), Graph->GetName(), TEXT("bind OnButtonBaseClicked"), TEXT("100"), DelegatePlan, DelegateError);
    TestTrue(TEXT("Reflected button dispatcher is discoverable"), DelegateFound);
    AddInfo(DelegateError + FTurboMCPNodeType::Render(DelegatePlan));
    bool CorrectOwner = false;
    for (const auto& Match : DelegatePlan.Matches)
    {
        FString Note;
        UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(BP, Graph, Match.TypeId, Note);
        if (const UK2Node_AddDelegate* Delegate = Cast<UK2Node_AddDelegate>(Probe))
            CorrectOwner |= Delegate->GetPropertyName() == FName(TEXT("OnButtonBaseClicked"))
                && Delegate->DelegateReference.GetMemberParentClass() == ButtonClass;
        if (Probe) Probe->DestroyNode();
        if (CorrectOwner) break;
    }
    TestTrue(TEXT("Discovered bind targets CommonButtonBase dispatcher"), CorrectOwner);
    // Inspect the reported stock increment action before inventing a writer alias.
    FTurboMCPNodeType::FPlan IncrementPlan; FString IncrementError;
    TestTrue(TEXT("Increment discovery query succeeds"), FTurboMCPNodeType::Plan(BP->GetPathName(), Graph->GetName(), TEXT("increment"), TEXT("100"), IncrementPlan, IncrementError));
    AddInfo(IncrementError + FTurboMCPNodeType::Render(IncrementPlan));
    bool FoundIncrement = false;
    for (const auto& Match : IncrementPlan.Matches)
    {
        if (!Match.TypeId.Contains(TEXT("Increment"), ESearchCase::IgnoreCase)) continue;
        FTurboMCPNodePins::FPlan Pins; FString PinError;
        if (!FTurboMCPNodePins::Plan(BP->GetPathName(), Graph->GetName(), Match.TypeId, Pins, PinError)) { AddError(PinError); continue; }
        FoundIncrement = true;
        AddInfo(FTurboMCPNodePins::Render(Pins));
        for (const auto& Pin : Pins.Pins)
            TestFalse(*FString::Printf(TEXT("Increment pin is addressable: %s %s position %d"), *Match.TypeId, *Pin.Direction, Pin.Position), Pin.Name.TrimStartAndEnd().IsEmpty());
    }
    TestTrue(TEXT("At least one stock increment action inspected"), FoundIncrement);
    return !HasAnyErrors();
}
#endif
