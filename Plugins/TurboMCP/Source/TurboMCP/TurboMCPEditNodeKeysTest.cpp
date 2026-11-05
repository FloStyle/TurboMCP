// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeId.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPEditNodeKeysTest,
    "TurboMCP.GraphAppend.EditNodeKeys", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPEditNodeKeysTest::RunTest(const FString& Parameters)
{
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/EditKeys_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_EditKeys"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if(!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    auto MakeGraph=[&](const TCHAR* Name){auto* G=FBlueprintEditorUtils::CreateNewGraph(BP,Name,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());BP->UbergraphPages.Add(G);return G;};
    UEdGraph* Graph=MakeGraph(TEXT("Source")); UEdGraph* Target=MakeGraph(TEXT("Target"));
    auto MakeNode=[&](int32 X){auto* N=NewObject<UK2Node_CallFunction>(Graph,NAME_None,RF_Transactional);N->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));Graph->AddNode(N,false,false);N->CreateNewGuid();N->AllocateDefaultPins();N->NodePosX=X;return N;};
    auto* A=MakeNode(0);auto* B=MakeNode(400);
    A->NodeGuid=FGuid(0xAABBCC11,1,2,3); B->NodeGuid=FGuid(0xDDEEFF11,4,5,6);
    const FString Asset=BP->GetPathName(), Key=TEXT("aabbcc"), Full=TurboMCPNodeId::MakeForNode(A,true);
    auto Positions=[&](const FString& Id){return UTurboMCPEditToolset::EditTurboMCPGraphPositions(Asset,Graph->GetName(),FString::Printf(TEXT("[{\"node\":\"%s\",\"x\":64,\"y\":32}]"),*Id));};
    auto Copy=[&](const FString& Id){return UTurboMCPPreviewToolset::PreviewTurboMCPGraphCopy(Asset,Graph->GetName(),{Id},Asset,Target->GetName(),TEXT(""));};
    auto Remove=[&](const FString& Id){return UTurboMCPPreviewToolset::PreviewTurboMCPGraphRemove(Asset,Graph->GetName(),TEXT("[\"")+Id+TEXT("\"]"),TEXT("[]"));};
    auto Tidy=[&](const FString& Id){return UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(Asset,Graph->GetName(),true,{Id});};
    TestTrue(TEXT("Unique key positions apply"),Positions(Key).StartsWith(TEXT("changed")));
    TestTrue(TEXT("Qualified positions control"),Positions(Full).StartsWith(TEXT("unchanged")));
    TestTrue(TEXT("Unique key copy preview"),Copy(Key).StartsWith(TEXT("would change")));
    TestTrue(TEXT("Qualified copy preview control"),Copy(Full).StartsWith(TEXT("would change")));
    TestFalse(TEXT("Unique key tidy resolves"),Tidy(Key).StartsWith(TEXT("error:")));
    TestFalse(TEXT("Qualified tidy control resolves"),Tidy(Full).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Unique key removal preview resolves"),Remove(Key).Contains(TEXT("preview (transient graph)")));
    TestTrue(TEXT("Qualified removal preview control"),Remove(Full).Contains(TEXT("preview (transient graph)")));
    B->NodeGuid=FGuid(0xAABBCC22,4,5,6);
    for(const FString& Id:TArray<FString>{Key,TEXT("ffffff"),TEXT("/Game/Wrong.Wrong:Source:aabbcc")})
    {
        const int32 X=A->NodePosX, Y=A->NodePosY, Count=Graph->Nodes.Num(), TargetCount=Target->Nodes.Num();
        const bool Dirty=Package->IsDirty();
        TestTrue(TEXT("Invalid or ambiguous position ID refuses"),Positions(Id).StartsWith(TEXT("error:")));
        TestFalse(TEXT("Invalid or ambiguous copy ID refuses"),Copy(Id).StartsWith(TEXT("would change")));
        TestTrue(TEXT("Invalid or ambiguous tidy ID refuses"),Tidy(Id).StartsWith(TEXT("error:")));
        TestTrue(TEXT("Invalid or ambiguous removal ID refuses"),Remove(Id).StartsWith(TEXT("unchanged\nrefused:")));
        TestEqual(TEXT("Refusal preserves X"),A->NodePosX,X);
        TestEqual(TEXT("Refusal preserves Y"),A->NodePosY,Y);
        TestEqual(TEXT("Refusal preserves dirty state"),Package->IsDirty(),Dirty);
        TestEqual(TEXT("Refusal preserves source nodes"),Graph->Nodes.Num(),Count);
        TestEqual(TEXT("Refusal preserves destination nodes"),Target->Nodes.Num(),TargetCount);
    }
    // Identical GUIDs must refuse even when the caller uses a qualified reader ID.
    B->NodeGuid=A->NodeGuid;
    TestTrue(TEXT("Duplicate identity positions refuse"),Positions(Full).StartsWith(TEXT("error:")));
    TestFalse(TEXT("Duplicate identity copy refuses"),Copy(Full).StartsWith(TEXT("would change")));
    TestTrue(TEXT("Duplicate identity tidy refuses"),Tidy(Full).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Duplicate identity removal refuses"),Remove(Full).StartsWith(TEXT("unchanged\nrefused:")));
    TestEqual(TEXT("Duplicate refusal preserves source nodes"),Graph->Nodes.Num(),2);
    TestEqual(TEXT("Duplicate refusal preserves target nodes"),Target->Nodes.Num(),0);
    B->NodeGuid=FGuid(0xDDEEFF11,4,5,6);
    const FString Tidied=UTurboMCPEditToolset::EditTurboMCPGraphTidy(Asset,Graph->GetName(),true,{Key});
    TestFalse(TEXT("Unique key tidy edit resolves"),Tidied.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Tidy preserves source nodes"),Graph->Nodes.Num(),2);
    TestTrue(TEXT("Tidy preserves other node"),Graph->Nodes.Contains(B));
    const FString Copied=UTurboMCPEditToolset::EditTurboMCPGraphCopy(Asset,Graph->GetName(),{Key},Asset,Target->GetName(),TEXT(""));
    TestTrue(TEXT("Unique key copy edit"),Copied.StartsWith(TEXT("changed")));
    TestEqual(TEXT("Exactly one node copied"),Target->Nodes.Num(),1);
    const FString Removed=UTurboMCPEditToolset::EditTurboMCPGraphRemove(Asset,Graph->GetName(),TEXT("[\"")+Key+TEXT("\"]"),TEXT("[]"));
    TestTrue(TEXT("Unique key removal edit"),Removed.StartsWith(TEXT("changed")));
    TestEqual(TEXT("Only selected source removed"),Graph->Nodes.Num(),1);
    TestTrue(TEXT("Other source preserved"),Graph->Nodes.Contains(B));
    const FString ObjectPath=B->GetPathName();
    Package->SetDirtyFlag(false);
    TestTrue(TEXT("Object path preview resolves"),Remove(ObjectPath).Contains(TEXT("preview (transient graph)")));
    TestFalse(TEXT("Object path preview stays clean"),Package->IsDirty());
    TestTrue(TEXT("Wrong graph object path refuses"),Remove(Target->Nodes[0]->GetPathName()).StartsWith(TEXT("unchanged\nrefused:")));
    TestTrue(TEXT("Stale object path refuses"),Remove(ObjectPath+TEXT("_missing")).StartsWith(TEXT("unchanged\nrefused:")));
    const FString PathRemoved=UTurboMCPEditToolset::EditTurboMCPGraphRemove(Asset,Graph->GetName(),TEXT("[\"")+ObjectPath+TEXT("\"]"),TEXT("[]"));
    TestTrue(TEXT("Exact object path removal applies"),PathRemoved.StartsWith(TEXT("changed")));
    TestEqual(TEXT("Exact path removes only remaining source"),Graph->Nodes.Num(),0);
    TestEqual(TEXT("Other graph preserved"),Target->Nodes.Num(),1);
    TestTrue(TEXT("Object path removal undo"),GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores node"),Graph->Nodes.Num(),1);
    return !HasAnyErrors();
}
#endif
