#include "Tools/TurboMCPToolsets.h"
// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "TurboMCPNodeId.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCustomEventTest,"TurboMCP.Workflow.CustomEvent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPCustomEventTest::RunTest(const FString&)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/CustomEvent_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("custom-event-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Owned, never-saved synthetic fixture only.
    };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_CustomEventReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ReviewEvents"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    const FString Asset=BP->GetPathName(), GraphName=Graph->GetName();
    auto Run=[&](bool Apply,const FString& Name,const FString& Inputs)
    {
        const FString Out=Apply?UTurboMCPEditToolset::EditTurboMCPCustomEventAdd(Asset,GraphName,Name,Inputs):UTurboMCPPreviewToolset::PreviewTurboMCPCustomEventAdd(Asset,GraphName,Name,Inputs);
        Transcript+=Out+TEXT("\n"); return Out;
    };
    const bool BeforeDirty=Package->IsDirty();
    TestTrue(TEXT("Preview validates typed signature"),Run(false,TEXT("HandlePurchase"),TEXT("Amount:float, Label:string")).StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Preview adds no nodes"),Graph->Nodes.Num(),0);
    TestEqual(TEXT("Preview preserves dirty flag"),Package->IsDirty(),BeforeDirty);
    TestTrue(TEXT("Creation succeeds"),Run(true,TEXT("HandlePurchase"),TEXT("Amount:float, Label:string")).StartsWith(TEXT("created custom event:")));
    UK2Node_CustomEvent* Event=nullptr;
    for (UEdGraphNode* Node:Graph->Nodes) if (auto* Found=Cast<UK2Node_CustomEvent>(Node)) Event=Found;
    if (!TestNotNull(TEXT("Created event"),Event)) return false;
    TestNotNull(TEXT("Amount output"),Event->FindPin(TEXT("Amount"),EGPD_Output));
    TestNotNull(TEXT("Label output"),Event->FindPin(TEXT("Label"),EGPD_Output));
    const int32 BeforeCount=Graph->Nodes.Num();
    const FIntPoint Position(Event->NodePosX,Event->NodePosY);
    for (bool Apply:{false,true})
    {
        TestTrue(TEXT("Duplicate event refused"),Run(Apply,TEXT("HandlePurchase"),TEXT("")).StartsWith(TEXT("unchanged:")));
        TestTrue(TEXT("Duplicate parameters refused"),Run(Apply,TEXT("Other"),TEXT("Value:int, value:float")).StartsWith(TEXT("unchanged:")));
        TestTrue(TEXT("None parameter refused"),Run(Apply,TEXT("Other"),TEXT("None:int")).StartsWith(TEXT("unchanged:")));
        TestTrue(TEXT("Invalid type refused"),Run(Apply,TEXT("Other"),TEXT("Value:unknown-type")).StartsWith(TEXT("unchanged:")));
        TestEqual(TEXT("Refusals preserve nodes"),Graph->Nodes.Num(),BeforeCount);
        TestEqual(TEXT("Refusals preserve position"),FIntPoint(Event->NodePosX,Event->NodePosY),Position);
    }
    const FString Body=FString::Printf(TEXT("(Development|PrintString\n execute (-> %s:then)\n InString (-> %s:Label))"),*TurboMCPNodeId::MakeForNode(Event,true),*TurboMCPNodeId::MakeForNode(Event,true));
    const FString Append=UTurboMCPEditToolset::EditTurboMCPGraphAppend(Asset,GraphName,Body,TEXT(""));
    Transcript+=TEXT("\nAppend:\n")+Append;
    TestEqual(TEXT("One call appended"),Graph->Nodes.Num(),BeforeCount+1);
    TestTrue(TEXT("Event reaches appended call"),Event->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num()==1);
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Compiled"),BP->Status!=BS_Error);
    const FString Read=UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Asset,GraphName,TEXT("read"),TEXT("full"));
    Transcript+=TEXT("\nReadback:\n")+Read;
    TestTrue(TEXT("Readback contains event and data pin"),Read.Contains(TEXT("HandlePurchase"))&&Read.Contains(TEXT("Label")));
    Transcript+=TEXT("\nCapture:\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,GraphName,TEXT("graph"),TEXT(""),TEXT("1200x700"));
    TMap<FGuid,FIntPoint> ExistingPositions;
    for (UEdGraphNode* Node:Graph->Nodes) if (Node) ExistingPositions.Add(Node->NodeGuid,FIntPoint(Node->NodePosX,Node->NodePosY));
    const bool DirtyBeforeSecond=Package->IsDirty();
    TestTrue(TEXT("Preview second event"),Run(false,TEXT("OtherEntry"),TEXT("")).StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Second preview preserves dirty state"),Package->IsDirty(),DirtyBeforeSecond);
    TestTrue(TEXT("Create beside existing logic"),Run(true,TEXT("OtherEntry"),TEXT("")).StartsWith(TEXT("created custom event:")));
    for (UEdGraphNode* Node:Graph->Nodes)
        if (Node && ExistingPositions.Contains(Node->NodeGuid))
            TestEqual(TEXT("Existing logic remains fixed"),FIntPoint(Node->NodePosX,Node->NodePosY),ExistingPositions[Node->NodeGuid]);
    TestTrue(TEXT("Inherited event name refused"),Run(true,TEXT("ReceiveBeginPlay"),TEXT("")).StartsWith(TEXT("unchanged:")));
    return true;
}
#endif
