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
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPIntegerDefaultTest,
    "TurboMCP.GraphAppend.IntegerDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPIntegerDefaultTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/IntegerDefault_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_IntegerDefault"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("IntegerDefaults"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    FKismetEditorUtilities::CompileBlueprint(BP);
    FString TypeId;
    for (const FString& Id : FTurboMCPNodeType::AvailableTypeIds(BP, Graph))
        if (Id.Contains(TEXT("MakeLiteralInt"))) { TypeId = Id; break; }
    if (!TestFalse(TEXT("Stock literal integer type discovered"), TypeId.IsEmpty())) return false;
    AddInfo(TypeId);
    const TArray<FString> Values = {TEXT("0"), TEXT("")};
    for (const FString& Value : Values)
    {
        const FString Text = Value.IsEmpty() ? FString::Printf(TEXT("(\"%s\")"), *TypeId)
            : FString::Printf(TEXT("(\"%s\"\n Value %s\n)"), *TypeId, *Value);
        FTurboMCPGraphAppend::FPlan Plan;
        FString Error, Report;
        const int32 Before = Graph->Nodes.Num();
        Package->SetDirtyFlag(false);
        if (!TestTrue(TEXT("Integer default parses"), FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), Text, Plan, Error))
            || !TestTrue(TEXT("Integer default accepted"), Plan.bAcceptable()))
        { AddError(Text + TEXT("\n") + Error + FTurboMCPGraphAppend::RenderPreview(Plan)); return false; }
        TestEqual(TEXT("Preview preserves node count"), Graph->Nodes.Num(), Before);
        TestFalse(TEXT("Preview preserves clean package"), Package->IsDirty());
        if (!TestTrue(TEXT("Integer default applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error))) { AddError(Error); return false; }
        if (!TestEqual(TEXT("One node created"), Graph->Nodes.Num(), Before + 1)) return false;
        UEdGraphPin* Pin = Graph->Nodes.Last()->FindPin(TEXT("Value"), EGPD_Input);
        if (!TestNotNull(TEXT("Integer input exists"), Pin)) return false;
        TestEqual(TEXT("Input is integer"), Pin->PinType.PinCategory, UEdGraphSchema_K2::PC_Int);
        TestEqual(TEXT("Zero read back"), Pin->DefaultValue, FString(TEXT("0")));
    }
    // The graph-text format distinguishes strings from numbers. Quoted zero
    // must refuse with a type explanation, rather than silently coercing it.
    FTurboMCPGraphAppend::FPlan QuotedPlan;
    FString QuotedError;
    const int32 QuotedBefore = Graph->Nodes.Num();
    Package->SetDirtyFlag(false);
    const FString Quoted = FString::Printf(TEXT("(\"%s\"\n Value \"0\"\n)"), *TypeId);
    const bool QuotedParsed = FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), Quoted, QuotedPlan, QuotedError);
    TestTrue(TEXT("Quoted integer refuses"), !QuotedParsed || !QuotedPlan.bAcceptable());
    TestTrue(TEXT("Quoted integer explains the type mismatch"), (QuotedError + FTurboMCPGraphAppend::RenderPreview(QuotedPlan)).Contains(TEXT("quoted string")));
    TestEqual(TEXT("Quoted refusal preserves nodes"), Graph->Nodes.Num(), QuotedBefore);
    TestFalse(TEXT("Quoted refusal preserves clean package"), Package->IsDirty());
    FTurboMCPGraphAppend::FPlan InvalidPlan;
    FString Error, Report;
    const int32 Before = Graph->Nodes.Num();
    Package->SetDirtyFlag(false);
    const FString Invalid = FString::Printf(TEXT("(\"%s\"\n Value not_a_number\n)"), *TypeId);
    const bool Planned = FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), Invalid, InvalidPlan, Error);
    if (Planned && InvalidPlan.bAcceptable())
        TestFalse(TEXT("Invalid number cannot apply"), FTurboMCPGraphAppend::Apply(InvalidPlan, Report, Error));
    TestEqual(TEXT("Invalid number preserves node count"), Graph->Nodes.Num(), Before);
    TestFalse(TEXT("Invalid number preserves clean package"), Package->IsDirty());
    AddInfo(Error + Report + FTurboMCPGraphAppend::RenderPreview(InvalidPlan));
    return !HasAnyErrors();
}
#endif
