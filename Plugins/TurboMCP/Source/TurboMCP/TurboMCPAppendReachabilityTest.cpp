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
    TestFalse(TEXT("Live execution rejects disconnected call"),FTurboMCPGraphAppend::ExecuteGraph(Plan,BP,Graph,Report,Error,false,false));
    TestEqual(TEXT("Rollback preserves node count"),Graph->Nodes.Num(),BeforeCount);
    TestTrue(TEXT("Rollback preserves entry output"),Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.IsEmpty());
    const FString Wired=FString::Printf(TEXT("(\"%s\"\n execute (-> %s:then)) ; label Message"),*PrintId,*TurboMCPNodeId::MakeForNode(Entry,true));
    if (!TestTrue(TEXT("Wired call parses"),FTurboMCPGraphAppend::Plan(BP->GetPathName(),Graph->GetName(),Wired,Plan,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Wired preview succeeds"),FTurboMCPGraphAppend::Preflight(Plan,Report,Error))) {AddError(Error);return false;}
    if (!TestTrue(TEXT("Wired edit succeeds"),FTurboMCPGraphAppend::Apply(Plan,Report,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("Only requested node added"),Graph->Nodes.Num(),BeforeCount+1);
    TestEqual(TEXT("Entry connected to created node"),Entry->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num(),1);
    return !HasAnyErrors();
}
#endif
