// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Knot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPTidyFeedbackTest,
    "TurboMCP.GraphAppend.TidyFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPTidyFeedbackTest::RunTest(const FString& Parameters)
{
    UBlueprint* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (!TestNotNull(TEXT("Stock macro library"),Macros)) return false;
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/TidyFeedback_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_Feedback"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    for (const FName MacroName : {FName(TEXT("ForLoopWithBreak")),FName(TEXT("DoOnce"))})
    {
        UEdGraph* Definition=nullptr;
        for (UEdGraph* Candidate:Macros->MacroGraphs) if (Candidate && Candidate->GetFName()==MacroName) Definition=Candidate;
        if (!TestNotNull(TEXT("Stock macro definition"),Definition)) return false;
        UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,MacroName,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        BP->UbergraphPages.Add(Graph);
        auto Init=[&](UEdGraphNode* Node,int32 X,int32 Y){Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;};
        auto* Event=NewObject<UK2Node_CustomEvent>(Graph);Event->CustomFunctionName=FName(*(TEXT("Start")+MacroName.ToString()));Init(Event,0,0);
        auto* Macro=NewObject<UK2Node_MacroInstance>(Graph);Macro->SetMacroGraph(Definition);Init(Macro,600,200);
        auto* Body=NewObject<UK2Node_CallFunction>(Graph);Body->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));Init(Body,300,400);
        auto* First=NewObject<UK2Node_Knot>(Graph);Init(First,900,500);
        auto* Second=NewObject<UK2Node_Knot>(Graph);Init(Second,1000,600);
        auto Connect=[&](UEdGraphPin* A,UEdGraphPin* B){return TestTrue(TEXT("Fixture connection"),A && B && Graph->GetSchema()->TryCreateConnection(A,B));};
        UEdGraphPin* Input=nullptr;
        for (UEdGraphPin* Pin:Macro->Pins) if (Pin && Pin->Direction==EGPD_Input && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec && Pin->PinName!=TEXT("Break") && Pin->PinName!=TEXT("Reset")) {Input=Pin;break;}
        const bool Loop=MacroName==TEXT("ForLoopWithBreak");
        if (!Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then),Input) ||
            !Connect(Macro->FindPin(Loop?TEXT("LoopBody"):TEXT("Completed")),Body->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Connect(Body->FindPin(UEdGraphSchema_K2::PN_Then),First->GetInputPin()) ||
            !Connect(First->GetOutputPin(),Second->GetInputPin()) ||
            !Connect(Second->GetOutputPin(),Macro->FindPin(Loop?TEXT("Break"):TEXT("Reset")))) return false;
        // Snapshot every pin, not only the feedback edge: tidy may move geometry but not behavior.
        TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
        TMap<UEdGraphPin*,FString> Defaults;
        TMap<UEdGraphNode*,FGuid> Identities;
        TMap<UEdGraphNode*,FIntPoint> Positions;
        for (UEdGraphNode* Node:Graph->Nodes) {Identities.Add(Node,Node->NodeGuid);Positions.Add(Node,FIntPoint(Node->NodePosX,Node->NodePosY));for (UEdGraphPin* Pin:Node->Pins) if (Pin) {Links.Add(Pin,Pin->LinkedTo);Defaults.Add(Pin,Pin->DefaultValue);}}
        const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{});
        TestTrue(TEXT("Feedback preview succeeds"),Preview.Contains(TEXT("preview:")) && Preview.Contains(TEXT("control feedback: 1")));
        for (const auto& Pair:Positions) TestTrue(TEXT("Preview preserves positions"),Pair.Value==FIntPoint(Pair.Key->NodePosX,Pair.Key->NodePosY));
        const FString AutoPreview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{},TEXT("auto"));
        TestTrue(TEXT("Automatic main path accepts stock feedback"),AutoPreview.Contains(TEXT("preview:")));
        const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{});
        TestTrue(TEXT("Feedback edit succeeds"),Edit.StartsWith(TEXT("changed:")) && Edit.Contains(TEXT("control feedback: 1")));
        TestTrue(TEXT("Feedback leaves the body on its right"),First->NodePosX>Body->NodePosX);
        TestTrue(TEXT("Feedback returns from the macro left"),Second->NodePosX<Macro->NodePosX);
        TestTrue(TEXT("Feedback routed below the macro"),First->NodePosY>Macro->NodePosY+120 && Second->NodePosY==First->NodePosY);
        TestEqual(TEXT("Node count preserved"),Graph->Nodes.Num(),Identities.Num());
        for (const auto& Pair:Identities) TestEqual(TEXT("Identity preserved"),Pair.Key->NodeGuid,Pair.Value);
        for (const auto& Pair:Links) {TestTrue(TEXT("Links preserved"),Pair.Key->LinkedTo==Pair.Value);TestEqual(TEXT("Default preserved"),Pair.Key->DefaultValue,Defaults[Pair.Key]);}
        const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
        TestTrue(TEXT("Native capture produced"),Capture.Contains(TEXT("renderer: native Slate")) && Capture.Contains(TEXT("image:")));AddInfo(Capture);
    }
    // An unrelated fixed cycle must not block an explicitly selected acyclic flow.
    UEdGraph* Scoped=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ScopedCycle"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Scoped);
    auto MakeCall=[&](int32 X,int32 Y){auto* Node=NewObject<UK2Node_CallFunction>(Scoped);Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));Scoped->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;return Node;};
    auto* A=MakeCall(0,0);auto* B=MakeCall(-400,100);
    auto* C=MakeCall(0,1000);auto* D=MakeCall(500,1000);
    // Direct pin links deliberately model a pre-existing cycle, independent of editor connection guards.
    A->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(B->FindPin(UEdGraphSchema_K2::PN_Execute));
    C->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(D->FindPin(UEdGraphSchema_K2::PN_Execute));
    D->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(C->FindPin(UEdGraphSchema_K2::PN_Execute));
    const TArray<FString> Selection={TurboMCPNodeId::MakeForNode(A),TurboMCPNodeId::MakeForNode(B)};
    const FString ScopedPreview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,Selection,TEXT("auto"));
    TestTrue(TEXT("Fixed unrelated cycle allows selected preview"),ScopedPreview.Contains(TEXT("preview:")));
    const FString ScopedEdit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,Selection,TEXT("auto"));
    TestTrue(TEXT("Fixed unrelated cycle allows selected edit"),ScopedEdit.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Fixed cycle positions preserved"),C->NodePosX==0 && C->NodePosY==1000 && D->NodePosX==500 && D->NodePosY==1000);
    const FIntPoint BeforeA(A->NodePosX,A->NodePosY),BeforeB(B->NodePosX,B->NodePosY);
    const bool DirtyBefore=Package->IsDirty();
    const FString Refused=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,{});
    TestTrue(TEXT("Unsupported whole-graph cycle refuses"),Refused.StartsWith(TEXT("error:")));
    TestTrue(TEXT("Refusal preserves positions"),BeforeA==FIntPoint(A->NodePosX,A->NodePosY) && BeforeB==FIntPoint(B->NodePosX,B->NodePosY));
    TestEqual(TEXT("Refusal preserves dirty state"),Package->IsDirty(),DirtyBefore);
    TestTrue(TEXT("Cycle links preserved"),C->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(D->FindPin(UEdGraphSchema_K2::PN_Execute)) && D->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(C->FindPin(UEdGraphSchema_K2::PN_Execute)));
    // Data cycles are not control feedback, even when represented by reroutes.
    auto MakeDataKnot=[&](int32 X){auto* Knot=NewObject<UK2Node_Knot>(Scoped);Scoped->AddNode(Knot,false,false);Knot->CreateNewGuid();Knot->AllocateDefaultPins();Knot->NodePosX=X;Knot->NodePosY=2000;for (UEdGraphPin* Pin:Knot->Pins) {Pin->PinType.PinCategory=UEdGraphSchema_K2::PC_Int;}return Knot;};
    auto* DataA=MakeDataKnot(0);auto* DataB=MakeDataKnot(300);
    DataA->GetOutputPin()->MakeLinkTo(DataB->GetInputPin());DataB->GetOutputPin()->MakeLinkTo(DataA->GetInputPin());
    const TArray<FString> DataSelection={TurboMCPNodeId::MakeForNode(DataA),TurboMCPNodeId::MakeForNode(DataB)};
    TestTrue(TEXT("Data cycle preview refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,DataSelection).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Data cycle edit refuses"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,DataSelection).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Data refusal preserves positions and links"),DataA->NodePosX==0 && DataB->NodePosX==300 && DataA->NodePosY==2000 && DataB->NodePosY==2000 && DataA->GetOutputPin()->LinkedTo.Contains(DataB->GetInputPin()) && DataB->GetOutputPin()->LinkedTo.Contains(DataA->GetInputPin()));
    return !HasAnyErrors();
}
#endif
