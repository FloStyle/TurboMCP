// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphTidy.h"
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Parse.h"
#include "Editor.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "TurboMCPNodeId.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPScopedTidyTest,
    "TurboMCP.Graph.ScopedTidy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPScopedTidyTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/ScopedTidy_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("ScopedTidy"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Blueprint fixture"), BP)) return false;
    FKismetEditorUtilities::CompileBlueprint(BP);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("LocalEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    auto* Event = NewObject<UK2Node_CustomEvent>(Graph);
    Event->CustomFunctionName = TEXT("LocalFlow");
    Graph->AddNode(Event, false, false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    auto MakeCall = [&](int32 X, int32 Y)
    {
        auto* Call = NewObject<UK2Node_CallFunction>(Graph);
        Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Call, false, false); Call->CreateNewGuid(); Call->AllocateDefaultPins();
        Call->NodePosX = X; Call->NodePosY = Y;
        return Call;
    };
    auto* Moving = MakeCall(6000, 3000);
    auto* Fixed = MakeCall(1800, 0);
    Event->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Moving->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Moving->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Fixed->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    // Unrelated boxes must not determine where this small connected edit starts.
    for (int32 I = 0; I < 20; ++I)
    {
        auto* Box = NewObject<UEdGraphNode_Comment>(Graph);
        Graph->AddNode(Box, false, false); Box->CreateNewGuid();
        Box->NodePosX = 10000 + I * 500; Box->NodePosY = 1000;
        Box->NodeWidth = 400; Box->NodeHeight = 400; Box->NodeComment = TEXT("Unrelated section");
        MakeCall(Box->NodePosX + 40, Box->NodePosY + 70);
        MakeCall(Box->NodePosX + 40, Box->NodePosY + 220);
    }
    Package->SetDirtyFlag(false);
    FTurboMCPGraphTidy::FPlan Plan;
    FString Error;
    const bool Planned = FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), false, false,
        {TurboMCPNodeId::MakeForNode(Moving, true)}, Plan, Error);
    AddInfo(Error);
    TestTrue(TEXT("Feasible local selection produces a plan"), Planned);
    if (Planned)
    {
        AddInfo(FTurboMCPGraphTidy::RenderPreview(Plan));
        TestTrue(TEXT("Feasible local selection is accepted"), Plan.bWouldApply);
        for (const auto& Node : Plan.Nodes)
        {
            if (Node.Node == Moving)
            {
                TestTrue(TEXT("Selected call stays between its fixed neighbours"), Node.PlannedX > Event->NodePosX && Node.PlannedX < Fixed->NodePosX);
                TestTrue(TEXT("Selected call returns near the execution lane"), FMath::Abs(Node.PlannedY) < 500);
            }
            else
            {
                TestEqual(TEXT("Fixed X unchanged"), Node.PlannedX, Node.X);
                TestEqual(TEXT("Fixed Y unchanged"), Node.PlannedY, Node.Y);
            }
        }
    }
    TestEqual(TEXT("Planning preserves actual X"), Moving->NodePosX, 6000);
    TestEqual(TEXT("Planning preserves actual Y"), Moving->NodePosY, 3000);
    TestFalse(TEXT("Planning preserves clean flag"), Package->IsDirty());
    const FString MovingId = TurboMCPNodeId::MakeForNode(Moving, true);
    FTurboMCPGraphTidy::FPlan AnchoredPlan;
    FString AnchorError;
    const FString AnchorPath = TurboMCPNodeId::MakeForNode(Event, true) + TEXT(",") + MovingId;
    TestTrue(TEXT("Explicit MainPath accepts a fixed first predecessor"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), false, false, {MovingId}, AnchoredPlan, AnchorError, AnchorPath));
    AddInfo(AnchorError);
    for (const auto& Node : AnchoredPlan.Nodes)
        if (Node.Node == Event)
        {
            TestEqual(TEXT("Explicit anchor planned X remains fixed"), Node.PlannedX, Node.X);
            TestEqual(TEXT("Explicit anchor planned Y remains fixed"), Node.PlannedY, Node.Y);
            TestFalse(TEXT("Explicit anchor is not added to the selection"), AnchoredPlan.SelectedNodeKeys.Contains(Node.Key));
        }
    FTurboMCPGraphTidy::FPlan InvalidPathPlan;
    FString InvalidPathError;
    TestFalse(TEXT("MainPath refuses a fixed node after the first anchor"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), false, false, {MovingId}, InvalidPathPlan, InvalidPathError, AnchorPath + TEXT(",") + TurboMCPNodeId::MakeForNode(Fixed, true)));

    TestFalse(TEXT("A fixed anchor without a selected successor is refused"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), false, false, {MovingId}, InvalidPathPlan, InvalidPathError, TurboMCPNodeId::MakeForNode(Event, true)));
    TestFalse(TEXT("A disconnected fixed first anchor is refused"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), false, false, {MovingId}, InvalidPathPlan, InvalidPathError, TurboMCPNodeId::MakeForNode(Fixed, true) + TEXT(",") + MovingId));
    TestEqual(TEXT("Path refusals preserve selected X"), Moving->NodePosX, 6000);
    TestEqual(TEXT("Path refusals preserve selected Y"), Moving->NodePosY, 3000);
    TestFalse(TEXT("Path refusals preserve clean package"), Package->IsDirty());

    AddInfo(TEXT("Before capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), Graph->GetName(), TEXT("neighbours"), MovingId, TEXT("1")));
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(), Graph->GetName(), false, {MovingId});
    AddInfo(Edit);
    TestTrue(TEXT("Public edit applies feasible local placement"), Edit.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Edited call remains between fixed neighbours"), Moving->NodePosX > Event->NodePosX && Moving->NodePosX < Fixed->NodePosX);
    TestTrue(TEXT("Near-equal placement prefers incoming execution anchor"), Moving->NodePosX < 500);
    for (const auto& Node : Plan.Nodes)
        if (Node.Node != Moving)
        {
            TestEqual(TEXT("Edit preserves fixed X"), double(Node.Node->NodePosX), Node.X);
            TestEqual(TEXT("Edit preserves fixed Y"), double(Node.Node->NodePosY), Node.Y);
        }
    TestTrue(TEXT("Edit preserves upstream link"), Moving->FindPinChecked(UEdGraphSchema_K2::PN_Execute)->LinkedTo.Contains(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then)));
    TestTrue(TEXT("Edit preserves downstream link"), Moving->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Fixed->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
    AddInfo(TEXT("After capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), Graph->GetName(), TEXT("neighbours"), MovingId, TEXT("1")));
    // Inject a bad planned position to exercise the independent post-edit guard.
    // This is a rollback fault-injection case, not a natural planner reproduction.
    FTurboMCPGraphTidy::FPlan UnsafePlan;
    FString UnsafeError;
    TestTrue(TEXT("Current local graph can be replanned"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), Graph->GetName(), true, false, {MovingId}, UnsafePlan, UnsafeError));
    const int32 SavedX = Moving->NodePosX, SavedY = Moving->NodePosY;
    Package->SetDirtyFlag(false);
    UnsafePlan.bAlreadyTidy = false;
    UnsafePlan.bForce = true;
    for (auto& Node : UnsafePlan.Nodes)
        if (Node.Node == Moving) Node.PlannedX += 5000;
    const FString Rollback = FTurboMCPGraphTidy::Apply(UnsafePlan);
    AddInfo(Rollback);
    TestTrue(TEXT("Boundary failure rolls back even with Force"), Rollback.StartsWith(TEXT("unchanged:")) && Rollback.Contains(TEXT("fixed-boundary")));
    TestEqual(TEXT("Rollback restores X"), Moving->NodePosX, SavedX);
    TestEqual(TEXT("Rollback restores Y"), Moving->NodePosY, SavedY);
    TestFalse(TEXT("Rollback restores clean package"), Package->IsDirty());
    TestTrue(TEXT("Rollback preserves upstream link"), Moving->FindPinChecked(UEdGraphSchema_K2::PN_Execute)->LinkedTo.Contains(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then)));
    TestTrue(TEXT("Rollback preserves downstream link"), Moving->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Fixed->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));

    UEdGraph* Function = UBlueprintEditorLibrary::AddFunctionGraph(BP, TEXT("SevenNodeFunction"));
    TArray<UK2Node_FunctionEntry*> Entries;
    Function->GetNodesOfClass(Entries);
    if (!TestEqual(TEXT("Function entry exists"), Entries.Num(), 1)) return false;
    auto* Entry = Entries[0];
    auto* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
    if (!TestNotNull(TEXT("Function result exists"), Result)) return false;
    Entry->NodePosX = 0; Entry->NodePosY = 0;
    Result->NodePosX = 2500; Result->NodePosY = 0;
    TArray<FString> SelectedCalls;
    TArray<UK2Node_CallFunction*> Calls;
    UEdGraphPin* Previous = Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    Previous->BreakAllPinLinks();
    for (int32 I = 0; I < 5; ++I)
    {
        auto* Call = NewObject<UK2Node_CallFunction>(Function);
        Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Function->AddNode(Call, false, false); Call->CreateNewGuid(); Call->AllocateDefaultPins();
        Call->NodePosX = 5000 + I * 400; Call->NodePosY = 2000 + I * 100;
        Previous->MakeLinkTo(Call->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
        Previous = Call->FindPinChecked(UEdGraphSchema_K2::PN_Then);
        SelectedCalls.Add(TurboMCPNodeId::MakeForNode(Call, true)); Calls.Add(Call);
    }
    Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute)->BreakAllPinLinks();
    Previous->MakeLinkTo(Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    TestEqual(TEXT("Seven-node function fixture"), Function->Nodes.Num(), 7);
    const FString FunctionEdit = UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(), Function->GetName(), false, SelectedCalls);
    AddInfo(FunctionEdit);
    TestTrue(TEXT("Five-node selected chain is placed locally"), FunctionEdit.StartsWith(TEXT("changed:")));
    int32 LastX = Entry->NodePosX;
    for (const auto* Call : Calls)
    {
        TestTrue(TEXT("Function execution order stays left to right"), Call->NodePosX > LastX && Call->NodePosX < Result->NodePosX);
        TestTrue(TEXT("Function calls return near fixed lane"), FMath::Abs(Call->NodePosY) < 500);
        LastX = Call->NodePosX;
    }
    TestEqual(TEXT("Function result remains fixed"), Result->NodePosX, 2500);
    AddInfo(TEXT("Function capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), Function->GetName(), TEXT("graph"), TEXT(""), TEXT("2048x512")));
    // Whole-graph control: the selected-boundary repair must not change the
    // existing ability to organize a small function when every node can move.
    for (int32 I = 0; I < Calls.Num(); ++I)
    { Calls[I]->NodePosX = 600 - I * 200; Calls[I]->NodePosY = 500 + I * 170; }
    const FString WholeEdit = UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(), Function->GetName(), false, {});
    AddInfo(WholeEdit);
    TestTrue(TEXT("Whole-graph tidy remains usable"), WholeEdit.StartsWith(TEXT("changed:")));
    LastX = Entry->NodePosX;
    for (const auto* Call : Calls)
    {
        TestTrue(TEXT("Whole-graph control retains forward order"), Call->NodePosX > LastX);
        LastX = Call->NodePosX;
    }
    TestTrue(TEXT("Whole-graph result follows calls"), Result->NodePosX > LastX);

    // A partial comment selection is an explicit ownership obstruction. Refuse
    // through the public tool without moving nodes or changing box geometry.
    auto* ProtectedBox = NewObject<UEdGraphNode_Comment>(Graph);
    Graph->AddNode(ProtectedBox, false, false); ProtectedBox->CreateNewGuid();
    ProtectedBox->NodePosX = -100; ProtectedBox->NodePosY = -100;
    ProtectedBox->NodeWidth = 2400; ProtectedBox->NodeHeight = 600;
    ProtectedBox->NodeComment = TEXT("Keep this group together");
    Package->SetDirtyFlag(false);
    const FString Refused = UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(), Graph->GetName(), true, {MovingId});
    AddInfo(Refused);
    TestTrue(TEXT("Obstructed partial-box edit refuses clearly"), !Refused.StartsWith(TEXT("changed:")) && Refused.Contains(TEXT("partially selected comment")));
    TestEqual(TEXT("Obstruction preserves selected X"), Moving->NodePosX, SavedX);
    TestEqual(TEXT("Obstruction preserves selected Y"), Moving->NodePosY, SavedY);
    TestEqual(TEXT("Obstruction preserves box width"), ProtectedBox->NodeWidth, 2400);
    TestEqual(TEXT("Obstruction preserves box height"), ProtectedBox->NodeHeight, 600);
    TestFalse(TEXT("Obstruction preserves clean flag"), Package->IsDirty());
    // TD-49 shape: a new four-node lane feeds a fixed call inside a comment.
    // Only the lane is selected; the target's box remains an immutable obstacle.
    UEdGraph* OriginalGraph = Graph;
    Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("BoxedTarget"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    auto* LaneEvent = NewObject<UK2Node_CustomEvent>(Graph);
    LaneEvent->CustomFunctionName = TEXT("NewLane");
    Graph->AddNode(LaneEvent, false, false); LaneEvent->CreateNewGuid(); LaneEvent->AllocateDefaultPins();
    LaneEvent->NodePosX = 6000; LaneEvent->NodePosY = 3000;
    TArray<FString> LaneIds{TurboMCPNodeId::MakeForNode(LaneEvent, true)};
    TArray<UEdGraphNode*> LaneNodes{LaneEvent};
    UEdGraphPin* LaneOutput = LaneEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Call = MakeCall(6400 + I * 400, 3000);
        LaneOutput->MakeLinkTo(Call->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
        LaneOutput = Call->FindPinChecked(UEdGraphSchema_K2::PN_Then);
        LaneIds.Add(TurboMCPNodeId::MakeForNode(Call, true)); LaneNodes.Add(Call);
    }
    auto* BoxedCall = MakeCall(2400, 80);
    LaneOutput->MakeLinkTo(BoxedCall->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    auto* TargetBox = NewObject<UEdGraphNode_Comment>(Graph);
    Graph->AddNode(TargetBox, false, false); TargetBox->CreateNewGuid();
    TargetBox->NodePosX = 2360; TargetBox->NodePosY = 0;
    TargetBox->NodeWidth = 400; TargetBox->NodeHeight = 350; TargetBox->NodeComment = TEXT("Existing handler");
    const FString LaneEdit = UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(), Graph->GetName(), false, LaneIds);
    AddInfo(LaneEdit);
    TestTrue(TEXT("Four-node lane moves beside boxed consumer"), LaneEdit.StartsWith(TEXT("changed:")));
    LastX = -100000;
    for (const auto* Node : LaneNodes)
    {
        TestTrue(TEXT("Box-target lane flows forward"), Node->NodePosX > LastX && Node->NodePosX < TargetBox->NodePosX);
        TestTrue(TEXT("Box-target lane remains local"), FMath::Abs(Node->NodePosY - BoxedCall->NodePosY) < 500);
        LastX = Node->NodePosX;
    }
    TestEqual(TEXT("Boxed consumer stays fixed"), BoxedCall->NodePosX, 2400);
    TestEqual(TEXT("Target comment stays fixed"), TargetBox->NodePosX, 2360);
    TestEqual(TEXT("Target comment is not resized"), TargetBox->NodeWidth, 400);
    AddInfo(TEXT("Boxed-target capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), Graph->GetName(), TEXT("graph"), TEXT(""), TEXT("2048x512")));
    Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("DenseReports"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    TArray<UEdGraphNode*> DenseNodes;
    for (int32 I = 0; I < 390; ++I) DenseNodes.Add(MakeCall((I % 30) * 900, (I / 30) * 600));
    DenseNodes[1]->NodePosX = DenseNodes[0]->NodePosX + 20;
    DenseNodes[1]->NodePosY = DenseNodes[0]->NodePosY + 20;
    UEdGraphNode_Comment* FirstBox = nullptr;
    for (int32 I = 0; I < 29; ++I)
    {
        auto* Box = NewObject<UEdGraphNode_Comment>(Graph);
        Graph->AddNode(Box, false, false); Box->CreateNewGuid();
        Box->NodePosX = I * 900 - 40; Box->NodePosY = -40;
        Box->NodeWidth = 800; Box->NodeHeight = 500; Box->NodeComment = FString::Printf(TEXT("Dense section %d"), I);
        if (I == 0) FirstBox = Box;
    }
    Package->SetDirtyFlag(false);
    const FString DenseAsset = BP->GetPathName();
    const FString NodeScope = TEXT("node:") + TurboMCPNodeId::MakeForNode(DenseNodes[0], true);
    const FString BoxScope = TEXT("box:") + TurboMCPNodeId::MakeForNode(FirstBox, true);
    const FString ScopedReport = UTurboMCPLookToolset::LookTurboMCPLayoutReport(DenseAsset, Graph->GetName(), 64, NodeScope, 0, 10);
    AddInfo(ScopedReport);
    TestTrue(TEXT("Dense scoped overlap remains visible"), ScopedReport.Contains(TEXT("overlap:")));
    TestTrue(TEXT("Dense layout stays bounded"), ScopedReport.Len() <= 24000);
    TestTrue(TEXT("Dense layout reports paging"), ScopedReport.Contains(TEXT("nextOffset=")));
    const FString Comments = UTurboMCPReadToolset::ReadTurboMCPCommentList(DenseAsset, Graph->GetName(), BoxScope, 0, 1);
    AddInfo(Comments);
    TestTrue(TEXT("Box scope keeps selected comment"), Comments.Contains(TEXT("Dense section 0")));
    TestFalse(TEXT("Box scope excludes distant comment"), Comments.Contains(TEXT("Dense section 28")));
    const FString Page = UTurboMCPReadToolset::ReadTurboMCPCommentList(DenseAsset, Graph->GetName(), TEXT(""), 0, 1);
    TestTrue(TEXT("Comment pagination retains all matching count"), Page.Contains(TEXT("matched=29")) && Page.Contains(TEXT("nextOffset=1")));
    // An unprintable record must not send clients back to the same cursor forever.
    const FString OriginalTitle = FirstBox->NodeComment;
    FirstBox->NodeComment = FString::ChrN(25000, TCHAR('X'));
    const FString OversizedPage = UTurboMCPReadToolset::ReadTurboMCPCommentList(DenseAsset, Graph->GetName(), TEXT(""), 0, 1);
    TestTrue(TEXT("Oversized record reports blocked paging without a partial record"),
        OversizedPage.Len() <= 24000 && OversizedPage.Contains(TEXT("pageBlocked:"))
        && OversizedPage.Contains(TEXT("nextOffset=-1")) && OversizedPage.Contains(TEXT("use Offset=1")));
    const FString AfterOversized = UTurboMCPReadToolset::ReadTurboMCPCommentList(DenseAsset, Graph->GetName(), TEXT(""), 1, 1);
    TestTrue(TEXT("Explicit skip reaches the next complete box"), AfterOversized.Contains(TEXT("Dense section 1"))
        && AfterOversized.Contains(TEXT("nextOffset=2")));
    FirstBox->NodeComment = OriginalTitle;
    TestTrue(TEXT("Region scope reports overlap"), UTurboMCPLookToolset::LookTurboMCPLayoutReport(DenseAsset, Graph->GetName(), 64, TEXT("region:-40,-40,800,500")).Contains(TEXT("overlap:")));
    TestTrue(TEXT("Unknown scope refuses"), UTurboMCPLookToolset::LookTurboMCPLayoutReport(DenseAsset, Graph->GetName(), 64, TEXT("box:missing")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Invalid region refuses"), UTurboMCPReadToolset::ReadTurboMCPCommentList(DenseAsset, Graph->GetName(), TEXT("region:0,0,-1,4")).StartsWith(TEXT("error:")));
    for (const FString Tool : {TEXT("LookTurboMCPLayoutReport"), TEXT("ReadTurboMCPCommentList")})
    {
        const FString Help = UTurboMCPReadToolset::ReadTurboMCPToolHelp(
            Tool.StartsWith(TEXT("Look")) ? TEXT("TurboMCP.TurboMCPLookToolset") : TEXT("TurboMCP.TurboMCPReadToolset"), Tool);
        TestTrue(*(Tool + TEXT(" exposes scope and paging in live help")), Help.Contains(TEXT("Scope"), ESearchCase::IgnoreCase)
            && Help.Contains(TEXT("Offset"), ESearchCase::IgnoreCase) && Help.Contains(TEXT("Limit"), ESearchCase::IgnoreCase));
    }
    TestEqual(TEXT("Report preserves dense graph entries"), Graph->Nodes.Num(), 419);
    TestFalse(TEXT("Reports preserve clean package"), Package->IsDirty());
    AddInfo(TEXT("Dense targeted capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(DenseAsset, Graph->GetName(), TEXT("node"), TurboMCPNodeId::MakeForNode(DenseNodes[0], true), TEXT("1200x800")));
    Graph = OriginalGraph;
    BP->ClearFlags(RF_Public | RF_Standalone);
    Package->SetDirtyFlag(false);
    return !HasAnyErrors();
}

// Group movement is a rigid translation, not a graph tidy or a rewiring operation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPGroupMovementTest,
    "TurboMCP.Graph.GroupMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPGroupMovementTest::RunTest(const FString& Parameters)
{
    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/GroupMove_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("GroupMove"),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Group fixture"), BP)) return false;
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("MoveGroup"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
    auto MakeCall = [&](int32 X, int32 Y)
    {
        auto* Node = NewObject<UK2Node_CallFunction>(Graph);
        Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->AllocateDefaultPins();
        Node->SetFlags(RF_Transactional);
        Node->NodePosX = X; Node->NodePosY = Y;
        return Node;
    };
    auto* First = MakeCall(0, 0);
    auto* Second = MakeCall(600, 0);
    auto* Outside = MakeCall(2200, 0);
    First->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Second->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    Second->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Outside->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    auto* Box = NewObject<UEdGraphNode_Comment>(Graph);
    Graph->AddNode(Box, false, false); Box->CreateNewGuid();
    Box->SetFlags(RF_Transactional);
    Box->NodePosX = 3000; Box->NodePosY = -100; Box->NodeWidth = 1200; Box->NodeHeight = 800;
    Box->NodeComment = TEXT("Destination section");
    const FString Asset = BP->GetPathName();
    const TArray<FString> Ids = {TurboMCPNodeId::MakeForNode(First, true), TurboMCPNodeId::MakeForNode(Second, true)};
    Package->SetDirtyFlag(false);
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupMove(Asset, Graph->GetName(), Ids, 0, 600, TEXT(""));
    AddInfo(Preview);
    TestTrue(TEXT("Translation preview succeeds"), Preview.StartsWith(TEXT("preview:")));
    TestEqual(TEXT("Preview leaves Y unchanged"), First->NodePosY, 0);
    TestFalse(TEXT("Preview preserves clean package"), Package->IsDirty());
    const FString Applied = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), Ids, 0, 600, TEXT(""));
    AddInfo(Applied);
    TestTrue(TEXT("Translation changes nodes"), Applied.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("First translated"), First->NodePosY, 600);
    TestEqual(TEXT("Second translated"), Second->NodePosY, 600);
    TestEqual(TEXT("Internal X spacing preserved"), Second->NodePosX - First->NodePosX, 600);
    TestEqual(TEXT("Outside node stays fixed"), Outside->NodePosY, 0);
    TestTrue(TEXT("Internal link preserved"), First->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Second->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
    TestTrue(TEXT("Boundary link preserved"), Second->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Outside->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
    const FString Collision = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), Ids, 1600, -600, TEXT(""));
    TestTrue(TEXT("Collision refuses"), Collision.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Refusal preserves position"), Second->NodePosY, 600);
    const FString Crossing = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), {Ids[0]}, 2950, -600, TEXT(""));
    AddInfo(TEXT("Comment boundary refusal: ") + Crossing);
    TestTrue(TEXT("Explicit movement refuses partial comment entry"), Crossing.StartsWith(TEXT("error:")));
    TestEqual(TEXT("Comment refusal preserves X"), First->NodePosX, 0);
    TestTrue(TEXT("Zero offset is a no-op"), UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), Ids, 0, 0, TEXT("")).StartsWith(TEXT("unchanged:")));
    TestTrue(TEXT("Missing node refuses"), UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), {TEXT("missing")}, 0, 50, TEXT("")).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Missing graph refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupPlacement(Asset, TEXT("missing"), Ids).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Empty selection refuses"), UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupPlacement(Asset, Graph->GetName(), {}).StartsWith(TEXT("error:")));
    const FString Proposal = UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupPlacement(Asset, Graph->GetName(), Ids);
    AddInfo(Proposal);
    TestTrue(TEXT("Connected fixture gets a placement proposal"), Proposal.StartsWith(TEXT("preview:")));
    int32 DX = 0, DY = 0;
    const bool HasOffsets = FParse::Value(*Proposal, TEXT("DeltaX="), DX) && FParse::Value(*Proposal, TEXT("DeltaY="), DY);
    TestTrue(TEXT("Proposal contains explicit offsets"), HasOffsets);
    TestEqual(TEXT("Proposal preserves position"), First->NodePosY, 600);
    if (HasOffsets)
    {
        const FString Placed = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), Ids, DX, DY, TEXT(""));
        AddInfo(Placed);
        TestTrue(TEXT("Apply proposed free placement"), Placed.StartsWith(TEXT("changed:")));
        TestEqual(TEXT("Proposal X applied exactly"), First->NodePosX, DX);
        TestEqual(TEXT("Proposal Y applied exactly"), First->NodePosY, 600 + DY);
        AddInfo(TEXT("Proposed group placement capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(Asset, Graph->GetName(), TEXT("graph"), TEXT(""), TEXT("1600x900")));
        TestTrue(TEXT("One undo restores placement"), GEditor->UndoTransaction(false));
        TestEqual(TEXT("Undo restores first X"), First->NodePosX, 0);
        TestEqual(TEXT("Undo restores second X"), Second->NodePosX, 600);
        TestEqual(TEXT("Undo restores first Y"), First->NodePosY, 600);
    }
    const FString BoxId = TurboMCPNodeId::MakeForNode(Box, true);
    TestTrue(TEXT("Comment entry requires destination"), UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), {Ids[0]}, 3100, -500, TEXT("")).StartsWith(TEXT("error:")));
    const FString Transfer = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), {Ids[0]}, 3100, -500, BoxId);
    AddInfo(Transfer);
    TestTrue(TEXT("Explicit padded comment transfer succeeds"), Transfer.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Transferred first X"), First->NodePosX, 3100);
    TestEqual(TEXT("Destination width is unchanged"), Box->NodeWidth, 1200);
    const FString MovedBox = UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(Asset, Graph->GetName(), {BoxId}, 0, 1000, TEXT(""));
    AddInfo(MovedBox);
    TestTrue(TEXT("Selected comment moves its contents"), MovedBox.StartsWith(TEXT("changed:")));
    TestEqual(TEXT("Comment moved"), Box->NodePosY, 900);
    TestEqual(TEXT("Contained node moved with comment"), First->NodePosY, 1100);
    TestEqual(TEXT("Outside selected comment stays fixed"), Second->NodePosY, 600);
    TestTrue(TEXT("Undo comment and contents together"), GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo restores comment"), Box->NodePosY, -100);
    TestEqual(TEXT("Undo restores contents"), First->NodePosY, 100);
    TestTrue(TEXT("Undo transfer"), GEditor->UndoTransaction(false));
    TestEqual(TEXT("Undo transfer restores X"), First->NodePosX, 0);
    TestEqual(TEXT("Undo transfer restores Y"), First->NodePosY, 600);
    TestTrue(TEXT("Links survive transfer and undo"), First->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(Second->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
    AddInfo(TEXT("Group movement capture: ") + UTurboMCPLookToolset::LookTurboMCPCapture(Asset, Graph->GetName(), TEXT("graph"), TEXT(""), TEXT("1600x900")));
    BP->ClearFlags(RF_Public | RF_Standalone);
    Package->SetDirtyFlag(false);
    return !HasAnyErrors();
}
#endif
