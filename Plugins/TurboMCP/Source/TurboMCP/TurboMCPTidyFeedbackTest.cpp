// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPGraphTidy.h"
#include "Editor.h"
#include "TurboMCPLayoutReport.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetSystemLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "UObject/Package.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPTidyFeedbackTest,
    "TurboMCP.GraphAppend.TidyFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPTidyFeedbackTest::RunTest(const FString& Parameters)
{
    // Box fitting must check section bounds, even when no node rectangles overlap.
    {
        FTurboMCPGraphTidy::FPlan Growth;
        Growth.SelectedNodeKeys.Add(TEXT("member"));
        FTurboMCPGraphTidy::FBox Section;
        Section.Key = TEXT("section");
        Section.Width = Section.PlannedWidth = 300;
        Section.Height = 200;
        Section.PlannedHeight = 400;
        Section.bWouldFit = true;
        Section.MemberKeys.Add(TEXT("member"));
        Growth.Boxes.Add(Section);
        FTurboMCPGraphTidy::FBox Neighbor;
        Neighbor.Key = TEXT("neighbor");
        Neighbor.Y = Neighbor.PlannedY = 300;
        Neighbor.Width = Neighbor.PlannedWidth = 300;
        Neighbor.Height = Neighbor.PlannedHeight = 100;
        Growth.Boxes.Add(Neighbor);
        FString GrowthError;
        TestFalse(TEXT("Section growth refuses a fixed box below"), FTurboMCPGraphTidy::ValidateScopedBoxGrowth(Growth, GrowthError));
        TestTrue(TEXT("Growth refusal identifies the blocking section"), GrowthError.Contains(TEXT("neighbor")));
        Growth.Boxes[0].PlannedHeight = 200;
        Growth.Boxes[0].PlannedWidth = 600;
        TestTrue(TEXT("Free rightward growth is valid"), FTurboMCPGraphTidy::ValidateScopedBoxGrowth(Growth, GrowthError));
        FTurboMCPGraphTidy::FNode Outside;
        Outside.Key = TEXT("outside");
        Outside.X = Outside.PlannedX = 450;
        Outside.Y = Outside.PlannedY = 50;
        Outside.Width = Outside.Height = 50;
        Growth.Nodes.Add(Outside);
        TestFalse(TEXT("Growth cannot acquire an unrelated node"), FTurboMCPGraphTidy::ValidateScopedBoxGrowth(Growth, GrowthError));
        TestTrue(TEXT("Growth refusal identifies the unrelated node"), GrowthError.Contains(TEXT("outside")));
        Growth.Nodes.Reset();
        Growth.Boxes[1].ParentIndex = 0;
        Growth.Boxes[0].PlannedHeight = 450;
        TestTrue(TEXT("A known nested section is not an unrelated obstacle"), FTurboMCPGraphTidy::ValidateScopedBoxGrowth(Growth, GrowthError));
        TestEqual(TEXT("Validation does not move the fixed section"), Growth.Boxes[1].PlannedY, 300.0);
        Growth.Boxes[1].ParentIndex = INDEX_NONE;
        Growth.Boxes[0].PlannedWidth = 300;
        Growth.Boxes[0].PlannedHeight = 500;
        Growth.Boxes[0].MemberKeys.Reset();
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FTurboMCPGraphTidy::FNode Member;
            Member.Key = FString::Printf(TEXT("member%d"), Index);
            Member.X = Member.PlannedX = 40 + (Index == 1 ? 100 : 0);
            Member.Y = Member.PlannedY = Index < 2 ? 82 : 350;
            Member.Width = Member.Height = 50;
            Growth.Nodes.Add(Member);
            Growth.Boxes[0].MemberKeys.Add(Member.Key);
            Growth.SelectedNodeKeys.Add(Member.Key);
        }
        FTurboMCPGraphTidy::FWire InternalWire;
        InternalWire.SourceNodeIndex = 0;
        InternalWire.TargetNodeIndex = 1;
        Growth.Wires.Add(InternalWire);
        TestTrue(TEXT("Independent components can use free rightward space"), FTurboMCPGraphTidy::TryPackScopedSection(Growth, 0, 120));
        TestEqual(TEXT("Connected component retains internal spacing"), Growth.Nodes[1].PlannedX - Growth.Nodes[0].PlannedX, 100.0);
        TestEqual(TEXT("Second component is lifted into the section"), Growth.Nodes[2].PlannedY, 82.0);
        TestTrue(TEXT("Second component is placed to the right"), Growth.Nodes[2].PlannedX > Growth.Nodes[1].PlannedX + 50);
        TestEqual(TEXT("Packing preserves execution/data edge count"), Growth.Wires.Num(), 1);
        TestEqual(TEXT("Packing preserves neighboring box position"), Growth.Boxes[1].PlannedY, 300.0);
        // A parent may use space beside a child without translating that child.
        Growth.Boxes[1].ParentIndex = 0;
        Growth.Boxes[1].X = Growth.Boxes[1].PlannedX = 40;
        Growth.Boxes[1].Y = Growth.Boxes[1].PlannedY = 82;
        Growth.Boxes[1].Width = Growth.Boxes[1].PlannedWidth = 200;
        Growth.Boxes[1].Height = Growth.Boxes[1].PlannedHeight = 100;
        Growth.Boxes[1].MemberKeys = { TEXT("member0"), TEXT("member1") };
        Growth.Boxes[0].ChildBoxIndices.Add(1);
        Growth.Boxes[0].Height = 240;
        Growth.Boxes[0].PlannedHeight = 500;
        Growth.Nodes[2].PlannedY = 350;
        const double ChildNodeX = Growth.Nodes[0].PlannedX;
        TestTrue(TEXT("Parent packs direct members beside a fixed nested section"), FTurboMCPGraphTidy::TryPackScopedSection(Growth, 0, 120));
        TestEqual(TEXT("Child member retains its position"), Growth.Nodes[0].PlannedX, ChildNodeX);
        TestEqual(TEXT("Child box retains its anchor"), Growth.Boxes[1].PlannedX, 40.0);
        TestTrue(TEXT("Direct member clears the child rectangle"), Growth.Nodes[2].PlannedX >= 360.0);
        TestEqual(TEXT("Parent no longer needs downward growth"), Growth.Boxes[0].PlannedHeight, 240.0);
        Growth.Boxes[0].bWouldFit = true;
        FTurboMCPGraphTidy::FWire ChildBoundary;
        ChildBoundary.SourceNodeIndex = 1;
        ChildBoundary.TargetNodeIndex = 2;
        Growth.Wires.Add(ChildBoundary);
        const double BeforeBoundaryX = Growth.Nodes[2].PlannedX;
        TestFalse(TEXT("Packing does not split a component across an anchored child"),
            FTurboMCPGraphTidy::TryPackScopedSection(Growth, 0, 120));
        TestEqual(TEXT("Child-boundary refusal preserves the candidate"), Growth.Nodes[2].PlannedX, BeforeBoundaryX);


    }
    UBlueprint* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (Macros == nullptr)
    {
        AddError(TEXT("The stock macro library could not be loaded; no fixture edits were attempted."));
        return false;
    }
    UPackage* Package=CreatePackage(*(TEXT("/Game/Tests/TidyFeedback_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),Package,TEXT("BP_Feedback"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"),BP)) return false;
    // Exercise the public planning/application path, not only synthetic rectangles.
    {
        UEdGraph* SectionGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("SectionGrowth"),
            UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        BP->UbergraphPages.Add(SectionGraph);
        SectionGraph->SetFlags(RF_Transactional);
        auto AddSectionNode = [&](UEdGraphNode* Node, int32 X, int32 Y)
        {
            SectionGraph->AddNode(Node, false, false);
            Node->SetFlags(RF_Transactional);
            Node->CreateNewGuid();
            Node->AllocateDefaultPins();
            Node->NodePosX = X;
            Node->NodePosY = Y;
        };
        auto* Section = NewObject<UEdGraphNode_Comment>(SectionGraph);
        AddSectionNode(Section, 0, 0);
        Section->NodeComment = TEXT("Selected independent flows");
        Section->NodeWidth = 1000;
        Section->NodeHeight = 700;
        auto* Fixed = NewObject<UEdGraphNode_Comment>(SectionGraph);
        AddSectionNode(Fixed, 0, 720);
        Fixed->NodeComment = TEXT("Fixed neighboring section");
        Fixed->NodeWidth = 1000;
        Fixed->NodeHeight = 200;
        TArray<FString> Selected;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            auto* Event = NewObject<UK2Node_CustomEvent>(SectionGraph);
            Event->CustomFunctionName = FName(*FString::Printf(TEXT("SectionEntry%d"), Index));
            AddSectionNode(Event, 60, 90 + Index * 190);
            auto* Call = NewObject<UK2Node_CallFunction>(SectionGraph);
            Call->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
            AddSectionNode(Call, 430, 90 + Index * 190);
            Event->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Call->FindPin(UEdGraphSchema_K2::PN_Execute));
            Selected.Add(TurboMCPNodeId::MakeForNode(Event));
            Selected.Add(TurboMCPNodeId::MakeForNode(Call));
        }
        TMap<UEdGraphNode*, FIntPoint> OriginalPositions;
        TMap<UEdGraphPin*, TArray<UEdGraphPin*>> OriginalLinks;
        for (UEdGraphNode* Node : SectionGraph->Nodes)
        {
            OriginalPositions.Add(Node, FIntPoint(Node->NodePosX, Node->NodePosY));
            for (UEdGraphPin* Pin : Node->Pins) if (Pin) OriginalLinks.Add(Pin, Pin->LinkedTo);
        }
        FTurboMCPGraphTidy::FPlan SectionPlan;
        FString SectionError;
        if (!TestTrue(TEXT("Blocked section plans rightward: ") + SectionError,
            FTurboMCPGraphTidy::Plan(BP->GetPathName(), SectionGraph->GetName(), true, false,
                Selected, SectionPlan, SectionError))) return false;
        for (const auto& Pair : OriginalPositions)
            TestTrue(TEXT("Section preview does not move nodes"), Pair.Value == FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY));
        const FString SectionEdit = FTurboMCPGraphTidy::Apply(SectionPlan);
        if (!TestTrue(TEXT("Section edit: ") + SectionEdit, SectionEdit.StartsWith(TEXT("changed:")))) return false;
        TestTrue(TEXT("Section uses rightward space"), Section->NodeWidth > 1000);
        TestTrue(TEXT("Section clears the fixed box below"), Section->NodeHeight <= Fixed->NodePosY);
        TestTrue(TEXT("Fixed box geometry is unchanged"), Fixed->NodePosX == 0 && Fixed->NodePosY == 720
            && Fixed->NodeWidth == 1000 && Fixed->NodeHeight == 200);
        for (const auto& Node : SectionPlan.Nodes)
            if (Node.Node && !Node.bIsComment)
                TestTrue(TEXT("Section apply matches preview"), Node.Node->NodePosX == Node.PlannedX && Node.Node->NodePosY == Node.PlannedY);
        for (const auto& Pair : OriginalLinks) TestTrue(TEXT("Section links unchanged"), Pair.Key->LinkedTo == Pair.Value);
        const FString Capture = UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), SectionGraph->GetName(),
            TEXT("graph"), TEXT(""), TEXT("1600x900"));
        AddInfo(Capture);
        TestTrue(TEXT("Section native capture produced"), Capture.Contains(TEXT("image:")));
        if (!TestTrue(TEXT("Section undo succeeds"), GEditor->UndoTransaction(false))) return false;
        for (const auto& Pair : OriginalPositions)
            TestTrue(TEXT("Section undo restores positions"), Pair.Value == FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY));
        TestTrue(TEXT("Section undo restores size"), Section->NodeWidth == 1000 && Section->NodeHeight == 700);
        // Both available growth directions are blocked. A refusal must preserve
        // the live graph rather than moving an unselected neighboring section.
        auto* RightObstacle = NewObject<UEdGraphNode_Comment>(SectionGraph);
        AddSectionNode(RightObstacle, 1020, 0);
        RightObstacle->NodeComment = TEXT("Fixed right obstacle");
        RightObstacle->NodeWidth = 2000;
        RightObstacle->NodeHeight = 700;
        const bool DirtyBeforeRefusal = Package->IsDirty();
        FTurboMCPGraphTidy::FPlan BlockedPlan;
        const bool BlockedResult = FTurboMCPGraphTidy::Plan(BP->GetPathName(), SectionGraph->GetName(),
            true, false, Selected, BlockedPlan, SectionError);
        TestFalse(TEXT("No space below or right refuses section growth"), BlockedResult);
        TestTrue(TEXT("Blocked section names the collision"), SectionError.Contains(TEXT("unrelated comment")));
        const FString BlockedEdit = UTurboMCPEditToolset::EditTurboMCPGraphTidy(
            BP->GetPathName(), SectionGraph->GetName(), true, Selected);
        TestTrue(TEXT("Blocked section edit refuses"), BlockedEdit.StartsWith(TEXT("error:")));
        for (const auto& Pair : OriginalPositions)
            TestTrue(TEXT("Blocked section preserves all positions"), Pair.Value == FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY));
        for (const auto& Pair : OriginalLinks)
            TestTrue(TEXT("Blocked section preserves all links"), Pair.Key->LinkedTo == Pair.Value);
        TestTrue(TEXT("Blocked section preserves box sizes"), Section->NodeWidth == 1000 && Section->NodeHeight == 700
            && Fixed->NodeWidth == 1000 && Fixed->NodeHeight == 200);
        TestTrue(TEXT("Right obstacle remains unchanged"), RightObstacle->NodePosX == 1020 && RightObstacle->NodePosY == 0
            && RightObstacle->NodeWidth == 2000 && RightObstacle->NodeHeight == 700);
        TestEqual(TEXT("Blocked section preserves dirty state"), Package->IsDirty(), DirtyBeforeRefusal);
    }
    for (const FName MacroName : {FName(TEXT("ForLoopWithBreak")),FName(TEXT("DoOnce"))})
    {
        UEdGraph* Definition=nullptr;
        for (UEdGraph* Candidate:Macros->MacroGraphs) if (Candidate && Candidate->GetFName()==MacroName) Definition=Candidate;
        if (!TestNotNull(TEXT("Stock macro definition"),Definition)) return false;
        UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,MacroName,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
        BP->UbergraphPages.Add(Graph);
        auto Init=[&](UEdGraphNode* Node,int32 X,int32 Y){Graph->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;};
        auto* Event=NewObject<UK2Node_CustomEvent>(Graph);Event->CustomFunctionName=FName(*(TEXT("Start")+MacroName.ToString()));Init(Event,0,0);
        auto* Macro=NewObject<UK2Node_MacroInstance>(Graph);Macro->SetMacroGraph(Definition);Init(Macro,600,200);
        auto* Body=NewObject<UK2Node_CallFunction>(Graph);Body->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));Init(Body,300,400);
        auto* First=NewObject<UK2Node_Knot>(Graph);Init(First,900,500);
        auto* Second=NewObject<UK2Node_Knot>(Graph);Init(Second,1000,600);
        auto Connect=[&](UEdGraphPin* A,UEdGraphPin* B){return TestTrue(TEXT("Fixture connection"),A && B && Graph->GetSchema()->TryCreateConnection(A,B));};
        UEdGraphPin* Input=nullptr;
        for (UEdGraphPin* Pin:Macro->Pins) if (Pin && Pin->Direction==EGPD_Input && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec && Pin->PinName!=TEXT("Break") && Pin->PinName!=TEXT("Reset")) {Input=Pin;break;}
        const bool Loop=MacroName==TEXT("ForLoopWithBreak");
        if (!Connect(Event->FindPin(UEdGraphSchema_K2::PN_Then),Input) ||
            !Connect(Macro->FindPin(Loop?TEXT("LoopBody"):TEXT("Completed")),Body->FindPin(UEdGraphSchema_K2::PN_Execute)) ||
            !Connect(Body->FindPin(UEdGraphSchema_K2::PN_Then),First->GetInputPin()) ||
            !Connect(First->GetOutputPin(),Second->GetInputPin()) ||
            !Connect(Second->GetOutputPin(),Macro->FindPin(Loop?TEXT("Break"):TEXT("Reset")))) return false;
        // Snapshot every pin, not only the feedback edge: tidy may move geometry but not behavior.
        TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
        TMap<UEdGraphPin*,FString> Defaults;
        TMap<UEdGraphNode*,FGuid> Identities;
        TMap<UEdGraphNode*,FIntPoint> Positions;
        for (UEdGraphNode* Node:Graph->Nodes) {Identities.Add(Node,Node->NodeGuid);Positions.Add(Node,FIntPoint(Node->NodePosX,Node->NodePosY));for (UEdGraphPin* Pin:Node->Pins) if (Pin) {Links.Add(Pin,Pin->LinkedTo);Defaults.Add(Pin,Pin->DefaultValue);}}
        const FString Preview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{});
        TestTrue(TEXT("Feedback preview succeeds"),Preview.Contains(TEXT("preview:")) && Preview.Contains(TEXT("control feedback: 1")));
        for (const auto& Pair:Positions) TestTrue(TEXT("Preview preserves positions"),Pair.Value==FIntPoint(Pair.Key->NodePosX,Pair.Key->NodePosY));
        const FString AutoPreview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{},TEXT("auto"));
        TestTrue(TEXT("Automatic main path accepts stock feedback"),AutoPreview.Contains(TEXT("preview:")));
        const FString Edit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Graph->GetName(),true,{});
        TestTrue(TEXT("Feedback edit succeeds"),Edit.StartsWith(TEXT("changed:")) && Edit.Contains(TEXT("control feedback: 1")));
        TestTrue(TEXT("Feedback leaves the body on its right"),First->NodePosX>Body->NodePosX);
        TestTrue(TEXT("Feedback returns from the macro left"),Second->NodePosX<Macro->NodePosX);
        TestTrue(TEXT("Feedback routed below the macro"),First->NodePosY>Macro->NodePosY+120 && Second->NodePosY==First->NodePosY);
        TestEqual(TEXT("Node count preserved"),Graph->Nodes.Num(),Identities.Num());
        for (const auto& Pair:Identities) TestEqual(TEXT("Identity preserved"),Pair.Key->NodeGuid,Pair.Value);
        for (const auto& Pair:Links) {TestTrue(TEXT("Links preserved"),Pair.Key->LinkedTo==Pair.Value);TestEqual(TEXT("Default preserved"),Pair.Key->DefaultValue,Defaults[Pair.Key]);}
        const FString Capture=UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(),Graph->GetName(),TEXT("graph"),TEXT(""),TEXT("1600x900"));
        TestTrue(TEXT("Native capture produced"),Capture.Contains(TEXT("renderer: native Slate")) && Capture.Contains(TEXT("image:")));AddInfo(Capture);
    }
    // An unrelated fixed cycle must not block an explicitly selected acyclic flow.
    UEdGraph* Scoped=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("ScopedCycle"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(Scoped);
    auto MakeCall=[&](int32 X,int32 Y){auto* Node=NewObject<UK2Node_CallFunction>(Scoped);Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));Scoped->AddNode(Node,false,false);Node->CreateNewGuid();Node->AllocateDefaultPins();Node->NodePosX=X;Node->NodePosY=Y;return Node;};
    auto* A=MakeCall(0,0);auto* B=MakeCall(-400,100);
    auto* C=MakeCall(0,1000);auto* D=MakeCall(500,1000);
    // Direct pin links deliberately model a pre-existing cycle, independent of editor connection guards.
    A->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(B->FindPin(UEdGraphSchema_K2::PN_Execute));
    C->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(D->FindPin(UEdGraphSchema_K2::PN_Execute));
    D->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(C->FindPin(UEdGraphSchema_K2::PN_Execute));
    const TArray<FString> Selection={TurboMCPNodeId::MakeForNode(A),TurboMCPNodeId::MakeForNode(B)};
    const FString ScopedPreview=UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,Selection,TEXT("auto"));
    TestTrue(TEXT("Fixed unrelated cycle allows selected preview"),ScopedPreview.Contains(TEXT("preview:")));
    const FString ScopedEdit=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,Selection,TEXT("auto"));
    TestTrue(TEXT("Fixed unrelated cycle allows selected edit"),ScopedEdit.StartsWith(TEXT("changed:")));
    TestTrue(TEXT("Fixed cycle positions preserved"),C->NodePosX==0 && C->NodePosY==1000 && D->NodePosX==500 && D->NodePosY==1000);
    const FIntPoint BeforeA(A->NodePosX,A->NodePosY),BeforeB(B->NodePosX,B->NodePosY);
    const bool DirtyBefore=Package->IsDirty();
    const FString Refused=UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,{});
    TestTrue(TEXT("Unsupported whole-graph cycle refuses"),Refused.StartsWith(TEXT("error:")));
    TestTrue(TEXT("Refusal preserves positions"),BeforeA==FIntPoint(A->NodePosX,A->NodePosY) && BeforeB==FIntPoint(B->NodePosX,B->NodePosY));
    TestEqual(TEXT("Refusal preserves dirty state"),Package->IsDirty(),DirtyBefore);
    TestTrue(TEXT("Cycle links preserved"),C->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(D->FindPin(UEdGraphSchema_K2::PN_Execute)) && D->FindPin(UEdGraphSchema_K2::PN_Then)->LinkedTo.Contains(C->FindPin(UEdGraphSchema_K2::PN_Execute)));
    // Data cycles are not control feedback, even when represented by reroutes.
    auto MakeDataKnot=[&](int32 X){auto* Knot=NewObject<UK2Node_Knot>(Scoped);Scoped->AddNode(Knot,false,false);Knot->CreateNewGuid();Knot->AllocateDefaultPins();Knot->NodePosX=X;Knot->NodePosY=2000;for (UEdGraphPin* Pin:Knot->Pins) {Pin->PinType.PinCategory=UEdGraphSchema_K2::PC_Int;}return Knot;};
    auto* DataA=MakeDataKnot(0);auto* DataB=MakeDataKnot(300);
    DataA->GetOutputPin()->MakeLinkTo(DataB->GetInputPin());DataB->GetOutputPin()->MakeLinkTo(DataA->GetInputPin());
    const TArray<FString> DataSelection={TurboMCPNodeId::MakeForNode(DataA),TurboMCPNodeId::MakeForNode(DataB)};
    TestTrue(TEXT("Data cycle preview refuses"),UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,DataSelection).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Data cycle edit refuses"),UTurboMCPEditToolset::EditTurboMCPGraphTidy(BP->GetPathName(),Scoped->GetName(),true,DataSelection).StartsWith(TEXT("error:")));
    TestTrue(TEXT("Data refusal preserves positions and links"),DataA->NodePosX==0 && DataB->NodePosX==300 && DataA->NodePosY==2000 && DataB->NodePosY==2000 && DataA->GetOutputPin()->LinkedTo.Contains(DataB->GetInputPin()) && DataB->GetOutputPin()->LinkedTo.Contains(DataA->GetInputPin()));
    // TD-76: each bypass and optional action enter the next repeat together.
    UEdGraph* WrapGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("OptionalRepeats"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    BP->UbergraphPages.Add(WrapGraph);
    WrapGraph->SetFlags(RF_Transactional);
    TArray<UK2Node_IfThenElse*> Branches;
    TArray<UK2Node_CallFunction*> Actions;
    auto InitWrap = [&](UEdGraphNode* Node, int32 X)
    { Node->SetFlags(RF_Transactional); WrapGraph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->AllocateDefaultPins(); Node->NodePosX = X; Node->NodePosY = 0; };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Branch = NewObject<UK2Node_IfThenElse>(WrapGraph); InitWrap(Branch, Index * 800); Branches.Add(Branch);
        auto* Action = NewObject<UK2Node_CallFunction>(WrapGraph);
        Action->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
        InitWrap(Action, Index * 800 + 350); Actions.Add(Action);
    }
    auto ConnectWrap = [&](UEdGraphPin* From, UEdGraphPin* To)
    { if (!From || !To) { AddError(TEXT("Optional-repeat fixture pin missing.")); return false; } From->MakeLinkTo(To); return true; };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!ConnectWrap(Branches[Index]->GetThenPin(), Actions[Index]->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
        if (Index < 2 && (!ConnectWrap(Branches[Index]->GetElsePin(), Branches[Index + 1]->GetExecPin())
            || !ConnectWrap(Actions[Index]->FindPin(UEdGraphSchema_K2::PN_Then), Branches[Index + 1]->GetExecPin()))) return false;
    }
    TMap<UEdGraphPin*, TArray<UEdGraphPin*>> WrapLinks;
    TMap<UEdGraphNode*, FIntPoint> WrapPositions;
    for (UEdGraphNode* Node : WrapGraph->Nodes)
    {
        WrapPositions.Add(Node, FIntPoint(Node->NodePosX, Node->NodePosY));
        for (UEdGraphPin* Pin : Node->Pins) if (Pin) WrapLinks.Add(Pin, Pin->LinkedTo);
    }
    const FString WrapIds = TurboMCPNodeId::MakeForNode(Actions[0]) + TEXT(",") + TurboMCPNodeId::MakeForNode(Actions[1]);
    FTurboMCPGraphTidy::FPlan WrapPlan; FString WrapError;
    const bool Planned = FTurboMCPGraphTidy::Plan(BP->GetPathName(), WrapGraph->GetName(), true, false, {}, WrapPlan, WrapError,
        FString(), 120.0, 100.0, 280.0, WrapIds);
    if (!TestTrue(TEXT("Optional repeats plan two wraps: ") + WrapError, Planned)) return false;
    TestEqual(TEXT("Two actions and two bypasses form four boundaries"), WrapPlan.WrapBoundaryEdges.Num(), 4);
    const FString WrapEdit = FTurboMCPGraphTidy::Apply(WrapPlan);
    if (!TestTrue(TEXT("Optional repeat edit: ") + WrapEdit, WrapEdit.StartsWith(TEXT("changed:")))) return false;
    for (int32 Index = 0; Index < 2; ++Index)
        TestTrue(TEXT("Next repeat is below both incoming paths"), Branches[Index + 1]->NodePosY > Actions[Index]->NodePosY
            && Branches[Index + 1]->NodePosY > Branches[Index]->NodePosY);
    for (const auto& Node : WrapPlan.Nodes)
        if (Node.Node && !Node.bIsComment) TestTrue(TEXT("Applied coordinates match plan"),
            Node.Node->NodePosX == Node.PlannedX && Node.Node->NodePosY == Node.PlannedY);
    for (const auto& Pair : WrapLinks) TestTrue(TEXT("All optional action links preserved"), Pair.Key->LinkedTo == Pair.Value);
    const FString WrapCapture = UTurboMCPLookToolset::LookTurboMCPCapture(BP->GetPathName(), WrapGraph->GetName(), TEXT("graph"), TEXT(""), TEXT("1600x900"));
    AddInfo(WrapCapture);
    TestTrue(TEXT("Optional repeat image captured"), WrapCapture.Contains(TEXT("image:")));
    if (!TestTrue(TEXT("Optional wrap undo"), GEditor->UndoTransaction(false))) return false;
    for (const auto& Pair : WrapPositions) TestTrue(TEXT("Optional wrap undo restores geometry"),
        Pair.Value == FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY));
    TArray<FString> Partial{TurboMCPNodeId::MakeForNode(Actions[0]), TurboMCPNodeId::MakeForNode(Branches[1])};
    FTurboMCPGraphTidy::FPlan PartialPlan;
    TestFalse(TEXT("Missing bypass source refuses"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), WrapGraph->GetName(), true, false,
        Partial, PartialPlan, WrapError, FString(), 120.0, 100.0, 280.0, TurboMCPNodeId::MakeForNode(Actions[0])));
    auto* DifferentExit = NewObject<UK2Node_CallFunction>(WrapGraph);
    DifferentExit->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
    InitWrap(DifferentExit, 3000);
    if (!ConnectWrap(Branches[0]->GetElsePin(), DifferentExit->FindPin(UEdGraphSchema_K2::PN_Execute))) return false;
    FTurboMCPGraphTidy::FPlan ConflictingPlan;
    TestFalse(TEXT("Conflicting optional-action exit refuses"), FTurboMCPGraphTidy::Plan(BP->GetPathName(), WrapGraph->GetName(), true, false,
        {}, ConflictingPlan, WrapError, FString(), 120.0, 100.0, 280.0, WrapIds));
    for (const auto& Pair : WrapPositions) TestTrue(TEXT("Refused boundaries preserve geometry"),
        Pair.Value == FIntPoint(Pair.Key->NodePosX, Pair.Key->NodePosY));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPTidyHandoffAxisTest,
    "TurboMCP.Layout.HandoffAxis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPTidyHandoffAxisTest::RunTest(const FString& Parameters)
{
    UBlueprint* Macros = LoadObject<UBlueprint>(nullptr,
        TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (Macros == nullptr)
    {
        AddError(TEXT("The stock macro library could not be loaded; the handoff fixture was not run."));
        return false;
    }
    UEdGraph* LoopDefinition = nullptr;
    for (UEdGraph* Candidate : Macros->MacroGraphs)
        if (Candidate && Candidate->GetFName() == TEXT("ForLoopWithBreak")) LoopDefinition = Candidate;
    if (!TestNotNull(TEXT("Stock loop definition"), LoopDefinition)) return false;

    UPackage* Package = CreatePackage(*(TEXT("/Game/Tests/TidyHandoffAxis_")
        + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Package->SetDirtyFlag(false); };
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package,
        TEXT("BP_HandoffAxis"), BPTYPE_Normal, UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass());
    if (!TestNotNull(TEXT("Fixture Blueprint"), BP)) return false;

    for (const bool bLoop : {false, true})
    {
        const FString CaseName = bLoop ? TEXT("Completed") : TEXT("False");
        UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, FName(*CaseName),
            UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        BP->UbergraphPages.Add(Graph);
        auto Init = [Graph](UEdGraphNode* Node, int32 X, int32 Y)
        {
            Graph->AddNode(Node, false, false);
            Node->CreateNewGuid();
            Node->AllocateDefaultPins();
            Node->NodePosX = X;
            Node->NodePosY = Y;
        };
        UEdGraphNode* Control = nullptr;
        UEdGraphPin* MainPin = nullptr;
        UEdGraphPin* LowerPin = nullptr;
        if (bLoop)
        {
            auto* Loop = NewObject<UK2Node_MacroInstance>(Graph);
            Loop->SetMacroGraph(LoopDefinition);
            Init(Loop, 0, 0);
            Control = Loop;
            MainPin = Loop->FindPin(TEXT("LoopBody"));
            LowerPin = Loop->FindPin(TEXT("Completed"));
        }
        else
        {
            auto* Branch = NewObject<UK2Node_IfThenElse>(Graph);
            Init(Branch, 0, 0);
            Control = Branch;
            MainPin = Branch->FindPin(TEXT("then"));
            LowerPin = Branch->FindPin(TEXT("else"));
        }
        if (!TestTrue(CaseName + TEXT(" control outputs exist"), MainPin && LowerPin)) return false;

        auto MakeCall = [Graph, &Init](int32 X, int32 Y)
        {
            auto* Node = NewObject<UK2Node_CallFunction>(Graph);
            Node->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));
            Init(Node, X, Y);
            return Node;
        };
        UK2Node_CallFunction* Main = MakeCall(550, 0);
        UK2Node_CallFunction* Lower = MakeCall(550, 500);
        auto* Knot = NewObject<UK2Node_Knot>(Graph);
        Init(Knot, 300, 500);
        if (!TestTrue(CaseName + TEXT(" reroute pins exist"), Knot->GetInputPin() && Knot->GetOutputPin())) return false;
        MainPin->MakeLinkTo(Main->FindPin(UEdGraphSchema_K2::PN_Execute));
        LowerPin->MakeLinkTo(Knot->GetInputPin());
        Knot->GetOutputPin()->MakeLinkTo(Lower->FindPin(UEdGraphSchema_K2::PN_Execute));

        // Match the reader's estimated pin X exactly. Moving only this knot
        // sideways then creates a deliberately diagonal control-to-knot edge.
        const double DeltaX = FTurboMCPLayoutReport::PinPosition(LowerPin).X
            - FTurboMCPLayoutReport::PinPosition(Knot->GetInputPin()).X;
        Knot->NodePosX += FMath::RoundToInt(DeltaX);
        const FString ControlId = TurboMCPNodeId::MakeForNode(Control);
        const FString KnotId = TurboMCPNodeId::MakeForNode(Knot);
        auto FindControlEdge = [&](const FTurboMCPLayoutReport::FReport& Report)
            -> const FTurboMCPLayoutReport::FFinding*
        {
            for (const auto& Finding : Report.Findings)
                if (Finding.Kind == TEXT("exec-not-straight")
                    && Finding.NodeIds.Contains(ControlId) && Finding.NodeIds.Contains(KnotId))
                    return &Finding;
            return nullptr;
        };
        const auto Aligned = FTurboMCPLayoutReport::Measure(Graph, BP->GetPathName());
        const auto* AlignedEdge = FindControlEdge(Aligned);
        if (!TestNotNull(CaseName + TEXT(" aligned edge reported"), AlignedEdge)) return false;
        TestEqual(CaseName + TEXT(" aligned handoff classification"), AlignedEdge->Classification, FString(TEXT("intended")));
        TestEqual(CaseName + TEXT(" aligned handoff rule"), AlignedEdge->Rule,
            FString(bLoop ? TEXT("R3") : TEXT("R6")));

        Knot->NodePosX += 240;
        const auto Diagonal = FTurboMCPLayoutReport::Measure(Graph, BP->GetPathName());
        const auto* DiagonalEdge = FindControlEdge(Diagonal);
        if (!TestNotNull(CaseName + TEXT(" diagonal edge reported"), DiagonalEdge)) return false;
        TestEqual(CaseName + TEXT(" diagonal remains a defect candidate"),
            DiagonalEdge->Classification, FString(TEXT("defect")));
        TestTrue(CaseName + TEXT(" diagonal has no intended rule"), DiagonalEdge->Rule.IsEmpty());
    }
    return !HasAnyErrors();
}
#endif
