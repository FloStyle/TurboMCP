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
    TestTrue(TEXT("Identical preview reports zero changes"),Preview.Contains(TEXT("0 node(s) would change")));
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

    const FString InvalidRepoint = UTurboMCPEditToolset::EditTurboMCPGraphRepoint(
        Blueprint->GetPathName(),Graph->GetName(),TargetKey,TEXT("then"),SourceKey,TEXT(""),TEXT("execute"),TEXT("true"));
    AddInfo(InvalidRepoint);
    TestTrue(TEXT("Invalid target input preserves existing edge"),Consumer->FindPinChecked(TEXT("execute"))->LinkedTo.Contains(Target->FindPinChecked(TEXT("then"))));
    TestFalse(TEXT("Invalid target does not report changed"),InvalidRepoint.StartsWith(TEXT("changed:")));
    const FString InvalidPatch = UTurboMCPEditToolset::EditTurboMCPGraphPatch(
        Blueprint->GetPathName(),Graph->GetName(),Patch.Replace(TEXT("InString"),TEXT("MissingPin")));
    AddInfo(InvalidPatch);
    TestEqual(TEXT("Invalid patch preserves value"),Consumer->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("corrected")));
    TestFalse(TEXT("Invalid patch does not report changed"),InvalidPatch.StartsWith(TEXT("changed:")));
    const FString CreatedTarget = UTurboMCPEditToolset::EditTurboMCPGraphRepoint(
        Blueprint->GetPathName(),Graph->GetName(),TargetKey,TEXT("then"),TEXT(""),TEXT("Development|PrintString"),TEXT("then"),TEXT("false"));
    AddInfo(CreatedTarget);
    TestTrue(TEXT("Repoint creates target and removes old source"),CreatedTarget.StartsWith(TEXT("changed:")));
    TestFalse(TEXT("Deleted source is absent"),Graph->Nodes.Contains(Target));
    TestEqual(TEXT("Create/remove preserves net node count"),Graph->Nodes.Num(),3);
    if (!TestEqual(TEXT("Consumer has one replacement source"),Consumer->FindPinChecked(TEXT("execute"))->LinkedTo.Num(),1)) return false;
    Target = Cast<UK2Node_CallFunction>(Consumer->FindPinChecked(TEXT("execute"))->LinkedTo[0]->GetOwningNode());
    if (!TestNotNull(TEXT("Created source is a call node"),Target)) return false;
    TestEqual(TEXT("Create/remove preserves consumer default"),Consumer->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("corrected")));
    UEdGraph* Sibling = NewObject<UEdGraph>(Blueprint,TEXT("UnrelatedGraph"),RF_Transactional);
    Sibling->Schema = UEdGraphSchema_K2::StaticClass();
    Blueprint->UbergraphPages.Add(Sibling);
    auto EditExport = [&]()
    {
        return UTurboMCPReadToolset::ReadTurboMCPGraphLoad(Blueprint->GetPathName(), Graph->GetName(),
            TEXT("edit"), TEXT("bodies"), 0, 24000);
    };
    const FString BeforeEntry = EditExport();
    if (!TestTrue(TEXT("Exact graph has a complete edit export"), BeforeEntry.Contains(TEXT("\"complete\":true"))))
    { AddError(BeforeEntry); return false; }
    const FString Disconnected = BeforeEntry + TEXT("(Development|PrintString\n InString \"unconnected\"\n)\n");
    const int32 BeforeRefusalUndo = Buffer->GetQueueLength();
    const bool BeforeRefusalDirty = Package->IsDirty();
    const FString RefusedPreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Disconnected);
    const FString RefusedEdit = UTurboMCPEditToolset::EditTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Disconnected);
    AddInfo(RefusedPreview); AddInfo(RefusedEdit);
    TestTrue(TEXT("Disconnected replacement preview refuses"),RefusedPreview.Contains(TEXT("preflight refused")));
    TestTrue(TEXT("Disconnected replacement edit refuses before transaction"),RefusedEdit.Contains(TEXT("preflight refused")));
    TestEqual(TEXT("Refusal preserves undo history"),Buffer->GetQueueLength(),BeforeRefusalUndo);
    TestEqual(TEXT("Refusal preserves dirty flag"),Package->IsDirty(),BeforeRefusalDirty);
    TestEqual(TEXT("Refusal preserves nodes"),Graph->Nodes.Num(),3);
    TestTrue(TEXT("Refusal keeps existing connection and default"), Consumer->FindPinChecked(TEXT("execute"))->LinkedTo.Contains(Target->FindPinChecked(TEXT("then")))
        && Consumer->FindPinChecked(TEXT("InString"))->DefaultValue == TEXT("corrected"));
    UK2Node_CustomEvent* Entry = NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);
    Entry->CustomFunctionName = TEXT("ReviewEntry"); Graph->AddNode(Entry,false,false);
    Entry->CreateNewGuid(); Entry->AllocateDefaultPins();
    Entry->FindPinChecked(TEXT("then"))->MakeLinkTo(Target->FindPinChecked(TEXT("execute")));
    FString Replacement = EditExport();
    FString Deletions;
    for (const UEdGraphNode* Node : {Source, Consumer, Target})
    {
        const FString Id = Node->NodeGuid.ToString(EGuidFormats::DigitsLower);
        const int32 Tag = Replacement.Find(TEXT(" ; ") + Id + TEXT("\n"), ESearchCase::CaseSensitive);
        if (!TestTrue(TEXT("Export names each requested deletion"), Tag != INDEX_NONE)) return false;
        const int32 Begin = Replacement.Left(Tag).Find(TEXT("\n("), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1;
        const int32 End = Replacement.Find(TEXT("\n)\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Tag);
        if (!TestTrue(TEXT("Deletion form has exact bounds"), Begin > 0 && End != INDEX_NONE)) return false;
        Replacement.RemoveAt(Begin, End + 3 - Begin);
        Deletions += TEXT("; tmcp-delete-v1: {\"nodeId\":\"") + Id + TEXT("\"}\n");
    }
    Replacement += Deletions + FString::Printf(TEXT("(Development|PrintString\n InString \"replacement body\"\n execute (-> %s:then)\n)\n"),
        *Entry->NodeGuid.ToString(EGuidFormats::DigitsLower));

    const FString ReplacePreview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Replacement);
    AddInfo(ReplacePreview);
    TestEqual(TEXT("Replace preview does not remove nodes"),Graph->Nodes.Num(),4);
    const FString Replace = UTurboMCPEditToolset::EditTurboMCPGraphReplace(Blueprint->GetPathName(),Graph->GetName(),Replacement);
    AddInfo(Replace);
    TestEqual(TEXT("Replace retains entry and installs one body node"),Graph->Nodes.Num(),2);
    TestTrue(TEXT("Entry retained"),Graph->Nodes.Contains(Entry));
    int32 ReplacementCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (Node != Entry && Node->FindPin(TEXT("InString")))
        {
            ++ReplacementCount;
            TestTrue(TEXT("Replacement does not stack on retained entry"),
                FMath::Abs(Node->NodePosX-Entry->NodePosX)>=240 || FMath::Abs(Node->NodePosY-Entry->NodePosY)>=160);
            TestEqual(TEXT("Replacement content independently read back"),Node->FindPinChecked(TEXT("InString"))->DefaultValue,FString(TEXT("replacement body")));
            TestTrue(TEXT("Replacement connected to retained entry"),Node->FindPinChecked(TEXT("execute"))->LinkedTo.Contains(Entry->FindPinChecked(TEXT("then"))));
        }
    TestEqual(TEXT("Exactly one replacement call"),ReplacementCount,1);
    TestTrue(TEXT("Sibling graph retained"),Blueprint->UbergraphPages.Contains(Sibling));
    TestTrue(TEXT("Sibling remains empty"),Sibling->Nodes.IsEmpty());
    const FString Compile = UTurboMCPEditToolset::EditTurboMCPCompile(Blueprint->GetPathName());
    AddInfo(Compile);
    TestTrue(TEXT("Edited Blueprint compiles"),Blueprint->Status != BS_Error);
    const FString Capture = UTurboMCPLookToolset::LookTurboMCPCapture(Blueprint->GetPathName(),Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1200x700"));
    AddInfo(Capture);
    TestFalse(TEXT("Graph capture succeeds"),Capture.StartsWith(TEXT("error:")));
    return !HasAnyErrors();
}
#endif
