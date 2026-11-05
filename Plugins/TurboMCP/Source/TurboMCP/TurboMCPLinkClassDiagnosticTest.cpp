// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphAppend.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"
#include "BlueprintActionDatabase.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_AddDelegate.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPLinkClassDiagnosticTest,
    "TurboMCP.GraphAppend.LinkClassDiagnostic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPLinkClassDiagnosticTest::RunTest(const FString& Parameters)
{
    UClass* ButtonClass = LoadObject<UClass>(nullptr, TEXT("/Script/CommonUI.CommonButtonBase"));
    UClass* GroupClass = LoadObject<UClass>(nullptr, TEXT("/Script/CommonUI.CommonButtonGroupBase"));
    if (ButtonClass == nullptr || GroupClass == nullptr) { AddError(TEXT("Required stock CommonUI classes are unavailable.")); return false; }
    FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(GroupClass, TEXT("OnButtonBaseClicked"));
    if (!TestNotNull(TEXT("Group dispatcher property"), Property)) return false;
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/LinkClassDiagnostic_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_LinkClasses"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Object; Type.PinSubCategoryObject = ButtonClass;
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("TargetButton"), Type);
    Type.PinSubCategoryObject = GroupClass;
    FBlueprintEditorUtils::AddMemberVariable(BP, TEXT("TargetGroup"), Type);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("LinkClasses"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto Init = [&](UEdGraphNode* Node) { Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->AllocateDefaultPins(); };
    auto* Entry = NewObject<UK2Node_CustomEvent>(Graph); Entry->CustomFunctionName = TEXT("Start"); Init(Entry);
    auto* Wrong = NewObject<UK2Node_VariableGet>(Graph); Wrong->VariableReference.SetSelfMember(TEXT("TargetButton")); Init(Wrong);
    auto* Correct = NewObject<UK2Node_VariableGet>(Graph); Correct->VariableReference.SetSelfMember(TEXT("TargetGroup")); Init(Correct);
    // Discover the exact declaring-class action instead of guessing its display name.
    FBlueprintActionDatabase::Get().RefreshClassActions(GroupClass);
    FString BindType;
    for (const FString& Id : FTurboMCPNodeType::AvailableTypeIds(BP, Graph))
    {
        if (!Id.Contains(TEXT("Bind")) || !Id.Contains(TEXT("Button"))) continue;
        FString Note;
        const auto* Probe = Cast<UK2Node_AddDelegate>(FTurboMCPNodeType::BuildProbeNode(BP, Graph, Id, Note));
        if (Probe && Probe->DelegateReference.GetMemberName() == Property->GetFName()
            && Probe->DelegateReference.GetMemberParentClass() == GroupClass) { BindType = Id; break; }
    }
    if (!TestFalse(TEXT("Exact group dispatcher creation ID discovered"), BindType.IsEmpty())) return false;
    // Append a binding action connected to an existing event and object getter.
    const FString Header = FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then)\n self (-> "), *BindType, *TurboMCPNodeId::MakeForNode(Entry, true));
    const FString WrongText = Header + TurboMCPNodeId::MakeForNode(Wrong, true) + TEXT(":TargetButton))");
    FTurboMCPGraphAppend::FPlan Plan; FString Error, Report;
    Package->SetDirtyFlag(false);
    const int32 Before = Graph->Nodes.Num();
    const bool Planned = FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), WrongText, Plan, Error);
    if (!TestTrue(TEXT("Wrong-class request parses"), Planned) || !TestTrue(TEXT("Wrong-class request reaches schema validation"), Plan.bAcceptable())) { AddError(Error + FTurboMCPGraphAppend::RenderPreview(Plan)); return false; }
    if (Planned && Plan.bAcceptable())
        TestFalse(TEXT("Wrong-class preview refuses"), FTurboMCPGraphAppend::Preflight(Plan, Report, Error));
    FString Diagnostic = Error + Report + FTurboMCPGraphAppend::RenderPreview(Plan);
    Diagnostic.ReplaceInline(TEXT(" "), TEXT(""));
    TestTrue(TEXT("Refusal identifies actual and expected classes"), Diagnostic.Contains(TEXT("CommonButtonBase")) && Diagnostic.Contains(TEXT("CommonButtonGroupBase")));
    TestFalse(TEXT("Wrong-class edit refuses"), FTurboMCPGraphAppend::Apply(Plan, Report, Error));
    AddInfo(Error + Report);
    TestEqual(TEXT("Refusal preserves nodes"), Graph->Nodes.Num(), Before);
    TestTrue(TEXT("Refusal leaves target unlinked"), Wrong->FindPin(TEXT("TargetButton"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("Refusal preserves execution wire"), Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(), 0);
    TestFalse(TEXT("Refusal preserves clean package"), Package->IsDirty());
    const FString CorrectText = Header + TurboMCPNodeId::MakeForNode(Correct, true) + TEXT(":TargetGroup))");
    if (!TestTrue(TEXT("Compatible control plans"), FTurboMCPGraphAppend::Plan(BP->GetPathName(), Graph->GetName(), CorrectText, Plan, Error)) || !TestTrue(TEXT("Compatible control accepted"), Plan.bAcceptable())) { AddError(Error + FTurboMCPGraphAppend::RenderPreview(Plan)); return false; }
    TestTrue(TEXT("Compatible control applies"), FTurboMCPGraphAppend::Apply(Plan, Report, Error));
    TestEqual(TEXT("Compatible control adds exactly one binding"), Graph->Nodes.Num(), Before + 1);
    TestEqual(TEXT("Compatible target link retained"), Correct->FindPin(TEXT("TargetGroup"))->LinkedTo.Num(), 1);
    TestEqual(TEXT("Compatible execution link retained"), Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(), 1);
    return !HasAnyErrors();
}
#endif
