// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphAppend.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_AssignDelegate.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDelegateAppendTest,
    "TurboMCP.GraphAppend.DelegateSideEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPDelegateAppendTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/DelegateAppend_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_DelegateAppend"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("DelegateGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph, NAME_None, RF_Transactional);
    Entry->CustomFunctionName = TEXT("RegisterHandler");
    Graph->AddNode(Entry, false, false);
    Entry->CreateNewGuid();
    Entry->AllocateDefaultPins();
    FKismetEditorUtilities::CompileBlueprint(BP);
    FString AssignId;
    for (const FString& Id : FTurboMCPNodeType::AvailableTypeIds(BP, Graph))
        if (Id.Contains(TEXT("Assign")) && Id.Contains(TEXT("OnDestroyed"))) { AssignId = Id; break; }
    if (!TestFalse(TEXT("Assign OnDestroyed discovered"), AssignId.IsEmpty())) return false;
    AddInfo(AssignId);
    const int32 Before = Graph->Nodes.Num();
    FTurboMCPGraphAppend::FPlan Plan;
    FString Error, Report;
    const FString Text = FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then))"), *AssignId, *TurboMCPNodeId::MakeForNode(Entry, true));
    if (!TestTrue(TEXT("Delegate append plans"), FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), Text, Plan, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Preview leaves live node count unchanged"), Graph->Nodes.Num(), Before);
    if (!TestTrue(TEXT("Delegate append applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
    AddInfo(Report);
    const int32 Added = Graph->Nodes.Num() - Before;
    TestTrue(TEXT("Fixture creates delegate and generated event"), Added >= 2);
    TestTrue(TEXT("Report identifies engine-created additions"), Report.Contains(TEXT("engine-created")));
    UK2Node_CustomEvent* Handler = nullptr;
    UK2Node_AssignDelegate* Assignment = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Event = Cast<UK2Node_CustomEvent>(Node)) if (Event != Entry) Handler = Event;
        if (auto* Delegate = Cast<UK2Node_AssignDelegate>(Node)) Assignment = Delegate;
    }
    if (!TestNotNull(TEXT("Generated handler"), Handler) || !TestNotNull(TEXT("Assignment"), Assignment)) return false;
    const FString HandlerId = TurboMCPNodeId::MakeForNode(Handler, true);
    TestTrue(TEXT("Generated handler ID is reported"), Report.Contains(HandlerId));
    const int32 BeforeExplicit = Graph->Nodes.Num();
    const FString ExplicitText = FString::Printf(
        TEXT("(\"%s\"\n execute (-> %s:then)\n %s (-> %s:%s))"), *AssignId,
        *TurboMCPNodeId::MakeForNode(Assignment, true), *Assignment->GetDelegatePin()->PinName.ToString(),
        *HandlerId, *UK2Node_CustomEvent::DelegateOutputName.ToString());
    if (!TestTrue(TEXT("Explicit existing handler plans"), FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), ExplicitText, Plan, Error))) { AddError(Error); return false; }
    if (!TestTrue(TEXT("Explicit existing handler applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Only requested assignment remains added"), Graph->Nodes.Num(), BeforeExplicit + 1);
    TestTrue(TEXT("Pre-existing handler retained"), Graph->Nodes.Contains(Handler));
    TestTrue(TEXT("Cleanup is reported"), Report.Contains(TEXT("1 superseded, disconnected engine-created event(s) removed")));
    return !HasAnyErrors();
}
#endif
