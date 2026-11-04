// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
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
        const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{});
        TestTrue(TEXT("Feedback edit succeeds"),Edit.StartsWith(TEXT("changed:")) && Edit.Contains(TEXT("control feedback: 1")));
        TestEqual(TEXT("Node count preserved"),Graph->Nodes.Num(),Identities.Num());
        for (const auto& Pair:Identities) TestEqual(TEXT("Identity preserved"),Pair.Key->NodeGuid,Pair.Value);
        for (const auto& Pair:Links) {TestTrue(TEXT("Links preserved"),Pair.Key->LinkedTo==Pair.Value);TestEqual(TEXT("Default preserved"),Pair.Key->DefaultValue,Defaults[Pair.Key]);}
        const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
        TestTrue(TEXT("Native capture produced"),Capture.Contains(TEXT("renderer: native Slate")) && Capture.Contains(TEXT("image:")));AddInfo(Capture);
    }
    return !HasAnyErrors();
}
#endif
