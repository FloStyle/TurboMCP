// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphTidy.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPTidyOverlapDiagnosticTest,
    "TurboMCP.GraphAppend.TidyOverlapDiagnostic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPTidyOverlapDiagnosticTest::RunTest(const FString& Parameters)
{
    // Exercise the exact final plan validator. No live graph or asset is needed
    // for its geometry predicate, and no visual layout quality is claimed here.
    FTurboMCPGraphTidy::FPlan Plan;
    FTurboMCPGraphTidy::FNode Left;
    Left.Key = TEXT("EventGraph/11111111111111111111111111111111");
    Left.X = -200; Left.PlannedX = 0; Left.Width = 100; Left.Height = 80;
    FTurboMCPGraphTidy::FNode Right;
    Right.Key = TEXT("EventGraph/22222222222222222222222222222222");
    Right.X = 50; Right.PlannedX = 50; Right.Width = 100; Right.Height = 80;
    Plan.Nodes = {Left, Right};
    FString Error;
    TestFalse(TEXT("Moved overlapping rectangles refuse"), FTurboMCPGraphTidy::ValidatePlannedNodeOverlaps(Plan, Error));
    TestTrue(TEXT("Both full IDs reported"), Error.Contains(Left.Key) && Error.Contains(Right.Key));
    TestTrue(TEXT("Left planned rectangle reported"), Error.Contains(TEXT("x=0.0 y=0.0 width=100.0 height=80.0")));
    TestTrue(TEXT("Right planned rectangle reported"), Error.Contains(TEXT("x=50.0 y=0.0 width=100.0 height=80.0")));
    TestTrue(TEXT("No mutation and bounded diagnosis explained"), Error.Contains(TEXT("nothing changed")) && Error.Contains(TEXT("first detected pair")));
    TestTrue(TEXT("Existing inspection tools named"), Error.Contains(TEXT("LookTurboMCPGraphPositions")) && Error.Contains(TEXT("LookTurboMCPLayoutReport")));
    TestEqual(TEXT("Original position preserved"), Plan.Nodes[0].X, -200.0);
    TestEqual(TEXT("Planned position preserved"), Plan.Nodes[0].PlannedX, 0.0);
    AddInfo(Error);
    Plan.Nodes[1].PlannedX = 100;
    TestTrue(TEXT("Touching edges are not overlap"), FTurboMCPGraphTidy::ValidatePlannedNodeOverlaps(Plan, Error));
    TestTrue(TEXT("Successful validation clears stale diagnostic"), Error.IsEmpty());
    Plan.Nodes[1].PlannedX = 50;
    Plan.Nodes[0].X = Plan.Nodes[0].PlannedX;
    TestTrue(TEXT("Unchanged preexisting overlap stays exempt"), FTurboMCPGraphTidy::ValidatePlannedNodeOverlaps(Plan, Error));
    Plan.Nodes[0].X = -200;
    Plan.Nodes[1].bIsComment = true;
    TestTrue(TEXT("Comment container is not a node collision"), FTurboMCPGraphTidy::ValidatePlannedNodeOverlaps(Plan, Error));
    return !HasAnyErrors();
}
#endif
