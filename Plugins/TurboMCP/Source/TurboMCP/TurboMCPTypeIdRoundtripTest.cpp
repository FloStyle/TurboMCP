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
#include "K2Node_Self.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPTypeIdRoundtripTest,
    "TurboMCP.GraphAppend.TypeIdRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPTypeIdRoundtripTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/TypeIdRoundtrip_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_TypeIds"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), Blueprint)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("TypeIds"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    Blueprint->UbergraphPages.Add(Graph);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    FString Note;
    UEdGraphNode* Invalid = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, TEXT("Invalid|self"), Note);
    if (!TestNull(TEXT("Unknown self ID is refused"), Invalid)) return false;
    const FString Prefix = TEXT("Supported self-reference creation ID(s): ");
    const int32 Start = Note.Find(Prefix);
    if (!TestTrue(TEXT("Refusal supplies self creation guidance"), Start != INDEX_NONE)) return false;
    FString Suggested = Note.Mid(Start + Prefix.Len());
    Suggested.RemoveFromEnd(TEXT("."));
    // This fixture has one stock self action. Ambiguous guidance must not silently choose one.
    if (!TestFalse(TEXT("Self suggestion is singular"), Suggested.Contains(TEXT(", ")))) return false;

    FTurboMCPGraphAppend::FPlan Plan;
    FString Error, Report;
    const int32 Before = Graph->Nodes.Num();
    const FString Text = FString::Printf(TEXT("(\"%s\")"), *Suggested);
    if (!TestTrue(TEXT("Suggested ID parses"), FTurboMCPGraphAppend::Plan(Blueprint->GetPathName(), Graph->GetName(), Text, Plan, Error))
        || !TestTrue(TEXT("Suggested ID is accepted"), Plan.bAcceptable())) { AddError(Error + FTurboMCPGraphAppend::RenderPreview(Plan)); return false; }
    TestEqual(TEXT("Preview preserves graph nodes"), Graph->Nodes.Num(), Before);
    if (!TestTrue(TEXT("Suggested ID applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Self node is created"), Graph->Nodes.Num(), Before + 1);
    if (!TestTrue(TEXT("Created node has expected class"), Graph->Nodes.Last()->IsA<UK2Node_Self>())) return false;
    const int32 TypeStart = Report.Find(TEXT(": type "));
    if (!TestTrue(TEXT("Edit reports a type ID"), TypeStart != INDEX_NONE)) return false;
    FString Reported = Report.Mid(TypeStart + 7);
    int32 End = 0;
    if (Reported.FindChar(TEXT('\n'), End)) Reported.LeftInline(End);
    const FString Repeat = FString::Printf(TEXT("(\"%s\")"), *Reported);
    if (!TestTrue(TEXT("Reported ID parses"), FTurboMCPGraphAppend::Plan(Blueprint->GetPathName(), Graph->GetName(), Repeat, Plan, Error))
        || !TestTrue(TEXT("Reported ID can be reused"), Plan.bAcceptable())) { AddError(Error + FTurboMCPGraphAppend::RenderPreview(Plan)); return false; }
    if (!TestTrue(TEXT("Reported ID applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Reusing reported ID adds another node"), Graph->Nodes.Num(), Before + 2);
    return !HasAnyErrors();
}
#endif
