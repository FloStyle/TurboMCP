// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPNodeType.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPGraphAppend.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CustomEvent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPPinContainerTest,
    "TurboMCP.GraphAppend.PinContainers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPPinContainerTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/PinContainers_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_PinContainers"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    FEdGraphPinType Scalar; Scalar.PinCategory = UEdGraphSchema_K2::PC_Name;
    FEdGraphPinType Map = Scalar; Map.ContainerType = EPinContainerType::Map;
    Map.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_String;
    FEdGraphPinType Set = Scalar; Set.ContainerType = EPinContainerType::Set;
    FEdGraphPinType Array = Scalar; Array.ContainerType = EPinContainerType::Array;
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("NameMap"), Map);
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("OtherMap"), Map);
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("NameSet"), Set);
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("NameArray"), Array);
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("ScalarName"), Scalar);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("ContainerGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph, NAME_None, RF_Transactional);
    Entry->CustomFunctionName = TEXT("Start"); Graph->AddNode(Entry, false, false); Entry->CreateNewGuid(); Entry->AllocateDefaultPins();
    UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
    Getter->VariableReference.SetSelfMember(TEXT("NameMap")); Graph->AddNode(Getter, false, false); Getter->CreateNewGuid(); Getter->AllocateDefaultPins();
    FKismetEditorUtilities::CompileBlueprint(BP);
    const TArray<FString> Ids = FTurboMCPNodeType::AvailableTypeIds(BP, Graph);
    auto FindId = [&](const FString& Name, bool bSetter)
    {
        for (const FString& Id : Ids)
        {
            if (!Id.Contains(Name)) continue;
            FString Note;
            UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(BP, Graph, Id, Note);
            if (bSetter ? Cast<UK2Node_VariableSet>(Probe) != nullptr : Cast<UK2Node_VariableGet>(Probe) != nullptr) return Id;
        }
        return FString();
    };
    const int32 Before = Graph->Nodes.Num();
    for (const TPair<FString,FString>& Case : TArray<TPair<FString,FString>>{
        {TEXT("NameMap"), TEXT("Map<name, string>")}, {TEXT("NameSet"), TEXT("Set<name>")},
        {TEXT("NameArray"), TEXT("Array<name>")}, {TEXT("ScalarName"), TEXT("name")}})
    {
        FTurboMCPNodePins::FPlan Pins; FString Error;
        if (!TestTrue(TEXT("Pin discovery accepts variable creation ID"), FTurboMCPNodePins::Plan(BP->GetPathName(), Graph->GetName(), FindId(Case.Key,false), Pins, Error))) { AddError(Error); return false; }
        const auto* Pin = Pins.Pins.FindByPredicate([&](const auto& P){return P.Name == Case.Key;});
        if (TestNotNull(TEXT("Variable value pin"), Pin)) TestEqual(TEXT("Complete pin type"), Pin->Type, Case.Value);
    }
    TestEqual(TEXT("Discovery preserves live graph"), Graph->Nodes.Num(), Before);
    auto TextFor = [&](const FString& Variable)
    {
        return FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then)\n %s (-> %s:NameMap))"),
            *FindId(Variable,true), *TurboMCPNodeId::MakeForNode(Entry,true), *Variable, *TurboMCPNodeId::MakeForNode(Getter,true));
    };
    FTurboMCPGraphAppend::FPlan Plan; FString Error, Report;
    if (!TestTrue(TEXT("Mismatch parses"), FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),TextFor(TEXT("ScalarName")),Plan,Error))) {AddError(Error);return false;}
    TestFalse(TEXT("Map to scalar preview refuses"), FTurboMCPGraphAppend::Preflight(Plan,Report,Error));
    TestTrue(TEXT("Refusal explains incompatible endpoints"), Error.Contains(TEXT("cannot connect")) && Error.Contains(TEXT("NameMap")) && Error.Contains(TEXT("ScalarName")));
    TestEqual(TEXT("Refusal preserves live graph"), Graph->Nodes.Num(), Before);
    if (!TestTrue(TEXT("Compatible map assignment plans"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),TextFor(TEXT("OtherMap")),Plan,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Compatible map assignment applies"),FTurboMCPGraphAppend::Apply(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Compatible control adds one node"),Graph->Nodes.Num(),Before+1);
    return !HasAnyErrors();
}
#endif
