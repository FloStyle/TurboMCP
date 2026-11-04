// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphAppend.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeType.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
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
    FKismetEditorUtilities::CompileBlueprint(BP);
    FString AssignId;
    for (const FString& Id : FTurboMCPNodeType::AvailableTypeIds(BP, Graph))
        if (Id.Contains(TEXT("Assign")) && Id.Contains(TEXT("OnDestroyed"))) { AssignId = Id; break; }
    if (!TestFalse(TEXT("Assign OnDestroyed discovered"), AssignId.IsEmpty())) return false;
    AddInfo(AssignId);
    const int32 Before = Graph->Nodes.Num();
    FTurboMCPGraphAppend::FPlan Plan;
    FString Error, Report;
    const FString Text = FString::Printf(TEXT("(\"%s\"\n)"), *AssignId);
    if (!TestTrue(TEXT("Delegate append plans"), FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), Text, Plan, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Preview leaves live node count unchanged"), Graph->Nodes.Num(), Before);
    if (!TestTrue(TEXT("Delegate append applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
    AddInfo(Report);
    const int32 Added = Graph->Nodes.Num() - Before;
    TestTrue(TEXT("Fixture creates delegate and generated event"), Added >= 2);
    TestTrue(TEXT("Report identifies engine-created additions"), Report.Contains(TEXT("engine-created")));
    return true;
}
#endif
