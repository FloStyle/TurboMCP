// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphText.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPNodeId.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPGraphInventoryTest, "TurboMCP.GraphAppend.Inventory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPGraphInventoryTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/GraphInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_Inventory"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("Inventory"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Graph);
    TSet<FString> Expected;
    for (int32 Index = 0; Index < 200; ++Index)
    {
        auto* Event = NewObject<UK2Node_CustomEvent>(Graph);
        Event->CustomFunctionName = FName(*FString::Printf(TEXT("InventoryEvent%d"), Index));
        Graph->AddNode(Event, false, false);
        Event->CreateNewGuid();
        Event->AllocateDefaultPins();
        Event->NodePosX = (Index % 10) * 400;
        Event->NodePosY = (Index / 10) * 200;
        Expected.Add(TurboMCPNodeId::MakeForNode(Event, true));
    }
    const FString Asset = BP->GetPathName();
    const bool DirtyBefore = Package->IsDirty();
    TSet<FString> Seen;
    int32 Offset = 0;
    bool SawTextLimit = false;
    for (int32 Page = 0; Page < 20; ++Page)
    {
        const FString Output = UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, Graph->GetName(), TEXT(""), 256, Offset);
        if (Output.StartsWith(TEXT("error:"))) { AddError(Output); return false; }
        TArray<FString> Lines;
        Output.ParseIntoArrayLines(Lines);
        int32 Rows = 0;
        for (const FString& Line : Lines)
        {
            int32 At = 0;
            At = Line.Find(TEXT(" x="));
            if (At == INDEX_NONE) continue;
            const FString Id = Line.Left(At);
            TestTrue(TEXT("Row belongs to fixture"), Expected.Contains(Id));
            TestFalse(TEXT("No duplicate row across pages"), Seen.Contains(Id));
            Seen.Add(Id);
            ++Rows;
        }
        if (Output.Contains(TEXT("complete=true"))) break;
        SawTextLimit = true;
        if (!TestTrue(TEXT("Truncated page makes progress"), Rows > 0)) return false;
        Offset += Rows;
        TestTrue(TEXT("Continuation reports exact next offset"), Output.Contains(FString::Printf(TEXT("next_offset=%d;"), Offset)));
    }
    TestTrue(TEXT("Text budget required pagination"), SawTextLimit);
    TestEqual(TEXT("All nodes retrieved"), Seen.Num(), Expected.Num());
    TestTrue(TEXT("Past-end offset refuses"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, Graph->GetName(), TEXT(""), 128, 201).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Negative offset refuses"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, Graph->GetName(), TEXT(""), 128, -1).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Exact end is complete"), UTurboMCPLookToolset::LookTurboMCPGraphPositions(Asset, Graph->GetName(), TEXT(""), 128, 200).Contains(TEXT("complete=true")));
    const FString FirstId = TurboMCPNodeId::MakeForNode(Graph->Nodes[0], true);
    for (auto Detail : {FTurboMCPGraphText::EDetail::Manifest, FTurboMCPGraphText::EDetail::Signatures, FTurboMCPGraphText::EDetail::Bodies, FTurboMCPGraphText::EDetail::Full})
    {
        TArray<FString> Notes;
        const FString Read = FTurboMCPGraphText().Render(Graph, Detail, Asset, Notes);
        TestTrue(TEXT("Empty event has ID at every detail"), Read.Contains(FirstId));
    }
    TestTrue(TEXT("Existing finder discovers empty event"), UTurboMCPReadToolset::ReadTurboMCPNodeFind(Asset, TEXT("InventoryEvent0"), Graph->GetName()).Contains(TurboMCPNodeId::MakeKey(Graph->Nodes[0]->NodeGuid)));
    TestEqual(TEXT("Inventory preserves node count"), Graph->Nodes.Num(), 200);
    TestEqual(TEXT("Inventory preserves dirty state"), Package->IsDirty(), DirtyBefore);
    return !HasAnyErrors();
}
#endif
