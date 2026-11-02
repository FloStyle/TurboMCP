// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphStructure.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPGraphCopyReviewTest,
    "TurboMCP.Graph.CopyReview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPGraphCopyReviewTest::RunTest(const FString& Parameters)
{
    auto MakeBP=[&](const TCHAR* Name)
    {
        UPackage* Package=CreatePackage(*(FString(TEXT("/Game/Tests/"))+Name+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        return FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,Name,BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    };
    UBlueprint* Source=MakeBP(TEXT("CopySource")); UBlueprint* Target=MakeBP(TEXT("CopyTarget"));
    if(!Source || !Target) return false;
    auto MakeGraph=[](UBlueprint* BP,const TCHAR* Name)
    {
        UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,Name,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(BP,G); return G;
    };
    UEdGraph* SG=MakeGraph(Source,TEXT("SourceFlow")); UEdGraph* TG=MakeGraph(Target,TEXT("TargetFlow"));
    auto Event=[](UEdGraph* G,const TCHAR* Name)
    {
        auto* N=NewObject<UK2Node_CustomEvent>(G,NAME_None,RF_Transactional); N->CustomFunctionName=Name;
        G->AddNode(N,false,false); N->CreateNewGuid(); N->AllocateDefaultPins(); return N;
    };
    auto Call=[](UEdGraph* G,int32 X,const TCHAR* Value)
    {
        auto* N=NewObject<UK2Node_CallFunction>(G,NAME_None,RF_Transactional);
        N->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        G->AddNode(N,false,false); N->CreateNewGuid(); N->AllocateDefaultPins(); N->NodePosX=X;
        N->FindPinChecked(TEXT("InString"))->DefaultValue=Value; return N;
    };
    auto In=[](UEdGraphNode* N){return N->FindPinChecked(TEXT("execute"));};
    auto Out=[](UEdGraphNode* N){return N->FindPinChecked(TEXT("then"));};
    auto Id=[](UEdGraphNode* N){return TurboMCPNodeId::MakeForNode(N,true);};
    auto* Start=Event(SG,TEXT("RunSource")); auto* A=Call(SG,500,TEXT("First copied step")); auto* B=Call(SG,1000,TEXT("Second copied step")); auto* Tail=Call(SG,1500,TEXT("Source-only tail"));
    Out(Start)->MakeLinkTo(In(A)); Out(A)->MakeLinkTo(In(B)); Out(B)->MakeLinkTo(In(Tail));
    auto* TargetStart=Event(TG,TEXT("RunCopiedFlow")); auto* Existing=Call(TG,2000,TEXT("Target-only tail")); Existing->NodePosY=400;
    const FString SA=Source->GetPathName(),TA=Target->GetPathName(); const TArray<FString> Selected={Id(A),Id(B)};
    Source->GetPackage()->SetDirtyFlag(false); Target->GetPackage()->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(SA,SG->GetName(),Selected,TA,TG->GetName(),TEXT("")); AddInfo(Preview);
    TestTrue(TEXT("Copy preview accepted"),Preview.StartsWith(TEXT("would change")));
    TestFalse(TEXT("Preview source clean"),Source->GetPackage()->IsDirty()); TestFalse(TEXT("Preview target clean"),Target->GetPackage()->IsDirty());
    TestEqual(TEXT("Preview target untouched"),TG->Nodes.Num(),2);
    const FString Same=UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(SA,SG->GetName(),Selected,SA,SG->GetName().ToLower(),TEXT(""));
    TestTrue(TEXT("Same graph with different case refuses"),Same.StartsWith(TEXT("unchanged")));
    auto Copy=[&](){return UTurboMCPEditToolset::EditTurboMCPGraphCopy(SA,SG->GetName(),Selected,TA,TG->GetName(),TEXT(""));};
    const FString Result=Copy(); AddInfo(Result); TestTrue(TEXT("Copy succeeds"),Result.StartsWith(TEXT("changed")));
    TestEqual(TEXT("Exactly two new nodes"),TG->Nodes.Num(),4);
    TestTrue(TEXT("Boundary links explicitly reported"),Result.Contains(TEXT("UNCONNECTED (2)")));
    TestFalse(TEXT("Copy source stays clean"),Source->GetPackage()->IsDirty());
    TestTrue(TEXT("Original source links intact"),Out(Start)->LinkedTo.Contains(In(A)) && Out(A)->LinkedTo.Contains(In(B)) && Out(B)->LinkedTo.Contains(In(Tail)));
    TestEqual(TEXT("Source position retained"),A->NodePosX,500); TestEqual(TEXT("Existing destination position retained"),Existing->NodePosY,400);
    UEdGraphNode* CA=nullptr; UEdGraphNode* CB=nullptr;
    auto LocateCopies=[&]()
    {
        CA=nullptr; CB=nullptr;
        for(UEdGraphNode* N:TG->Nodes) if(N!=Existing && Cast<UK2Node_CallFunction>(N))
        {
            auto* P=N->FindPin(TEXT("InString"));
            if(P && P->DefaultValue==TEXT("First copied step")) CA=N;
            if(P && P->DefaultValue==TEXT("Second copied step")) CB=N;
        }
    };
    LocateCopies(); if(!TestNotNull(TEXT("First default preserved"),CA) || !TestNotNull(TEXT("Second default preserved"),CB)) return false;
    TestTrue(TEXT("New node identities"),CA->NodeGuid!=A->NodeGuid && CB->NodeGuid!=B->NodeGuid);
    TestTrue(TEXT("Internal wire reciprocal"),Out(CA)->LinkedTo.Contains(In(CB)) && In(CB)->LinkedTo.Contains(Out(CA)));
    TestTrue(TEXT("Boundary intentionally unconnected"),In(CA)->LinkedTo.IsEmpty() && Out(CB)->LinkedTo.IsEmpty());
    TestTrue(TEXT("Undo copy"),GEditor->UndoTransaction(false)); TestEqual(TEXT("Undo target count"),TG->Nodes.Num(),2);
    // Cross-asset self-call names alone do not establish signature compatibility.
    auto AddFunction=[](UBlueprint* BP,FName Type)
    {
        UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("SharedHandler"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP,G,true,nullptr);
        FEdGraphPinType PinType; PinType.PinCategory=Type;
        for(UEdGraphNode* N:G->Nodes) if(auto* Entry=Cast<UK2Node_FunctionEntry>(N)) Entry->CreateUserDefinedPin(TEXT("Value"),PinType,EGPD_Output);
    };
    AddFunction(Source,UEdGraphSchema_K2::PC_Int); AddFunction(Target,UEdGraphSchema_K2::PC_String);
    FKismetEditorUtilities::CompileBlueprint(Source); FKismetEditorUtilities::CompileBlueprint(Target);
    auto* SelfCall=NewObject<UK2Node_CallFunction>(SG,NAME_None,RF_Transactional);
    SelfCall->FunctionReference.SetSelfMember(TEXT("SharedHandler")); SG->AddNode(SelfCall,false,false); SelfCall->CreateNewGuid(); SelfCall->AllocateDefaultPins();
    const FString Mismatch=UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(SA,SG->GetName(),{Id(SelfCall)},TA,TG->GetName(),TEXT("")); AddInfo(Mismatch);
    TestTrue(TEXT("Different signature refuses"),Mismatch.StartsWith(TEXT("unchanged")));
    UBlueprint* Compatible = MakeBP(TEXT("CopyCompatible"));
    UEdGraph* CompatibleGraph = MakeGraph(Compatible,TEXT("CompatibleFlow"));
    AddFunction(Compatible,UEdGraphSchema_K2::PC_Int);
    FKismetEditorUtilities::CompileBlueprint(Compatible);
    const FString CompatibleResult = UTurboMCPEditToolset::EditTurboMCPGraphCopy(SA,SG->GetName(),{Id(SelfCall)},Compatible->GetPathName(),CompatibleGraph->GetName(),TEXT(""));
    AddInfo(CompatibleResult);
    TestTrue(TEXT("Compatible self function copies"),CompatibleResult.StartsWith(TEXT("changed")));
    TestEqual(TEXT("One compatible call copied"),CompatibleGraph->Nodes.Num(),1);
    FKismetEditorUtilities::CompileBlueprint(Compatible);
    TestTrue(TEXT("Compatible copied call compiles"),Compatible->Status!=BS_Error);
    SelfCall->BreakAllNodeLinks(); SG->RemoveNode(SelfCall);
    AddInfo(Copy()); LocateCopies(); if(!CA || !CB) return false;
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphConnect(TA,TG->GetName(),Id(TargetStart),TEXT("then"),Id(CA),TEXT("execute")));
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphConnect(TA,TG->GetName(),Id(CB),TEXT("then"),Id(Existing),TEXT("execute")));
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphTidy(TA,TG->GetName(),false,{}));
    const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(TA,TG->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
    AddInfo(TEXT("Copy capture: ")+Capture); TestTrue(TEXT("Capture"),Capture.StartsWith(TEXT("look:")));
    FKismetEditorUtilities::CompileBlueprint(Target); TestTrue(TEXT("Compile target"),Target->Status!=BS_Error);
    return true;
}
#endif
