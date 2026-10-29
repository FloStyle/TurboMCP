// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tools/TurboMCPToolsets.h"
#include "Editor/TransBuffer.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Tunnel.h"
#include "Editor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPGraphEditWorkflowTest,
    "TurboMCP.Workflow.GraphEdits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPGraphEditWorkflowTest::RunTest(const FString& Parameters)
{
    const FString PackageName = TEXT("/Game/Tests/GraphEditReview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("ReplaceFixture"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TurboMCPReplaceTest"));
    if (!TestNotNull(TEXT("Blueprint factory succeeds"), Blueprint)) return false;
    FAssetRegistryModule::AssetCreated(Blueprint);
    ON_SCOPE_EXIT
    {
        FAssetRegistryModule::AssetDeleted(Blueprint);
        Blueprint->ClearFlags(RF_Public | RF_Standalone);
        Blueprint->MarkAsGarbage();
        Package->SetDirtyFlag(false); // Disposable, unsaved fixture only.
        Package->MarkAsGarbage();
    };
    UEdGraph* Graph = NewObject<UEdGraph>(Blueprint, TEXT("FailureGraph"), RF_Transactional);
    Graph->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Graph);
    auto MakeCall = [&](int32 X)
    {
        UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transactional);
        Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Node, false, false);
        Node->CreateNewGuid();
        Node->AllocateDefaultPins();
        Node->NodePosX = X;
        return Node;
    };
    UK2Node_CallFunction* Source = MakeCall(0);
    UK2Node_CallFunction* Consumer = MakeCall(300);
    UK2Node_CallFunction* Target = MakeCall(600);
    Source->FindPinChecked(TEXT("then"))->MakeLinkTo(Consumer->FindPinChecked(TEXT("execute")));
    Consumer->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("preserve-me");
    Package->SetDirtyFlag(false);


    const FString Key = TurboMCPNodeId::MakeForNode(Consumer, true);
    const FString Patch = FString::Printf(TEXT("(Development|PrintString ; %s\n InString \"preserve-me\"\n)"), *Key);
    UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
    if (!TestNotNull(TEXT("Editor transaction buffer"), Buffer)) return false;
    const int32 UndoCount = Buffer->GetQueueLength();
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphPatch(Blueprint->GetPathName(),Graph->GetName(),Patch);
    AddInfo(Preview);
    TestFalse(TEXT("Preview preserves clean package"),Package->IsDirty());
    const FString Same = UTurboMCPEditToolset::EditTurboMCPGraphPatch(Blueprint->GetPathName(),Graph->GetName(),Patch);
    AddInfo(Same);
    TestTrue(TEXT("Identical patch reports unchanged"),Same.StartsWith(TEXT("unchanged:")));
    TestFalse(TEXT("Identical patch does not dirty package"),Package->IsDirty());
    TestEqual(TEXT("Identical patch adds no undo entry"),Buffer->GetQueueLength(),UndoCount);
    const FString Changed = UTurboMCPEditToolset::EditTurboMCPGraphPatch(Blueprint->GetPathName(),Graph->GetName(),Patch.Replace(TEXT("preserve-me"),TEXT("corrected")));
    AddInfo(Changed);
    TestTrue(TEXT("Different default reports changed"),Changed.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Actual default changed"),Consumer->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("corrected")));
    TestEqual(TEXT("No nodes added or removed"),Graph->Nodes.Num(),3);
    TestEqual(TEXT("Consumer position preserved"),Consumer->NodePosX,300);
    TestTrue(TEXT("Execution link preserved"),Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Consumer->FindPinChecked(TEXT("execute"))));
    TestEqual(TEXT("Unrelated default preserved"),Target->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("Hello")));
    const FString SourceKey = TurboMCPNodeId::MakeForNode(Source, true);
    const FString TargetKey = TurboMCPNodeId::MakeForNode(Target, true);
    const FString RepointPreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphRepoint(
        Blueprint->GetPathName(),Graph->GetName(),SourceKey,TEXT("then"),TargetKey,TEXT(""),TEXT("then"),TEXT("true"));
    AddInfo(RepointPreview);
    TestTrue(TEXT("Repoint preview preserves original edge"),Source->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Consumer->FindPinChecked(TEXT("execute"))));
    const FString Repoint = UTurboMCPEditToolset::EditTurboMCPGraphRepoint(
        Blueprint->GetPathName(),Graph->GetName(),SourceKey,TEXT("then"),TargetKey,TEXT(""),TEXT("then"),TEXT("true"));
    AddInfo(Repoint);
    TestTrue(TEXT("Repoint moves consumer to target"),Consumer->FindPinChecked(TEXT("execute"))->LinkedTo.Contains(Target->FindPinChecked(TEXT("then"))));
    TestTrue(TEXT("Old source no longer feeds consumer"),Source->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    TestEqual(TEXT("Repoint keeps source when requested"),Graph->Nodes.Num(),3);
    TestEqual(TEXT("Repoint preserves patched default"),Consumer->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("corrected")));
    TestEqual(TEXT("Repoint preserves consumer position"),Consumer->NodePosX,300);

    UEdGraph* Sibling = NewObject<UEdGraph>(Blueprint,TEXT("UnrelatedGraph"),RF_Transactional);
    Sibling->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Sibling);
    const FString Replacement = TEXT("(Development|PrintString\n InString \"replacement body\"\n)");
    const FString ReplacePreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Replacement);
    AddInfo(ReplacePreview);
    TestEqual(TEXT("Replace preview does not remove nodes"),Graph->Nodes.Num(),3);
    const FString Replace = UTurboMCPEditToolset::EditTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Replacement);
    AddInfo(Replace);
    TestEqual(TEXT("Replace installs exactly one body node"),Graph->Nodes.Num(),1);
    if (Graph->Nodes.Num()==1)
        TestEqual(TEXT("Replacement content independently read back"),Graph->Nodes[0]->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("replacement body")));
    TestTrue(TEXT("Sibling graph retained"),Blueprint->UbergraphPages.Contains(Sibling));
    TestTrue(TEXT("Sibling remains empty"),Sibling->Nodes.IsEmpty());
    return !HasAnyErrors();
}
#endif
