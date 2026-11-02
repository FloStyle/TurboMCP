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
#include "K2Node_ExecutionSequence.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPSequenceReviewTest,
    "TurboMCP.Graph.SequenceReview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPSequenceReviewTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/SequenceReview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("SequenceReview"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture"), BP)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("OrderedFlow"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    auto* Event = NewObject<UK2Node_CustomEvent>(Graph, NAME_None, RF_Transactional);
    Event->CustomFunctionName = TEXT("RunOrderedCalls"); Graph->AddNode(Event, false, false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    auto MakeCall = [&](const TCHAR* Name, int32 X, int32 Y)
    {
        auto* N = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        N->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(Name));
        Graph->AddNode(N, false, false); N->CreateNewGuid(); N->AllocateDefaultPins(); N->NodePosX=X; N->NodePosY=Y; return N;
    };
    auto* A=MakeCall(TEXT("PrintString"), 500, 0);
    auto* B=MakeCall(TEXT("PrintString"), 1000, 0);
    auto* C=MakeCall(TEXT("PrintString"), 1500, 0);
    auto* Tail=MakeCall(TEXT("PrintString"), 2000, 0);
    auto In=[](UEdGraphNode* N){return N->FindPinChecked(UEdGraphSchema_K2::PN_Execute);};
    auto Out=[](UEdGraphNode* N){return N->FindPinChecked(UEdGraphSchema_K2::PN_Then);};
    Out(Event)->MakeLinkTo(In(A)); Out(A)->MakeLinkTo(In(B)); Out(B)->MakeLinkTo(In(C)); Out(C)->MakeLinkTo(In(Tail));
    A->FindPinChecked(TEXT("InString"))->DefaultValue=TEXT("First");
    B->FindPinChecked(TEXT("InString"))->DefaultValue=TEXT("Second");
    C->FindPinChecked(TEXT("InString"))->DefaultValue=TEXT("Third");
    auto* Unrelated=MakeCall(TEXT("MakeLiteralString"), 0, 900);
    // An unsupported, unused value elsewhere must not block this scoped edit.
    Unrelated->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_String, TEXT("Extra"));
    const FString Asset=BP->GetPathName();
    auto Id=[](UEdGraphNode* N){return TurboMCPNodeId::MakeForNode(N,true);};
    const FString Groups=FString::Printf(TEXT("[[\"%s\",\"%s\"],[\"%s\"]]"),*Id(A),*Id(B),*Id(C));
    Package->SetDirtyFlag(false);
    const int32 OriginalCount=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,true); AddInfo(Preview);
    TestTrue(TEXT("Preview succeeds with unrelated value"),Preview.Contains(TEXT("preview (transient graph)")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty()); TestEqual(TEXT("Preview count"),Graph->Nodes.Num(),OriginalCount);
    TestTrue(TEXT("Preview preserves chain"),Out(B)->LinkedTo.Contains(In(C)));
    TestTrue(TEXT("Empty graph refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphSequence(Asset,TEXT(""),Groups,false).Contains(TEXT("refused:")));
    const FString Reordered=FString::Printf(TEXT("[[\"%s\"],[\"%s\",\"%s\"]]"),*Id(C),*Id(A),*Id(B));
    TestTrue(TEXT("Reordering refuses"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Reordered,false).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusal clean"),Package->IsDirty());
    const FString Result=UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,true); AddInfo(Result);
    TestTrue(TEXT("Edit succeeds"),Result.StartsWith(TEXT("changed\n")));
    UK2Node_ExecutionSequence* Seq=nullptr;
    for(UEdGraphNode* N:Graph->Nodes) if(auto* S=Cast<UK2Node_ExecutionSequence>(N)) { TestNull(TEXT("Only one Sequence"),Seq); Seq=S; }
    if(!Seq) return false;
    TestTrue(TEXT("Entry preserved"),Out(Event)->LinkedTo.Contains(Seq->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
    TestTrue(TEXT("First group"),Seq->GetThenPinGivenIndex(0)->LinkedTo.Contains(In(A)) && Out(A)->LinkedTo.Contains(In(B)));
    TestTrue(TEXT("Second group"),Seq->GetThenPinGivenIndex(1)->LinkedTo.Contains(In(C)));
    TestTrue(TEXT("Continuation last"),Seq->GetThenPinGivenIndex(2)->LinkedTo.Contains(In(Tail)));
    TestTrue(TEXT("Row ends detached"),Out(B)->LinkedTo.IsEmpty() && Out(C)->LinkedTo.IsEmpty());
    TestEqual(TEXT("Defaults preserved"),B->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("Second")));
    TestEqual(TEXT("Existing position preserved"),B->NodePosX,1000);
    TestTrue(TEXT("Result identifies Sequence"),Result.Contains(Id(Seq)));
    TestTrue(TEXT("Single undo succeeds"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo node count"),Graph->Nodes.Num(),OriginalCount);
    TestTrue(TEXT("Undo restores all boundaries"),Out(Event)->LinkedTo.Contains(In(A)) && Out(A)->LinkedTo.Contains(In(B)) && Out(B)->LinkedTo.Contains(In(C)) && Out(C)->LinkedTo.Contains(In(Tail)));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Boolean IDs refuse"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),TEXT("[[true],[false]]"),false).Contains(TEXT("refused:")));
    const FString Duplicate=FString::Printf(TEXT("[[\"%s\"],[\"%s\"]]"),*Id(A),*Id(A));
    TestTrue(TEXT("Duplicate selection refuses"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Duplicate,false).Contains(TEXT("refused:")));
    TestFalse(TEXT("Malformed and duplicate selection refusals stay clean"),Package->IsDirty());
    auto* Bad=MakeCall(TEXT("PrintString"),0,1200); Bad->NodeGuid=A->NodeGuid;
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Duplicate GUID refuses"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,false).Contains(TEXT("refused:")));
    TestFalse(TEXT("Duplicate identity refusal stays clean"),Package->IsDirty());
    Graph->RemoveNode(Bad);
    auto* Latent=MakeCall(TEXT("Delay"),0,1200);
    const FString LatentGroups=FString::Printf(TEXT("[[\"%s\"],[\"%s\"]]"),*Id(Latent),*Id(A));
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Latent call refuses"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),LatentGroups,false).Contains(TEXT("refused:")));
    TestFalse(TEXT("Latent refusal stays clean"),Package->IsDirty());
    Graph->RemoveNode(Latent);
    auto* Shared=MakeCall(TEXT("MakeLiteralString"),0,600);
    Shared->FindPinChecked(TEXT("Value"))->DefaultValue=TEXT("Shared value");
    auto* Value=Shared->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
    Value->MakeLinkTo(A->FindPinChecked(TEXT("InString"))); Value->MakeLinkTo(C->FindPinChecked(TEXT("InString"))); Value->MakeLinkTo(Tail->FindPinChecked(TEXT("InString")));
    TestTrue(TEXT("No-split edit succeeds"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,false).StartsWith(TEXT("changed\n")));
    TestEqual(TEXT("No-split preserves shared fanout"),Value->LinkedTo.Num(),3);
    TestTrue(TEXT("Undo no-split"),GEditor->UndoTransaction(false));
    const FString SplitResult=UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,true); AddInfo(SplitResult);
    TestTrue(TEXT("Split edit succeeds"),SplitResult.StartsWith(TEXT("changed\n")));
    TestTrue(TEXT("Two copies reported"),SplitResult.Contains(TEXT("value copies=2")));
    TestEqual(TEXT("Original remains for unselected continuation"),Value->LinkedTo.Num(),1);
    TestTrue(TEXT("Unselected source retained"),Value->LinkedTo.Contains(Tail->FindPinChecked(TEXT("InString"))));
    for (UEdGraphNode* Consumer:{static_cast<UEdGraphNode*>(A),static_cast<UEdGraphNode*>(C)})
    {
        auto* Input=Consumer->FindPinChecked(TEXT("InString"));
        if(TestEqual(TEXT("One copied source"),Input->LinkedTo.Num(),1))
        {
            auto* Copy=Input->LinkedTo[0]->GetOwningNode();
            TestTrue(TEXT("Source is a copy"),Copy!=Shared);
            TestEqual(TEXT("Copied literal preserved"),Copy->FindPinChecked(TEXT("Value"))->DefaultValue,FString(TEXT("Shared value")));
        }
    }
    TestTrue(TEXT("Undo copies"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores shared fanout"),Value->LinkedTo.Num(),3);
    Shared->BreakAllNodeLinks(); Graph->RemoveNode(Shared);
    Out(C)->BreakLinkTo(In(Tail));
    TestTrue(TEXT("No-continuation edit succeeds"),UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,false).StartsWith(TEXT("changed\n")));
    int32 OutputCount=0;
    for (UEdGraphNode* N:Graph->Nodes) if(auto* S=Cast<UK2Node_ExecutionSequence>(N))
        for(UEdGraphPin* P:S->Pins) if(P && P->Direction==EGPD_Output) ++OutputCount;
    TestEqual(TEXT("No extra continuation output"),OutputCount,2);
    TestTrue(TEXT("Undo no-continuation"),GEditor->UndoTransaction(false));
    Out(C)->MakeLinkTo(In(Tail));
    // Reapply and use the existing layout tool as part of the human-readable workflow.
    Graph->RemoveNode(Unrelated);
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphSequence(Asset,Graph->GetName(),Groups,false));
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),false,{}));
    const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
    AddInfo(TEXT("Sequence capture: ")+Capture); TestTrue(TEXT("Capture succeeds"),Capture.StartsWith(TEXT("look:")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Refactored graph compiles"),BP->Status!=BS_Error);
    return true;
}
#endif
