// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPSplitValueReviewTest,
    "TurboMCP.Graph.SplitValueReview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPSplitValueReviewTest::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/SplitValueReview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("SplitValueReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if(!TestNotNull(TEXT("Fixture"),BP)) return false;
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ValueFlow"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto MakeCall=[&](const TCHAR* Name,int32 X,int32 Y)
    {
        auto* N=NewObject<UK2Node_CallFunction>(Graph,NAME_None,RF_Transactional);
        N->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(Name));
        Graph->AddNode(N,false,false); N->CreateNewGuid(); N->AllocateDefaultPins(); N->NodePosX=X; N->NodePosY=Y; return N;
    };
    auto* Event=NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);
    Event->CustomFunctionName=TEXT("ShowMessages"); Graph->AddNode(Event,false,false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    auto* A=MakeCall(TEXT("PrintString"),600,0); auto* B=MakeCall(TEXT("PrintString"),1200,0);
    Event->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(A->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    A->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(B->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    auto* Source=MakeCall(TEXT("MakeLiteralString"),200,500);
    Source->FindPinChecked(TEXT("Value"))->DefaultValue=TEXT("Shared greeting");
    auto* Output=Source->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
    auto* InputA=A->FindPinChecked(TEXT("InString")); auto* InputB=B->FindPinChecked(TEXT("InString"));
    Output->MakeLinkTo(InputA); Output->MakeLinkTo(InputB);
    const FString Asset=BP->GetPathName(), Id=TurboMCPNodeId::MakeForNode(Source,true);
    const int32 Count=Graph->Nodes.Num(); Package->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id); AddInfo(Preview);
    TestTrue(TEXT("Preview success"),Preview.Contains(TEXT("preview (transient graph)")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty()); TestEqual(TEXT("Preview node count"),Graph->Nodes.Num(),Count);
    TestEqual(TEXT("Preview fanout"),Output->LinkedTo.Num(),2);
    const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id); AddInfo(Edit);
    TestTrue(TEXT("Edit success"),Edit.StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("Two copies replace original"),Graph->Nodes.Num(),Count+1);
    TestFalse(TEXT("Original removed"),Graph->Nodes.Contains(Source));
    for(auto* Input:{InputA,InputB}) if(TestEqual(TEXT("Single source per input"),Input->LinkedTo.Num(),1))
    {
        auto* Copy=Input->LinkedTo[0]->GetOwningNode();
        TestTrue(TEXT("New identity"),Copy->NodeGuid!=Source->NodeGuid);
        TestEqual(TEXT("Copied default"),Copy->FindPinChecked(TEXT("Value"))->DefaultValue,FString(TEXT("Shared greeting")));
        TestTrue(TEXT("Result maps copy"),Edit.Contains(TurboMCPNodeId::MakeForNode(Copy,true)));
    }
    TestEqual(TEXT("Existing consumer position"),A->NodePosX,600);
    Package->SetDirtyFlag(false);
    const FString CopyId=TurboMCPNodeId::MakeForNode(InputA->LinkedTo[0]->GetOwningNode(),true);
    TestTrue(TEXT("Single consumer is unchanged"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),CopyId).StartsWith(TEXT("unchanged\n")));
    TestFalse(TEXT("No-op clean"),Package->IsDirty());
    TestTrue(TEXT("Undo"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo count"),Graph->Nodes.Num(),Count); TestTrue(TEXT("Undo original"),Graph->Nodes.Contains(Source));
    TestTrue(TEXT("Undo both links"),Output->LinkedTo.Contains(InputA) && Output->LinkedTo.Contains(InputB));
    // A short exact hexadecimal key must target the same source as the reader ID.
    const FString Key=Source->NodeGuid.ToString(EGuidFormats::Digits);
    TestTrue(TEXT("Unique node key accepted"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(Asset,Graph->GetName(),Key).Contains(TEXT("preview (transient graph)")));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Impure call refused"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),TurboMCPNodeId::MakeForNode(A,true)).Contains(TEXT("refused:")));
    TestTrue(TEXT("Missing source refused"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),TEXT("missing")).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty());
    TestTrue(TEXT("Empty graph refused"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,TEXT(""),Id).Contains(TEXT("refused:")));
    const int32 OriginalX=A->NodePosX; A->NodePosX=MIN_int32;
    TestTrue(TEXT("Coordinate underflow refused"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id).Contains(TEXT("refused:")));
    A->NodePosX=OriginalX;
    Output->bOrphanedPin=true;
    TestTrue(TEXT("Orphaned output refused"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id).Contains(TEXT("refused:")));
    Output->bOrphanedPin=false;
    TestFalse(TEXT("Structural refusals clean"),Package->IsDirty());
    auto* Unused=MakeCall(TEXT("MakeLiteralString"),0,900); Package->SetDirtyFlag(false);
    TestTrue(TEXT("Unused source no-op"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),TurboMCPNodeId::MakeForNode(Unused,true)).Contains(TEXT("No copies needed")));
    TestFalse(TEXT("Unused source remains clean"),Package->IsDirty()); Graph->RemoveNode(Unused);
    auto* Both=NewObject<UK2Node_CallFunction>(Graph,NAME_None,RF_Transactional);
    Both->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")));
    Graph->AddNode(Both,false,false); Both->CreateNewGuid(); Both->AllocateDefaultPins(); Both->NodePosX=1600; Both->NodePosY=500;
    Output->MakeLinkTo(Both->FindPinChecked(TEXT("A"))); Output->MakeLinkTo(Both->FindPinChecked(TEXT("B")));
    TestTrue(TEXT("Two inputs on one node supported"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id).StartsWith(TEXT("changed\n")));
    auto* Left=Both->FindPinChecked(TEXT("A")); auto* Right=Both->FindPinChecked(TEXT("B"));
    if(TestEqual(TEXT("First input connected"),Left->LinkedTo.Num(),1) && TestEqual(TEXT("Second input connected"),Right->LinkedTo.Num(),1))
        TestTrue(TEXT("Each input gets a distinct copy"),Left->LinkedTo[0]->GetOwningNode()!=Right->LinkedTo[0]->GetOwningNode());
    TestTrue(TEXT("Undo same-node fanout"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo all four inputs"),Output->LinkedTo.Num(),4);
    Both->BreakAllNodeLinks(); Graph->RemoveNode(Both);
    // Linked inputs must be restored on each copy, not silently replaced by defaults.
    auto* Upstream=MakeCall(TEXT("MakeLiteralString"),-300,500);
    auto* UpstreamOutput=Upstream->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
    UpstreamOutput->MakeLinkTo(Source->FindPinChecked(TEXT("Value")));
    TestTrue(TEXT("Linked source edit"),UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id).StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("Both copies retain input source"),UpstreamOutput->LinkedTo.Num(),2);
    for(auto* Input:{InputA,InputB}) TestTrue(TEXT("Copied input reciprocal"),Input->LinkedTo[0]->GetOwningNode()->FindPinChecked(TEXT("Value"))->LinkedTo.Contains(UpstreamOutput));
    TestTrue(TEXT("Undo linked source"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores upstream link"),UpstreamOutput->LinkedTo.Num(),1);
    Upstream->BreakAllNodeLinks(); Graph->RemoveNode(Upstream);
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphSplitValue(Asset,Graph->GetName(),Id));
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),false,{}));
    const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
    AddInfo(TEXT("Split-value capture: ")+Capture); TestTrue(TEXT("Capture succeeds"),Capture.StartsWith(TEXT("look:")));
    FKismetEditorUtilities::CompileBlueprint(BP); TestTrue(TEXT("Compile succeeds"),BP->Status!=BS_Error);
    return true;
}
#endif
