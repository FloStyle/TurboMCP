// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphAppend.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_IfThenElse.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPAppendReachabilityTest,
    "TurboMCP.GraphAppend.Reachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPAppendReachabilityTest::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/AppendReachability_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_Reachability"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Reachability"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());BP->UbergraphPages.Add(Graph);
    auto* Entry=NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);Entry->CustomFunctionName=TEXT("Start");Graph->AddNode(Entry,false,false);Entry->CreateNewGuid();Entry->AllocateDefaultPins();
    FKismetEditorUtilities::CompileBlueprint(BP);
    FString PrintId;
    for (const FString& Id:FTurboMCPNodeType::AvailableTypeIds(BP,Graph)) if (Id.Contains(TEXT("PrintString"))) {PrintId=Id;break;}
    if (!TestFalse(TEXT("PrintString type discovered"),PrintId.IsEmpty())) return false;
    FString Error,Report;FTurboMCPGraphAppend::FPlan Plan;
    const FString Disconnected=FString::Printf(TEXT("(\"%s\") ; label Message"),*PrintId);
    if (!TestTrue(TEXT("Disconnected call parses"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),Disconnected,Plan,Error))) {AddError(Error);return false;}
    const int32 BeforeCount=Graph->Nodes.Num();const bool BeforeDirty=Package->IsDirty();
    TestFalse(TEXT("Preview rejects disconnected impure call"),FTurboMCPGraphAppend::Preflight(Plan,Report,Error));
    TestTrue(TEXT("Refusal explains node and pin"),Error.Contains(TEXT("impure node")) && Error.Contains(TEXT("execute is disconnected")) && Error.Contains(TEXT("Print")) && Error.Contains(TEXT("label=Message")));
    TestEqual(TEXT("Preview preserves live nodes"),Graph->Nodes.Num(),BeforeCount);
    TestEqual(TEXT("Preview preserves dirty state"),Package->IsDirty(),BeforeDirty);
    TestFalse(TEXT("Public edit rejects disconnected call"),FTurboMCPGraphAppend::Apply(Plan,Report,Error));
    TestEqual(TEXT("Refused edit preserves node count"),Graph->Nodes.Num(),BeforeCount);
    TestTrue(TEXT("Refused edit preserves entry output"),Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.IsEmpty());
    const FString Wired=FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then)) ; label Message"),*PrintId,*TurboMCPNodeId::MakeForNode(Entry,true));
    if (!TestTrue(TEXT("Wired call parses"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),Wired,Plan,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Wired preview succeeds"),FTurboMCPGraphAppend::Preflight(Plan,Report,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Wired edit succeeds"),FTurboMCPGraphAppend::Apply(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Only requested node added"),Graph->Nodes.Num(),BeforeCount+1);
    TestEqual(TEXT("Entry connected to created node"),Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(),1);
    FString PureId;
    for (const FString& Id:FTurboMCPNodeType::AvailableTypeIds(BP,Graph)) if (Id.Contains(TEXT("GetObjectName"))) {PureId=Id;break;}
    if (!TestFalse(TEXT("Pure data function discovered"),PureId.IsEmpty())) return false;
    const FString PureText=FString::Printf(TEXT("(\"%s\")"),*PureId);
    if (!TestTrue(TEXT("Pure-only append parses"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),PureText,Plan,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Pure-only plan is acceptable"),Plan.bAcceptable())) {AddError(FTurboMCPGraphAppend::RenderPreview(Plan));return false;}
    const int32 BeforePureCount=Graph->Nodes.Num();
    if (!TestTrue(TEXT("Pure-only preview succeeds"),FTurboMCPGraphAppend::Preflight(Plan,Report,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Pure-only edit succeeds"),FTurboMCPGraphAppend::Apply(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Pure-only edit adds one node"),Graph->Nodes.Num(),BeforePureCount+1);
    UBlueprint* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (Macros==nullptr) {AddError(TEXT("Stock macro library is unavailable."));return false;}
    UEdGraph* DoOnceDefinition=nullptr;
    for (UEdGraph* Definition:Macros->MacroGraphs) if (Definition && Definition->GetName()==TEXT("DoOnce")) DoOnceDefinition=Definition;
    if (!TestNotNull(TEXT("Stock DoOnce definition"),DoOnceDefinition)) return false;
    UEdGraph* MergeGraph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("MergeGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());BP->UbergraphPages.Add(MergeGraph);
    auto MakeEvent=[&](const TCHAR* Name,int32 Y){auto* Event=NewObject<UK2Node_CustomEvent>(MergeGraph);Event->CustomFunctionName=Name;MergeGraph->AddNode(Event,false,false);Event->CreateNewGuid();Event->AllocateDefaultPins();Event->NodePosY=Y;return Event;};
    auto* ExistingEntry=MakeEvent(TEXT("ExistingStart"),0);auto* NewEntry=MakeEvent(TEXT("NewStart"),500);
    auto* Once=NewObject<UK2Node_MacroInstance>(MergeGraph);Once->SetMacroGraph(DoOnceDefinition);MergeGraph->AddNode(Once,false,false);Once->CreateNewGuid();Once->AllocateDefaultPins();Once->NodePosX=1000;
    UEdGraphPin* OnceInput=nullptr;
    for (UEdGraphPin* Pin:Once->Pins) if (Pin && Pin->Direction==EGPD_Input && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec && Pin->PinName!=TEXT("Reset")) OnceInput=Pin;
    if (!TestNotNull(TEXT("DoOnce execution input"),OnceInput)) return false;
    if (!TestTrue(TEXT("Existing event connected"),MergeGraph->GetSchema()->TryCreateConnection(ExistingEntry->FindPin(UEdGraphSchema_K2::PN_Then),OnceInput))) return false;
    auto* BranchProbe=NewObject<UK2Node_IfThenElse>(MergeGraph);BranchProbe->AllocateDefaultPins();
    const FString BranchId=TurboMCPNodeId::GetNodeTypeId(BranchProbe);
    const FString MergeText=FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then)\n) ; label Choice\n(\"%s\" ; %s\n %s (-> Choice:then)\n %s (-> Choice:else))"),*BranchId,*TurboMCPNodeId::MakeForNode(NewEntry,true),*TurboMCPNodeId::GetNodeTypeId(Once),*TurboMCPNodeId::MakeForNode(Once,true),*OnceInput->PinName.ToString(),*OnceInput->PinName.ToString());
    const int32 MergeCount=MergeGraph->Nodes.Num();
    if (!TestTrue(TEXT("Existing merge append parses"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),MergeGraph->GetName(),MergeText,Plan,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Existing merge plan is acceptable"),Plan.bAcceptable())) {AddError(FTurboMCPGraphAppend::RenderPreview(Plan));return false;}
    if (!TestTrue(TEXT("Existing merge preview succeeds"),FTurboMCPGraphAppend::Preflight(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Merge preview preserves original input"),OnceInput->LinkedTo.Num(),1);
    if (!TestTrue(TEXT("Existing merge edit succeeds"),FTurboMCPGraphAppend::Apply(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Merge adds only branch"),MergeGraph->Nodes.Num(),MergeCount+1);
    TestEqual(TEXT("DoOnce receives existing and both branch outputs"),OnceInput->LinkedTo.Num(),3);
    TestTrue(TEXT("Original execution link retained"),OnceInput->LinkedTo.Contains(ExistingEntry->FindPin(UEdGraphSchema_K2::PN_Then)));
    return !HasAnyErrors();
}
#endif
