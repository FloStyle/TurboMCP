// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Self.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPGraphScratch.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPDelegateRetargetTest,"TurboMCP.GraphAppend.DelegateRetarget",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPDelegateRetargetTest::RunTest(const FString&)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/Retarget_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_Retarget"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture"),BP)) return false;
    for (const TCHAR* Name:{TEXT("FirstHandler"),TEXT("SecondHandler")})
    {
        auto* Function=FBlueprintEditorUtils::CreateNewGraph(BP,FName(Name),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(BP,Function,true,nullptr);
    }
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph=BP->UbergraphPages[0];
    auto* Delegate=NewObject<UK2Node_CreateDelegate>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Delegate,false,false);Delegate->CreateNewGuid();Delegate->AllocateDefaultPins();Delegate->NodePosX=160;Delegate->NodePosY=240;
    auto* Self=NewObject<UK2Node_Self>(Graph,NAME_None,RF_Transactional);
    Graph->AddNode(Self,false,false);Self->CreateNewGuid();Self->AllocateDefaultPins();
    TestTrue(TEXT("Explicit self connected"),Graph->GetSchema()->TryCreateConnection(Self->FindPinChecked(TEXT("self")),Delegate->GetObjectInPin()));
    TArray<UK2Node_CallFunction*> Timers;
    for (int32 Index=0;Index<2;++Index)
    {
        auto* Timer=NewObject<UK2Node_CallFunction>(Graph,NAME_None,RF_Transactional);
        Timer->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("K2_SetTimerDelegate")));
        Graph->AddNode(Timer,false,false);Timer->CreateNewGuid();Timer->AllocateDefaultPins();Timer->NodePosX=640;Timer->NodePosY=Index*300;
        Timer->FindPinChecked(TEXT("Time"))->DefaultValue=TEXT("2.0");
        TestTrue(TEXT("Consumer connected"),Graph->GetSchema()->TryCreateConnection(Delegate->GetDelegateOutPin(),Timer->FindPinChecked(TEXT("Delegate"))));
        Timers.Add(Timer);
    }
    Delegate->SetFunction(TEXT("FirstHandler"));Delegate->HandleAnyChangeWithoutNotifying();
    const FString Asset=BP->GetPathName(),Id=TurboMCPNodeId::MakeForNode(Delegate,true);
    const FGuid Guid=Delegate->NodeGuid;const int32 Count=Graph->Nodes.Num();
    auto Edit=[&](const FString& Function){return UTurboMCPEditToolset::EditTurboMCPDelegateRetarget(Asset,Graph->GetName(),Id,Function);};
    Package->SetDirtyFlag(false);
    const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPDelegateRetarget(Asset,Graph->GetName(),Id,TEXT("SecondHandler"));AddInfo(Preview);
    TestTrue(TEXT("Preview describes in-place retarget"),Preview.Contains(TEXT("preview: retarget existing Create Event")));
    TestFalse(TEXT("Preview clean"),Package->IsDirty());TestEqual(TEXT("Preview retains function"),Delegate->GetFunctionName(),FName(TEXT("FirstHandler")));
    for (const TCHAR* Invalid:{TEXT("MissingHandler"),TEXT("K2_GetActorLocation")})
        TestTrue(TEXT("Missing/incompatible function refused"),Edit(Invalid).Contains(TEXT("refused:")));
    TestFalse(TEXT("Refusals clean"),Package->IsDirty());
    const FString Changed=Edit(TEXT("SecondHandler"));AddInfo(Changed);
    TestTrue(TEXT("Retarget changed"),Changed.StartsWith(TEXT("changed")));TestEqual(TEXT("Function changed"),Delegate->GetFunctionName(),FName(TEXT("SecondHandler")));
    TestEqual(TEXT("No new or orphan nodes"),Graph->Nodes.Num(),Count);TestTrue(TEXT("Same identity"),Delegate->NodeGuid==Guid);
    TestEqual(TEXT("X preserved"),Delegate->NodePosX,160);TestEqual(TEXT("Y preserved"),Delegate->NodePosY,240);
    TestEqual(TEXT("Both consumers retained"),Delegate->GetDelegateOutPin()->LinkedTo.Num(),2);
    TestTrue(TEXT("Object input retained"),Delegate->GetObjectInPin()->LinkedTo.Contains(Self->FindPinChecked(TEXT("self"))));
    for (auto* Timer:Timers) {TestTrue(TEXT("Consumer link retained"),Timer->FindPinChecked(TEXT("Delegate"))->LinkedTo.Contains(Delegate->GetDelegateOutPin()));TestEqual(TEXT("Consumer default retained"),Timer->FindPinChecked(TEXT("Time"))->DefaultValue,FString(TEXT("2.0")));}
    Package->SetDirtyFlag(false);TestTrue(TEXT("Same function no-op"),Edit(TEXT("SecondHandler")).StartsWith(TEXT("unchanged")));TestFalse(TEXT("No-op clean"),Package->IsDirty());
    TestTrue(TEXT("Undo"),GEditor->UndoTransaction(false));TestEqual(TEXT("Undo restores function"),Delegate->GetFunctionName(),FName(TEXT("FirstHandler")));
    TestTrue(TEXT("Reapply"),Edit(TEXT("SecondHandler")).StartsWith(TEXT("changed")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    auto* Compiled=Cast<UK2Node_CreateDelegate>(TurboMCPGraphScratch::ByGuid(Graph,Guid));
    TestTrue(TEXT("Compiled selected function retained"),Compiled && Compiled->GetFunctionName()==TEXT("SecondHandler"));
    TestTrue(TEXT("Compile successful"),BP->Status==BS_UpToDate || BP->Status==BS_UpToDateWithWarnings);
    for (const TCHAR* Tool:{TEXT("PreviewTurboMCPDelegateRetarget"),TEXT("EditTurboMCPDelegateRetarget")})
    {
        const FString Help=UTurboMCPReadToolset::ReadTurboMCPToolHelp(FString(Tool).StartsWith(TEXT("Preview"))?TEXT("TurboMCP.TurboMCPPreviewToolset"):TEXT("TurboMCP.TurboMCPEditToolset"),Tool);AddInfo(Help);
        TestTrue(TEXT("Help explains identity and runtime limits"),Help.Contains(TEXT("identity")) && Help.Contains(TEXT("runtime")));
    }
    return true;
}
#endif
