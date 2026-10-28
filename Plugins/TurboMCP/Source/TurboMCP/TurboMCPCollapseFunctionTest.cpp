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
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Timeline.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPGraphTidy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPCollapseFunctionTest,"TurboMCP.Workflow.CollapseFunction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTurboMCPCollapseFunctionTest::RunTest(const FString&)
{
    const FString Suffix=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/Collapse_")+Suffix));
    FString Transcript;
    ON_SCOPE_EXIT
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TurboMCP/Workflow"),Suffix);
        IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(Transcript,*FPaths::Combine(Folder,TEXT("collapse-review.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Package->SetDirtyFlag(false); // Owned synthetic fixture only.
    };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_CollapseReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture"),BP)) return false;
    const FString Asset=BP->GetPathName();
    Transcript+=UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,TEXT("FormatLabel"),TEXT("Value:string"),TEXT("ReturnValue:string"));
    UEdGraph* Graph=nullptr;
    for (UEdGraph* G:BP->FunctionGraphs) if (G->GetName()==TEXT("FormatLabel")) Graph=G;
    if (!TestNotNull(TEXT("Source function"),Graph)) return false;
    UK2Node_FunctionEntry* Entry=nullptr;UK2Node_FunctionResult* Result=nullptr;
    for (UEdGraphNode* Node:Graph->Nodes)
    {
        if (auto* Found=Cast<UK2Node_FunctionEntry>(Node)) Entry=Found;
        if (auto* Found=Cast<UK2Node_FunctionResult>(Node)) Result=Found;
    }
    if (!Entry || !Result) return false;
    Entry->NodePosX=0;Entry->NodePosY=0;Result->NodePosX=1500;Result->NodePosY=0;
    const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
    UEdGraphPin* Previous=Entry->FindPin(TEXT("Value"));
    TArray<FString> Ids;
    for (int32 Index=0;Index<3;++Index)
    {
        UK2Node_CallFunction* Node=NewObject<UK2Node_CallFunction>(Graph);
        Node->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")));
        Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();
        Node->NodePosX=320+Index*320;Node->NodePosY=160;
        Node->FindPin(TEXT("B"))->DefaultValue=FString::FromInt(Index+1);
        if (!TestTrue(TEXT("Chain connection"),Schema->TryCreateConnection(Previous,Node->FindPin(TEXT("A"))))) return false;
        Previous=Node->FindPin(TEXT("ReturnValue"));Ids.Add(TurboMCPNodeId::MakeForNode(Node,true));
    }
    if (!TestTrue(TEXT("External consumer"),Schema->TryCreateConnection(Previous,Result->FindPin(TEXT("ReturnValue"))))) return false;
    auto Evaluate=[&]()
    {
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (!TestTrue(TEXT("Compile"),BP->Status!=BS_Error)) return FString(TEXT("COMPILE_FAILED"));
        UFunction* Function=BP->GeneratedClass->FindFunctionByName(TEXT("FormatLabel"));
        if (!Function) return FString(TEXT("FUNCTION_MISSING"));
        FStructOnScope Parameters(Function);
        FStrProperty* Input=FindFProperty<FStrProperty>(Function,TEXT("Value"));
        FStrProperty* Output=FindFProperty<FStrProperty>(Function,TEXT("ReturnValue"));
        if (!Input || !Output) return FString(TEXT("PARAMETER_MISSING"));
        Input->SetPropertyValue_InContainer(Parameters.GetStructMemory(),TEXT("Order"));
        BP->GeneratedClass->GetDefaultObject()->ProcessEvent(Function,Parameters.GetStructMemory());
        return Output->GetPropertyValue_InContainer(Parameters.GetStructMemory());
    };
    UEdGraphNode_Comment* Comment=NewObject<UEdGraphNode_Comment>(Graph);
    Graph->AddNode(Comment,false,false);Comment->CreateNewGuid();
    Comment->NodeComment=TEXT("Append the three label suffixes");
    Comment->NodePosX=280;Comment->NodePosY=120;Comment->NodeWidth=1000;Comment->NodeHeight=220;
    Ids.Add(TurboMCPNodeId::MakeForNode(Comment,true));
    TestEqual(TEXT("Baseline runtime"),Evaluate(),FString(TEXT("Order123")));
    Package->SetDirtyFlag(false);
    // An unrelated node crosses the proposed replacement position from outside
    // the selected comment. Collapse must place around it, never move it.
    UK2Node_CallFunction* Obstacle=NewObject<UK2Node_CallFunction>(Graph);
    Obstacle->SetFromFunction(UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")));
    Graph->AddNode(Obstacle,false,false);Obstacle->CreateNewGuid();Obstacle->AllocateDefaultPins();
    Obstacle->NodePosX=100;Obstacle->NodePosY=120;
    Package->SetDirtyFlag(false);
    const int32 Count=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),Ids,TEXT("AppendSuffix"),TEXT("function"));
    Transcript+=TEXT("\n")+Preview;
    TestTrue(TEXT("Preview accepted"),Preview.StartsWith(TEXT("would change")));
    TestTrue(TEXT("Signature shown"),Preview.Contains(TEXT("Input_1"))&&Preview.Contains(TEXT("Output_2")));
    TestEqual(TEXT("Preview node count"),Graph->Nodes.Num(),Count);TestFalse(TEXT("Preview clean"),Package->IsDirty());

    for (const FString& InvalidName:{FString(TEXT("FormatLabel")),FString(TEXT("bad name!"))})
    {
        const FString Refused=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),Ids,InvalidName,TEXT("function"));
        Transcript+=TEXT("\n")+Refused;
        TestTrue(TEXT("Invalid/conflicting name refused"),Refused.StartsWith(TEXT("unchanged")));
        TestEqual(TEXT("Name refusal preserves nodes"),Graph->Nodes.Num(),Count);
        TestFalse(TEXT("Name refusal stays clean"),Package->IsDirty());
    }
    TArray<FString> WithEntry=Ids;WithEntry.Add(TurboMCPNodeId::MakeForNode(Entry,true));
    const FString BoundaryRefusal=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),WithEntry,TEXT("InvalidSelection"),TEXT("function"));
    Transcript+=TEXT("\n")+BoundaryRefusal;
    TestTrue(TEXT("Entry selection refused"),BoundaryRefusal.StartsWith(TEXT("unchanged")));
    TestEqual(TEXT("Boundary refusal preserves nodes"),Graph->Nodes.Num(),Count);
    TestFalse(TEXT("Boundary refusal stays clean"),Package->IsDirty());
    // A comment cannot be moved while one of its annotated nodes stays behind.
    TArray<FString> PartialComment=Ids;
    PartialComment.RemoveAt(1);
    const FString CommentRefusal=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),PartialComment,TEXT("PartialComment"),TEXT("function"));
    Transcript+=TEXT("\n")+CommentRefusal;
    TestTrue(TEXT("Partial comment refused with explanation"),CommentRefusal.StartsWith(TEXT("unchanged")) && CommentRefusal.Contains(TEXT("unselected node")));
    TestEqual(TEXT("Comment refusal preserves node count"),Graph->Nodes.Num(),Count);
    TestTrue(TEXT("Comment refusal preserves owner"),Comment->GetGraph()==Graph);
    TestFalse(TEXT("Comment refusal stays clean"),Package->IsDirty());
    TestEqual(TEXT("Refusal preserves runtime"),Evaluate(),FString(TEXT("Order123")));
    // A latent call is never legal inside the extracted function. Exercise
    // the public refusal before compiling this intentionally invalid fixture.
    UK2Node_CallFunction* Latent=NewObject<UK2Node_CallFunction>(Graph);
    Latent->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("Delay")));
    Graph->AddNode(Latent,false,false);Latent->CreateNewGuid();Latent->AllocateDefaultPins();
    Latent->NodePosX=2000;Latent->NodePosY=800;
    TArray<FString> WithLatent=Ids;WithLatent.Add(TurboMCPNodeId::MakeForNode(Latent,true));
    Package->SetDirtyFlag(false);
    const int32 LatentCount=Graph->Nodes.Num();
    const int32 FunctionCount=BP->FunctionGraphs.Num();
    const FString LatentRefusal=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),WithLatent,TEXT("InvalidLatent"),TEXT("function"));
    Transcript+=TEXT("\n")+LatentRefusal;
    TestTrue(TEXT("Latent selection refused"),LatentRefusal.StartsWith(TEXT("unchanged")) && LatentRefusal.Contains(TEXT("latent")));
    TestEqual(TEXT("Latent refusal preserves nodes"),Graph->Nodes.Num(),LatentCount);
    TestEqual(TEXT("Latent refusal creates no function"),BP->FunctionGraphs.Num(),FunctionCount);
    TestFalse(TEXT("Latent refusal stays clean"),Package->IsDirty());
    Graph->RemoveNode(Latent);
    UK2Node_Timeline* Timeline=NewObject<UK2Node_Timeline>(Graph);
    Graph->AddNode(Timeline,false,false);Timeline->CreateNewGuid();
    Timeline->NodePosX=2000;Timeline->NodePosY=800;
    TArray<FString> WithTimeline=Ids;WithTimeline.Add(TurboMCPNodeId::MakeForNode(Timeline,true));
    Package->SetDirtyFlag(false);
    const FString TimelineRefusal=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),WithTimeline,TEXT("InvalidTimeline"),TEXT("function"));
    Transcript+=TEXT("\n")+TimelineRefusal;
    TestTrue(TEXT("Event-only timeline refused"),TimelineRefusal.StartsWith(TEXT("unchanged")) && TimelineRefusal.Contains(TEXT("timeline")));
    TestEqual(TEXT("Timeline refusal creates no function"),BP->FunctionGraphs.Num(),FunctionCount);
    TestFalse(TEXT("Timeline refusal stays clean"),Package->IsDirty());
    Graph->RemoveNode(Timeline);

    const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("FormatLabel"),Ids,TEXT("AppendSuffix"),TEXT("function"));
    Transcript+=TEXT("\n")+Edit;
    if (!TestTrue(TEXT("Collapse applied"),Edit.StartsWith(TEXT("changed:")))) return false;
    TestEqual(TEXT("One replacement for three nodes"),Graph->Nodes.Num(),Count-Ids.Num()+1);
    TestEqual(TEXT("Runtime preserved"),Evaluate(),FString(TEXT("Order123")));
    TestEqual(TEXT("Obstacle X preserved"),Obstacle->NodePosX,100);
    TestEqual(TEXT("Obstacle Y preserved"),Obstacle->NodePosY,120);
    UK2Node_CallFunction* Replacement=nullptr;
    for (UEdGraphNode* Node:Graph->Nodes)
        if (auto* Call=Cast<UK2Node_CallFunction>(Node))
            if (Call->FunctionReference.GetMemberName()==TEXT("AppendSuffix")) Replacement=Call;
    FString MeasureError;
    if (TestNotNull(TEXT("Replacement call"),Replacement) &&
        TestTrue(TEXT("Measure replacement"),FTurboMCPGraphTidy::PrepareAlignment(BP,Graph,MeasureError)))
    {
        const FVector2D A=FTurboMCPGraphTidy::AlignmentNodeSize(Replacement);
        const FVector2D B=FTurboMCPGraphTidy::AlignmentNodeSize(Obstacle);
        const bool Overlap=Replacement->NodePosX<Obstacle->NodePosX+B.X && Replacement->NodePosX+A.X>Obstacle->NodePosX &&
            Replacement->NodePosY<Obstacle->NodePosY+B.Y && Replacement->NodePosY+A.Y>Obstacle->NodePosY;
        TestFalse(TEXT("Replacement avoids unrelated node"),Overlap);
    }

    TestEqual(TEXT("Comment moved into function"),Comment->GetGraph()->GetName(),FString(TEXT("AppendSuffix")));
    TestEqual(TEXT("Comment text preserved"),Comment->NodeComment,FString(TEXT("Append the three label suffixes")));
    TestEqual(TEXT("Entry position preserved"),Entry->NodePosX,0);TestEqual(TEXT("Return position preserved"),Result->NodePosX,1500);
    for (const FString& Name:{FString(TEXT("FormatLabel")),FString(TEXT("AppendSuffix"))})
        Transcript+=TEXT("\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Name,TEXT("graph"),TEXT(""),TEXT("1200x700"));

    FEdGraphPinType Integer;Integer.PinCategory=UEdGraphSchema_K2::PC_Int;
    TestTrue(TEXT("Add execution fixture member"),FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("Counter"),Integer,TEXT("0")));
    Transcript+=UTurboMCPEditToolset::EditTurboMCPFunctionAdd(Asset,TEXT("WriteCounter"),TEXT(""),TEXT("ReturnValue:int"));
    UEdGraph* ExecGraph=nullptr;
    for (UEdGraph* G:BP->FunctionGraphs) if (G->GetName()==TEXT("WriteCounter")) ExecGraph=G;
    if (!ExecGraph) return false;
    UK2Node_FunctionEntry* ExecEntry=nullptr;UK2Node_FunctionResult* ExecResult=nullptr;
    for (UEdGraphNode* Node:ExecGraph->Nodes)
    { if(auto* N=Cast<UK2Node_FunctionEntry>(Node))ExecEntry=N;if(auto* N=Cast<UK2Node_FunctionResult>(Node))ExecResult=N; }
    if (!ExecEntry || !ExecResult) return false;
    UK2Node_VariableSet* Set=NewObject<UK2Node_VariableSet>(ExecGraph);
    Set->VariableReference.SetSelfMember(TEXT("Counter"));ExecGraph->AddNode(Set,false,false);Set->CreateNewGuid();Set->AllocateDefaultPins();
    Set->NodePosX=320;Set->NodePosY=0;ExecResult->NodePosX=700;
    Set->FindPin(TEXT("Counter"))->DefaultValue=TEXT("7");
    ExecEntry->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    TestTrue(TEXT("Execution into selection"),Schema->TryCreateConnection(ExecEntry->FindPin(UEdGraphSchema_K2::PN_Then),Set->FindPin(UEdGraphSchema_K2::PN_Execute)));
    TestTrue(TEXT("Execution out of selection"),Schema->TryCreateConnection(Set->FindPin(UEdGraphSchema_K2::PN_Then),ExecResult->FindPin(UEdGraphSchema_K2::PN_Execute)));
    auto ExecuteWrite=[&]()
    {
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (!TestTrue(TEXT("Execution compile"),BP->Status!=BS_Error)) return;
        UObject* Object=BP->GeneratedClass->GetDefaultObject();
        FIntProperty* Counter=FindFProperty<FIntProperty>(BP->GeneratedClass,TEXT("Counter"));
        UFunction* Write=BP->GeneratedClass->FindFunctionByName(TEXT("WriteCounter"));
        if (!Counter || !Write) {AddError(TEXT("Execution fixture reflection missing"));return;}
        Counter->SetPropertyValue_InContainer(Object,0);
        FStructOnScope Parameters(Write);Object->ProcessEvent(Write,Parameters.GetStructMemory());
        TestEqual(TEXT("Execution side effect preserved"),Counter->GetPropertyValue_InContainer(Object),7);
    };
    ExecuteWrite();
    UK2Node_ExecutionSequence* Sequence=NewObject<UK2Node_ExecutionSequence>(ExecGraph);
    ExecGraph->AddNode(Sequence,false,false);Sequence->CreateNewGuid();Sequence->AllocateDefaultPins();
    UK2Node_VariableSet* SecondSet=NewObject<UK2Node_VariableSet>(ExecGraph);
    SecondSet->VariableReference.SetSelfMember(TEXT("Counter"));ExecGraph->AddNode(SecondSet,false,false);SecondSet->CreateNewGuid();SecondSet->AllocateDefaultPins();
    ExecEntry->FindPin(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    TestTrue(TEXT("Mixed fixture entry"),Schema->TryCreateConnection(ExecEntry->FindPin(UEdGraphSchema_K2::PN_Then),Sequence->GetExecPin()));
    TestTrue(TEXT("Mixed first entry"),Schema->TryCreateConnection(Sequence->GetThenPinGivenIndex(0),Set->FindPin(UEdGraphSchema_K2::PN_Execute)));
    TestTrue(TEXT("Mixed second entry"),Schema->TryCreateConnection(Sequence->GetThenPinGivenIndex(1),SecondSet->FindPin(UEdGraphSchema_K2::PN_Execute)));
    Package->SetDirtyFlag(false);
    const int32 MixedNodeCount=ExecGraph->Nodes.Num();
    TArray<FString> MixedIds{TurboMCPNodeId::MakeForNode(Set,true),TurboMCPNodeId::MakeForNode(SecondSet,true)};
    const FString MixedRefusal=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("WriteCounter"),MixedIds,TEXT("MixedFlow"),TEXT("function"));
    Transcript+=TEXT("\n")+MixedRefusal;
    TestTrue(TEXT("Mixed entries refused"),MixedRefusal.StartsWith(TEXT("unchanged")) && MixedRefusal.Contains(TEXT("exactly one incoming")));
    TestEqual(TEXT("Mixed refusal preserves nodes"),ExecGraph->Nodes.Num(),MixedNodeCount);
    TestTrue(TEXT("First entry link preserved"),Set->FindPin(UEdGraphSchema_K2::PN_Execute)->LinkedTo.Contains(Sequence->GetThenPinGivenIndex(0)));
    TestTrue(TEXT("Second entry link preserved"),SecondSet->FindPin(UEdGraphSchema_K2::PN_Execute)->LinkedTo.Contains(Sequence->GetThenPinGivenIndex(1)));
    TestFalse(TEXT("Mixed refusal stays clean"),Package->IsDirty());
    Sequence->BreakAllNodeLinks();SecondSet->BreakAllNodeLinks();ExecGraph->RemoveNode(Sequence);ExecGraph->RemoveNode(SecondSet);
    TestTrue(TEXT("Restore single entry fixture"),Schema->TryCreateConnection(ExecEntry->FindPin(UEdGraphSchema_K2::PN_Then),Set->FindPin(UEdGraphSchema_K2::PN_Execute)));
    TArray<FString> ExecIds{TurboMCPNodeId::MakeForNode(Set,true)};
    const FString ExecEdit=UTurboMCPEditToolset::EditTurboMCPGraphCollapse(Asset,TEXT("WriteCounter"),ExecIds,TEXT("AssignCounter"),TEXT("function"));
    Transcript+=TEXT("\n")+ExecEdit;
    TestTrue(TEXT("Execution selection extracted"),ExecEdit.StartsWith(TEXT("changed:")));
    ExecuteWrite();
    for (const FString& Name:{FString(TEXT("WriteCounter")),FString(TEXT("AssignCounter"))})
        Transcript+=TEXT("\n")+UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Name,TEXT("graph"),TEXT(""),TEXT("1200x700"));
    return true;
}
#endif
