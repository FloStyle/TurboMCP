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
#include "K2Node_CreateDelegate.h"
#include "K2Node_Self.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPBindFunctionReviewTest,
    "TurboMCP.Graph.BindFunctionReview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPBindFunctionReviewTest::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/BindFunctionReview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BindFunctionReview"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if(!TestNotNull(TEXT("Fixture"),BP)) return false;
    for(const TCHAR* Name:{TEXT("OnTimer"),TEXT("OtherHandler")})
    {
        UEdGraph* Function=FBlueprintEditorUtils::CreateNewGraph(BP,FName(Name),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP,Function,true,nullptr);
    }
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("TimerFlow"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto* Entry=NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);
    Entry->CustomFunctionName=TEXT("StartTimer"); Graph->AddNode(Entry,false,false); Entry->CreateNewGuid(); Entry->AllocateDefaultPins();
    auto* Timer=NewObject<UK2Node_CallFunction>(Graph,NAME_None,RF_Transactional);
    Timer->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("K2_SetTimerDelegate")));
    Graph->AddNode(Timer,false,false); Timer->CreateNewGuid(); Timer->AllocateDefaultPins(); Timer->NodePosX=600;
    Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Timer->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Timer->FindPinChecked(TEXT("Time"))->DefaultValue=TEXT("1.0");
    auto* Self=NewObject<UK2Node_Self>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Self,false,false); Self->CreateNewGuid(); Self->AllocateDefaultPins(); Self->NodePosY=400;
    auto* Input=Timer->FindPinChecked(TEXT("Delegate"));
    const FString Asset=BP->GetPathName(), Target=TurboMCPNodeId::MakeForNode(Timer,true), SelfId=TurboMCPNodeId::MakeForNode(Self,true);
    auto Edit=[&](const FString& Function,const FString& Object=TEXT(""),const FString& ObjectPin=TEXT(""))
    { return UTurboMCPEditToolset::EditTurboMCPBindFunction(Asset,Graph->GetName(),Target,TEXT("Delegate"),Function,Object,ObjectPin); };
    Package->SetDirtyFlag(false); const int32 Count=Graph->Nodes.Num();
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPBindFunction(Asset,Graph->GetName(),Target,TEXT("Delegate"),TEXT("OnTimer")); AddInfo(Preview);
    TestTrue(TEXT("Preview succeeds"),Preview.Contains(TEXT("preview (transient graph)")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty()); TestEqual(TEXT("Preview node count"),Graph->Nodes.Num(),Count); TestTrue(TEXT("Preview leaves input empty"),Input->LinkedTo.IsEmpty());
    const FString First=Edit(TEXT("OnTimer")); AddInfo(First); TestTrue(TEXT("Self edit succeeds"),First.StartsWith(TEXT("changed\n")));
    if(!TestEqual(TEXT("Delegate input connected once"),Input->LinkedTo.Num(),1)) return false;
    auto* Delegate=Cast<UK2Node_CreateDelegate>(Input->LinkedTo[0]->GetOwningNode());
    if(!TestNotNull(TEXT("Create Event node"),Delegate)) return false;
    TestEqual(TEXT("Function selected"),Delegate->GetFunctionName(),FName(TEXT("OnTimer")));
    TestTrue(TEXT("Implicit self"),Delegate->GetObjectInPin()->LinkedTo.IsEmpty());
    TestTrue(TEXT("Output identifies created node"),First.Contains(TurboMCPNodeId::MakeForNode(Delegate,true)));
    Package->SetDirtyFlag(false);
    const FString Repeat=Edit(TEXT("OnTimer")); AddInfo(Repeat);
    TestTrue(TEXT("Repeated identical binding unchanged"),Repeat.StartsWith(TEXT("unchanged\n")));
    TestFalse(TEXT("Repeated binding clean"),Package->IsDirty());
    // Preserve the original transaction for later assertions even on a failing candidate.
    if(Repeat.StartsWith(TEXT("changed\n"))) GEditor->UndoTransaction(false);
    const FString OldId=TurboMCPNodeId::MakeForNode(Delegate,true);
    const FString Replacement=Edit(TEXT("OtherHandler")); AddInfo(Replacement);
    TestTrue(TEXT("Replacement succeeds"),Replacement.StartsWith(TEXT("changed\n")));
    TestTrue(TEXT("Displaced node identified"),Replacement.Contains(OldId));
    TestTrue(TEXT("Old source retained for inspection"),Graph->Nodes.Contains(Delegate));
    TestTrue(TEXT("Undo replacement"),GEditor->UndoTransaction(false));
    TestTrue(TEXT("Undo restores original binding"),Input->LinkedTo.Contains(Delegate->GetDelegateOutPin()));
    TestTrue(TEXT("Undo initial edit"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo initial count"),Graph->Nodes.Num(),Count); TestTrue(TEXT("Undo empties input"),Input->LinkedTo.IsEmpty());
    Package->SetDirtyFlag(false);
    for(const FString Function:{TEXT("MissingHandler"),TEXT("K2_GetActorLocation")})
        TestTrue(TEXT("Missing/incompatible function refuses"),Edit(Function).Contains(TEXT("refused:")));
    TestTrue(TEXT("Partial object pair refuses"),Edit(TEXT("OnTimer"),SelfId).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty()); TestTrue(TEXT("Refusals preserve input"),Input->LinkedTo.IsEmpty());
    const FString Explicit=Edit(TEXT("OnTimer"),SelfId,TEXT("self")); AddInfo(Explicit);
    TestTrue(TEXT("Explicit object succeeds"),Explicit.StartsWith(TEXT("changed\n")));
    if(Input->LinkedTo.Num()==1)
    {
        auto* Bound=Cast<UK2Node_CreateDelegate>(Input->LinkedTo[0]->GetOwningNode());
        TestTrue(TEXT("Object endpoint preserved"),Bound && Bound->GetObjectInPin()->LinkedTo.Contains(Self->FindPinChecked(TEXT("self"))));
    }
    AddInfo(UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),false,{}));
    const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(Asset,Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
    AddInfo(TEXT("Bind-function capture: ")+Capture); TestTrue(TEXT("Capture"),Capture.StartsWith(TEXT("look:")));
    FKismetEditorUtilities::CompileBlueprint(BP); TestTrue(TEXT("Compile"),BP->Status!=BS_Error);
    return true;
}
#endif
