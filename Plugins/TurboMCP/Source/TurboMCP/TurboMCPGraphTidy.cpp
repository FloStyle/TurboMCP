// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphTidy.h"
#include "TurboMCPGraphScratch.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#include "BlueprintEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "CoreGlobals.h"
#include "Templates/UnrealTemplate.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "GraphEditorSettings.h"
#include "Fonts/FontMeasure.h"
#include "Styling/AppStyle.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Self.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Guid.h"
#include "Slate/WidgetRenderer.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SGraphPin.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Function.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPComment.h"
#include "TurboMCPLayoutReport.h"
#include "TurboMCPNodeId.h"

namespace
{
    // Only known stock control inputs are feedback. A user pin called Reset or
    // an arbitrary execution/data cycle must not silently become a layout hint.
    bool IsControlFeedbackInput(const UEdGraphPin* Pin)
    {
        if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory != TEXT("exec")) return false;
        const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Pin->GetOwningNode());
        const UEdGraph* Definition = Macro ? Macro->GetMacroGraph() : nullptr;
        if (!Definition || Definition->GetOutermost()->GetName() != TEXT("/Engine/EditorBlueprintResources/StandardMacros")) return false;
        const FString Name = Definition->GetName();
        return (Pin->PinName == TEXT("Break") && (Name == TEXT("ForLoopWithBreak") || Name == TEXT("ForEachLoopWithBreak")))
            || (Pin->PinName == TEXT("Reset") && Name == TEXT("DoOnce"));
    }

    constexpr double GetterClearance = 16.0; // Local readers need space, not a whole execution lane.

    // Keep value readers below the incoming exec wire, not below the consumer body.
    // Input pins define the incoming corridor; entry nodes fall back to their first output.
    double ValueReaderFloor(UEdGraphNode* Node, double PlannedY)
    {
        if (!Node) return PlannedY;
        for (const UEdGraphPin* Pin : Node->Pins)
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == TEXT("exec"))
                return PlannedY + FTurboMCPGraphTidy::AlignmentPinPosition(Pin).Y - Node->NodePosY + 8.0;
        for (const UEdGraphPin* Pin : Node->Pins)
            if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == TEXT("exec"))
                return PlannedY + FTurboMCPGraphTidy::AlignmentPinPosition(Pin).Y - Node->NodePosY + 8.0;
        return PlannedY;
    }
    struct FStructuralQuality
    {
        int32 Overlaps=0;
        int32 UnwrappedBreaks=0;
        double BodyRowDrift=0.0;
        int32 CompletedAboveBody=0;
        int32 BackwardExec=0;
        double LinearExecDrift=0;
        int32 PureInputRowConflicts=0;
        int32 BackwardData=0;
        int32 FanOutRowConflicts=0;
        double InputPinDrift=0;
        bool bHasBlockMeasures = false;
        double BlockMeasures[7] = {};
    };
    static const TCHAR* TidyBlockMeasureNames[] = {
        TEXT("unrelated box crossings"), TEXT("execution segment crossings"),
        TEXT("diagonal execution length"), TEXT("within-block row breaks"),
        TEXT("data segment crossings"), TEXT("total segment length"), TEXT("bounding area")
    };

    // Return the physical wire segments of one logical connection. The gap
    // inside a reroute is node geometry, not wire length. Actual measurement
    // requires the caller's created-node mapping, never planned coordinates.
    TArray<TPair<FVector2D,FVector2D>> TidyWireSegments(
        const FTurboMCPGraphTidy::FPlan& Plan, int32 WireIndex, int32 Mode,
        const FVector2D& Start, const FVector2D& End)
    {
        TArray<TPair<FVector2D,FVector2D>> Segments;
        FVector2D Previous=Start;
        if (Mode!=0)
            for (const auto& Route:Plan.WireRoutes)
            {
                if (Route.WireIndex!=WireIndex || Route.bSkipped) continue;
                for (int32 Index=0;Index<Route.KnotPositions.Num();++Index)
                {
                    FVector2D Input,Output;
                    if (Mode==2)
                    {
                        // The live apply path must reject incomplete mappings
                        // before asking for a post-edit quality measurement.
                        if (!Route.AppliedKnots.IsValidIndex(Index) || !Route.AppliedKnots[Index])
                            return {};
                        Input=FTurboMCPGraphTidy::AlignmentPinPosition(Route.AppliedKnots[Index]->GetInputPin());
                        Output=FTurboMCPGraphTidy::AlignmentPinPosition(Route.AppliedKnots[Index]->GetOutputPin());
                    }
                    else
                    {
                        Input=Route.KnotPositions[Index]+Route.InputOffset;
                        Output=Route.KnotPositions[Index]+Route.OutputOffset;
                    }
                    Segments.Emplace(Previous,Input); Previous=Output;
                }
                break;
            }
        Segments.Emplace(Previous,End);
        return Segments;
    }

    // Appendix H uses straight pin-to-pin segments, not rendered Slate splines.
    // Keep the same block membership in before/after measurements; geometry
    // changes must not redefine a crossing or row break out of existence.
    void MeasureCompactGeometry(const FTurboMCPGraphTidy::FPlan& Plan, int32 Mode,
        const TArray<FTurboMCPGraphTidy::FGetterCopy>& Copies, FStructuralQuality& Quality)
    {
        if (!Plan.bCompactBlocks) return;
        auto PinPosition=[&](const UEdGraphPin* Pin)
        {
            return FTurboMCPGraphTidy::AlignmentPinPosition(Pin);
        };
        Quality.bHasBlockMeasures = true;
        auto Position = [Mode](const FTurboMCPGraphTidy::FNode& Node)
        {
            return Mode == 0 ? FVector2D(Node.X, Node.Y)
                : Mode == 2 && Node.Node ? FVector2D(Node.Node->NodePosX, Node.Node->NodePosY)
                : FVector2D(Node.PlannedX, Node.PlannedY);
        };
        struct FSegment { FVector2D A, B; int32 Source, Target; bool bExec; };
        TArray<FSegment> Segments;
        double Left = TNumericLimits<double>::Max(), Top = Left, Right = -Left, Bottom = -Left;
        auto Include = [&](double X, double Y, double Width, double Height)
        {
            Left = FMath::Min(Left, X); Top = FMath::Min(Top, Y);
            Right = FMath::Max(Right, X + Width); Bottom = FMath::Max(Bottom, Y + Height);
        };
        for (const auto& Node : Plan.Nodes)
            if (!Node.bIsComment) { const FVector2D P = Position(Node); Include(P.X, P.Y, Node.Width, Node.Height); }
        for (const auto& Box : Plan.Boxes)
            if (Mode != 0 || !Box.bGenerated) Include(Mode == 0 ? Box.X : Box.PlannedX, Mode == 0 ? Box.Y : Box.PlannedY,
                Mode == 0 ? Box.Width : Box.PlannedWidth, Mode == 0 ? Box.Height : Box.PlannedHeight);
        if (Mode != 0)
            for (const auto& Copy : Copies)
            {
                if (Mode == 2 && !Copy.Copy) continue;
                Include(Mode == 2 ? Copy.Copy->NodePosX : Copy.PlannedX,
                    Mode == 2 ? Copy.Copy->NodePosY : Copy.PlannedY, Copy.Width, Copy.Height);
            }
        if (Mode!=0)
            for (const auto& Route:Plan.WireRoutes)
                if (!Route.bSkipped)
                    for (int32 Index=0;Index<Route.KnotPositions.Num();++Index)
                    {
                        if (Mode==2 && (!Route.AppliedKnots.IsValidIndex(Index) || !Route.AppliedKnots[Index])) continue;
                        const FVector2D P=Mode==2 ? FVector2D(Route.AppliedKnots[Index]->NodePosX,Route.AppliedKnots[Index]->NodePosY) : Route.KnotPositions[Index];
                        Include(P.X,P.Y,Route.KnotSize.X,Route.KnotSize.Y);
                    }
        if (Left != TNumericLimits<double>::Max()) Quality.BlockMeasures[6] = (Right - Left) * (Bottom - Top);
        auto Cross = [](const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; };
        auto Intersects = [&](const FSegment& L, const FSegment& R)
        {
            // Shared endpoints and collinear runs are not proper crossings.
            const double A = Cross(L.B-L.A, R.A-L.A), B = Cross(L.B-L.A, R.B-L.A);
            const double C = Cross(R.B-R.A, L.A-R.A), D = Cross(R.B-R.A, L.B-R.A);
            return ((A > 0.001 && B < -0.001) || (A < -0.001 && B > 0.001))
                && ((C > 0.001 && D < -0.001) || (C < -0.001 && D > 0.001));
        };
        for (int32 WireIndex=0;WireIndex<Plan.Wires.Num();++WireIndex)
        {
            const auto& Wire=Plan.Wires[WireIndex];
            if (!Wire.OutputPin || !Wire.InputPin || !Plan.Nodes.IsValidIndex(Wire.SourceNodeIndex)
                || !Plan.Nodes.IsValidIndex(Wire.TargetNodeIndex)) continue;
            const auto& Source = Plan.Nodes[Wire.SourceNodeIndex];
            const auto& Target = Plan.Nodes[Wire.TargetNodeIndex];
            if (!Source.Node || !Target.Node) continue;
            FVector2D A = PinPosition(Wire.OutputPin) + Position(Source)
                - FVector2D(Source.Node->NodePosX, Source.Node->NodePosY);
            const FVector2D B = PinPosition(Wire.InputPin) + Position(Target)
                - FVector2D(Target.Node->NodePosX, Target.Node->NodePosY);
            bool bCopiedSource=false;
            if (Mode != 0)
                for (const auto& Copy : Copies)
                    if (Copy.OriginalValuePin == Wire.OutputPin && Copy.ConsumerPin == Wire.InputPin
                        && (Mode != 2 || Copy.Copy))
                    { bCopiedSource=true; A = FVector2D(Mode == 2 ? Copy.Copy->NodePosX : Copy.PlannedX,
                            Mode == 2 ? Copy.Copy->NodePosY : Copy.PlannedY) + Copy.OutputOffset; }
            const bool bExec = Wire.OutputPin->PinType.PinCategory == TEXT("exec");
            // Row membership is a property of the logical edge, even when its
            // visible connection is split into multiple reroute segments.
            if (bExec && !Wire.bControlFeedback && Source.BlockIndex != INDEX_NONE
                && Source.BlockIndex == Target.BlockIndex && FMath::Abs(B.Y-A.Y) > 16.0) ++Quality.BlockMeasures[3];
            TSet<const FTurboMCPGraphTidy::FBox*> CrossedBoxes;
            for (const auto& Endpoints:TidyWireSegments(Plan,WireIndex,Mode,A,B))
            {
                const FVector2D SegmentStart=Endpoints.Key, SegmentEnd=Endpoints.Value;
                const double Length=FVector2D::Distance(SegmentStart,SegmentEnd);
                Quality.BlockMeasures[5]+=Length;
                const double DX=FMath::Abs(SegmentEnd.X-SegmentStart.X), DY=FMath::Abs(SegmentEnd.Y-SegmentStart.Y);
                const double TanTenDegrees=0.176326980708465;
                if (bExec && DY>DX*TanTenDegrees && DX>DY*TanTenDegrees) Quality.BlockMeasures[2]+=Length;
                const FSegment Segment{SegmentStart,SegmentEnd,Wire.SourceNodeIndex,Wire.TargetNodeIndex,bExec};
                for (const auto& Other:Segments)
                    if (Other.bExec==bExec && Intersects(Segment,Other)) ++Quality.BlockMeasures[bExec?1:4];
                Segments.Add(Segment);
                for (const auto& Box : Plan.Boxes)
                {
                    if (Mode == 0 && Box.bGenerated) continue;
                    auto Contains = [&](const FTurboMCPGraphTidy::FNode& Node)
                    { return Box.MemberKeys.Contains(Node.Key) || (Node.Node && Box.MemberKeys.Contains(Node.Node->GetName())); };
                    if (Contains(bCopiedSource?Target:Source) || Contains(Target)) continue;
                    const double X = Mode == 0 ? Box.X : Box.PlannedX, Y = Mode == 0 ? Box.Y : Box.PlannedY;
                    const double Width = Mode == 0 ? Box.Width : Box.PlannedWidth, Height = Mode == 0 ? Box.Height : Box.PlannedHeight;
                    // Slab clipping counts a passage through the interior, including
                    // a segment wholly inside an unrelated box. Touching an edge is not a crossing.
                    double Enter = 0.0, Leave = 1.0;
                    auto Clip = [&](double Start, double Delta, double Min, double Max)
                    {
                        if (FMath::Abs(Delta) < 0.000001) return Start > Min && Start < Max;
                        double T0 = (Min-Start)/Delta, T1 = (Max-Start)/Delta;
                        if (T0 > T1) Swap(T0, T1);
                        Enter = FMath::Max(Enter, T0); Leave = FMath::Min(Leave, T1);
                        return Enter < Leave;
                    };
                    if (Length > 0.001 && Clip(SegmentStart.X, SegmentEnd.X-SegmentStart.X, X+0.001, X+Width-0.001)
                        && Clip(SegmentStart.Y, SegmentEnd.Y-SegmentStart.Y, Y+0.001, Y+Height-0.001)) CrossedBoxes.Add(&Box);
                }
            }
            Quality.BlockMeasures[0]+=CrossedBoxes.Num();
        }
    }

    FString CompactMeasureSummary(const FStructuralQuality& Before, const FStructuralQuality& After)
    {
        if (!Before.bHasBlockMeasures || !After.bHasBlockMeasures) return FString();
        FString Text = TEXT("compact score: native pin endpoints and straight segments; crossings exclude shared endpoints and collinear runs; data crossings count data/data pairs. Block identities are held fixed.\n");
        for (int32 Index = 0; Index < 7; ++Index)
        {
            const double Old = Before.BlockMeasures[Index], New = After.BlockMeasures[Index];
            const bool bRegression = New > Old * 1.10 + 0.000001;
            Text += FString::Printf(TEXT("%s: %.1f -> %.1f%s\n"), TidyBlockMeasureNames[Index], Old, New,
                bRegression ? TEXT("; regression >10% (zero baseline: any increase)") : TEXT(""));
        }
        Text += TEXT("ranking: listed order; differences below 5% or below 1 for counts are ties. Any >10% regression refuses the edit unless Force is explicit. These measures do not establish visual acceptance.");
        return Text;
    }

    // Export the measured geometry so the harness can independently recompute
    // Appendix H without copying this translation unit's scoring arithmetic.
    FString CompactGeometryEvidence(const FTurboMCPGraphTidy::FPlan& Plan,int32 Mode,
        const TArray<FTurboMCPGraphTidy::FGetterCopy>& Copies)
    {
        if (!Plan.bCompactBlocks) return FString();
        const TCHAR* Side=Mode==0?TEXT("before"):TEXT("after"); FString Text;
        auto Position=[Mode](const FTurboMCPGraphTidy::FNode& N){return Mode==0?FVector2D(N.X,N.Y)
            :Mode==2 && N.Node?FVector2D(N.Node->NodePosX,N.Node->NodePosY):FVector2D(N.PlannedX,N.PlannedY);};
        auto PinPosition=[&](const UEdGraphPin* Pin){return FTurboMCPGraphTidy::AlignmentPinPosition(Pin);};
        auto Rect=[&](double X,double Y,double W,double H)
        { Text+=FString::Printf(TEXT("score rect side=%s x=%.6f y=%.6f w=%.6f h=%.6f\n"),Side,X,Y,W,H); };
        for (const auto& N:Plan.Nodes) if (!N.bIsComment) { const FVector2D P=Position(N); Rect(P.X,P.Y,N.Width,N.Height); }
        for (int32 I=0;I<Plan.Boxes.Num();++I)
        {
            const auto& Box=Plan.Boxes[I]; if (Mode==0 && Box.bGenerated) continue;
            const double X=Mode==0?Box.X:Box.PlannedX,Y=Mode==0?Box.Y:Box.PlannedY,W=Mode==0?Box.Width:Box.PlannedWidth,H=Mode==0?Box.Height:Box.PlannedHeight;
            Rect(X,Y,W,H); TArray<FString> Owned;
            for (int32 WireIndex=0;WireIndex<Plan.Wires.Num();++WireIndex)
            {
                const auto& Wire=Plan.Wires[WireIndex];
                if (!Plan.Nodes.IsValidIndex(Wire.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(Wire.TargetNodeIndex)) continue;
                auto Contains=[&](const auto& N){return Box.MemberKeys.Contains(N.Key)||(N.Node && Box.MemberKeys.Contains(N.Node->GetName()));};
                bool Copied=false;
                if (Mode!=0) for (const auto& Copy:Copies) if (Copy.OriginalValuePin==Wire.OutputPin && Copy.ConsumerPin==Wire.InputPin && (Mode!=2 || Copy.Copy)) { Copied=true; break; }
                if (Contains(Plan.Nodes[Copied?Wire.TargetNodeIndex:Wire.SourceNodeIndex]) || Contains(Plan.Nodes[Wire.TargetNodeIndex])) Owned.Add(FString::FromInt(WireIndex));
            }
            Text+=FString::Printf(TEXT("score box side=%s index=%d x=%.6f y=%.6f w=%.6f h=%.6f owned=%s\n"),Side,I,X,Y,W,H,*FString::Join(Owned,TEXT(",")));
        }
        if (Mode!=0)
        {
            for (const auto& Copy:Copies) if (Mode!=2 || Copy.Copy)
                Rect(Mode==2?Copy.Copy->NodePosX:Copy.PlannedX,Mode==2?Copy.Copy->NodePosY:Copy.PlannedY,Copy.Width,Copy.Height);
            for (const auto& Route:Plan.WireRoutes) if (!Route.bSkipped)
                for (int32 I=0;I<Route.KnotPositions.Num();++I)
                {
                    if (Mode==2 && (!Route.AppliedKnots.IsValidIndex(I)||!Route.AppliedKnots[I])) continue;
                    const FVector2D P=Mode==2?FVector2D(Route.AppliedKnots[I]->NodePosX,Route.AppliedKnots[I]->NodePosY):Route.KnotPositions[I];
                    Rect(P.X,P.Y,Route.KnotSize.X,Route.KnotSize.Y);
                }
        }
        for (int32 I=0;I<Plan.Wires.Num();++I)
        {
            const auto& Wire=Plan.Wires[I];
            if (!Wire.OutputPin || !Wire.InputPin || !Plan.Nodes.IsValidIndex(Wire.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(Wire.TargetNodeIndex)) continue;
            const auto& Source=Plan.Nodes[Wire.SourceNodeIndex]; const auto& Target=Plan.Nodes[Wire.TargetNodeIndex];
            if (!Source.Node || !Target.Node) continue;
            FVector2D A=PinPosition(Wire.OutputPin)+Position(Source)-FVector2D(Source.Node->NodePosX,Source.Node->NodePosY);
            const FVector2D B=PinPosition(Wire.InputPin)+Position(Target)-FVector2D(Target.Node->NodePosX,Target.Node->NodePosY);
            if (Mode!=0) for (const auto& Copy:Copies)
                if (Copy.OriginalValuePin==Wire.OutputPin && Copy.ConsumerPin==Wire.InputPin && (Mode!=2 || Copy.Copy))
                    A=FVector2D(Mode==2?Copy.Copy->NodePosX:Copy.PlannedX,Mode==2?Copy.Copy->NodePosY:Copy.PlannedY)+Copy.OutputOffset;
            const bool Exec=Wire.OutputPin->PinType.PinCategory==TEXT("exec");
            const bool Row=Exec && !Wire.bControlFeedback && Source.BlockIndex!=INDEX_NONE && Source.BlockIndex==Target.BlockIndex;
            Text+=FString::Printf(TEXT("score edge side=%s wire=%d row=%d ax=%.6f ay=%.6f bx=%.6f by=%.6f source=%s target=%s pin=%s\n"),Side,I,Row?1:0,A.X,A.Y,B.X,B.Y,*Source.Key,*Target.Key,*Wire.OutputPin->PinName.ToString().Replace(TEXT(" "),TEXT("_")));
            for (const auto& Segment:TidyWireSegments(Plan,I,Mode,A,B))
                Text+=FString::Printf(TEXT("score segment side=%s wire=%d exec=%d ax=%.6f ay=%.6f bx=%.6f by=%.6f\n"),Side,I,Exec?1:0,Segment.Key.X,Segment.Key.Y,Segment.Value.X,Segment.Value.Y);
        }
        return Text;
    }

    FString GeometryEvidencePage(const FTurboMCPGraphTidy::FPlan& Plan, int32 Offset, int32 Limit)
    {
        if (!Plan.bCompactBlocks) return TEXT("geometry evidence: available only in blocks mode.");
        if (Limit == 0) return TEXT("geometry evidence: omitted by default. Request GeometryOffset=0 and GeometryLimit=40 for the first measured page.");

        TArray<FString> Rows;
        CompactGeometryEvidence(Plan, 0, Plan.GetterCopies).ParseIntoArray(Rows, TEXT("\n"), true);
        TArray<FString> AfterRows;
        CompactGeometryEvidence(Plan, 1, Plan.GetterCopies).ParseIntoArray(AfterRows, TEXT("\n"), true);
        Rows.Append(AfterRows);

        const int32 Total = Rows.Num();
        if (Offset >= Total)
            return FString::Printf(TEXT("geometry evidence: offset %d is outside %d measured row(s); no rows returned."), Offset, Total);
        const int32 End = Offset + FMath::Min(Limit, Total - Offset);
        TArray<FString> Page;
        Page.Reserve(End - Offset + 2);
        Page.Add(FString::Printf(TEXT("geometry evidence: rows %d..%d of %d for this plan."), Offset, End - 1, Total));
        for (int32 Index = Offset; Index < End; ++Index) Page.Add(Rows[Index]);
        if (End < Total)
            Page.Add(FString::Printf(TEXT("geometry evidence next: GeometryOffset=%d GeometryLimit=%d. Replan the current graph; pages from different graph states cannot be combined."), End, Limit));
        return FString::Join(Page, TEXT("\n"));
    }

    // Mode: original, planned, or actual post-edit coordinates. Pin offsets remain native.
    FStructuralQuality StructuralQuality(const FTurboMCPGraphTidy::FPlan& Plan,int32 Mode,
        const TArray<FTurboMCPGraphTidy::FGetterCopy>* ActualCopies = nullptr)
    {
        FStructuralQuality Q;
        auto Position=[Mode](const FTurboMCPGraphTidy::FNode& N)
        {
            if(Mode==2 && N.Node) return FVector2D(N.Node->NodePosX,N.Node->NodePosY);
            return Mode==1?FVector2D(N.PlannedX,N.PlannedY):FVector2D(N.X,N.Y);
        };
        struct FQualityRect { FVector2D Position; double Width; double Height; };
        TArray<FQualityRect> Rectangles;
        for (const auto& Node : Plan.Nodes)
            if (!Node.bIsComment) Rectangles.Add({Position(Node), Node.Width, Node.Height});
        const auto& Copies = ActualCopies ? *ActualCopies : Plan.GetterCopies;
        auto CopyPosition = [Mode](const FTurboMCPGraphTidy::FGetterCopy& Copy)
        {
            return Mode == 2 && Copy.Copy
                ? FVector2D(Copy.Copy->NodePosX, Copy.Copy->NodePosY)
                : FVector2D(Copy.PlannedX, Copy.PlannedY);
        };
        if (Mode != 0)
            for (const auto& Copy : Copies)
            {
                if (Mode == 2 && !Copy.Copy) continue;
                // Each reader carries its measured outline; an entry is not its getter.
                Rectangles.Add({CopyPosition(Copy), Copy.Width, Copy.Height});
            }
        if (Mode!=0)
            for (const auto& Route:Plan.WireRoutes)
                if (!Route.bSkipped)
                    for (int32 Index=0;Index<Route.KnotPositions.Num();++Index)
                    {
                        if (Mode==2 && (!Route.AppliedKnots.IsValidIndex(Index) || !Route.AppliedKnots[Index])) continue;
                        const FVector2D P=Mode==2 ? FVector2D(Route.AppliedKnots[Index]->NodePosX,Route.AppliedKnots[Index]->NodePosY) : Route.KnotPositions[Index];
                        Rectangles.Add({P,Route.KnotSize.X,Route.KnotSize.Y});
                    }
        for (int32 A = 0; A < Rectangles.Num(); ++A)
            for (int32 B = A + 1; B < Rectangles.Num(); ++B)
            {
                const auto& L = Rectangles[A]; const auto& R = Rectangles[B];
                if (L.Position.X < R.Position.X + R.Width && L.Position.X + L.Width > R.Position.X
                    && L.Position.Y < R.Position.Y + R.Height && L.Position.Y + L.Height > R.Position.Y)
                    ++Q.Overlaps;
            }
        // Data readers sit beside the consumer, below its incoming exec wire.
        for (const auto& DataWire : Plan.Wires)
        {
            if (!DataWire.OutputPin || !DataWire.InputPin || DataWire.OutputPin->PinType.PinCategory == TEXT("exec")
                || !Plan.Nodes.IsValidIndex(DataWire.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(DataWire.TargetNodeIndex)) continue;
            const auto& Source = Plan.Nodes[DataWire.SourceNodeIndex];
            const auto& Consumer = Plan.Nodes[DataWire.TargetNodeIndex];
            if(!Source.Node || !Consumer.Node) continue;
            FVector2D EffectiveSource = Position(Source);
            bool IsValueReader=Source.bIsPure;
            FVector2D OutputPosition=FTurboMCPGraphTidy::AlignmentPinPosition(DataWire.OutputPin)
                +EffectiveSource-FVector2D(Source.Node->NodePosX,Source.Node->NodePosY);
            if (Mode != 0)
                for (const auto& Copy : Copies)
                    if (Copy.OriginalValuePin == DataWire.OutputPin && Copy.ConsumerPin == DataWire.InputPin
                        && (Mode != 2 || Copy.Copy))
                    { EffectiveSource = CopyPosition(Copy); OutputPosition=EffectiveSource+Copy.OutputOffset; IsValueReader=true; break; }
            if (IsValueReader && !Source.bIsKnot && !Consumer.bIsPure && !Consumer.bIsComment
                && EffectiveSource.Y < ValueReaderFloor(Consumer.Node, Position(Consumer).Y))
                ++Q.PureInputRowConflicts;
            const FVector2D InputPosition=FTurboMCPGraphTidy::AlignmentPinPosition(DataWire.InputPin)
                +Position(Consumer)-FVector2D(Consumer.Node->NodePosX,Consumer.Node->NodePosY);
            if(InputPosition.X<OutputPosition.X) ++Q.BackwardData;
            if(IsValueReader && !Source.bIsKnot)
                Q.InputPinDrift+=FMath::Max(0.0,FMath::Abs(InputPosition.Y-OutputPosition.Y)-8.0);
        }
        for(const auto& W:Plan.Wires)
        {
            if(W.bControlFeedback || !W.OutputPin || !W.InputPin || W.OutputPin->PinType.PinCategory!=TEXT("exec") || !Plan.Nodes.IsValidIndex(W.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(W.TargetNodeIndex)) continue;
            const auto& S=Plan.Nodes[W.SourceNodeIndex]; const auto& T=Plan.Nodes[W.TargetNodeIndex];
            if(!S.Node || !T.Node) continue;
            const FVector2D Start=FTurboMCPGraphTidy::AlignmentPinPosition(W.OutputPin)+Position(S)-FVector2D(S.Node->NodePosX,S.Node->NodePosY);
            const FVector2D End=FTurboMCPGraphTidy::AlignmentPinPosition(W.InputPin)+Position(T)-FVector2D(T.Node->NodePosX,T.Node->NodePosY);
            if(Plan.IsWrapBoundary(W.SourceNodeIndex, W.TargetNodeIndex))
            {
                if(Position(T).Y<=Position(S).Y+S.Height) ++Q.UnwrappedBreaks;
                continue; // A requested return wire is neither a straight lane nor a backward-flow defect.
            }
            if(End.X<Start.X) ++Q.BackwardExec;
            int32 Outputs=0;
            for(const UEdGraphPin* P:S.Node->Pins) if(P && P->Direction==EGPD_Output && P->PinType.PinCategory==TEXT("exec")) ++Outputs;
            // Branch/Sequence fan-out and merge inputs deliberately occupy separate rows.
            if (Plan.bCompactBlocks && W.OutputPin->PinName == TEXT("LoopBody"))
                Q.BodyRowDrift += FMath::Max(0.0, FMath::Abs(End.Y-Start.Y)-16.0);
            if(Outputs==1 && W.InputPin->LinkedTo.Num()==1) Q.LinearExecDrift+=FMath::Max(0.0,FMath::Abs(End.Y-Start.Y)-16.0);
        }
        if (Plan.bCompactBlocks)
        {
            // Exclude the loop's continuation and merge descendants from its body.
            // Feedback edges never expand either forward reachability set.
            for (int32 Controller = 0; Controller < Plan.Nodes.Num(); ++Controller)
            {
                TArray<int32> BodyStarts, CompletedStarts;
                for (const auto& Wire : Plan.Wires)
                    if (Wire.SourceNodeIndex == Controller && Wire.OutputPin && !Wire.bControlFeedback)
                    {
                        if (Wire.OutputPin->PinName == TEXT("LoopBody")) BodyStarts.Add(Wire.TargetNodeIndex);
                        if (Wire.OutputPin->PinName == TEXT("Completed")) CompletedStarts.Add(Wire.TargetNodeIndex);
                    }
                if (BodyStarts.IsEmpty() || CompletedStarts.IsEmpty()) continue;
                auto Reach = [&](TArray<int32> Pending, const TSet<int32>& Excluded)
                {
                    TSet<int32> Seen;
                    while (!Pending.IsEmpty())
                    {
                        const int32 Current = Pending.Pop(EAllowShrinking::No);
                        if (Seen.Contains(Current) || Excluded.Contains(Current) || !Plan.Nodes.IsValidIndex(Current)) continue;
                        Seen.Add(Current);
                        for (const auto& Wire : Plan.Wires)
                            if (Wire.SourceNodeIndex == Current && !Wire.bControlFeedback && Wire.OutputPin
                                && Wire.OutputPin->PinType.PinCategory == TEXT("exec")) Pending.Add(Wire.TargetNodeIndex);
                    }
                    return Seen;
                };
                const TSet<int32> Continuation = Reach(CompletedStarts, TSet<int32>());
                TSet<int32> Body = Reach(BodyStarts, Continuation);
                // A block includes its upstream pure expressions, not just execution
                // nodes. Stop at impure producers: their values do not make their
                // execution chain part of this loop. Shared pure expressions count
                // conservatively in each consuming body; layout must not hide them.
                TArray<int32> PendingValues = Body.Array();
                while (!PendingValues.IsEmpty())
                {
                    const int32 Consumer = PendingValues.Pop(EAllowShrinking::No);
                    for (const auto& Wire : Plan.Wires)
                    {
                        if (Wire.TargetNodeIndex != Consumer || !Wire.OutputPin
                            || Wire.OutputPin->PinType.PinCategory == TEXT("exec")
                            || !Plan.Nodes.IsValidIndex(Wire.SourceNodeIndex)) continue;
                        const int32 Producer = Wire.SourceNodeIndex;
                        const auto& ValueNode = Plan.Nodes[Producer];
                        if ((!ValueNode.bIsPure && !ValueNode.bIsKnot) || ValueNode.bIsComment
                            || Body.Contains(Producer) || Continuation.Contains(Producer)) continue;
                        Body.Add(Producer);
                        PendingValues.Add(Producer);
                    }
                }
                double BodyBottom = Position(Plan.Nodes[Controller]).Y + Plan.Nodes[Controller].Height;
                for (int32 Member : Body)
                    BodyBottom = FMath::Max(BodyBottom, Position(Plan.Nodes[Member]).Y + Plan.Nodes[Member].Height);
                // Generated local readers have their own outlines and are not in
                // Plan.Nodes. Include only copies actually present in this mode.
                if (Mode != 0)
                    for (const auto& Copy : Copies)
                    {
                        if ((Mode == 2 && !Copy.Copy) || !Copy.ConsumerPin) continue;
                        for (int32 Member : Body)
                            if (Plan.Nodes[Member].Node == Copy.ConsumerPin->GetOwningNode())
                            {
                                BodyBottom = FMath::Max(BodyBottom, CopyPosition(Copy).Y + Copy.Height);
                                break;
                            }
                    }
                for (int32 Completed : CompletedStarts)
                    if (Position(Plan.Nodes[Completed]).Y <= BodyBottom) ++Q.CompletedAboveBody;
            }
        }
        // Distinct fan-out paths need separate bands. A shared immediate target
        // is a merge, not two lanes, so do not penalize it for coincident endpoints.
        for(int32 A=0;A<Plan.Wires.Num();++A)
        {
            const auto& L=Plan.Wires[A];
            if(L.bControlFeedback || !L.OutputPin || !L.InputPin || L.OutputPin->PinType.PinCategory!=TEXT("exec")
                || !Plan.Nodes.IsValidIndex(L.TargetNodeIndex)) continue;
            for(int32 B=A+1;B<Plan.Wires.Num();++B)
            {
                const auto& R=Plan.Wires[B];
                if(R.bControlFeedback || !R.OutputPin || !R.InputPin || R.OutputPin->PinType.PinCategory!=TEXT("exec")
                    || L.SourceNodeIndex!=R.SourceNodeIndex || L.OutputPin==R.OutputPin
                    || L.TargetNodeIndex==R.TargetNodeIndex || !Plan.Nodes.IsValidIndex(R.TargetNodeIndex)) continue;
                const auto& LN=Plan.Nodes[L.TargetNodeIndex];const auto& RN=Plan.Nodes[R.TargetNodeIndex];
                if(!LN.Node || !RN.Node) continue;
                const double LY=FTurboMCPGraphTidy::AlignmentPinPosition(L.InputPin).Y+Position(LN).Y-LN.Node->NodePosY;
                const double RY=FTurboMCPGraphTidy::AlignmentPinPosition(R.InputPin).Y+Position(RN).Y-RN.Node->NodePosY;
                if(FMath::Abs(LY-RY)<16.0) ++Q.FanOutRowConflicts;
            }
        }
        MeasureCompactGeometry(Plan, Mode, Copies, Q);
        return Q;
    }
    bool BetterLayout(const FStructuralQuality& Before, const FStructuralQuality& After,
        double OldLength, double NewLength, FString* OutReason = nullptr)
    {
        if (Before.bHasBlockMeasures && After.bHasBlockMeasures)
        {
            if (After.Overlaps > Before.Overlaps || After.UnwrappedBreaks > Before.UnwrappedBreaks)
            { if (OutReason) *OutReason = TEXT("compact layout worsens overlaps or requested row breaks"); return false; }
            for (int32 Index = 0; Index < 7; ++Index)
                if (After.BlockMeasures[Index] > Before.BlockMeasures[Index] * 1.10 + 0.000001)
                {
                    if (OutReason) *OutReason = FString::Printf(TEXT("compact regression exceeds 10%% for %s: %.1f -> %.1f"),
                        TidyBlockMeasureNames[Index], Before.BlockMeasures[Index], After.BlockMeasures[Index]);
                    return false;
                }
            for (int32 Index = 0; Index < 7; ++Index)
            {
                const double Old = Before.BlockMeasures[Index], New = After.BlockMeasures[Index];
                const double Difference = FMath::Abs(New-Old);
                const bool bCount = Index == 0 || Index == 1 || Index == 3 || Index == 4;
                if (Difference <= 0.000001 || Difference < FMath::Abs(Old) * 0.05 || (bCount && Difference < 1.0)) continue;
                if (OutReason && New > Old) *OutReason = FString::Printf(TEXT("compact ranking worsens %s: %.1f -> %.1f"), TidyBlockMeasureNames[Index], Old, New);
                return New < Old;
            }
            if (OutReason) *OutReason = TEXT("all seven compact measures tie within the stated thresholds");
            return false;
        }
        // This ordered table is also the source of the refusal explanation.
        // Never describe a different quality order from the one used to decide.
        struct FCriterion { const TCHAR* Name; double Before; double After; double Tolerance; };
        const FCriterion Criteria[] = {
            {TEXT("node overlaps"), double(Before.Overlaps), double(After.Overlaps), 0.0},
            {TEXT("requested row breaks not respected"), double(Before.UnwrappedBreaks), double(After.UnwrappedBreaks), 0.0},
            {TEXT("loop Body row drift"), Before.BodyRowDrift, After.BodyRowDrift, 1.0},
            {TEXT("Completed continuations above body bottom"), double(Before.CompletedAboveBody), double(After.CompletedAboveBody), 0.0},
            {TEXT("linear execution alignment drift"), Before.LinearExecDrift, After.LinearExecDrift, 1.0},
            {TEXT("fan-out row conflicts"), double(Before.FanOutRowConflicts), double(After.FanOutRowConflicts), 0.0},
            {TEXT("backward execution wires"), double(Before.BackwardExec), double(After.BackwardExec), 0.0},
            {TEXT("backward data wires"), double(Before.BackwardData), double(After.BackwardData), 0.0},
            {TEXT("pure input row conflicts"), double(Before.PureInputRowConflicts), double(After.PureInputRowConflicts), 0.0},
            {TEXT("input pin alignment drift"), Before.InputPinDrift, After.InputPinDrift, 1.0},
            {TEXT("total wire length"), OldLength, NewLength, 0.0}
        };
        for (const FCriterion& Criterion : Criteria)
            if (FMath::Abs(Criterion.After - Criterion.Before) > Criterion.Tolerance)
            {
                if (OutReason && Criterion.After > Criterion.Before)
                    *OutReason = FString::Printf(TEXT("candidate worsens %s: %.1f -> %.1f; earlier quality criteria are unchanged"), Criterion.Name, Criterion.Before, Criterion.After);
                return Criterion.After < Criterion.Before;
            }
        if (OutReason) *OutReason = TEXT("candidate does not improve any structural criterion or total wire length");
        return false;
    }
    // Boundary safety is independent of structural scoring and Force. A small
    // clearance allowance accommodates native node widths, but cannot justify
    // moving a selected group thousands of units from its fixed neighbours.
    bool CheckBoundaryGrowth(const FTurboMCPGraphTidy::FPlan& Plan, int32 Mode, FString& Reason,
        const TArray<FTurboMCPGraphTidy::FGetterCopy>* ActualCopies = nullptr)
    {
        if (Plan.SelectedNodeKeys.IsEmpty()) return true;
        double BeforeTotal = 0.0, AfterTotal = 0.0, TotalAllowance = 0.0;
        for (const auto& Wire : Plan.Wires)
        {
            const auto& Source = Plan.Nodes[Wire.SourceNodeIndex];
            const auto& Target = Plan.Nodes[Wire.TargetNodeIndex];
            if (!Wire.OutputPin || !Wire.InputPin || !Source.Node || !Target.Node
                || Plan.SelectedNodeKeys.Contains(Source.Key) == Plan.SelectedNodeKeys.Contains(Target.Key)) continue;
            auto PinAt = [](const UEdGraphPin* Pin, const FTurboMCPGraphTidy::FNode& Node, int32 PositionMode)
            {
                const FVector2D Current(Node.Node->NodePosX, Node.Node->NodePosY);
                const FVector2D Position = PositionMode == 2 ? Current : PositionMode == 1
                    ? FVector2D(Node.PlannedX, Node.PlannedY) : FVector2D(Node.X, Node.Y);
                return FTurboMCPGraphTidy::AlignmentPinPosition(Pin) + Position - Current;
            };
            const double Before = FVector2D::Distance(PinAt(Wire.OutputPin, Source, 0), PinAt(Wire.InputPin, Target, 0));
            FVector2D Output = PinAt(Wire.OutputPin, Source, Mode);
            const auto& Copies = ActualCopies ? *ActualCopies : Plan.GetterCopies;
            for (const auto& Copy : Copies)
                if (Copy.OriginalValuePin == Wire.OutputPin && Copy.ConsumerPin == Wire.InputPin && (Mode != 2 || Copy.Copy))
                    Output = (Mode == 2 ? FVector2D(Copy.Copy->NodePosX, Copy.Copy->NodePosY)
                        : FVector2D(Copy.PlannedX, Copy.PlannedY)) + Copy.OutputOffset;
            const double After = FVector2D::Distance(Output, PinAt(Wire.InputPin, Target, Mode));
            const double Allowance = FMath::Clamp(FMath::Max(Source.Width, Target.Width), 64.0, 256.0);
            BeforeTotal += Before; AfterTotal += After; TotalAllowance = FMath::Max(TotalAllowance, Allowance);
            if (After > Before + Allowance + 1.0)
            {
                Reason = FString::Printf(TEXT("fixed-boundary wire %s:%s -> %s:%s grows from %.1f to %.1f px; allowed growth %.1f px (native node width, clamped to 64-256). Include the connected chain or choose a local position. Force does not bypass this limit"),
                    *Source.Key, *Wire.OutputPin->PinName.ToString(), *Target.Key, *Wire.InputPin->PinName.ToString(), Before, After, Allowance);
                return false;
            }
        }
        if (AfterTotal > BeforeTotal + TotalAllowance + 1.0)
        {
            Reason = FString::Printf(TEXT("fixed-boundary total grows from %.1f to %.1f px; allowed aggregate growth %.1f px. Include the connected chain or choose a local position. Force does not bypass this limit"), BeforeTotal, AfterTotal, TotalAllowance);
            return false;
        }
        return true;
    }

    // A scoped compact layout cannot call a loop Body handoff improved when it
    // pulls the selected body farther from its fixed controller. The general
    // boundary allowance above protects node clearance; it is too wide for this
    // semantic row relationship. Force may accept this specific tradeoff, but
    // it still cannot bypass the general boundary or overlap checks.
    bool FindScopedLoopBodyRegression(const FTurboMCPGraphTidy::FPlan& Plan, int32 Mode, FString& Detail)
    {
        if (!Plan.bCompactBlocks || Plan.SelectedNodeKeys.IsEmpty()) return false;
        for (const auto& Wire : Plan.Wires)
        {
            if (!Wire.OutputPin || !Wire.InputPin
                || Wire.OutputPin->PinType.PinCategory != TEXT("exec")
                || Wire.OutputPin->PinName != TEXT("LoopBody")) continue;
            const auto& Source = Plan.Nodes[Wire.SourceNodeIndex];
            const auto& Target = Plan.Nodes[Wire.TargetNodeIndex];
            if (!Source.Node || !Target.Node
                || Plan.SelectedNodeKeys.Contains(Source.Key) == Plan.SelectedNodeKeys.Contains(Target.Key)) continue;
            auto PinAt = [](const UEdGraphPin* Pin, const FTurboMCPGraphTidy::FNode& Node, int32 PositionMode)
            {
                const FVector2D Current(Node.Node->NodePosX, Node.Node->NodePosY);
                const FVector2D Position = PositionMode == 2 ? Current : PositionMode == 1
                    ? FVector2D(Node.PlannedX, Node.PlannedY) : FVector2D(Node.X, Node.Y);
                return FTurboMCPGraphTidy::AlignmentPinPosition(Pin) + Position - Current;
            };
            const double Before = FMath::Abs(PinAt(Wire.OutputPin, Source, 0).Y - PinAt(Wire.InputPin, Target, 0).Y);
            const double After = FMath::Abs(PinAt(Wire.OutputPin, Source, Mode).Y - PinAt(Wire.InputPin, Target, Mode).Y);
            if (After <= Before + 1.0) continue;
            Detail = FString::Printf(TEXT("scoped LoopBody handoff %s:%s -> %s:%s increases vertical pin separation from %.1f to %.1f px. Expand nodeIds to include the fixed loop controller and connected body, or keep the current layout"),
                *Source.Key, *Wire.OutputPin->PinName.ToString(), *Target.Key, *Wire.InputPin->PinName.ToString(), Before, After);
            return true;
        }
        return false;
    }

    // Match stock Kismet's actual spline endpoints: whole pin-widget right/left
    // midpoints with its four-unit brush inset (Blueprint arrows have radius zero).
    // GetNodeOffset alone is the left edge even for an output with a wide label;
    // image centers also lose the distinct input/output anchors on reroute nodes.
    FVector2D MeasuredConnectorOffset(const TSharedPtr<SGraphNode>& NodeWidget,const TSharedPtr<SGraphPin>& PinWidget)
    {
        const FGeometry& Geometry=PinWidget->GetTickSpaceGeometry();
        const bool Output=PinWidget->GetDirection()==EGPD_Output;
        FVector2D Offset(NodeWidget->GetTickSpaceGeometry().AbsoluteToLocal(
            Geometry.GetAbsolutePositionAtCoordinates(FVector2f(Output?1.0f:0.0f,0.5f))));
        Offset.X+=Output?-4.0:4.0;
        return Offset;
    }

    TMap<const UEdGraphNode*, FVector2D> CachedAlignmentNodeSizes;
    TMap<const UEdGraphPin*, FVector2D> MeasuredPinOffsets;
    TWeakObjectPtr<UEdGraph> AlignmentGraph;

	/** True when the node is a comment box rather than a node that carries flow. */
	bool IsCommentNode(const UEdGraphNode* InNode)
	{
		return Cast<UEdGraphNode_Comment>(InNode) != nullptr;
	}

	bool IsKnotNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_Knot>(InNode) != nullptr;
	}

	bool IsGetterNode(const UEdGraphNode* InNode)
	{
		return Cast<UK2Node_VariableGet>(InNode) != nullptr || Cast<UK2Node_Self>(InNode) != nullptr;
	}

    // Only leaf value readers may be duplicated: no exec, target or split-pin state.
    UEdGraphPin* SplitValuePin(UEdGraphNode* Node)
    {
        if (!Node || Node->Pins.Num() != 1) return nullptr;
        UEdGraphPin* Pin = Node->Pins[0];
        return Pin && Pin->Direction == EGPD_Output && Pin->SubPins.Num() == 0 ? Pin : nullptr;
    }
    bool CanSplitGetter(UEdGraphNode* Node)
    {
        const UK2Node* K2 = Cast<UK2Node>(Node);
        return K2 && K2->IsNodePure() && SplitValuePin(Node)
            && (Node->GetClass() == UK2Node_VariableGet::StaticClass() || Node->GetClass() == UK2Node_Self::StaticClass());
    }
    bool CanReadValuePin(UEdGraphNode* Source, UEdGraphPin* Pin)
    {
        if (!Source || !Pin || !Source->Pins.Contains(Pin)) return false;
        if (Source->IsA<UK2Node_FunctionEntry>())
            return Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != TEXT("exec")
                && Pin->SubPins.IsEmpty() && !Pin->bOrphanedPin && !Pin->bHidden;
        return CanSplitGetter(Source) && SplitValuePin(Source) == Pin;
    }
    UEdGraphPin* ReaderOutput(UEdGraphNode* Reader, const UEdGraphPin* SourcePin)
    {
        if (!Reader || !SourcePin) return nullptr;
        for (UEdGraphPin* Pin : Reader->Pins)
            if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == SourcePin->PinName
                && Pin->PinType == SourcePin->PinType && Pin->SubPins.IsEmpty()) return Pin;
        return nullptr;
    }
    UEdGraphPin* CloneInputPinTree(UK2Node_VariableGet* Getter, const UEdGraphPin* Source, UEdGraphPin* Parent = nullptr)
    {
        UEdGraphPin* Pin = Getter->CreatePin(EGPD_Output, Source->PinType, Source->PinName);
        Pin->ParentPin = Parent;
        Pin->bHidden = Source->bHidden;
        Pin->PinFriendlyName = Source->PinFriendlyName;
        Pin->DefaultValue = Source->DefaultValue;
        Pin->AutogeneratedDefaultValue = Source->AutogeneratedDefaultValue;
        Pin->DefaultObject = Source->DefaultObject;
        Pin->DefaultTextValue = Source->DefaultTextValue;
        if (Parent) Parent->SubPins.Add(Pin);
        for (const UEdGraphPin* Child : Source->SubPins)
            if (Child) CloneInputPinTree(Getter, Child, Pin);
        return Pin;
    }
    UEdGraphNode* DuplicateValueReader(UEdGraphNode* Source, UEdGraph* Graph,
        UEdGraphPin* SourcePin, const FString& FunctionScope)
    {
        if (!CanReadValuePin(Source, SourcePin)) return nullptr;
        if (Source->IsA<UK2Node_FunctionEntry>())
        {
            UK2Node_VariableGet* Getter = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
            Getter->CreateNewGuid();
            const UEdGraphPin* RootPin = SourcePin;
            while (RootPin->ParentPin) RootPin = RootPin->ParentPin;
            Getter->VariableReference.SetLocalMember(RootPin->PinName, FunctionScope, FGuid());
            // Preserve the existing split tree, including nested member names/types.
            // The variable reference names the parameter, never an individual member.
            CloneInputPinTree(Getter, RootPin);
            Graph->AddNode(Getter, false, false);
            return Getter;
        }
        UEdGraphNode* Copy = DuplicateObject<UEdGraphNode>(Source, Graph);
        if (!Copy) return nullptr;
        // Duplicated pins retain references to the old neighbours. Clear only the
        // copy's array; BreakAllPinLinks would touch the original neighbours.
        for (UEdGraphPin* Pin : Copy->Pins) if (Pin) Pin->LinkedTo.Reset();
        Copy->SetFlags(RF_Transactional);
        Copy->CreateNewGuid();
        Graph->AddNode(Copy, false, false);
        return Copy;
    }

    bool MeasureInputReader(UEdGraphNode* Source, UEdGraphPin* Pin, FVector2D& Size, FVector2D& Offset)
    {
        if (!FSlateApplication::IsInitialized()) return false;
        TGuardValue<ITransaction*> SuppressScratchUndo(GUndo, nullptr);
        TStrongObjectPtr<UEdGraph> Scratch(NewObject<UEdGraph>(GetTransientPackage(), NAME_None, RF_Transient));
        Scratch->Schema = Source->GetGraph()->Schema;
        UEdGraphNode* Reader = DuplicateValueReader(Source, Scratch.Get(), Pin, Source->GetGraph()->GetName());
        if (!Reader) return false;
        Reader->NodePosX = Reader->NodePosY = 0;
        const TSharedRef<SGraphEditor> View = SNew(SGraphEditor).GraphToEdit(Scratch.Get()).IsEditable(false).ShowGraphStateOverlay(false);
        const FVector2D Canvas(1024, 512);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Canvas, TF_Bilinear, true));
        if (!Target.IsValid()) return false;
        FWidgetRenderer Renderer(true);
        View->GetGraphPanel()->Update();
        View->SetViewLocation(FVector2f::ZeroVector, 1.0f);
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            View->SlatePrepass(1.0f);
            Renderer.DrawWidget(Target.Get(), View, Canvas, 0.0f);
        }
        FSlateRect Bounds;
        const TSharedPtr<SGraphNode> Widget = View->GetGraphPanel()->GetNodeWidgetFromGuid(Reader->NodeGuid);
        if (!Widget || !View->GetBoundsForNode(Reader, Bounds, 0.f)) return false;
        const TSharedPtr<SGraphPin> PinWidget = Widget->FindWidgetForPin(ReaderOutput(Reader, Pin));
        if (!PinWidget || PinWidget->GetTickSpaceGeometry().GetLocalSize().Y <= 0) return false;
        Size = FVector2D(Bounds.Right - Bounds.Left, Bounds.Bottom - Bounds.Top);
        Offset = MeasuredConnectorOffset(Widget,PinWidget);
        return Size.X > 0 && Size.Y > 0 && FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y)
            && FMath::IsFinite(Offset.X) && FMath::IsFinite(Offset.Y);
    }

	/** Accept both node-id forms emitted by the graph readers. */
	bool NodeAnswersToId(const UEdGraphNode* InNode, const FString& InId)
	{
        return TurboMCPNodeId::MatchesNode(InNode, InId);
	}

	/**
	 * Purity, read from the node class rather than from a list of class names.
	 *
	 * `UK2Node::IsNodePure` is the engine's own answer (`K2Node.h:244`). A class that is not a
	 * `UK2Node` leaves purity unknown rather than crashing: the templated
	 * `GetDefaultObject<UK2Node>()` ASSERTS when the class is not one, which took the whole editor down
	 * once already, so the untemplated call is used and the cast is checked.
	 */
	bool IsNodePure(const UEdGraphNode* InNode)
	{
		if (const UK2Node* AsK2 = Cast<UK2Node>(InNode))
		{
			return AsK2->IsNodePure();
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// The graph, the nodes, the boxes
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Everything resolved before the layout begins. */
	struct FContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString ObjectPath;
	};

	/**
	 * Resolves an asset and finds one graph in it.
	 *
	 * The default is the event graph, and the answer always names the graph it used so a caller who
	 * omitted the name is not guessing.
	 */
	bool ResolveGraph(const FString& InAsset, const FString& InGraphName, FContext& OutContext,
		FString& OutError)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
		if (!ResolveError.IsEmpty())
		{
			OutError = ResolveError;
			return false;
		}
		OutContext.ObjectPath = Resolved.ObjectPath;

		UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
		if (Blueprint == nullptr)
		{
			OutError = FString::Printf(
				TEXT("%s is not a Blueprint, so there is no graph to tidy."), *Resolved.ObjectPath);
			return false;
		}
		OutContext.Blueprint = Blueprint;

        // Match reader discovery, including interface implementations and nested graphs.
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);
        for (UEdGraph* EventGraph : Blueprint->EventGraphs)
        {
            if (!EventGraph) continue;
            AllGraphs.Add(EventGraph);
            EventGraph->GetAllChildrenGraphs(AllGraphs);
        }
        TArray<UEdGraph*> Graphs;
        for (UEdGraph* Graph : AllGraphs)
            if (Graph) Graphs.AddUnique(Graph);
        if (Graphs.IsEmpty())
        {
            OutError = FString::Printf(TEXT("%s has no graphs to tidy."), *Resolved.ObjectPath);
            return false;
        }

        const FString RequestedName = InGraphName.TrimStartAndEnd();
        const FString LookupName = RequestedName.IsEmpty() ? TEXT("EventGraph") : RequestedName;
        int32 MatchCount = 0;
        UEdGraph* MatchedGraph = TurboMCPNodeId::FindGraphByName(Blueprint, LookupName, MatchCount);
        if (MatchCount > 1)
        {
            OutContext.Graph = nullptr;
            OutError = TEXT("Ambiguous graph name; no layout was planned or applied. Pass a unique graph name.");
            return false;
        }
        if (MatchedGraph != nullptr)
        {
            OutContext.Graph = MatchedGraph;
            return true;
        }
        if (RequestedName.IsEmpty())
        {
            if (Graphs.Num() != 1)
            {
                OutError = TEXT("Multiple graphs and no EventGraph; pass an explicit graph name.");
                return false;
            }
            OutContext.Graph = Graphs[0];
            return true;
        }

		TArray<FString> Names;
		for (UEdGraph* Candidate : Graphs) { Names.Add(Candidate->GetName()); }
		Names.Sort();
		OutError = FString::Printf(
			TEXT("'%s' is not a graph of %s. Its graphs are: %s"),
			*InGraphName, *Resolved.ObjectPath, *FString::Join(Names, TEXT(", ")));
		return false;
	}

    // Measure a transient copy: never change a live editor's zoom, focus or paint caches.
    bool EnsureMeasuredNodeOutlines(UBlueprint* Blueprint, UEdGraph* Graph,
        TMap<const UEdGraphNode*, FVector2D>& Sizes, FString& Error)
    {
        Sizes.Reset(); CachedAlignmentNodeSizes.Reset(); MeasuredPinOffsets.Reset(); AlignmentGraph.Reset();
        if (!Blueprint || !Graph) { Error = TEXT("graph measurement context unavailable"); return false; }
        if (!FSlateApplication::IsInitialized())
        {
            for (const UEdGraphNode* Node : Graph->Nodes)
                if (Node)
                {
                    if (Node->NodeWidth <= 0 || Node->NodeHeight <= 0)
                    { Error = TEXT("Tidy needs Slate to measure missing node outlines; no nodes moved."); Sizes.Reset(); return false; }
                    Sizes.Add(Node, FVector2D(Node->NodeWidth, Node->NodeHeight));
                }
            CachedAlignmentNodeSizes = Sizes; AlignmentGraph = Graph;
            return true;
        }
        TGuardValue<ITransaction*> SuppressScratchUndo(GUndo, nullptr);
        TurboMCPGraphScratch::FCopy Scratch;
        if (!Scratch.Init(Blueprint, Graph)) { Error = TEXT("transient measurement graph unavailable"); return false; }
        // Stack only the throwaway nodes at the viewport origin. Every widget gets
        // full-detail layout without scrolling/ticking the live editor, even for
        // graphs whose real nodes are thousands of units outside the viewport.
        for (UEdGraphNode* Node : Scratch.Graph->Nodes)
            if (Node) Node->NodePosX = Node->NodePosY = 0;
        const TSharedRef<SGraphEditor> View = SNew(SGraphEditor).GraphToEdit(Scratch.Graph).IsEditable(false).ShowGraphStateOverlay(false);
        const FVector2D Canvas(2048, 2048);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Canvas, TF_Bilinear, true));
        if (!Target.IsValid()) { Error = TEXT("measurement render target unavailable"); return false; }
        FWidgetRenderer Renderer(true);
        View->GetGraphPanel()->Update();
        View->SetViewLocation(FVector2f::ZeroVector, 1.0f);
        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            View->SlatePrepass(1.0f);
            Renderer.DrawWidget(Target.Get(), View, Canvas, 0.0f);
        }
        TMap<const UEdGraphPin*, FVector2D> PinOffsets;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            UEdGraphNode* Copy = TurboMCPGraphScratch::ByGuid(Scratch.Graph, Node->NodeGuid);
            FSlateRect Bounds;
            if (!Copy || !View->GetBoundsForNode(Copy, Bounds, 0.f)
                || !FMath::IsFinite(Bounds.Right - Bounds.Left) || !FMath::IsFinite(Bounds.Bottom - Bounds.Top)
                || Bounds.Right <= Bounds.Left || Bounds.Bottom <= Bounds.Top)
            { Error = TEXT("some scratch graph widgets have no measured bounds; no nodes moved"); Sizes.Reset(); return false; }
            Sizes.Add(Node, FVector2D(Bounds.Right - Bounds.Left, Bounds.Bottom - Bounds.Top));
            const TSharedPtr<SGraphNode> Widget = View->GetGraphPanel()->GetNodeWidgetFromGuid(Copy->NodeGuid);
            if (!Widget.IsValid()) continue;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                for (UEdGraphPin* CopyPin : Copy->Pins)
                    if (CopyPin && CopyPin->PinId == Pin->PinId)
                    {
                        const TSharedPtr<SGraphPin> PinWidget = Widget->FindWidgetForPin(CopyPin);
                        if (PinWidget.IsValid() && PinWidget->GetTickSpaceGeometry().GetLocalSize().Y > 0)
                        {
                            const FVector2D Offset=MeasuredConnectorOffset(Widget,PinWidget);
                            if (FMath::IsFinite(Offset.X) && FMath::IsFinite(Offset.Y)) PinOffsets.Add(Pin, Offset);
                        }
                        break;
                    }
            }
        }
        CachedAlignmentNodeSizes = Sizes; MeasuredPinOffsets = MoveTemp(PinOffsets); AlignmentGraph = Graph;
        return true;
    }

}

// ---------------------------------------------------------------------------------------------
// The layout
// ---------------------------------------------------------------------------------------------

namespace
{
	/** The graph-space rectangle of a comment box, using the rule `P/E-COMMENT` uses. */
	void BoxRect(const UEdGraphNode_Comment* InComment, double& OutX, double& OutY,
		double& OutWidth, double& OutHeight)
	{
		OutX = static_cast<double>(InComment->NodePosX);
		OutY = static_cast<double>(InComment->NodePosY) + FTurboMCPGraphTidy::TitleBarHeight;
		OutWidth = static_cast<double>(InComment->NodeWidth);
		OutHeight = static_cast<double>(InComment->NodeHeight)
			- FTurboMCPGraphTidy::TitleBarHeight;
	}

	/** True when `InOuter` wholly contains `InInner`, on all four sides. */
	bool Contains(const FTurboMCPLayoutReport::FRect& InOuter,
		const FTurboMCPLayoutReport::FRect& InInner)
	{
		return InOuter.MinX <= InInner.MinX && InOuter.MinY <= InInner.MinY
			&& InOuter.MaxX >= InInner.MaxX && InOuter.MaxY >= InInner.MaxY;
	}

	double AreaOf(const FTurboMCPLayoutReport::FRect& InRect)
	{
		return (InRect.MaxX - InRect.MinX) * (InRect.MaxY - InRect.MinY);
	}


}

// ---------------------------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------------------------

bool FTurboMCPGraphTidy::PrepareAlignment(UBlueprint* Blueprint,UEdGraph* Graph,FString& Error)
{
    TMap<const UEdGraphNode*,FVector2D> Sizes;
    return EnsureMeasuredNodeOutlines(Blueprint,Graph,Sizes,Error);
}
FVector2D FTurboMCPGraphTidy::AlignmentNodeSize(const UEdGraphNode* Node)
{
    if(Node && Node->IsA<UEdGraphNode_Comment>()) return FVector2D(Node->NodeWidth,Node->NodeHeight);
    if(Node && AlignmentGraph.Get()==Node->GetGraph())
        if(const FVector2D* Size=CachedAlignmentNodeSizes.Find(Node)) return *Size;
    return Node?FVector2D(Node->NodeWidth,Node->NodeHeight):FVector2D::ZeroVector;
}
FVector2D FTurboMCPGraphTidy::AlignmentPinPosition(const UEdGraphPin* Pin)
{
    const UEdGraphNode* Node=Pin?Pin->GetOwningNodeUnchecked():nullptr;
    if(Node && AlignmentGraph.Get()==Node->GetGraph())
        if(const FVector2D* Offset=MeasuredPinOffsets.Find(Pin)) return FVector2D(Node->NodePosX,Node->NodePosY)+*Offset;
    return FTurboMCPLayoutReport::PinPosition(Pin);
}

bool FTurboMCPGraphTidy::Plan(const FString& InAsset, const FString& InGraphName, bool bInForce,
	bool bInMergeGetters, const TArray<FString>& InNodeIds, FPlan& OutPlan, FString& OutError, const FString& MainPath, double InHorizontalGap, double InVerticalGap, double InLanePitch, const FString& WrapAfter, const FString& LayoutMode, const FString& RouteWires, double ChannelWidth, bool bCreateBoxes)
{
    if(!FMath::IsFinite(InHorizontalGap) || !FMath::IsFinite(InVerticalGap) || !FMath::IsFinite(InLanePitch)
        || InHorizontalGap<40 || InHorizontalGap>2000 || InVerticalGap<40 || InVerticalGap>2000 || InLanePitch<100 || InLanePitch>10000)
    { OutError=TEXT("layout gaps must be finite 40..2000; lane pitch 100..10000 graph units"); return false; }
    const double SpacingX=InHorizontalGap, SpacingY=InVerticalGap;
    const double BlockBandGap=bCreateBoxes?FMath::Max(InVerticalGap,2*(FitPadding+TitleBarHeight)+FitPadding+1):InVerticalGap;
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.bForce = bInForce;
    const FString Mode = LayoutMode.TrimStartAndEnd().ToLower();
    if (Mode != TEXT("rows") && Mode != TEXT("blocks"))
    { OutError = TEXT("LayoutMode must be rows or blocks; unchanged."); return false; }
    OutPlan.bCompactBlocks = Mode == TEXT("blocks");
    if (bCreateBoxes && (!OutPlan.bCompactBlocks || !InNodeIds.IsEmpty()))
    { OutError=TEXT("CreateBoxes requires whole-graph LayoutMode=blocks; unchanged."); return false; }
    const FString Routing=RouteWires.TrimStartAndEnd().ToLower();
    if (Routing!=TEXT("off") && Routing!=TEXT("orthogonal"))
    { OutError=TEXT("RouteWires must be off or orthogonal; unchanged."); return false; }
    if (!FMath::IsFinite(ChannelWidth) || ChannelWidth<20 || ChannelWidth>400)
    { OutError=TEXT("ChannelWidth must be finite and between 20 and 400 graph units; unchanged."); return false; }
	OutPlan.bMergeGettersRequested = bInMergeGetters;

	if (OutPlan.Asset.IsEmpty())
	{
		OutError = TEXT("no asset given. Pass a Blueprint in any path form, for example ")
			TEXT("BP_NodeTest.");
		return false;
	}

	// GETTER MERGING IS REFUSED BY NAME RATHER THAN SILENTLY IGNORED. The spec makes it an option that
	// is off by default; a caller who turns it on is entitled to be told this build does not do it,
	// rather than to receive a layout that quietly did not merge and looks like it did.
	if (bInMergeGetters)
	{
		OutError = TEXT("merging variable getters is not implemented in this build. The spec makes it ")
			TEXT("an option that is off by default, and it is refused rather than ignored so the answer is not mistaken for a merge that happened. Call again with merging off: getters that feed several consumers are still SPLIT, one per consumer.");
		return false;
	}

	FContext Context;
	if (!ResolveGraph(OutPlan.Asset, InGraphName, Context, OutError))
	{
		return false;
	}
	UEdGraph* Graph = Context.Graph;
	if (OutPlan.GraphName.IsEmpty())
	{
		OutPlan.GraphName = Graph->GetName();
	}

	if (Graph->Nodes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("graph %s is empty, so there is nothing to tidy."),
			*OutPlan.GraphName);
		return false;
	}

	// A plan moves nodes, so it must start from Slate-measured outlines. This actively opens and
	// lays out the graph when needed, but only the postcondition below permits planning.
	TMap<const UEdGraphNode*, FVector2D> MeasuredSizes;
	if (!EnsureMeasuredNodeOutlines(Context.Blueprint, Graph, MeasuredSizes, OutError))
	{
		return false;
	}

	// -----------------------------------------------------------------------------------------
	// STEP 1: read every node once, with its measured rectangle.
	// -----------------------------------------------------------------------------------------
	TMap<const UEdGraphNode*, int32> IndexByNode;
	for (int32 Order = 0; Order < Graph->Nodes.Num(); ++Order)
	{
		UEdGraphNode* Node = Graph->Nodes[Order];
		if (Node == nullptr)
		{
			continue;
		}

		FNode Entry;
		Entry.Node = Node;
		Entry.Key = TurboMCPNodeId::MakeForNode(Node);
		Entry.ClassName = Node->GetClass()->GetName();
		Entry.GraphOrder = Order;
		Entry.X = static_cast<double>(Node->NodePosX);
		Entry.Y = static_cast<double>(Node->NodePosY);
		Entry.bIsComment = IsCommentNode(Node);
		Entry.bIsKnot = IsKnotNode(Node);
		Entry.bIsGetter = IsGetterNode(Node);
		Entry.bIsPure = !Entry.bIsComment && IsNodePure(Node);

		Entry.Width = MeasuredSizes.FindChecked(Node).X;
		Entry.Height = MeasuredSizes.FindChecked(Node).Y;

		Entry.PlannedX = Entry.X;
		Entry.PlannedY = Entry.Y;

		IndexByNode.Add(Node, OutPlan.Nodes.Num());
		OutPlan.Nodes.Add(Entry);
	}

	// A non-empty selection makes every other node a fixed anchor. Resolve every id before planning
	// so preview and edit reject the same stale selection and a partial block is never moved.
	if (InNodeIds.Num() > 0)
	{
        const bool bUnboxedOnly = InNodeIds.ContainsByPredicate([](const FString& Id) { return Id.TrimStartAndEnd() == TEXT("@unboxed"); });
        bool bExplicitNodes = false;
		for (const FString& RequestedId : InNodeIds)
		{
			const FString Id = RequestedId.TrimStartAndEnd();
            if (Id == TEXT("@unboxed")) continue;
            bExplicitNodes = true;
            const FNode* Found = nullptr;
            int32 MatchCount = 0;
            for (const FNode& Candidate : OutPlan.Nodes)
                if (NodeAnswersToId(Candidate.Node, Id)) { Found = &Candidate; ++MatchCount; }
            if (MatchCount > 1)
            { OutError = TEXT("Ambiguous node ID; use a longer unique key or an exact reader ID: ") + Id; return false; }

			if (Found == nullptr)
			{
				OutError = FString::Printf(TEXT("no node with id '%s' in graph '%s'; tidy did not move anything."),
					*RequestedId, *OutPlan.GraphName);
				return false;
			}
			if (Found->bIsComment)
			{
				TArray<FString> Members;
				FTurboMCPComment::MembersOf(CastChecked<UEdGraphNode_Comment>(Found->Node), Members);
				for (const FNode& N : OutPlan.Nodes)
					if (N.Node && Members.Contains(N.Node->GetName())) OutPlan.SelectedNodeKeys.Add(N.Key);
				continue;
			}
			OutPlan.SelectedNodeKeys.Add(Found->Key);
		}
        if (bUnboxedOnly)
        {
            if (!bExplicitNodes)
                for (const FNode& Node : OutPlan.Nodes) if (!Node.bIsComment) OutPlan.SelectedNodeKeys.Add(Node.Key);
            for (const FNode& BoxNode : OutPlan.Nodes)
                if (UEdGraphNode_Comment* Box = Cast<UEdGraphNode_Comment>(BoxNode.Node))
                {
                    TArray<FString> Members;
                    FTurboMCPComment::MembersOf(Box, Members);
                    for (const FNode& Node : OutPlan.Nodes)
                        if (Members.Contains(Node.Node->GetName())) OutPlan.SelectedNodeKeys.Remove(Node.Key);
                }
        }
        if (OutPlan.SelectedNodeKeys.IsEmpty()) { OutError = TEXT("selection contains no eligible nodes (after @unboxed filtering, if requested); unchanged"); return false; }
		OutPlan.AnchorCount = 0;
		for (const FNode& Candidate : OutPlan.Nodes)
		{
			if (!Candidate.bIsComment && !OutPlan.SelectedNodeKeys.Contains(Candidate.Key))
			{
				++OutPlan.AnchorCount;
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 2: the boxes, their membership, and their parents.
	//
	// MEMBERSHIP COMES FROM `FTurboMCPComment::MembersOf`, the ONE copy of the geometric rule in this
	// plugin. A second copy would agree with the first until one of them was changed.
	// -----------------------------------------------------------------------------------------
	{
		TArray<FTurboMCPLayoutReport::FRect> BoxRects;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (!OutPlan.Nodes[Index].bIsComment)
			{
				continue;
			}

			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(OutPlan.Nodes[Index].Node);
			if (Comment == nullptr)
			{
				continue;
			}

			FBox Box;
			Box.Comment = Comment;
			Box.Key = OutPlan.Nodes[Index].Key;
			Box.Text = Comment->NodeComment;

			double BoxX = 0.0;
			double BoxY = 0.0;
			double BoxWidth = 0.0;
			double BoxHeight = 0.0;
			BoxRect(Comment, BoxX, BoxY, BoxWidth, BoxHeight);
			Box.X = BoxX;
            Box.PlannedX = BoxX;
			Box.Y = BoxY;
            Box.PlannedY = BoxY;
			Box.Width = BoxWidth;
			Box.Height = BoxHeight;
			Box.PlannedWidth = BoxWidth;
			Box.PlannedHeight = BoxHeight;

			FTurboMCPLayoutReport::FRect Rect;
			Rect.MinX = BoxX;
			Rect.MinY = BoxY;
			Rect.MaxX = BoxX + BoxWidth;
			Rect.MaxY = BoxY + BoxHeight;
			BoxRects.Add(Rect);

			FTurboMCPComment::MembersOf(Comment, Box.MemberKeys);

			OutPlan.Boxes.Add(Box);
		}

		// PARENTS ARE RESOLVED GEOMETRICALLY, by the SAME containment rule as membership, and the
		// parent is the SMALLEST box that contains this one (`D-LAY-3`). THE ENGINE'S `CommentDepth`
		// IS NOT READ HERE AT ALL, because a stored field can disagree with geometry after a hand
		// resize and geometry cannot disagree with itself.
		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			int32 BestParent = INDEX_NONE;
			double BestArea = TNumericLimits<double>::Max();
			for (int32 Other = 0; Other < OutPlan.Boxes.Num(); ++Other)
			{
				if (Other == Index)
				{
					continue;
				}
				if (!Contains(BoxRects[Other], BoxRects[Index]))
				{
					continue;
				}
				// An IDENTICAL rectangle is not a parent: two boxes drawn on top of each other are not
				// nested, and treating one as the other's parent would make each the other's ancestor.
				if (BoxRects[Other].MinX == BoxRects[Index].MinX
					&& BoxRects[Other].MinY == BoxRects[Index].MinY
					&& BoxRects[Other].MaxX == BoxRects[Index].MaxX
					&& BoxRects[Other].MaxY == BoxRects[Index].MaxY)
				{
					continue;
				}

				const double Area = AreaOf(BoxRects[Other]);
				if (Area < BestArea
					|| (Area == BestArea && BestParent != INDEX_NONE
						&& OutPlan.Boxes[Other].Key > OutPlan.Boxes[BestParent].Key))
				{
					BestArea = Area;
					BestParent = Other;
				}
			}
			OutPlan.Boxes[Index].ParentIndex = BestParent;
		}

		// The depth in the RESOLVED chain, and a cycle check. Two boxes each containing the other is
		// geometrically impossible but reachable from a corrupted asset, and the inside-out pass would
		// then recurse forever.
		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			int32 Depth = 0;
			int32 Walk = OutPlan.Boxes[Index].ParentIndex;
			TSet<int32> Seen;
			Seen.Add(Index);
			while (Walk != INDEX_NONE)
			{
				if (Seen.Contains(Walk))
				{
					OutError = FString::Printf(
						TEXT("comment box '%s' and '%s' each contain the other, so no box order exists. Tidy the boxes by hand and run this again."),
						*OutPlan.Boxes[Index].Text, *OutPlan.Boxes[Walk].Text);
					return false;
				}
				Seen.Add(Walk);
				++Depth;
				Walk = OutPlan.Boxes[Walk].ParentIndex;
			}
			OutPlan.Boxes[Index].Depth = Depth;
		}

		for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
		{
			const int32 Parent = OutPlan.Boxes[Index].ParentIndex;
			if (Parent != INDEX_NONE)
			{
				OutPlan.Boxes[Parent].ChildBoxIndices.Add(Index);
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 3: the wires, read once. The measure sums these, so the list IS the number's basis.
	// -----------------------------------------------------------------------------------------
	for (const FNode& Entry : OutPlan.Nodes)
	{
		if (Entry.Node == nullptr)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Entry.Node->Pins)
		{
			// Each link is counted ONCE, from the output side, which is what the reader does. Summing
			// from both sides would double every number and make the comparison meaningless.
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}
			for (UEdGraphPin* Other : Pin->LinkedTo)
			{
				// A node-to-node path is directed only from an output pin to an input pin.
				// Malformed same-direction links do not establish a dependency for layout.
				if (Other == nullptr || Other->Direction != EGPD_Input)
				{
					continue;
				}
				FWire Wire;
				Wire.OutputPin = Pin;
				Wire.InputPin = Other;
				if (const int32* Source = IndexByNode.Find(Pin->GetOwningNodeUnchecked()))
				{
					Wire.SourceNodeIndex = *Source;
				}
				if (const int32* Target = IndexByNode.Find(Other->GetOwningNodeUnchecked()))
				{
					Wire.TargetNodeIndex = *Target;
				}
                Wire.bControlFeedback = IsControlFeedbackInput(Wire.InputPin);
				OutPlan.Wires.Add(Wire);
			}
		}
	}
	OutPlan.WireCount = OutPlan.Wires.Num();

    TMap<int32,int32> MainPredecessor;
    TArray<FString> WrapIds;
    WrapAfter.ParseIntoArray(WrapIds,TEXT(","),true);
    for(FString Id:WrapIds)
    {
        Id=Id.TrimStartAndEnd();
        int32 Found=INDEX_NONE,Matches=0;
        for(int32 I=0;I<OutPlan.Nodes.Num();++I)
            if(NodeAnswersToId(OutPlan.Nodes[I].Node,Id)) {Found=I;++Matches;}
        if(Matches!=1 || OutPlan.Nodes[Found].bIsPure || OutPlan.Nodes[Found].bIsComment
            || (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Found].Key)))
        {OutError=TEXT("WrapAfter requires unique execution node IDs within the selected scope");return false;}
        bool HasContinuation=false;
        for(const FWire& W:OutPlan.Wires)
            if(W.SourceNodeIndex==Found && W.OutputPin && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec")) HasContinuation=true;
        if(!HasContinuation) {OutError=TEXT("WrapAfter node has no execution continuation");return false;}
        OutPlan.WrapAfterNodes.Add(Found);
    }
    // A bypass into the next repeat must cross the same row boundary as the
    // optional action. Never cut every branch output: its action stays on this row.
    TSet<int32> WrapTargets;
    for (const FWire& W : OutPlan.Wires)
        if (OutPlan.WrapAfterNodes.Contains(W.SourceNodeIndex) && W.OutputPin
            && W.OutputPin->PinType.PinCategory == TEXT("exec") && !W.bControlFeedback)
        {
            OutPlan.WrapBoundaryEdges.Add(FPlan::WrapEdgeKey(W.SourceNodeIndex, W.TargetNodeIndex));
            WrapTargets.Add(W.TargetNodeIndex);
        }
    for (int32 Source : OutPlan.WrapAfterNodes)
    {
        TSet<int32> Targets;
        for (const FWire& W : OutPlan.Wires)
            if (W.SourceNodeIndex == Source && W.OutputPin
                && W.OutputPin->PinType.PinCategory == TEXT("exec") && !W.bControlFeedback)
                Targets.Add(W.TargetNodeIndex);
        if (Targets.Num() != 1) continue; // Preserve existing explicit fan-out wrap behavior.
        const int32 Join = *Targets.CreateConstIterator();
        TSet<int32> Incoming;
        for (const FWire& W : OutPlan.Wires)
            if (W.TargetNodeIndex == Join && W.OutputPin && W.OutputPin->PinType.PinCategory == TEXT("exec")
                && !W.bControlFeedback) Incoming.Add(W.SourceNodeIndex);
        if (Incoming.Num() < 2) continue;
        TSet<int32> Region; Region.Add(Source);
        TArray<int32> Pending{Source};
        for (int32 Cursor = 0; Cursor < Pending.Num(); ++Cursor)
        {
            const int32 Current = Pending[Cursor];
            // A previous requested continuation starts this repeat; do not absorb
            // the previous repeat across its already requested boundary.
            if (WrapTargets.Contains(Current)) continue;
            for (const FWire& W : OutPlan.Wires)
            {
                if (W.TargetNodeIndex != Current || !W.OutputPin || W.bControlFeedback
                    || W.OutputPin->PinType.PinCategory != TEXT("exec") || W.SourceNodeIndex == Join) continue;
                const FNode& Previous = OutPlan.Nodes[W.SourceNodeIndex];
                if (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(Previous.Key)) continue;
                if (!Region.Contains(W.SourceNodeIndex)) { Region.Add(W.SourceNodeIndex); Pending.Add(W.SourceNodeIndex); }
            }
        }
        for (int32 Parent : Incoming) if (!Region.Contains(Parent))
        { OutError = TEXT("WrapAfter merge has an unrelated or unselected incoming path. Include the complete optional-action region. Unchanged."); return false; }
        TMap<int32, int32> InDegree;
        for (int32 Node : Region) InDegree.Add(Node, 0);
        for (const FWire& W : OutPlan.Wires)
        {
            if (!Region.Contains(W.SourceNodeIndex) || !W.OutputPin || W.OutputPin->PinType.PinCategory != TEXT("exec")) continue;
            if (W.bControlFeedback || (!Region.Contains(W.TargetNodeIndex) && W.TargetNodeIndex != Join))
            { OutError = TEXT("WrapAfter optional-action region has feedback or a conflicting exit. All exits must reach one continuation. Unchanged."); return false; }
            if (Region.Contains(W.TargetNodeIndex)) ++InDegree[W.TargetNodeIndex];
        }
        TArray<int32> Ready;
        for (const auto& Entry : InDegree) if (Entry.Value == 0) Ready.Add(Entry.Key);
        int32 Visited = 0;
        for (int32 Cursor = 0; Cursor < Ready.Num(); ++Cursor)
        {
            ++Visited;
            for (const FWire& W : OutPlan.Wires)
                if (W.SourceNodeIndex == Ready[Cursor] && W.OutputPin && W.OutputPin->PinType.PinCategory == TEXT("exec")
                    && Region.Contains(W.TargetNodeIndex) && --InDegree[W.TargetNodeIndex] == 0) Ready.Add(W.TargetNodeIndex);
        }
        if (Visited != Region.Num())
        { OutError = TEXT("WrapAfter optional-action region contains a cycle. Unchanged."); return false; }
        for (int32 Parent : Incoming) OutPlan.WrapBoundaryEdges.Add(FPlan::WrapEdgeKey(Parent, Join));
    }
    TArray<FString> MainIds; MainPath.ParseIntoArray(MainIds,TEXT(","),true);
    const bool bAutoMainPath = MainPath.TrimStartAndEnd().Equals(TEXT("auto"),ESearchCase::IgnoreCase);
    if(bAutoMainPath)
    {
        MainIds.Reset();
        TMap<int32,TArray<int32>> Paths; TSet<int32> Visiting;
        auto Eligible=[&](int32 I) { return !OutPlan.Nodes[I].bIsPure && !OutPlan.Nodes[I].bIsComment && (OutPlan.SelectedNodeKeys.IsEmpty() || OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[I].Key)); };
        TFunction<bool(int32)> Visit=[&](int32 I)
        {
            if(Paths.Contains(I)) return true;
            if(Visiting.Contains(I)) return false;
            Visiting.Add(I); TArray<int32> Best;
            for(const FWire& W:OutPlan.Wires) if(!W.bControlFeedback && W.SourceNodeIndex==I && W.OutputPin->PinType.PinCategory==TEXT("exec") && Eligible(W.TargetNodeIndex))
            {
                if(!Visit(W.TargetNodeIndex)) return false;
                if(Paths[W.TargetNodeIndex].Num()>Best.Num()) Best=Paths[W.TargetNodeIndex];
            }
            Best.Insert(I,0); Paths.Add(I,Best); Visiting.Remove(I); return true;
        };
        TArray<int32> Longest;
        for(int32 I=0;I<OutPlan.Nodes.Num();++I) if(Eligible(I))
        {
            if(!Visit(I)) { OutError=TEXT("MainPath auto found an unsupported execution cycle in the selected scope. Select an acyclic region or use explicit node positions; no nodes moved."); return false; }
            if(Paths[I].Num()>Longest.Num()) Longest=Paths[I];
        }
        for(int32 I:Longest) MainIds.Add(TurboMCPNodeId::MakeForNode(OutPlan.Nodes[I].Node));
    }
    for(FString& Id:MainIds) Id=Id.TrimStartAndEnd();
    int32 Previous=INDEX_NONE; TSet<int32> MainSeen;
    for(const FString& Id:MainIds)
    {
        int32 Current=INDEX_NONE;
        for(int32 I=0;I<OutPlan.Nodes.Num();++I) if(NodeAnswersToId(OutPlan.Nodes[I].Node,Id))
        { if(Current!=INDEX_NONE) { OutError=TEXT("Ambiguous MainPath node ID: ")+Id; return false; } Current=I; }
        // An explicit path may start at an immovable predecessor. It supplies direction,
        // not selection ownership; every subsequent node must belong to the selection.
        const bool bFixedFirstAnchor = !bAutoMainPath && Previous == INDEX_NONE && MainIds.Num() > 1;
        if(Current==INDEX_NONE || MainSeen.Contains(Current) || OutPlan.Nodes[Current].bIsPure || OutPlan.Nodes[Current].bIsComment
            || (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Current].Key) && !bFixedFirstAnchor))
        { OutError=TEXT("MainPath requires distinct execution nodes in the selected scope. Only the first explicit node may be a fixed predecessor connected to the next selected node."); return false; }
        if(Previous!=INDEX_NONE)
        {
            bool Connected=false;
            for(const FWire& W:OutPlan.Wires) if(W.SourceNodeIndex==Previous && W.TargetNodeIndex==Current && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec")) { Connected=true; break; }
            if(!Connected) { OutError=TEXT("MainPath nodes must be connected in order by execution wires"); return false; }
            MainPredecessor.Add(Current,Previous);
        }
        MainSeen.Add(Current); Previous=Current;
    }

	// -----------------------------------------------------------------------------------------
	// STEP 4: the columns, by longest incoming chain, over exec AND data links.
	//
	// Data links must count. A pure node has no exec wire of its own - it is reached only because a
	// live node reads its value - so a column computed on exec alone would put every pure node in
	// column 0 and stack them against the entry. `P/E-GRAPH-DEAD` reached the same conclusion for
	// liveness and `TestRecord.md` section 6b records it.
	// -----------------------------------------------------------------------------------------
	{
		TArray<TArray<int32>> Outgoing;
		TArray<int32> InDegree;
		Outgoing.SetNum(OutPlan.Nodes.Num());
		InDegree.Init(0, OutPlan.Nodes.Num());

		for (const FWire& Wire : OutPlan.Wires)
		{
			if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
			{
				continue;
			}
            if (Wire.bControlFeedback) continue;
            // Fixed nodes remain obstacles/anchors, but a cycle wholly outside
            // the selection is not a dependency of the requested layout.
            if (!OutPlan.SelectedNodeKeys.IsEmpty()
                && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Wire.SourceNodeIndex].Key)
                && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Wire.TargetNodeIndex].Key)) continue;
			// Exec and data pins can create parallel wires between the same nodes. They are one
			// directed dependency, so count the edge once in both adjacency and indegree.
			if (!Outgoing[Wire.SourceNodeIndex].Contains(Wire.TargetNodeIndex))
			{
				Outgoing[Wire.SourceNodeIndex].Add(Wire.TargetNodeIndex);
				++InDegree[Wire.TargetNodeIndex];
			}
		}

		TArray<int32> Queue;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (InDegree[Index] == 0)
			{
				Queue.Add(Index);
			}
		}

		int32 Processed = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			++Processed;
			for (const int32 Next : Outgoing[Current])
			{
				OutPlan.Nodes[Next].Column = FMath::Max(OutPlan.Nodes[Next].Column,
					OutPlan.Nodes[Current].Column + 1);
				if (--InDegree[Next] == 0)
				{
					Queue.Add(Next);
				}
			}
		}

		// A CYCLE IS A REFUSAL, NOT A SILENT ZERO. Column is a longest path and a cycle has no answer
		// for one; guessing an order would move nodes on no basis.
		if (Processed < OutPlan.Nodes.Num())
		{
			TArray<uint8> VisitState;
			VisitState.Init(0, OutPlan.Nodes.Num());
			TArray<int32> Path;
			TArray<int32> Cycle;
			TFunction<bool(int32)> FindCycle = [&](const int32 Current)
			{
				VisitState[Current] = 1;
				Path.Add(Current);
				for (const int32 Next : Outgoing[Current])
				{
					if (VisitState[Next] == 0 && FindCycle(Next))
					{
						return true;
					}
					if (VisitState[Next] == 1)
					{
						const int32 Start = Path.IndexOfByKey(Next);
						for (int32 Index = Start; Index < Path.Num(); ++Index)
						{
							Cycle.Add(Path[Index]);
						}
						Cycle.Add(Next);
						return true;
					}
				}
				Path.Pop();
				VisitState[Current] = 2;
				return false;
			};
			for (int32 Index = 0; Index < OutPlan.Nodes.Num() && Cycle.Num() == 0; ++Index)
			{
				if (VisitState[Index] == 0)
				{
					FindCycle(Index);
				}
			}

			TArray<FString> Titles;
			for (const int32 Index : Cycle)
			{
				Titles.Add(TurboMCPNodeId::GetNodeTitle(OutPlan.Nodes[Index].Node));
			}
			OutError = FString::Printf(
				TEXT("graph %s has a directed node cycle: %s. No supported forward layout order exists for this scope; no nodes moved. Select an acyclic region or use explicit positions. Graph links were preserved."),
				*OutPlan.GraphName, *FString::Join(Titles, TEXT(" -> ")));
			return false;
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 5: reachability. Unreachable nodes are EXCLUDED from the layout and left exactly where
	// they are, and the count is reported so a caller is not surprised by nodes that did not move.
	// `P/E-GRAPH-DEAD` owns their removal.
	// -----------------------------------------------------------------------------------------
	{
		TSet<int32> Reached;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			// An entry point is a node with no incoming link that is not a comment. A comment is not
			// flow and cannot make anything reachable.
			if (!OutPlan.Nodes[Index].bIsComment && OutPlan.Nodes[Index].Column == 0)
			{
				Reached.Add(Index);
			}
		}

		bool bGrew = true;
		while (bGrew)
		{
			bGrew = false;
			for (const FWire& Wire : OutPlan.Wires)
			{
				if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
				{
					continue;
				}
				if (Reached.Contains(Wire.SourceNodeIndex)
					&& !Reached.Contains(Wire.TargetNodeIndex))
				{
					Reached.Add(Wire.TargetNodeIndex);
					bGrew = true;
				}
			}
		}

		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			if (OutPlan.Nodes[Index].bIsComment)
			{
				continue;
			}
			if (!Reached.Contains(Index))
			{
				OutPlan.Nodes[Index].bUnreachable = true;
				++OutPlan.UnreachableCount;
			}
		}
	}

	// -----------------------------------------------------------------------------------------
	// STEP 6: the layout.
	//
	// THE ORDER HERE IS THE PART MOST LIKELY TO BE WRONG, so it is stated once and followed exactly.
	// Inside-out: a box at the greatest depth is arranged and FITTED first, and an outer box then sees
	// its children already fitted and treats each as ONE BLOCK. Fitting an outer box first would
	// compute its rectangle from stale child sizes, and the outer box would then have to MOVE - which
	// the spec forbids.
	// -----------------------------------------------------------------------------------------

	// Which box owns a node: the DEEPEST box whose membership contains it. A node inside two nested
	// boxes is laid out by the innermost one, so it is laid out exactly once (`D-LAY-2`).
	TMap<FString, int32> NodeIndexByKey;
	for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
	{
		NodeIndexByKey.Add(OutPlan.Nodes[Index].Key, Index);
		if (OutPlan.Nodes[Index].Node) NodeIndexByKey.Add(OutPlan.Nodes[Index].Node->GetName(), Index);
	}

	TArray<int32> OwnerOfNode;
	OwnerOfNode.Init(INDEX_NONE, OutPlan.Nodes.Num());
	for (int32 BoxIndex = 0; BoxIndex < OutPlan.Boxes.Num(); ++BoxIndex)
	{
		const FBox& Box = OutPlan.Boxes[BoxIndex];
		for (const FString& Key : Box.MemberKeys)
		{
			const int32* Found = NodeIndexByKey.Find(Key);
			if (Found == nullptr)
			{
				continue;
			}
			const int32 NodeIndex = *Found;
			if (OutPlan.Nodes[NodeIndex].bIsComment || OutPlan.Nodes[NodeIndex].bUnreachable)
			{
				continue;
			}
			if (OwnerOfNode[NodeIndex] == INDEX_NONE
				|| OutPlan.Boxes[OwnerOfNode[NodeIndex]].Depth < Box.Depth)
			{
				OwnerOfNode[NodeIndex] = BoxIndex;
			}
		}
	}

	// Deepest first, so a child's fitted rectangle is final before its parent measures it.
	TArray<int32> BoxOrder;
	for (int32 Index = 0; Index < OutPlan.Boxes.Num(); ++Index)
	{
		BoxOrder.Add(Index);
	}
	BoxOrder.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
	{
		if (OutPlan.Boxes[InLeft].Depth != OutPlan.Boxes[InRight].Depth)
		{
			return OutPlan.Boxes[InLeft].Depth > OutPlan.Boxes[InRight].Depth;
		}
		// A stable order among equals, so a re-run of tidy changes nothing.
		return OutPlan.Boxes[InLeft].Key < OutPlan.Boxes[InRight].Key;
	});

	auto ArrangeRows = [&OutPlan, &MainPredecessor, SpacingX, SpacingY, BlockBandGap, InLanePitch](const TArray<int32>& Free, double FreeStartX, double FreeStartY)
	{
			// Execution components form separate rows; data sharing must not merge event chains.
			TMap<int32, int32> RowOf;
			for (const int32 I : Free) RowOf.Add(I, I);
			for (int32 Pass = 0; Pass < Free.Num(); ++Pass)
			{
				bool Changed = false;
				for (const FWire& W : OutPlan.Wires)
				{
					if (W.bControlFeedback || !W.OutputPin || W.OutputPin->PinType.PinCategory != TEXT("exec")
						|| OutPlan.IsWrapBoundary(W.SourceNodeIndex, W.TargetNodeIndex)
                        || !RowOf.Contains(W.SourceNodeIndex) || !RowOf.Contains(W.TargetNodeIndex)) continue;
					const int32 R = FMath::Min(RowOf[W.SourceNodeIndex], RowOf[W.TargetNodeIndex]);
					Changed |= RowOf[W.SourceNodeIndex] != R || RowOf[W.TargetNodeIndex] != R;
					RowOf[W.SourceNodeIndex] = RowOf[W.TargetNodeIndex] = R;
				}
				if (!Changed) break;
			}
			// Attach upstream pure expressions to their first consumer's row, including pure chains.
			for (int32 Pass = 0; Pass < Free.Num(); ++Pass)
			{
				bool Changed = false;
				for (int32 N = Free.Num() - 1; N >= 0; --N)
				{
					const int32 I = Free[N];
					if (!OutPlan.Nodes[I].bIsPure) continue;
					for (const FWire& W : OutPlan.Wires)
						if (W.SourceNodeIndex == I && W.TargetNodeIndex != I && RowOf.Contains(W.TargetNodeIndex))
						{
							Changed |= RowOf[I] != RowOf[W.TargetNodeIndex];
							RowOf[I] = RowOf[W.TargetNodeIndex]; break;
						}
				}
				if (!Changed) break;
			}
			TArray<int32> Rows;
			for (int32 I : Free) Rows.AddUnique(RowOf[I]);
            if(!OutPlan.WrapAfterNodes.IsEmpty())
            {
                TArray<int32> PendingRows=Rows,OrderedRows;
                while(!PendingRows.IsEmpty())
                {
                    int32 Ready=INDEX_NONE;
                    for(int32 CandidateRow:PendingRows)
                    {
                        bool HasEarlierRow=false;
                        for(const FWire& W:OutPlan.Wires)
                            if(OutPlan.IsWrapBoundary(W.SourceNodeIndex, W.TargetNodeIndex)
                                && RowOf.Contains(W.SourceNodeIndex) && RowOf.Contains(W.TargetNodeIndex)
                                && RowOf[W.TargetNodeIndex]==CandidateRow && RowOf[W.SourceNodeIndex]!=CandidateRow
                                && PendingRows.Contains(RowOf[W.SourceNodeIndex])) {HasEarlierRow=true;break;}
                        if(!HasEarlierRow) {Ready=CandidateRow;break;}
                    }
                    if(Ready==INDEX_NONE) break; // Final validation reports an unsatisfied cyclic break.
                    OrderedRows.Add(Ready);PendingRows.RemoveSingle(Ready);
                }
                OrderedRows.Append(PendingRows);Rows=MoveTemp(OrderedRows);
            }
            // Optimize all inter-row relationships; delegates carry extra weight.
            // Adjacent swaps reduce weighted distance without disturbing execution components.
            TMap<int32,int32> Rank;
            auto Cost=[&]()
            {
                Rank.Empty(); for(int32 I=0;I<Rows.Num();++I) Rank.Add(Rows[I],I);
                int64 Sum=0;
                for(const FWire& W:OutPlan.Wires) if(RowOf.Contains(W.SourceNodeIndex) && RowOf.Contains(W.TargetNodeIndex))
                {
                    const int32 Weight=W.OutputPin && W.OutputPin->PinType.PinCategory==TEXT("delegate")?8:1;
                    Sum+=int64(Weight)*FMath::Abs(Rank[RowOf[W.SourceNodeIndex]]-Rank[RowOf[W.TargetNodeIndex]]);
                }
                return Sum;
            };
            int64 Best=Cost();
            for(int32 Pass=0;Pass<8 && OutPlan.WrapAfterNodes.IsEmpty();++Pass)
            {
                bool Changed=false;
                for(int32 I=1;I<Rows.Num();++I)
                {
                    Rows.Swap(I-1,I); const int64 Candidate=Cost();
                    if(Candidate<Best) { Best=Candidate; Changed=true; } else Rows.Swap(I-1,I);
                }
                if(!Changed) break;
            }
			double RowY = FreeStartY;
			for (int32 Row : Rows)
			{
				double CursorX = FreeStartX;
				double Bottom = RowY;
				double LaneHeight = SpacingY;
				for (int32 I : Free) if (RowOf[I] == Row) LaneHeight = FMath::Max(LaneHeight, OutPlan.Nodes[I].Height + SpacingY);
				TMap<int32,int32> LaneOf;
                TMap<const UEdGraphPin*,int32> OutputLane;
                int32 NextLane=1;
                TMap<int32,int32> LaneParent;
                TMap<int32,int32> LaneSource;
                auto CommonLane=[&LaneParent](int32 A,int32 B)
                {
                    TSet<int32> Ancestors;
                    for(;;) {Ancestors.Add(A);if(A==0) break;A=LaneParent.FindRef(A);}
                    while(!Ancestors.Contains(B) && B!=0) B=LaneParent.FindRef(B);
                    return B;
                };
                TMap<int32,double> LaneCursor;
                TMap<int32,TSet<int32>> ReservedCalculations;
                LaneCursor.Add(0,FreeStartX);
				for (int32 I : Free)
				{
					FNode& N = OutPlan.Nodes[I];
					if (RowOf[I] != Row || N.bIsPure) continue;
					int32 Lane = MAX_int32; const FWire* Incoming = nullptr;
                    TSet<int32> IncomingLanes;
					for (const FWire& W : OutPlan.Wires)
						if (W.TargetNodeIndex == I && W.OutputPin && !W.bControlFeedback && W.OutputPin->PinType.PinCategory == TEXT("exec") && LaneOf.Contains(W.SourceNodeIndex))
						{
                            const bool Preferred=MainPredecessor.Contains(I) && MainPredecessor[I]==W.SourceNodeIndex;
                            const bool SequenceEdge=OutPlan.Nodes[W.SourceNodeIndex].Node->IsA<UK2Node_ExecutionSequence>();
                            if(MainPredecessor.Contains(I) && !Preferred && !SequenceEdge) continue;
                            const int32* ReservedLane=OutputLane.Find(W.OutputPin);
                            const int32 Candidate=ReservedLane?*ReservedLane:LaneOf[W.SourceNodeIndex];
                            IncomingLanes.Add(Candidate);
                            if(Lane==MAX_int32) {Lane=Candidate;Incoming=&W;}
                            else
                            {
                                if(Candidate<Lane) Incoming=&W;
                                Lane=CommonLane(Lane,Candidate);
                            }
						}
					if (Lane == MAX_int32) Lane = 0;
                    if (OutPlan.bCompactBlocks && IncomingLanes.Num() > 1)
                    {
                        // Keep the merge on its longest execution path. A short
                        // fallback exits along the bottom and returns to this row;
                        // it must not pull the finish block below the fallback.
                        const FWire* Primary=nullptr; int32 BestColumn=MIN_int32;
                        for (const FWire& Wire:OutPlan.Wires)
                            if (Wire.TargetNodeIndex==I && Wire.OutputPin && !Wire.bControlFeedback
                                && Wire.OutputPin->PinType.PinCategory==TEXT("exec") && LaneOf.Contains(Wire.SourceNodeIndex)
                                && OutPlan.Nodes[Wire.SourceNodeIndex].Column>BestColumn)
                            { Primary=&Wire; BestColumn=OutPlan.Nodes[Wire.SourceNodeIndex].Column; }
                        if (Primary)
                        {
                            const int32* Reserved=OutputLane.Find(Primary->OutputPin);
                            Lane=Reserved?*Reserved:LaneOf[Primary->SourceNodeIndex]; Incoming=Primary;
                            OutPlan.MergePredecessors.Add(I,Primary->SourceNodeIndex);
                        }
                    }
                    LaneOf.Add(I, Lane);
                    NextLane=FMath::Max(NextLane,Lane+1);
                    // Reserve distinct lanes per fan-out pin once, at the source.
                    // Adding branch indices to the parent lane lets an inner false
                    // path collide with its outer sibling (both become lane 1).
                    const UEdGraphPin* MainOutput=nullptr;
                    for(const UEdGraphPin* Pin:N.Node->Pins)
                        if(Pin && Pin->Direction==EGPD_Output && Pin->PinType.PinCategory==TEXT("exec") && !Pin->LinkedTo.IsEmpty())
                        {
                            if (!MainOutput || (!OutPlan.bCompactBlocks && Pin->PinName == TEXT("Completed"))
                                || (OutPlan.bCompactBlocks && Pin->PinName == TEXT("LoopBody"))) MainOutput=Pin;
                        }
                    for(const FWire& W:OutPlan.Wires)
                        if(W.SourceNodeIndex==I && W.OutputPin && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec")
                            && MainPredecessor.Contains(W.TargetNodeIndex) && MainPredecessor[W.TargetNodeIndex]==I)
                        { MainOutput=W.OutputPin;break; }
                    const bool IsSequence=N.Node->IsA<UK2Node_ExecutionSequence>();
                    for(const UEdGraphPin* Pin:N.Node->Pins)
                        if(Pin && Pin->Direction==EGPD_Output && Pin->PinType.PinCategory==TEXT("exec") && !Pin->LinkedTo.IsEmpty())
                        {
                            // Compact Sequence output zero continues the current row;
                            // subsequent outputs reserve lower child blocks.
                            if((!IsSequence || OutPlan.bCompactBlocks) && Pin==MainOutput) OutputLane.Add(Pin,Lane);
                            else {LaneParent.Add(NextLane,Lane);LaneSource.Add(NextLane,I);OutputLane.Add(Pin,NextLane++);}
                        }

					double PureSpace=0;
                    TSet<int32> SeenPure;
                    TSet<int32>& Reserved=ReservedCalculations.FindOrAdd(Lane);
                    TFunction<double(int32)> Required=[&](int32 Target)
                    {
                        if(SeenPure.Contains(Target)) return 0.0;
                        SeenPure.Add(Target); double Width=0;
                        for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && RowOf.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure
                            && (!OutPlan.bCompactBlocks || !Reserved.Contains(W.SourceNodeIndex)))
                            Width=FMath::Max(Width,OutPlan.Nodes[W.SourceNodeIndex].Width+SpacingX+Required(W.SourceNodeIndex));
                        return Width;
                    };
                    PureSpace=Required(I);
                    // A shared calculation is drawn once beside its earliest
                    // consumer. Later consumers on this lane need no second empty
                    // tree column. Getters still reserve room for local copies;
                    // other lanes retain independent input space. Rows is unchanged.
                    if (OutPlan.bCompactBlocks) for (int32 Pure:SeenPure)
                        if (OutPlan.Nodes[Pure].bIsPure && !OutPlan.Nodes[Pure].bIsGetter) Reserved.Add(Pure);
                    int32 IncomingExecCount = 0;
                    for (const FWire& Edge : OutPlan.Wires)
                        if (Edge.TargetNodeIndex == I && !Edge.bControlFeedback && Edge.OutputPin
                            && Edge.OutputPin->PinType.PinCategory == TEXT("exec")) ++IncomingExecCount;
                    const bool bCompactHandoff = OutPlan.bCompactBlocks && Incoming && IncomingExecCount == 1
                        && LaneOf.Contains(Incoming->SourceNodeIndex) && LaneOf[Incoming->SourceNodeIndex] != Lane;
                    // A continuation block starts beneath its controller, rather than
                    // extending the width of the preceding body. Merges still honor all predecessors.
                    if(!LaneCursor.Contains(Lane))
                    {
                        const double Start=Incoming ? OutPlan.Nodes[Incoming->SourceNodeIndex].PlannedX
                            + (bCompactHandoff ? 0.0 : OutPlan.Nodes[Incoming->SourceNodeIndex].Width+SpacingX) : FreeStartX;
                        LaneCursor.Add(Lane,Start);
                    }
                    // A merge stays on its main lane but follows every already
                    // placed predecessor, not just that lane's shorter branch.
                    double IncomingRight=LaneCursor[Lane];
                    for(const FWire& W:OutPlan.Wires)
                        if(W.TargetNodeIndex==I && W.OutputPin && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec")
                            && LaneOf.Contains(W.SourceNodeIndex) && W.SourceNodeIndex!=I)
                        {
                            const FNode& Predecessor=OutPlan.Nodes[W.SourceNodeIndex];
                            if (!bCompactHandoff) IncomingRight=FMath::Max(IncomingRight,Predecessor.PlannedX+Predecessor.Width+SpacingX);
                        }
                    CursorX=IncomingRight+PureSpace;
                    N.PlannedX = CursorX; N.PlannedY = RowY + Lane * LaneHeight;
					if (Incoming && LaneOf[Incoming->SourceNodeIndex] == Lane)
					{
						const FNode& Source = OutPlan.Nodes[Incoming->SourceNodeIndex];
						N.PlannedY = FTurboMCPGraphTidy::AlignmentPinPosition(Incoming->OutputPin).Y + Source.PlannedY - Source.Y
							- (FTurboMCPGraphTidy::AlignmentPinPosition(Incoming->InputPin).Y - N.Y);
					}
					CursorX += N.Width + SpacingX;
                    LaneCursor[Lane]=CursorX;
					Bottom = FMath::Max(Bottom, N.PlannedY + N.Height);
				}
                // Give each execution lane its own input band. A graph-wide floor
                // sends main-row readers beneath every branch and creates long wires
                // back through unrelated lanes. Reserve shared readers in each lane
                // that uses them so later safe getter copies have space as well.
                TMap<int32,TSet<int32>> LaneInputs;
                TMap<int32,int32> PureLane;
                TFunction<void(int32,int32)> CollectInputs=[&](int32 Target,int32 Lane)
                {
                    for(const FWire& W:OutPlan.Wires)
                    {
                        if(W.TargetNodeIndex!=Target || !RowOf.Contains(W.SourceNodeIndex)
                            || RowOf[W.SourceNodeIndex]!=Row || !OutPlan.Nodes[W.SourceNodeIndex].bIsPure) continue;
                        TSet<int32>& Inputs=LaneInputs.FindOrAdd(Lane);
                        if(Inputs.Contains(W.SourceNodeIndex)) continue;
                        Inputs.Add(W.SourceNodeIndex);
                        if(!PureLane.Contains(W.SourceNodeIndex)) PureLane.Add(W.SourceNodeIndex,Lane);
                        CollectInputs(W.SourceNodeIndex,Lane);
                    }
                };
                TArray<int32> LaneOrder;
                for(int32 I:Free) if(RowOf[I]==Row && !OutPlan.Nodes[I].bIsPure)
                { LaneOrder.AddUnique(LaneOf[I]);CollectInputs(I,LaneOf[I]); }
                // Creation order can interleave an inner branch with its parent's
                // unrelated sibling. Visit lane ancestry so each subtree stays together.
                const TArray<int32> ActiveLanes = LaneOrder;
                LaneOrder.Reset();
                TArray<int32> PendingLanes;
                PendingLanes.Add(0);
                while (!PendingLanes.IsEmpty())
                {
                    const int32 CurrentLane = PendingLanes.Last();
                    PendingLanes.RemoveAt(PendingLanes.Num() - 1);
                    if (ActiveLanes.Contains(CurrentLane)) LaneOrder.Add(CurrentLane);
                    TArray<int32> Children;
                    for (int32 ChildLane = 1; ChildLane < NextLane; ++ChildLane)
                        if (LaneParent.FindRef(ChildLane) == CurrentLane) Children.Add(ChildLane);
                    if (OutPlan.bCompactBlocks)
                    {
                        // Later control nodes belong to the current body's inner flow.
                        // Visit their continuations before an earlier outer exit. Outputs
                        // of the same Sequence retain their declared order.
                        Children.Sort([&](int32 A, int32 B)
                        {
                            const int32 SourceA = LaneSource.FindRef(A), SourceB = LaneSource.FindRef(B);
                            const int32 ColumnA = OutPlan.Nodes[SourceA].Column, ColumnB = OutPlan.Nodes[SourceB].Column;
                            return ColumnA != ColumnB ? ColumnA > ColumnB : A < B;
                        });
                    }
                    for (int32 Child = Children.Num() - 1; Child >= 0; --Child) PendingLanes.Add(Children[Child]);
                }
                if (OutPlan.bCompactBlocks)
                {
                    // Keep the nesting preference, but never place a join above
                    // one of its predecessors. Feedback is not forward ordering.
                    TArray<int32> PendingOrder = LaneOrder, OrderedLanes;
                    while (!PendingOrder.IsEmpty())
                    {
                        int32 ReadyLane = INDEX_NONE;
                        for (int32 CandidateLane : PendingOrder)
                        {
                            bool bWaiting = false;
                            for (const FWire& Wire : OutPlan.Wires)
                                if (!Wire.bControlFeedback && Wire.OutputPin
                                    && Wire.OutputPin->PinType.PinCategory == TEXT("exec")
                                    && (!OutPlan.MergePredecessors.Contains(Wire.TargetNodeIndex) || OutPlan.MergePredecessors[Wire.TargetNodeIndex]==Wire.SourceNodeIndex)
                                    && LaneOf.Contains(Wire.SourceNodeIndex) && LaneOf.Contains(Wire.TargetNodeIndex)
                                    && LaneOf[Wire.TargetNodeIndex] == CandidateLane
                                    && LaneOf[Wire.SourceNodeIndex] != CandidateLane
                                    && PendingOrder.Contains(LaneOf[Wire.SourceNodeIndex]))
                                { bWaiting = true; break; }
                            if (!bWaiting) { ReadyLane = CandidateLane; break; }
                        }
                        if (ReadyLane == INDEX_NONE) break;
                        OrderedLanes.Add(ReadyLane);
                        PendingOrder.RemoveSingle(ReadyLane);
                    }
                    OrderedLanes.Append(PendingOrder);
                    LaneOrder = MoveTemp(OrderedLanes);
                }
                if (OutPlan.bCompactBlocks)
                    for (int32 Lane : LaneOrder)
                    {
                        const int32 Block = OutPlan.BlockCount++;
                        for (int32 I : Free)
                            if (RowOf[I] == Row && ((!OutPlan.Nodes[I].bIsPure && LaneOf.FindRef(I) == Lane)
                                || (OutPlan.Nodes[I].bIsPure && PureLane.Contains(I) && PureLane[I] == Lane)))
                                OutPlan.Nodes[I].BlockIndex = Block;
                    }
                TMap<int32,double> InputFloorByLane;
                double BandTop=RowY;
                Bottom=RowY;
                for(int32 Lane:LaneOrder)
                {
                    double ExecTop=TNumericLimits<double>::Max(),ExecBottom=-TNumericLimits<double>::Max();
                    for(int32 I:Free) if(RowOf[I]==Row && !OutPlan.Nodes[I].bIsPure && LaneOf[I]==Lane)
                    { ExecTop=FMath::Min(ExecTop,OutPlan.Nodes[I].PlannedY);ExecBottom=FMath::Max(ExecBottom,OutPlan.Nodes[I].PlannedY+OutPlan.Nodes[I].Height); }
                    const double Shift=BandTop-ExecTop;
                    for(int32 I:Free) if(RowOf[I]==Row && !OutPlan.Nodes[I].bIsPure && LaneOf[I]==Lane)
                        OutPlan.Nodes[I].PlannedY+=Shift;
                    double InputFloor=BandTop;
                    for(int32 I:Free) if(RowOf[I]==Row && !OutPlan.Nodes[I].bIsPure && LaneOf[I]==Lane)
                        InputFloor=FMath::Max(InputFloor,ValueReaderFloor(OutPlan.Nodes[I].Node,OutPlan.Nodes[I].PlannedY));
                    InputFloorByLane.Add(Lane,InputFloor);
                    double InputHeight=0.0;
                    if(const TSet<int32>* Inputs=LaneInputs.Find(Lane))
                        for(int32 I:*Inputs) InputHeight+=OutPlan.Nodes[I].Height+SpacingY;
                    Bottom=FMath::Max(Bottom,ExecBottom+Shift);
                    BandTop=FMath::Max(FMath::Max(BandTop+InLanePitch,ExecBottom+Shift+SpacingY),InputFloor+InputHeight+SpacingY);
                }
                const double FallbackInputFloor=Bottom+SpacingY;
				double PureBottom = Bottom;
				TArray<int32> PlacedPure;
				for (int32 I : Free) if (RowOf[I] == Row && !OutPlan.Nodes[I].bIsPure) PlacedPure.Add(I);
                // Visit consumers before their inputs, and siblings in visual pin order.
                // Graph insertion order is unrelated to pin order and can invert getter wires.
                TArray<int32> PureOrder;
                TSet<int32> OrderedInputs;
                TMap<int32, const FWire*> PreferredConsumer;
                TFunction<void(int32)> OrderInputs = [&](int32 Target)
                {
                    TArray<const FWire*> Incoming;
                    for (const FWire& W : OutPlan.Wires)
                        if (W.TargetNodeIndex == Target && W.SourceNodeIndex != Target
                            && RowOf.Contains(W.SourceNodeIndex) && RowOf[W.SourceNodeIndex] == Row
                            && OutPlan.Nodes[W.SourceNodeIndex].bIsPure)
                            Incoming.Add(&W);
                    Incoming.Sort([](const FWire& A, const FWire& B)
                    {
                        const double AY = FTurboMCPGraphTidy::AlignmentPinPosition(A.InputPin).Y;
                        const double BY = FTurboMCPGraphTidy::AlignmentPinPosition(B.InputPin).Y;
                        if (AY != BY) return AY < BY;
                        return A.SourceNodeIndex < B.SourceNodeIndex;
                    });
                    TArray<int32> Added;
                    for (const FWire* W : Incoming)
                    {
                        const int32 Source = W->SourceNodeIndex;
                        if (OrderedInputs.Contains(Source)) continue;
                        if (PureLane.Contains(Source)
                            && ((!OutPlan.Nodes[Target].bIsPure && LaneOf.Contains(Target) && PureLane[Source] != LaneOf[Target])
                                || (PureLane.Contains(Target) && PureLane[Source] != PureLane[Target]))) continue;
                        OrderedInputs.Add(Source);
                        PureOrder.Add(Source);
                        PreferredConsumer.Add(Source, W);
                        Added.Add(Source);
                    }
                    for (int32 Source : Added) OrderInputs(Source);
                };
                for (int32 I : PlacedPure) OrderInputs(I);
                // Include disconnected pure components without recursing forever on cycles.
                for (int32 I : Free)
                    if (RowOf[I] == Row && OutPlan.Nodes[I].bIsPure && !OrderedInputs.Contains(I))
                    { OrderedInputs.Add(I); PureOrder.Add(I); OrderInputs(I); }
				for (int32 I : PureOrder)
				{
					FNode& N = OutPlan.Nodes[I];
					if (RowOf[I] != Row || !N.bIsPure) continue;
					N.PlannedX = FreeStartX; N.PlannedY = RowY;
                    const FWire* Chosen = PreferredConsumer.FindRef(I);
                    if (!Chosen) for(const FWire& W:OutPlan.Wires)
                        if(W.SourceNodeIndex==I && W.TargetNodeIndex!=I && RowOf.Contains(W.TargetNodeIndex)
                            && (!PureLane.Contains(I)
                                || (!OutPlan.Nodes[W.TargetNodeIndex].bIsPure && LaneOf.Contains(W.TargetNodeIndex) && LaneOf[W.TargetNodeIndex]==PureLane[I])
                                || (PureLane.Contains(W.TargetNodeIndex) && PureLane[W.TargetNodeIndex]==PureLane[I])))
                            if(!Chosen || FTurboMCPGraphTidy::AlignmentPinPosition(W.InputPin).Y-OutPlan.Nodes[W.TargetNodeIndex].Y
                                < FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->InputPin).Y-OutPlan.Nodes[Chosen->TargetNodeIndex].Y) Chosen=&W;
                    if(Chosen)
                    {
                        const FNode& Consumer=OutPlan.Nodes[Chosen->TargetNodeIndex];
                        N.PlannedX=FMath::Max(FreeStartX,Consumer.PlannedX-N.Width-SpacingX);
                        N.PlannedY=FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->InputPin).Y+Consumer.PlannedY-Consumer.Y
                            -(FTurboMCPGraphTidy::AlignmentPinPosition(Chosen->OutputPin).Y-N.Y);
                        const double InputFloor = Consumer.bIsPure ? -TNumericLimits<double>::Max() : ValueReaderFloor(Consumer.Node,Consumer.PlannedY);
                        N.PlannedY = FMath::Max(N.PlannedY, InputFloor);
                    }
                    const double* LaneFloor=PureLane.Contains(I)?InputFloorByLane.Find(PureLane[I]):nullptr;
                    N.PlannedY = FMath::Max(N.PlannedY, LaneFloor?*LaneFloor:FallbackInputFloor);
                    // Try neighbouring input columns before sending a reader below a
                    // tall obstacle. Keep every candidate left of its consumer and in
                    // this region; use pin distance to choose among collision-free slots.
                    const double PreferredX=N.PlannedX, PreferredY=N.PlannedY;
                    TArray<double> ReaderColumns;
                    ReaderColumns.Add(PreferredX);
                    for(int32 P:PlacedPure)
                    {
                        const FNode& O=OutPlan.Nodes[P];
                        const double Left=O.PlannedX-N.Width-SpacingX;
                        const double Right=O.PlannedX+O.Width+SpacingX;
                        if(Left>=FreeStartX && Left<=PreferredX) ReaderColumns.AddUnique(Left);
                        if(Right>=FreeStartX && Right<=PreferredX) ReaderColumns.AddUnique(Right);
                    }
                    double BestDistance=TNumericLimits<double>::Max();
                    double BestX=PreferredX,BestY=PreferredY;
                    for(double ReaderX:ReaderColumns)
                    {
                        double ReaderY=PreferredY;
                        for(int32 Pass=0;Pass<=PlacedPure.Num();++Pass)
                        {
                            double NextY=ReaderY;
                            for(int32 P:PlacedPure)
                            {
                                const FNode& O=OutPlan.Nodes[P];
                                if(ReaderX<O.PlannedX+O.Width+SpacingX && ReaderX+N.Width+SpacingX>O.PlannedX
                                    && ReaderY<O.PlannedY+O.Height+SpacingY && ReaderY+N.Height+SpacingY>O.PlannedY)
                                    NextY=FMath::Max(NextY,O.PlannedY+O.Height+SpacingY);
                            }
                            if(NextY==ReaderY) break;
                            ReaderY=NextY;
                        }
                        double Distance=FVector2D::Distance(FVector2D(ReaderX,ReaderY),FVector2D(PreferredX,PreferredY));
                        if(Chosen)
                        {
                            const FNode& Consumer=OutPlan.Nodes[Chosen->TargetNodeIndex];
                            const FVector2D Input=AlignmentPinPosition(Chosen->InputPin)+FVector2D(Consumer.PlannedX-Consumer.X,Consumer.PlannedY-Consumer.Y);
                            const FVector2D Output=AlignmentPinPosition(Chosen->OutputPin)+FVector2D(ReaderX-N.X,ReaderY-N.Y);
                            Distance=FVector2D::Distance(Input,Output);
                        }
                        if(FMath::IsFinite(Distance) && Distance<BestDistance)
                        { BestDistance=Distance;BestX=ReaderX;BestY=ReaderY; }
                    }
                    N.PlannedX=BestX;N.PlannedY=BestY;
					PlacedPure.Add(I);
					PureBottom = FMath::Max(PureBottom, N.PlannedY + N.Height);
				}
                // Provisional bands reserve enough space for any input stacking.
                // Once input columns exist, pack by their actual union height rather
                // than the sum of every reader's height (parallel columns overlap in Y).
                double PackedBandTop=RowY;
                for(int32 Lane:LaneOrder)
                {
                    TArray<int32> BandMembers;
                    double BandMin=TNumericLimits<double>::Max();
                    double BandMax=-TNumericLimits<double>::Max();
                    for(int32 I:Free)
                    {
                        if(RowOf[I]!=Row) continue;
                        const bool InBand=OutPlan.Nodes[I].bIsPure
                            ? PureLane.Contains(I) && PureLane[I]==Lane
                            : LaneOf.Contains(I) && LaneOf[I]==Lane;
                        if(!InBand) continue;
                        BandMembers.Add(I);
                        BandMin=FMath::Min(BandMin,OutPlan.Nodes[I].PlannedY);
                        BandMax=FMath::Max(BandMax,OutPlan.Nodes[I].PlannedY+OutPlan.Nodes[I].Height);
                    }
                    if(BandMembers.IsEmpty()) continue;
                    const double BandShift=PackedBandTop-BandMin;
                    for(int32 I:BandMembers) OutPlan.Nodes[I].PlannedY+=BandShift;
                    PackedBandTop+=FMath::Max(InLanePitch,BandMax-BandMin+BlockBandGap);
                }
                Bottom=RowY;PureBottom=RowY;
                for(int32 I:Free) if(RowOf[I]==Row)
                {
                    const FNode& PlacedNode=OutPlan.Nodes[I];
                    if(PlacedNode.bIsPure) PureBottom=FMath::Max(PureBottom,PlacedNode.PlannedY+PlacedNode.Height);
                    else Bottom=FMath::Max(Bottom,PlacedNode.PlannedY+PlacedNode.Height);
                }
				double Minimum=RowY;
                for(int32 I:Free) if(RowOf[I]==Row) Minimum=FMath::Min(Minimum,OutPlan.Nodes[I].PlannedY);
                const double Shift=RowY-Minimum;
                for(int32 I:Free) if(RowOf[I]==Row) OutPlan.Nodes[I].PlannedY+=Shift;
                RowY = FMath::Max(Bottom, PureBottom) + Shift + SpacingY;
			}
            // Native Sequence lanes: stage columns are shared across outputs rather than
            // inherited from graph-wide dependency columns (which include cross-row data).
            for(int32 SeqIndex:Free)
            {
                const FNode& SeqNode=OutPlan.Nodes[SeqIndex];
                if(!OutPlan.WrapAfterNodes.IsEmpty() || !SeqNode.Node->IsA<UK2Node_ExecutionSequence>()) continue;
                TArray<TArray<int32>> Lanes; TSet<int32> OwnedCalls; bool Supported=true;
                for(UEdGraphPin* Exec:SeqNode.Node->Pins) if(Exec && Exec->Direction==EGPD_Output && Exec->PinType.PinCategory==TEXT("exec"))
                {
                    int32 Next=INDEX_NONE;
                    for(const FWire& W:OutPlan.Wires) if(!W.bControlFeedback && W.OutputPin==Exec) { if(Next!=INDEX_NONE) Supported=false; Next=W.TargetNodeIndex; }
                    TArray<int32> Lane;
                    while(Supported && Next!=INDEX_NONE)
                    {
                        if(!Free.Contains(Next) || OwnedCalls.Contains(Next) || OutPlan.Nodes[Next].bIsPure || OutPlan.Nodes[Next].Node->IsA<UK2Node_ExecutionSequence>()) { Supported=false; break; }
                        int32 Inputs=0,Outputs=0,Following=INDEX_NONE;
                        for(const FWire& W:OutPlan.Wires) if(!W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec"))
                        {
                            if(W.TargetNodeIndex==Next) ++Inputs;
                            if(W.SourceNodeIndex==Next) { ++Outputs; Following=W.TargetNodeIndex; }
                        }
                        if(Inputs!=1 || Outputs>1) { Supported=false; break; }
                        Lane.Add(Next); OwnedCalls.Add(Next); Next=Following;
                    }
                    if(!Lane.IsEmpty()) Lanes.Add(Lane);
                }
                if(!Supported || Lanes.Num()<2) continue; // General branches retain conservative layout/guards.
                TMap<int32,int32> PureOwner;
                TFunction<void(int32,int32)> Assign=[&](int32 Target,int32 Owner)
                {
                    for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && Free.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure && !PureOwner.Contains(W.SourceNodeIndex))
                    { PureOwner.Add(W.SourceNodeIndex,Owner); Assign(W.SourceNodeIndex,Owner); }
                };
                for(const TArray<int32>& Lane:Lanes) for(int32 Call:Lane) Assign(Call,Call);
                int32 Stages=0; for(const auto& Lane:Lanes) Stages=FMath::Max(Stages,Lane.Num());
                TArray<double> Widths,Reserves,Columns; Widths.Init(0,Stages); Reserves.Init(0,Stages); Columns.Init(0,Stages);
                auto ReserveFor=[&](int32 Call)
                {
                    double Width=0;
                    // Sibling inputs occupy separate columns, not extra vertical lanes.
                    for(const auto& Pair:PureOwner) if(Pair.Value==Call)
                        Width+=OutPlan.Nodes[Pair.Key].Width+SpacingX;
                    return Width;
                };
                for(const auto& Lane:Lanes) for(int32 Stage=0;Stage<Lane.Num();++Stage)
                { Widths[Stage]=FMath::Max(Widths[Stage],OutPlan.Nodes[Lane[Stage]].Width); Reserves[Stage]=FMath::Max(Reserves[Stage],ReserveFor(Lane[Stage])); }
                const double Left=SeqNode.PlannedX+SeqNode.Width+SpacingX;
                double Column=Left;
                for(int32 Stage=0;Stage<Stages;++Stage) { Column+=Reserves[Stage]; Columns[Stage]=Column; Column+=Widths[Stage]+SpacingX; }
                double LaneY=SeqNode.PlannedY;
                for(const FWire& W:OutPlan.Wires)
                    if(W.SourceNodeIndex==SeqIndex && W.TargetNodeIndex==Lanes[0][0] && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec"))
                        LaneY=AlignmentPinPosition(W.OutputPin).Y+SeqNode.PlannedY-SeqNode.Y
                            -(AlignmentPinPosition(W.InputPin).Y-OutPlan.Nodes[Lanes[0][0]].Y);
                double PreviousRight=Left,PreviousLaneBottom=LaneY; TSet<int32> EarlierCalls;
                for(int32 LaneIndex=0;LaneIndex<Lanes.Num();++LaneIndex)
                {
                    const auto& Lane=Lanes[LaneIndex]; TSet<int32> Members;
                    for(int32 Call:Lane) Members.Add(Call);
                    for(const auto& Pair:PureOwner) if(Members.Contains(Pair.Value)) Members.Add(Pair.Key);
                    bool Summary=false;
                    if(LaneIndex==Lanes.Num()-1) for(const FWire& W:OutPlan.Wires)
                        if(EarlierCalls.Contains(W.SourceNodeIndex) && Members.Contains(W.TargetNodeIndex) && W.OutputPin->PinType.PinCategory!=TEXT("exec")) Summary=true;
                    const double Extra=Summary?FMath::Max(0.0,PreviousRight+SpacingX-Left):0.0;
                    double Bottom=LaneY;
                    for(int32 Stage=0;Stage<Lane.Num();++Stage)
                    {
                        FNode& N=OutPlan.Nodes[Lane[Stage]]; N.PlannedX=Columns[Stage]+Extra; N.PlannedY=LaneY;
                        if(Stage>0) for(const FWire& W:OutPlan.Wires) if(W.SourceNodeIndex==Lane[Stage-1] && W.TargetNodeIndex==Lane[Stage] && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec"))
                            N.PlannedY=AlignmentPinPosition(W.OutputPin).Y+OutPlan.Nodes[Lane[Stage-1]].PlannedY-OutPlan.Nodes[Lane[Stage-1]].Y-(AlignmentPinPosition(W.InputPin).Y-N.Y);
                        Bottom=FMath::Max(Bottom,N.PlannedY+N.Height);
                    }
                    // Readers may sit alongside tall calls while clearing the exec corridor.
                    double SequenceInputFloor = -TNumericLimits<double>::Max();
                    for(int32 Call:Lane)
                        SequenceInputFloor=FMath::Max(SequenceInputFloor,ValueReaderFloor(OutPlan.Nodes[Call].Node,OutPlan.Nodes[Call].PlannedY));
                    TSet<int32> Placed; for(int32 Call:Lane) Placed.Add(Call);
                    // Consumer-first traversal places pure trees using real pin offsets.
                    TFunction<void(int32)> PlaceInputs=[&](int32 Target)
                    {
                        TArray<const FWire*> Inputs;
                        for(const FWire& W:OutPlan.Wires) if(W.TargetNodeIndex==Target && Members.Contains(W.SourceNodeIndex) && OutPlan.Nodes[W.SourceNodeIndex].bIsPure) Inputs.Add(&W);
                        Inputs.Sort([](const FWire& A,const FWire& B) { return AlignmentPinPosition(A.InputPin).Y<AlignmentPinPosition(B.InputPin).Y; });
                        for(const FWire* W:Inputs)
                        {
                            const int32 I=W->SourceNodeIndex; if(Placed.Contains(I)) continue; Placed.Add(I);
                            FNode& N=OutPlan.Nodes[I]; const FNode& Consumer=OutPlan.Nodes[Target];
                            N.PlannedX=Consumer.PlannedX-N.Width-SpacingX;
                            N.PlannedY=AlignmentPinPosition(W->InputPin).Y+Consumer.PlannedY-Consumer.Y-(AlignmentPinPosition(W->OutputPin).Y-N.Y);
                            if(!Consumer.bIsPure) N.PlannedY=FMath::Max(N.PlannedY,ValueReaderFloor(Consumer.Node,Consumer.PlannedY));
                            N.PlannedY=FMath::Max(N.PlannedY,SequenceInputFloor);
                            for(int32 Pass=0;Pass<Members.Num();++Pass)
                            {
                                double NextX=N.PlannedX;
                                for(int32 Other:Placed) if(Other!=I)
                                {
                                    const FNode& O=OutPlan.Nodes[Other];
                                    if(N.PlannedX<O.PlannedX+O.Width+SpacingX && N.PlannedX+N.Width+SpacingX>O.PlannedX && N.PlannedY<O.PlannedY+O.Height+SpacingY && N.PlannedY+N.Height+SpacingY>O.PlannedY)
                                        NextX=FMath::Min(NextX,O.PlannedX-N.Width-SpacingX);
                                }
                                if(NextX==N.PlannedX) break; N.PlannedX=NextX;
                            }
                            Bottom=FMath::Max(Bottom,N.PlannedY+N.Height); PlaceInputs(I);
                        }
                    };
                    for(int32 Call:Lane) PlaceInputs(Call);
                    double MinY=LaneY;
                    for(int32 I:Members) MinY=FMath::Min(MinY,OutPlan.Nodes[I].PlannedY);
                    // Preserve lane zero on the Sequence pin. Later rows may grow only
                    // when their measured outlines need clearance from the previous row.
                    const double ShiftY=LaneIndex==0?0.0:FMath::Max(0.0,PreviousLaneBottom+BlockBandGap-MinY);
                    for(int32 I:Members) { OutPlan.Nodes[I].PlannedY+=ShiftY; PreviousRight=FMath::Max(PreviousRight,OutPlan.Nodes[I].PlannedX+OutPlan.Nodes[I].Width); }
                    for(int32 Call:Lane) EarlierCalls.Add(Call);
                    PreviousLaneBottom=Bottom+ShiftY;
                    LaneY=FMath::Max(LaneY+ShiftY+InLanePitch,PreviousLaneBottom+BlockBandGap);
                }
            }
	};

	const bool bFlowComments = OutPlan.bCompactBlocks && OutPlan.SelectedNodeKeys.IsEmpty();
	for (const int32 BoxIndex : BoxOrder)
	{
        // Whole-graph compact mode plans connected nodes together, then fits
        // comments to their original members. Scoped and rows modes keep anchors.
        if (bFlowComments) continue;
		FBox& Box = OutPlan.Boxes[BoxIndex];
		if (OutPlan.SelectedNodeKeys.Num() > 0)
		{
			bool AllSelected = !Box.MemberKeys.IsEmpty();
            for (const FString& Key : Box.MemberKeys)
            {
                const int32* Index = NodeIndexByKey.Find(Key);
                if (Index && !OutPlan.Nodes[*Index].bIsComment && !OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[*Index].Key)) AllSelected = false;
            }
            if (!AllSelected)
            {
                for (const FString& Key : Box.MemberKeys)
                    if (const int32* I = NodeIndexByKey.Find(Key))
                        if (OwnerOfNode[*I] == BoxIndex && !OutPlan.Nodes[*I].bIsComment && OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[*I].Key))
                        { OutError = FString::Printf(TEXT("selected node %s belongs to partially selected comment %s; select the whole comment, or include @unboxed with nodeIds to keep boxed nodes fixed"), *OutPlan.Nodes[*I].Key, *Box.Key); return false; }
                Box.bWouldFit = false; continue;
            }
		}

		// The nodes THIS box lays out: its own members, minus those a deeper box owns.
		TArray<int32> Owned;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			if (OwnerOfNode[NodeIndex] != BoxIndex || OutPlan.Nodes[NodeIndex].bIsComment)
			{
				continue;
			}
			Owned.Add(NodeIndex);
		}

		if (Owned.Num() == 0)
		{
			// A box with nothing to arrange is LEFT ALONE rather than collapsed to nothing: an empty
			// box is a label the user made, and shrinking it would be a change nobody asked for.
			Box.bWouldFit = false;
			continue;
		}

		// Left to right in column order, then by graph order then key so the result is deterministic.
		Owned.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
		{
			const FNode& L = OutPlan.Nodes[InLeft];
			const FNode& R = OutPlan.Nodes[InRight];
			if (L.Column != R.Column)
			{
				return L.Column < R.Column;
			}
			if (L.GraphOrder != R.GraphOrder)
			{
				return L.GraphOrder < R.GraphOrder;
			}
			return L.Key < R.Key;
		});

		// A BOX'S TOP-LEFT CORNER IS NEVER MOVED. Only sizes change, so the start position is the
		// box's own corner plus its padding - which is what `P/E-COMMENT`'s fit does.
		const double StartX = Box.X + FTurboMCPGraphTidy::FitPadding;
		double StartY = Box.Y + FTurboMCPGraphTidy::TitleBarHeight + FTurboMCPGraphTidy::FitPadding;
        for (int32 Child : Box.ChildBoxIndices) StartY = FMath::Max(StartY, OutPlan.Boxes[Child].Y + OutPlan.Boxes[Child].PlannedHeight + SpacingY);

		ArrangeRows(Owned, StartX, StartY);

		// FIT THE BOX TO ITS CONTENTS, AND ONLY ITS SIZE. A child box is already final because the
		// deepest boxes were processed first, so a whole child rectangle counts as one block here.
		bool bAny = false;
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
		for (const int32 NodeIndex : Owned)
		{
			const FNode& Entry = OutPlan.Nodes[NodeIndex];
			const double Left = Entry.PlannedX;
			const double Top = Entry.PlannedY;
			const double Right = Entry.PlannedX + Entry.Width;
			const double Bottom = Entry.PlannedY + Entry.Height;
			if (!bAny)
			{
				MinX = Left; MinY = Top; MaxX = Right; MaxY = Bottom;
				bAny = true;
			}
			else
			{
				MinX = FMath::Min(MinX, Left);
				MinY = FMath::Min(MinY, Top);
				MaxX = FMath::Max(MaxX, Right);
				MaxY = FMath::Max(MaxY, Bottom);
			}
		}
		for (const int32 ChildIndex : Box.ChildBoxIndices)
		{
			const FBox& Child = OutPlan.Boxes[ChildIndex];
			const double Left = Child.X;
			const double Top = Child.Y;
			const double Right = Child.X + Child.PlannedWidth;
			const double Bottom = Child.Y + Child.PlannedHeight;
			if (!bAny)
			{
				MinX = Left; MinY = Top; MaxX = Right; MaxY = Bottom;
				bAny = true;
			}
			else
			{
				MinX = FMath::Min(MinX, Left);
				MinY = FMath::Min(MinY, Top);
				MaxX = FMath::Max(MaxX, Right);
				MaxY = FMath::Max(MaxY, Bottom);
			}
		}

		if (bAny)
		{
			// The corner is Box.X and Box.Y AS STORED, so the fitted rectangle starts from where the
			// user put the box and grows right and down. `+ TitleBarHeight` because the stored Y is the
			// top of the title strip, which is above the content rectangle.
			const double WantedRight = MaxX + FTurboMCPGraphTidy::FitPadding;
			const double WantedBottom = MaxY + FTurboMCPGraphTidy::FitPadding;
			Box.PlannedWidth = FMath::Max(Box.Width, WantedRight - Box.X);
			Box.PlannedHeight = FMath::Max(Box.Height,
				WantedBottom - Box.Y + FTurboMCPGraphTidy::TitleBarHeight);
			Box.bWouldFit = !FMath::IsNearlyEqual(Box.PlannedWidth, Box.Width)
				|| !FMath::IsNearlyEqual(Box.PlannedHeight, Box.Height);
		}
	}

	// THE BOXLESS GROUP: ordered by execution order and placed left to right on one row, to the right
	// of every box so it cannot land inside one the user drew.
	{
		TArray<int32> Free;
		const bool bScoped = OutPlan.SelectedNodeKeys.Num() > 0;
		for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
		{
			const FNode& Candidate = OutPlan.Nodes[NodeIndex];
			if (Candidate.bIsComment || Candidate.bUnreachable)
			{
				continue;
			}
			if (bScoped)
			{
				if (OutPlan.SelectedNodeKeys.Contains(Candidate.Key) && OwnerOfNode[NodeIndex] == INDEX_NONE)
				{
					Free.Add(NodeIndex);
				}
			}
			else if (bFlowComments || OwnerOfNode[NodeIndex] == INDEX_NONE)
			{
				Free.Add(NodeIndex);
			}
		}

		if (Free.Num() > 0)
		{
            // Moving nodes are not fixed obstacles: including their old right
            // edges makes each forced repeat push the group farther right.
            double FreeStartX = OutPlan.Nodes[Free[0]].X;
            double FreeStartY = OutPlan.Nodes[Free[0]].Y;
            TSet<const UEdGraphNode*> MovingNodes;
            for (const int32 Index : Free)
            {
                FreeStartX = FMath::Min(FreeStartX, OutPlan.Nodes[Index].X);
                FreeStartY = FMath::Min(FreeStartY, OutPlan.Nodes[Index].Y);
                MovingNodes.Add(OutPlan.Nodes[Index].Node);
            }
			for (const FNode& Candidate : OutPlan.Nodes)
			{
				if (Candidate.bIsComment || MovingNodes.Contains(Candidate.Node))
				{
					continue;
				}
				const double Right = Candidate.X + Candidate.Width + FTurboMCPGraphTidy::BoxGap;
				FreeStartX = FMath::Max(FreeStartX, Right);
			}
			for (const FBox& Box : OutPlan.Boxes)
			{
                if (bFlowComments && !Box.MemberKeys.IsEmpty()) continue;
				const double Right = Box.X + Box.PlannedWidth + FTurboMCPGraphTidy::BoxGap;
				FreeStartX = FMath::Max(FreeStartX, Right);
			}

			Free.Sort([&OutPlan](const int32 InLeft, const int32 InRight)
			{
				const FNode& L = OutPlan.Nodes[InLeft];
				const FNode& R = OutPlan.Nodes[InRight];
				if (L.Column != R.Column)
				{
					return L.Column < R.Column;
				}
				if (L.GraphOrder != R.GraphOrder)
				{
					return L.GraphOrder < R.GraphOrder;
				}
				return L.Key < R.Key;
			});

            const bool bPureSelection = bScoped && !Free.ContainsByPredicate([&](int32 I)
            { return !OutPlan.Nodes[I].bIsPure; });
            bool bPlacedDependencies = false;
            if (bPureSelection)
            {
                // A selected data expression belongs before its fixed consumer, not
                // beyond every fixed node. Lay out the whole selected dependency DAG
                // so existing and newly appended readers obey the same ordering.
                TSet<int32> Selected;
                for (int32 I : Free) Selected.Add(I);
                TArray<int32> Order;
                TMap<int32, int32> Degree, Depth;
                for (int32 I : Free) { Degree.Add(I, 0); Depth.Add(I, 0); }
                double ConsumerLeft = TNumericLimits<double>::Max();
                for (const FWire& W : OutPlan.Wires)
                    if (Selected.Contains(W.SourceNodeIndex))
                    {
                        if (Selected.Contains(W.TargetNodeIndex)) ++Degree[W.TargetNodeIndex];
                        else ConsumerLeft = FMath::Min(ConsumerLeft, OutPlan.Nodes[W.TargetNodeIndex].X);
                    }
                for (int32 I : Free) if (Degree[I] == 0) Order.Add(I);
                for (int32 Cursor = 0; Cursor < Order.Num(); ++Cursor)
                    for (const FWire& W : OutPlan.Wires)
                        if (W.SourceNodeIndex == Order[Cursor] && Selected.Contains(W.TargetNodeIndex))
                        {
                            Depth[W.TargetNodeIndex] = FMath::Max(Depth[W.TargetNodeIndex], Depth[Order[Cursor]] + 1);
                            if (--Degree[W.TargetNodeIndex] == 0) Order.Add(W.TargetNodeIndex);
                        }
                if (Order.Num() == Free.Num() && ConsumerLeft < TNumericLimits<double>::Max())
                {
                    int32 MaxDepth = 0;
                    for (int32 I : Order) MaxDepth = FMath::Max(MaxDepth, Depth[I]);
                    TArray<double> Widths, Columns;
                    Widths.Init(0.0, MaxDepth + 1); Columns.Init(0.0, MaxDepth + 1);
                    for (int32 I : Order) Widths[Depth[I]] = FMath::Max(Widths[Depth[I]], OutPlan.Nodes[I].Width);
                    double RightEdge = ConsumerLeft - SpacingX;
                    for (int32 D = MaxDepth; D >= 0; --D)
                    { Columns[D] = RightEdge - Widths[D]; RightEdge = Columns[D] - SpacingX; }
                    double NextRow = TNumericLimits<double>::Max();
                    for (int32 I : Free) NextRow = FMath::Min(NextRow, OutPlan.Nodes[I].Y);
                    TSet<int32> Positioned;
                    TFunction<void(int32)> PlaceInputs = [&](int32 I)
                    {
                        if (Positioned.Contains(I)) return;
                        TArray<const FWire*> Inputs;
                        for (const FWire& W : OutPlan.Wires)
                            if (W.TargetNodeIndex == I && Selected.Contains(W.SourceNodeIndex)) Inputs.Add(&W);
                        Inputs.Sort([](const FWire& A, const FWire& B)
                        { return AlignmentPinPosition(A.InputPin).Y < AlignmentPinPosition(B.InputPin).Y; });
                        for (const FWire* W : Inputs) PlaceInputs(W->SourceNodeIndex);
                        FNode& N = OutPlan.Nodes[I];
                        N.PlannedX = Columns[Depth[I]] + Widths[Depth[I]] - N.Width;
                        if (Inputs.IsEmpty())
                        { N.PlannedY = NextRow; NextRow += N.Height + SpacingY; }
                        else
                        {
                            double Sum = 0.0;
                            for (const FWire* W : Inputs)
                            {
                                const FNode& Source = OutPlan.Nodes[W->SourceNodeIndex];
                                Sum += AlignmentPinPosition(W->OutputPin).Y + Source.PlannedY - Source.Y
                                    - (AlignmentPinPosition(W->InputPin).Y - N.Y);
                            }
                            N.PlannedY = Sum / Inputs.Num();
                        }
                        // Shared inputs can bring two consumers onto the same row.
                        // Reserve measured rectangles even for these DAG joins.
                        for (int32 Pass = 0; Pass <= Positioned.Num(); ++Pass)
                        {
                            bool bShifted = false;
                            for (int32 J : Positioned)
                            {
                                const FNode& Other = OutPlan.Nodes[J];
                                if (N.PlannedX < Other.PlannedX + Other.Width && N.PlannedX + N.Width > Other.PlannedX
                                    && N.PlannedY < Other.PlannedY + Other.Height + SpacingY && N.PlannedY + N.Height + SpacingY > Other.PlannedY)
                                { N.PlannedY = Other.PlannedY + Other.Height + SpacingY; bShifted = true; }
                            }
                            if (!bShifted) break;
                        }
                        Positioned.Add(I);
                    };
                    for (int32 K = Order.Num() - 1; K >= 0; --K) PlaceInputs(Order[K]);
                    // Translate the expression as one block below fixed obstacles;
                    // never move an unselected node or change a wire to make room.
                    for (int32 Pass = 0; Pass <= OutPlan.Nodes.Num(); ++Pass)
                    {
                        double Shift = 0.0;
                        for (int32 I : Free)
                            for (int32 J = 0; J < OutPlan.Nodes.Num(); ++J)
                                if (!Selected.Contains(J))
                                {
                                    const FNode& N = OutPlan.Nodes[I]; const FNode& O = OutPlan.Nodes[J];
                                    if (N.PlannedX < O.X + O.Width + SpacingX && N.PlannedX + N.Width + SpacingX > O.X
                                        && N.PlannedY < O.Y + O.Height + SpacingY && N.PlannedY + N.Height + SpacingY > O.Y)
                                        Shift = FMath::Max(Shift, O.Y + O.Height + SpacingY - N.PlannedY);
                                }
                        if (Shift <= 0.0) break;
                        for (int32 I : Free) OutPlan.Nodes[I].PlannedY += Shift;
                    }
                    bPlacedDependencies = true;
                }
            }
            if (!bPlacedDependencies) ArrangeRows(Free, FreeStartX, FreeStartY);
		}
	}

    // Re-anchor each unboxed selected component at its own boundary connections.
    // The row planner establishes internal order; unrelated distant sections must
    // not determine the component's final position. All translations are planned.
    if (!OutPlan.SelectedNodeKeys.IsEmpty())
    {
        TSet<int32> Pending;
        for (int32 I = 0; I < OutPlan.Nodes.Num(); ++I)
            if (!OutPlan.Nodes[I].bIsComment && !OutPlan.Nodes[I].bUnreachable
                && OwnerOfNode[I] == INDEX_NONE && OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[I].Key)) Pending.Add(I);
        while (!Pending.IsEmpty())
        {
            // Stable seed order keeps identical previews deterministic.
            int32 Seed = MAX_int32;
            for (int32 I : Pending) Seed = FMath::Min(Seed, I);
            TArray<int32> Component{Seed};
            TSet<int32> Members{Seed};
            Pending.Remove(Seed);
            for (int32 Cursor = 0; Cursor < Component.Num(); ++Cursor)
                for (const FWire& Wire : OutPlan.Wires)
                {
                    const int32 Other = Wire.SourceNodeIndex == Component[Cursor] ? Wire.TargetNodeIndex
                        : Wire.TargetNodeIndex == Component[Cursor] ? Wire.SourceNodeIndex : INDEX_NONE;
                    if (Pending.Contains(Other))
                    { Pending.Remove(Other); Members.Add(Other); Component.Add(Other); }
                }
            TArray<const FWire*> Boundary;
            TArray<FVector2D> Candidates;
            TArray<double> AnchorPreferences;
            auto PlannedPin = [&](const UEdGraphPin* Pin, int32 Index)
            {
                const FNode& Node = OutPlan.Nodes[Index];
                return AlignmentPinPosition(Pin) + FVector2D(Node.PlannedX - Node.X, Node.PlannedY - Node.Y);
            };
            for (const FWire& Wire : OutPlan.Wires)
            {
                const bool SourceMoves = Members.Contains(Wire.SourceNodeIndex);
                const bool TargetMoves = Members.Contains(Wire.TargetNodeIndex);
                if (SourceMoves == TargetMoves || !Wire.OutputPin || !Wire.InputPin) continue;
                const int32 FixedIndex = SourceMoves ? Wire.TargetNodeIndex : Wire.SourceNodeIndex;
                if (OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[FixedIndex].Key)) continue;
                Boundary.Add(&Wire);
                const FNode& Source = OutPlan.Nodes[Wire.SourceNodeIndex];
                const FNode& Target = OutPlan.Nodes[Wire.TargetNodeIndex];
                const FVector2D Start = PlannedPin(Wire.OutputPin, Wire.SourceNodeIndex);
                const FVector2D End = PlannedPin(Wire.InputPin, Wire.TargetNodeIndex);
                const double ShiftX = SourceMoves ? Target.X - SpacingX - Source.Width - Source.PlannedX
                    : Source.X + Source.Width + SpacingX - Target.PlannedX;
                const double ShiftY = SourceMoves ? End.Y - Start.Y : Start.Y - End.Y;
                Candidates.Add(FVector2D(ShiftX, ShiftY));
                // Break near-ties in favour of the incoming execution anchor.
                // Large distance differences still dominate this small preference.
                const bool ExecWire = !Wire.bControlFeedback && Wire.OutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                AnchorPreferences.Add(ExecWire ? (SourceMoves ? 32.0 : 0.0) : 64.0);
            }
            if (Boundary.IsEmpty()) continue;
            // A selected component may already have a valid boundary position.
            // Include that anchor so improving another component need not displace it.
            const FNode& SeedNode = OutPlan.Nodes[Seed];
            Candidates.Add(FVector2D(SeedNode.X - SeedNode.PlannedX, SeedNode.Y - SeedNode.PlannedY));
            AnchorPreferences.Add(0.0);
            double BestCost = TNumericLimits<double>::Max();
            int32 BestBackwardCount = MAX_int32;
            FVector2D BestShift = FVector2D::ZeroVector;
            for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
                for (int32 Row = -4; Row <= 4; ++Row)
                {
                    const FVector2D& Base = Candidates[CandidateIndex];
                    const FVector2D Shift(FMath::RoundToDouble(Base.X), FMath::RoundToDouble(Base.Y + Row * SpacingY));
                    bool Collision = false;
                    for (int32 I : Component)
                    {
                        const FNode& Node = OutPlan.Nodes[I];
                        const double X = Node.PlannedX + Shift.X, Y = Node.PlannedY + Shift.Y;
                        for (int32 J = 0; J < OutPlan.Nodes.Num(); ++J)
                        {
                            if (Members.Contains(J)) continue;
                            const FNode& Other = OutPlan.Nodes[J];
                            if (X < Other.PlannedX + Other.Width + 16.0 && X + Node.Width + 16.0 > Other.PlannedX
                                && Y < Other.PlannedY + Other.Height + 16.0 && Y + Node.Height + 16.0 > Other.PlannedY)
                            { Collision = true; break; }
                        }
                        if (Collision) break;
                    }
                    if (Collision) continue;
                    double Cost = AnchorPreferences[CandidateIndex];
                    bool BoundarySafe = true;
                    int32 BackwardCount = 0;
                    double OriginalTotal = 0.0, CandidateTotal = 0.0, ComponentAllowance = 0.0;
                    for (const FWire* Wire : Boundary)
                    {
                        FVector2D Start = PlannedPin(Wire->OutputPin, Wire->SourceNodeIndex);
                        FVector2D End = PlannedPin(Wire->InputPin, Wire->TargetNodeIndex);
                        if (Members.Contains(Wire->SourceNodeIndex)) Start += Shift; else End += Shift;
                        if (Start.X > End.X) ++BackwardCount;
                        const double CandidateLength = FVector2D::Distance(Start, End);
                        const double OriginalLength = FVector2D::Distance(AlignmentPinPosition(Wire->OutputPin), AlignmentPinPosition(Wire->InputPin));
                        const double Allowance = FMath::Clamp(FMath::Max(OutPlan.Nodes[Wire->SourceNodeIndex].Width,
                            OutPlan.Nodes[Wire->TargetNodeIndex].Width), 64.0, 256.0);
                        // Search within the same bounds enforced after planning and application.
                        // A shorter total cannot compensate for one unsafe boundary wire.
                        if (CandidateLength > OriginalLength + Allowance + 1.0) { BoundarySafe = false; break; }
                        OriginalTotal += OriginalLength;
                        CandidateTotal += CandidateLength;
                        ComponentAllowance = FMath::Max(ComponentAllowance, Allowance);
                        Cost += CandidateLength;
                        // Favor forward execution and its existing lane, without
                        // moving either fixed endpoint to make a candidate fit.
                        if (Wire->OutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                            Cost += FMath::Abs(Start.Y - End.Y) + 4.0 * FMath::Max(0.0, Start.X - End.X);
                    }
                    if (!BoundarySafe || CandidateTotal > OriginalTotal + ComponentAllowance + 1.0) continue;
                    // Match the final quality ordering: forward flow precedes shorter wires.
                    if (BackwardCount < BestBackwardCount || (BackwardCount == BestBackwardCount && Cost < BestCost))
                    { BestBackwardCount = BackwardCount; BestCost = Cost; BestShift = Shift; }
                }
            if (BestCost < TNumericLimits<double>::Max())
                for (int32 I : Component)
                { OutPlan.Nodes[I].PlannedX += BestShift.X; OutPlan.Nodes[I].PlannedY += BestShift.Y; }
        }
    }

    // Retain the execution lane when a selected terminal follows a fixed node.
    // The scoped row layout must not introduce a diagonal wire across data nodes.
    if (!OutPlan.SelectedNodeKeys.IsEmpty())
        for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
        {
            FNode& Moving = OutPlan.Nodes[NodeIndex];
            if (Moving.bIsPure || Moving.bIsComment || OwnerOfNode[NodeIndex] != INDEX_NONE
                || !OutPlan.SelectedNodeKeys.Contains(Moving.Key)) continue;
            const FWire* Incoming = nullptr;
            int32 IncomingCount = 0;
            for (const FWire& Wire : OutPlan.Wires)
                if (!Wire.bControlFeedback && Wire.TargetNodeIndex == NodeIndex && Wire.InputPin
                    && Wire.InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                { Incoming = &Wire; ++IncomingCount; }
            if (IncomingCount != 1 || Incoming == nullptr) continue;
            if (OutPlan.bCompactBlocks)
            {
                // This adjustment moves one node, not its downstream chain.
                // Preserve the compact planner's internal execution rows when
                // a selected successor still depends on this node's position.
                // Component anchoring above already considers the fixed boundary.
                bool bHasSelectedSuccessor = false;
                for (const FWire& Wire : OutPlan.Wires)
                    if (!Wire.bControlFeedback && Wire.SourceNodeIndex == NodeIndex
                        && Wire.OutputPin && Wire.OutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
                        && OutPlan.Nodes.IsValidIndex(Wire.TargetNodeIndex)
                        && OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[Wire.TargetNodeIndex].Key))
                    { bHasSelectedSuccessor = true; break; }
                if (bHasSelectedSuccessor) continue;
            }
            const FNode& Source = OutPlan.Nodes[Incoming->SourceNodeIndex];
            if (OutPlan.SelectedNodeKeys.Contains(Source.Key)) continue;
            const double AlignedY = FMath::RoundToDouble(AlignmentPinPosition(Incoming->OutputPin).Y
                + Source.PlannedY - Source.Y - (AlignmentPinPosition(Incoming->InputPin).Y - Moving.Y));
            bool bBlocked = false;
            for (int32 OtherIndex = 0; OtherIndex < OutPlan.Nodes.Num(); ++OtherIndex)
                if (OtherIndex != NodeIndex)
                {
                    const FNode& Other = OutPlan.Nodes[OtherIndex];
                    // A selected single-consumer reader is repositioned below;
                    // its temporary row is not a fixed obstacle for the exec lane.
                    if (Other.bIsGetter && Other.bIsPure && OwnerOfNode[OtherIndex] == INDEX_NONE
                        && OutPlan.SelectedNodeKeys.Contains(Other.Key))
                    {
                        int32 OutgoingCount = 0;
                        for (const FWire& Wire : OutPlan.Wires) if (Wire.SourceNodeIndex == OtherIndex) ++OutgoingCount;
                        if (OutgoingCount == 1) continue;
                    }
                    if (Moving.PlannedX < Other.PlannedX + Other.Width + 16.0 && Moving.PlannedX + Moving.Width + 16.0 > Other.PlannedX
                        && AlignedY < Other.PlannedY + Other.Height + 16.0 && AlignedY + Moving.Height + 16.0 > Other.PlannedY)
                    { bBlocked = true; break; }
                }
            if (!bBlocked) Moving.PlannedY = AlignedY;
        }

    // Scoped layout must not detach a split reader from its sole consumer.
    // Mixed selections use the general row layout above, which can place a
    // reader beyond unrelated fixed nodes. Re-anchor only selected, unboxed
    // getters; do not alter wires or move the consumer to accommodate them.
    if (!OutPlan.SelectedNodeKeys.IsEmpty())
    {
        for (int32 ReaderIndex = 0; ReaderIndex < OutPlan.Nodes.Num(); ++ReaderIndex)
        {
            FNode& Reader = OutPlan.Nodes[ReaderIndex];
            if (!Reader.bIsGetter || !Reader.bIsPure || Reader.bUnreachable
                || OwnerOfNode[ReaderIndex] != INDEX_NONE || !OutPlan.SelectedNodeKeys.Contains(Reader.Key)) continue;
            const FWire* OnlyWire = nullptr;
            int32 WireCount = 0;
            for (const FWire& Wire : OutPlan.Wires)
                if (Wire.SourceNodeIndex == ReaderIndex) { OnlyWire = &Wire; ++WireCount; }
            if (WireCount != 1 || OnlyWire == nullptr || OnlyWire->TargetNodeIndex == ReaderIndex) continue;
            const FNode& Consumer = OutPlan.Nodes[OnlyWire->TargetNodeIndex];
            const FVector2D Input = AlignmentPinPosition(OnlyWire->InputPin)
                + FVector2D(Consumer.PlannedX - Consumer.X, Consumer.PlannedY - Consumer.Y);
            const double PreferredX = Consumer.PlannedX - Reader.Width - SpacingX;
            const double PreferredY = Input.Y - (AlignmentPinPosition(OnlyWire->OutputPin).Y - Reader.Y);
            const double Floor = Consumer.bIsPure ? -TNumericLimits<double>::Max()
                : ValueReaderFloor(Consumer.Node, Consumer.PlannedY);
            TArray<double> Rows;
            Rows.Add(FMath::Max(Floor, PreferredY));
            for (int32 OtherIndex = 0; OtherIndex < OutPlan.Nodes.Num(); ++OtherIndex)
                if (OtherIndex != ReaderIndex)
                {
                    const FNode& Other = OutPlan.Nodes[OtherIndex];
                    Rows.Add(FMath::Max(Floor, Other.PlannedY - Reader.Height - SpacingY));
                    Rows.Add(FMath::Max(Floor, Other.PlannedY + Other.Height + SpacingY));
                }
            double BestScore = TNumericLimits<double>::Max();
            // Nearby alternate columns avoid an obstacle without introducing a
            // long wire. If no local slot fits, retain the original reader position.
            Reader.PlannedX = Reader.X; Reader.PlannedY = Reader.Y;
            for (int32 Column = 0; Column < 3; ++Column)
                for (double Row : Rows)
                {
                    const double X = FMath::RoundToDouble(PreferredX - Column * (Reader.Width + SpacingX));
                    const double Y = FMath::RoundToDouble(Row);
                    const double Score = FMath::Abs(X - PreferredX) + FMath::Abs(Y - PreferredY);
                    if (Score > 1000.0 || Score >= BestScore) continue;
                    bool bBlocked = false;
                    for (int32 OtherIndex = 0; OtherIndex < OutPlan.Nodes.Num(); ++OtherIndex)
                        if (OtherIndex != ReaderIndex)
                        {
                            const FNode& Other = OutPlan.Nodes[OtherIndex];
                            if (X < Other.PlannedX + Other.Width + 16.0 && X + Reader.Width + 16.0 > Other.PlannedX
                                && Y < Other.PlannedY + Other.Height + 16.0 && Y + Reader.Height + 16.0 > Other.PlannedY)
                            { bBlocked = true; break; }
                        }
                    if (!bBlocked) { Reader.PlannedX = X; Reader.PlannedY = Y; BestScore = Score; }
                }
        }
    }

    // Compact readers need a pin gap, not another execution column. Tighten
    // singly used, unboxed getters horizontally while retaining their input order.
    // This also leaves a real routing corridor beside control nodes.
    if (OutPlan.bCompactBlocks && OutPlan.SelectedNodeKeys.IsEmpty())
        for (int32 I=0;I<OutPlan.Nodes.Num();++I)
        {
            FNode& Reader=OutPlan.Nodes[I]; if (!Reader.bIsGetter || !Reader.bIsPure || Reader.bUnreachable || OwnerOfNode[I]!=INDEX_NONE) continue;
            const FWire* Only=nullptr; int32 Uses=0;
            for (const FWire& Wire:OutPlan.Wires) if (Wire.SourceNodeIndex==I) { Only=&Wire; ++Uses; }
            if (Uses!=1 || !Only || !OutPlan.Nodes.IsValidIndex(Only->TargetNodeIndex) || OwnerOfNode[Only->TargetNodeIndex]!=INDEX_NONE) continue;
            const FNode& Consumer=OutPlan.Nodes[Only->TargetNodeIndex];
            const double X=FMath::RoundToDouble(Consumer.PlannedX-Reader.Width-GetterClearance);
            if (X<=Reader.PlannedX) continue;
            bool Clear=true;
            for (int32 J=0;J<OutPlan.Nodes.Num();++J)
            {
                const FNode& Other=OutPlan.Nodes[J]; if (J==I || Other.bIsComment) continue;
                if (X<Other.PlannedX+Other.Width+GetterClearance && X+Reader.Width+GetterClearance>Other.PlannedX
                    && Reader.PlannedY<Other.PlannedY+Other.Height+GetterClearance && Reader.PlannedY+Reader.Height+GetterClearance>Other.PlannedY) { Clear=false; break; }
            }
            for (const FBox& Box:OutPlan.Boxes) if (Clear && Box.MemberKeys.IsEmpty())
                if (X<Box.X+Box.Width && X+Reader.Width>Box.X && Reader.PlannedY<Box.Y+Box.Height && Reader.PlannedY+Reader.Height>Box.Y) Clear=false;
            if (Clear) Reader.PlannedX=X;
        }

	// -----------------------------------------------------------------------------------------
	// STEP 7: split variable getters that feed several consumers, BEFORE the measure, because a copy
	// changes the graph's node set and therefore its wire set.
	//
	// This is one of the two irreversible steps and it is why the rollback exists as CODE rather than
	// as a design property. What is created is recorded so `Apply` can destroy exactly it.
	// -----------------------------------------------------------------------------------------
	struct FOccupiedGetterRect { double X, Y, Width, Height; };
	TArray<FOccupiedGetterRect> GetterObstacles;
	for (const FNode& Node : OutPlan.Nodes)
	{
		if (!Node.bIsComment)
			GetterObstacles.Add({FMath::RoundToDouble(Node.PlannedX), FMath::RoundToDouble(Node.PlannedY), Node.Width, Node.Height});
	}
	for (int32 NodeIndex = 0; NodeIndex < OutPlan.Nodes.Num(); ++NodeIndex)
	{
		FNode& Entry = OutPlan.Nodes[NodeIndex];
		if ((!Entry.bIsGetter && !(Entry.Node && Entry.Node->IsA<UK2Node_FunctionEntry>())) || Entry.bUnreachable || Entry.Node == nullptr
			|| (OutPlan.SelectedNodeKeys.Num() > 0 && !OutPlan.SelectedNodeKeys.Contains(Entry.Key)))
		{
			continue;
		}

		UEdGraphNode* Getter = Entry.Node;
        const bool bFunctionInput = Getter->IsA<UK2Node_FunctionEntry>();
        TArray<UEdGraphPin*> SourcePins;
        if (bFunctionInput)
        {
            for (UEdGraphPin* Pin : Getter->Pins) if (CanReadValuePin(Getter, Pin)) SourcePins.Add(Pin);
        }
        else if (CanSplitGetter(Getter)) SourcePins.Add(SplitValuePin(Getter));
        for (UEdGraphPin* ValuePin : SourcePins)
        {
		if (ValuePin == nullptr || ValuePin->LinkedTo.Num() < (bFunctionInput ? 1 : 2))
		{
			continue;
		}

        FVector2D ReaderSize(Entry.Width, Entry.Height);
        FVector2D ReaderOffset = AlignmentPinPosition(ValuePin) - FVector2D(Entry.X, Entry.Y);
        if (bFunctionInput && !MeasureInputReader(Getter, ValuePin, ReaderSize, ReaderOffset))
        { OutError = TEXT("function-input getter outline could not be measured; no graph changes applied"); return false; }

        // Keep the original beside a forward-facing planned consumer. Old canvas
        // order and distance alone can select the wrong input lane after tidy.
        TArray<UEdGraphPin*> Consumers = ValuePin->LinkedTo;
        const FVector2D OriginalOutput=AlignmentPinPosition(ValuePin)
            + FVector2D(Entry.PlannedX-Entry.X,Entry.PlannedY-Entry.Y);
        auto PlannedConsumerDistance=[&](const UEdGraphPin& Pin)
        {
            const UEdGraphNode* Owner=Pin.GetOwningNodeUnchecked();
            const int32* OwnerIndex=IndexByNode.Find(Owner);
            if(!Owner || !OwnerIndex) return TNumericLimits<double>::Max();
            const FNode& OwnerPlan=OutPlan.Nodes[*OwnerIndex];
            const FVector2D Input=AlignmentPinPosition(&Pin)+FVector2D(OwnerPlan.PlannedX-OwnerPlan.X,OwnerPlan.PlannedY-OwnerPlan.Y);
            return FVector2D::Distance(OriginalOutput,Input);
        };
        Consumers.Sort([&](const UEdGraphPin& InLeft,const UEdGraphPin& InRight)
        {
            // Preserve the forward-facing consumer chosen by layout. Euclidean
            // distance alone can prefer a node directly below the getter, leaving
            // a backward wire while the forward consumer receives a new copy.
            auto IsForwardConsumer = [&](const UEdGraphPin& Pin)
            {
                const int32* Index = IndexByNode.Find(Pin.GetOwningNodeUnchecked());
                return Index && OutPlan.Nodes[*Index].PlannedX >= Entry.PlannedX + Entry.Width;
            };
            const bool bLeftForward = IsForwardConsumer(InLeft);
            const bool bRightForward = IsForwardConsumer(InRight);
            if (bLeftForward != bRightForward) return bLeftForward;
            const double LeftDistance=PlannedConsumerDistance(InLeft),RightDistance=PlannedConsumerDistance(InRight);
            if(LeftDistance!=RightDistance) return LeftDistance<RightDistance;
            const UEdGraphNode* L=InLeft.GetOwningNodeUnchecked();
            const UEdGraphNode* R=InRight.GetOwningNodeUnchecked();
            if(!L || !R) return L!=nullptr && R==nullptr;
            if(L->NodeGuid!=R->NodeGuid) return L->NodeGuid.ToString()<R->NodeGuid.ToString();
            return InLeft.PinId.ToString()<InRight.PinId.ToString();
        });

		for (int32 ConsumerIndex = bFunctionInput ? 0 : 1; ConsumerIndex < Consumers.Num(); ++ConsumerIndex)
		{
			UEdGraphPin* ConsumerPin = Consumers[ConsumerIndex];
			if (ConsumerPin == nullptr)
			{
				continue;
			}
			UEdGraphNode* ConsumerNode = ConsumerPin->GetOwningNodeUnchecked();
			if (ConsumerNode == nullptr)
			{
				continue;
			}

			// NOTHING IS CREATED HERE. `Plan` MUST WRITE NOTHING: it is the half the Preview runs, and
			// a Preview that added a node would change the graph it was asked to describe. What the
			// split WOULD do is recorded as a position beside the planned layout, and `Apply` performs
			// it - which is the same discipline the planned positions follow, and it makes the ordinary
			// rollback "no write ever happened" for getters as well.
			FGetterCopy Planned;
			Planned.Original = Getter;
			Planned.ConsumerPin = ConsumerPin;
			Planned.OriginalValuePin = ValuePin;
            Planned.Width = ReaderSize.X; Planned.Height = ReaderSize.Y; Planned.OutputOffset = ReaderOffset;
			const int32* ConsumerPlanIndex = IndexByNode.Find(ConsumerNode);
			if (!ConsumerPlanIndex) continue;
            const int32 ConsumerBoxIndex = OwnerOfNode[*ConsumerPlanIndex];
			const FNode& ConsumerPlan = OutPlan.Nodes[*ConsumerPlanIndex];
            if (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(ConsumerPlan.Key)) continue;
			Planned.PlannedX = ConsumerPlan.PlannedX
				- Planned.Width - (OutPlan.bCompactBlocks ? GetterClearance : SpacingX);
            double ReaderFloor=ValueReaderFloor(ConsumerPlan.Node,ConsumerPlan.PlannedY);
            if(ConsumerPlan.bIsPure)
            {
                // A pure consumer still belongs to an execution lane. Find the
                // nearest downstream lane above it; traversing only pure nodes
                // avoids borrowing a later branch's unrelated execution band.
                TArray<int32> PendingConsumers;PendingConsumers.Add(*ConsumerPlanIndex);
                TSet<int32> VisitedConsumers;
                double NearestFloor=-TNumericLimits<double>::Max();
                double FirstFloor=TNumericLimits<double>::Max();
                while(!PendingConsumers.IsEmpty())
                {
                    const int32 CurrentConsumer=PendingConsumers.Last();
                    PendingConsumers.RemoveAt(PendingConsumers.Num()-1);
                    if(VisitedConsumers.Contains(CurrentConsumer)) continue;
                    VisitedConsumers.Add(CurrentConsumer);
                    for(const FWire& W:OutPlan.Wires)
                    {
                        if(W.SourceNodeIndex!=CurrentConsumer || !W.OutputPin || W.OutputPin->PinType.PinCategory==TEXT("exec")
                            || !OutPlan.Nodes.IsValidIndex(W.TargetNodeIndex)) continue;
                        const FNode& Downstream=OutPlan.Nodes[W.TargetNodeIndex];
                        if(Downstream.bIsPure) PendingConsumers.Add(W.TargetNodeIndex);
                        else if(!Downstream.bIsComment)
                        {
                            const double LaneFloor=ValueReaderFloor(Downstream.Node,Downstream.PlannedY);
                            FirstFloor=FMath::Min(FirstFloor,LaneFloor);
                            if(LaneFloor<=ConsumerPlan.PlannedY) NearestFloor=FMath::Max(NearestFloor,LaneFloor);
                        }
                    }
                }
                if(NearestFloor!=-TNumericLimits<double>::Max()) ReaderFloor=NearestFloor;
                else if(FirstFloor!=TNumericLimits<double>::Max()) ReaderFloor=FirstFloor;
            }
            Planned.PlannedY=ReaderFloor;
			Planned.PlannedX = FMath::RoundToDouble(Planned.PlannedX);
            TSet<int32> ContainingBoxes;
            for (int32 B = ConsumerBoxIndex; B != INDEX_NONE; B = OutPlan.Boxes[B].ParentIndex) ContainingBoxes.Add(B);
            if (ConsumerBoxIndex != INDEX_NONE && !bFlowComments)
            {
                const FBox& ConsumerBox = OutPlan.Boxes[ConsumerBoxIndex];
                Planned.PlannedX = FMath::Max(Planned.PlannedX, ConsumerBox.X + FitPadding);
                Planned.PlannedY = FMath::Max(Planned.PlannedY, ConsumerBox.Y + TitleBarHeight + FitPadding);
            }
            TArray<FOccupiedGetterRect> CopyObstacles = GetterObstacles;
            // Child/sibling notes are solid obstacles; only the consumer's box
            // and its ancestors may contain the new reader.
            for (int32 B = 0; B < OutPlan.Boxes.Num(); ++B)
                if (!ContainingBoxes.Contains(B) && (!bFlowComments || OutPlan.Boxes[B].MemberKeys.IsEmpty()))
                {
                    const FBox& OtherBox = OutPlan.Boxes[B];
                    CopyObstacles.Add({OtherBox.X, OtherBox.Y, OtherBox.PlannedWidth, OtherBox.PlannedHeight});
                }
            // Search adjacent input columns as well as downward. A blocked preferred
            // column must not push a tiny reader below every unrelated branch.
            const FVector2D ConsumerInput = FTurboMCPGraphTidy::AlignmentPinPosition(ConsumerPin)
                + FVector2D(ConsumerPlan.PlannedX - ConsumerPlan.X, ConsumerPlan.PlannedY - ConsumerPlan.Y);
            const double CopyFloor = FMath::CeilToDouble(Planned.PlannedY);
            // Keep a copied reader above sources feeding lower input pins. A
            // downward-only collision search otherwise reverses the input order.
            double CopyCeiling = TNumericLimits<double>::Max();
            double CopyRightLimit = Planned.PlannedX;
            for (UEdGraphPin* OtherInput : ConsumerNode->Pins)
            {
                if (!OtherInput || OtherInput == ConsumerPin || OtherInput->Direction != EGPD_Input
                    || AlignmentPinPosition(OtherInput).Y <= AlignmentPinPosition(ConsumerPin).Y) continue;
                for (const FGetterCopy& EarlierCopy : OutPlan.GetterCopies)
                    if (EarlierCopy.ConsumerPin == OtherInput)
                        CopyCeiling = FMath::Min(CopyCeiling,
                            EarlierCopy.PlannedY - Planned.Height - GetterClearance);
                for (UEdGraphPin* Source : OtherInput->LinkedTo)
                {
                    const int32* SourceIndex = Source ? IndexByNode.Find(Source->GetOwningNodeUnchecked()) : nullptr;
                    if (!SourceIndex || !OutPlan.Nodes[*SourceIndex].bIsPure) continue;
                    const FNode& SourcePlan = OutPlan.Nodes[*SourceIndex];
                    // A real sibling calculation owns its input-tree space. A
                    // leaf literal has no tree; letting it reserve a whole column
                    // can push this reader outside its execution block.
                    const bool bHasLinkedDataInput = SourcePlan.Node && SourcePlan.Node->Pins.ContainsByPredicate(
                        [](const UEdGraphPin* Pin)
                        { return Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && !Pin->LinkedTo.IsEmpty(); });
                    if (!SourcePlan.bIsGetter && bHasLinkedDataInput)
                        CopyRightLimit = FMath::Min(CopyRightLimit, SourcePlan.PlannedX - Planned.Width - SpacingX);
                    CopyCeiling = FMath::Min(CopyCeiling,
                        AlignmentPinPosition(Source).Y + SourcePlan.PlannedY - SourcePlan.Y - Planned.OutputOffset.Y - GetterClearance);
                }
            }
            // Use the exact integer coordinates that scratch and live edits apply.
            // Fractional native pin offsets must not leak into reader positions.
            CopyCeiling = FMath::FloorToDouble(CopyCeiling);
            const double PreferredCopyX = FMath::FloorToDouble(CopyRightLimit);
            const double MinimumCopyX = (bFlowComments || ConsumerBoxIndex == INDEX_NONE) ? static_cast<double>(MIN_int32)
                : OutPlan.Boxes[ConsumerBoxIndex].X + FitPadding;
            TArray<double> CopyColumns;
            if (PreferredCopyX >= MinimumCopyX) CopyColumns.Add(PreferredCopyX);
            for (const FOccupiedGetterRect& Obstacle : CopyObstacles)
            {
                const double LeftColumn = FMath::FloorToDouble(Obstacle.X - Planned.Width - SpacingX);
                const double RightColumn = FMath::CeilToDouble(Obstacle.X + Obstacle.Width + SpacingX);
                if (LeftColumn >= MinimumCopyX && LeftColumn <= PreferredCopyX) CopyColumns.AddUnique(LeftColumn);
                if (RightColumn >= MinimumCopyX && RightColumn <= PreferredCopyX) CopyColumns.AddUnique(RightColumn);
            }
            double BestCopyScore = TNumericLimits<double>::Max();
            for (double CopyX : CopyColumns)
            {
                TArray<double> CopyRows;
                CopyRows.Add(FMath::Max(CopyFloor, FMath::Min(CopyCeiling, FMath::RoundToDouble(ConsumerInput.Y - Planned.OutputOffset.Y))));
                // Try the free slot above an obstacle as well as below it. This
                // avoids moving a reader several columns left when a nearby gap fits.
                for (const FOccupiedGetterRect& Obstacle : CopyObstacles)
                {
                    const double Above = FMath::FloorToDouble(Obstacle.Y - Planned.Height - GetterClearance);
                    if (Above >= CopyFloor && Above <= CopyCeiling) CopyRows.AddUnique(Above);
                }
                for (double CopyY : CopyRows)
                {
                    for (int32 Pass = 0; Pass <= CopyObstacles.Num(); ++Pass)
                    {
                        double NextY = CopyY;
                        for (const FOccupiedGetterRect& Obstacle : CopyObstacles)
                            if (CopyX < Obstacle.X + Obstacle.Width + SpacingX
                                && CopyX + Planned.Width + SpacingX > Obstacle.X
                                && CopyY < Obstacle.Y + Obstacle.Height + GetterClearance
                                && CopyY + Planned.Height + GetterClearance > Obstacle.Y)
                                NextY = FMath::Max(NextY, FMath::CeilToDouble(Obstacle.Y + Obstacle.Height + GetterClearance));
                        if (NextY == CopyY) break;
                        CopyY = NextY;
                    }
                    if (CopyY > CopyCeiling) continue;
                    if (!FMath::IsFinite(CopyX) || !FMath::IsFinite(CopyY)
                        || CopyX < MIN_int32 || CopyX > MAX_int32 || CopyY < MIN_int32 || CopyY > MAX_int32) continue;
                    const double CopyScore = FVector2D::Distance(FVector2D(CopyX, CopyY) + Planned.OutputOffset, ConsumerInput);
                    if (CopyScore < BestCopyScore)
                    {
                        BestCopyScore = CopyScore;
                        Planned.PlannedX = CopyX;
                        Planned.PlannedY = CopyY;
                    }
                }
            }
            if (BestCopyScore == TNumericLimits<double>::Max())
            { OutError = TEXT("No collision-free shared-reader position preserves input order inside the layout region; no graph changes applied."); return false; }
			if (!FMath::IsFinite(Planned.PlannedX) || !FMath::IsFinite(Planned.PlannedY)
				|| Planned.PlannedX < MIN_int32 || Planned.PlannedX > MAX_int32
				|| Planned.PlannedY < MIN_int32 || Planned.PlannedY > MAX_int32)
			{
				OutError = TEXT("Getter-copy placement exceeds graph coordinate limits; no nodes moved.");
				return false;
			}
            // Keep an existing short function-input wire instead of adding a longer redundant getter.
            if (bFunctionInput && BestCopyScore >= PlannedConsumerDistance(*ConsumerPin)) continue;
            double RequiredRight = Planned.PlannedX + Planned.Width + FitPadding;
            double RequiredBottom = Planned.PlannedY + Planned.Height + FitPadding;
            for (int32 B = ConsumerBoxIndex; B != INDEX_NONE && !bFlowComments; B = OutPlan.Boxes[B].ParentIndex)
            {
                FBox& Container = OutPlan.Boxes[B];
                const double NewWidth = FMath::Max(Container.PlannedWidth, RequiredRight - Container.X);
                const double NewHeight = FMath::Max(Container.PlannedHeight, RequiredBottom - Container.Y);
                if (!FMath::IsFinite(NewWidth) || !FMath::IsFinite(NewHeight) || NewWidth > MAX_int32 || NewHeight > MAX_int32)
                { OutError = TEXT("shared-value copy exceeds comment size limits; no changes applied"); return false; }
                // Growing a comment must not capture an unrelated node or note.
                for (int32 OtherIndex = 0; OtherIndex < OutPlan.Nodes.Num(); ++OtherIndex)
                {
                    const FNode& Other = OutPlan.Nodes[OtherIndex];
                    bool bMember = Container.MemberKeys.Contains(Other.Key);
                    if (Other.bIsComment)
                        for (int32 Child = 0; Child < OutPlan.Boxes.Num(); ++Child)
                            if (OutPlan.Boxes[Child].Key == Other.Key)
                                for (int32 Parent = Child; Parent != INDEX_NONE; Parent = OutPlan.Boxes[Parent].ParentIndex)
                                    if (Parent == B) bMember = true;
                    if (bMember) continue;
                    const double OX = Other.bIsComment ? Other.X : Other.PlannedX;
                    const double OY = Other.bIsComment ? Other.Y : Other.PlannedY;
                    double OW = Other.Width, OH = Other.Height;
                    if (Other.bIsComment)
                        for (const FBox& OtherBox : OutPlan.Boxes)
                            if (OtherBox.Key == Other.Key) { OW = OtherBox.PlannedWidth; OH = OtherBox.PlannedHeight; break; }
                    const bool WasIntersecting = OX < Container.X + Container.PlannedWidth && OX + OW > Container.X
                        && OY < Container.Y + Container.PlannedHeight && OY + OH > Container.Y;
                    const bool WouldIntersect = OX < Container.X + NewWidth && OX + OW > Container.X
                        && OY < Container.Y + NewHeight && OY + OH > Container.Y;
                    if (WouldIntersect && !WasIntersecting)
                    { OutError = TEXT("shared-value copy needs comment space occupied by an unrelated node or box; no changes applied"); return false; }
                }
                Container.PlannedWidth = NewWidth;
                Container.PlannedHeight = NewHeight;
                Container.bWouldFit = !FMath::IsNearlyEqual(NewWidth, Container.Width) || !FMath::IsNearlyEqual(NewHeight, Container.Height);
                RequiredRight = Container.X + NewWidth + FitPadding;
                RequiredBottom = Container.Y + NewHeight + FitPadding;
            }
            GetterObstacles.Add({Planned.PlannedX, Planned.PlannedY, Planned.Width, Planned.Height});
            OutPlan.GetterCopies.Add(Planned);
		}
        }
	}

    // Route an existing exclusive feedback reroute chain below the graph body.
    // Keep its topology: the first reroute receives the body output on the right,
    // and the last returns to the macro input from the left. Fixed or boxed knots
    // retain their existing positions; this pass never expands selection ownership.
    for (const FWire& Feedback : OutPlan.Wires)
    {
        if (!Feedback.bControlFeedback) continue;
        TArray<int32> ReverseChain;
        int32 Cursor = Feedback.SourceNodeIndex;
        TSet<int32> Seen;
        bool Supported = true;
        while (OutPlan.Nodes.IsValidIndex(Cursor) && OutPlan.Nodes[Cursor].bIsKnot)
        {
            if (Seen.Contains(Cursor)) { Supported = false; break; }
            Seen.Add(Cursor);
            const FNode& Knot = OutPlan.Nodes[Cursor];
            if (Knot.bUnreachable || (!OutPlan.SelectedNodeKeys.IsEmpty() && !OutPlan.SelectedNodeKeys.Contains(Knot.Key)))
            { Supported = false; break; }
            for (const FBox& Box : OutPlan.Boxes)
                if (Box.MemberKeys.Contains(Knot.Key)) Supported = false;
            int32 IncomingCount = 0, OutgoingCount = 0, Predecessor = INDEX_NONE;
            for (const FWire& Wire : OutPlan.Wires)
            {
                if (Wire.TargetNodeIndex == Cursor) { ++IncomingCount; Predecessor = Wire.SourceNodeIndex; }
                if (Wire.SourceNodeIndex == Cursor) ++OutgoingCount;
            }
            if (!Supported || IncomingCount != 1 || OutgoingCount != 1) { Supported = false; break; }
            ReverseChain.Add(Cursor);
            Cursor = Predecessor;
        }
        if (!Supported || ReverseChain.Num() < 2 || !OutPlan.Nodes.IsValidIndex(Cursor)) continue;
        const FNode& Body = OutPlan.Nodes[Cursor];
        const FNode& Macro = OutPlan.Nodes[Feedback.TargetNodeIndex];
        double Below = FMath::Max(Body.PlannedY + Body.Height, Macro.PlannedY + Macro.Height);
        for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
            if (!Seen.Contains(Index))
                Below = FMath::Max(Below, OutPlan.Nodes[Index].PlannedY + OutPlan.Nodes[Index].Height);
        for (const FBox& Box : OutPlan.Boxes) Below = FMath::Max(Below, Box.Y + Box.PlannedHeight);
        for (const FGetterCopy& Copy : OutPlan.GetterCopies) Below = FMath::Max(Below, Copy.PlannedY + Copy.Height);
        Below += VerticalNodeGap;
        const double Left = Macro.PlannedX - HorizontalNodeGap - OutPlan.Nodes[ReverseChain[0]].Width;
        const double Right = Body.PlannedX + Body.Width + HorizontalNodeGap;
        if (!FMath::IsFinite(Left) || !FMath::IsFinite(Right) || !FMath::IsFinite(Below)
            || Left < MIN_int32 || Right > MAX_int32 || Below > MAX_int32)
        { OutError = TEXT("Feedback routing exceeds graph coordinate limits; no nodes moved."); return false; }
        for (int32 Index = 0; Index < ReverseChain.Num(); ++Index)
        {
            FNode& Knot = OutPlan.Nodes[ReverseChain[Index]];
            Knot.PlannedX = FMath::Lerp(Left, Right, static_cast<double>(Index) / (ReverseChain.Num() - 1));
            Knot.PlannedY = Below;
        }
    }

    if (bFlowComments)
    {
        // Fit inside-out using measured outlines. Membership is captured before
        // planning; a new position must never silently acquire unrelated nodes.
        for (int32 BoxIndex : BoxOrder)
        {
            FBox& Box = OutPlan.Boxes[BoxIndex];
            if (Box.MemberKeys.IsEmpty()) continue; // Empty notes remain anchors.
            double Left = TNumericLimits<double>::Max(), Top = Left;
            double Right = -Left, Bottom = -Left;
            auto Include = [&](double X, double Y, double Width, double Height)
            {
                Left = FMath::Min(Left, X); Top = FMath::Min(Top, Y);
                Right = FMath::Max(Right, X + Width); Bottom = FMath::Max(Bottom, Y + Height);
            };
            for (const FString& Key : Box.MemberKeys)
                if (const int32* Index = NodeIndexByKey.Find(Key))
                {
                    const FNode& Member = OutPlan.Nodes[*Index];
                    if (!Member.bIsComment) Include(Member.PlannedX, Member.PlannedY, Member.Width, Member.Height);
                }
            for (int32 ChildIndex : Box.ChildBoxIndices)
            {
                const FBox& Child = OutPlan.Boxes[ChildIndex];
                Include(Child.PlannedX, Child.PlannedY, Child.PlannedWidth, Child.PlannedHeight);
            }
            for (const FGetterCopy& Copy : OutPlan.GetterCopies)
                for (const FString& Key : Box.MemberKeys)
                    if (const int32* Index = NodeIndexByKey.Find(Key))
                        if (Copy.ConsumerPin && Copy.ConsumerPin->GetOwningNode() == OutPlan.Nodes[*Index].Node)
                        { Include(Copy.PlannedX, Copy.PlannedY, Copy.Width, Copy.Height); break; }
            if (Left == TNumericLimits<double>::Max()) continue;
            Box.PlannedX = FMath::FloorToDouble(Left - FitPadding);
            Box.PlannedY = FMath::FloorToDouble(Top - TitleBarHeight - FitPadding);
            Box.PlannedWidth = FMath::CeilToDouble(Right + FitPadding - Box.PlannedX);
            Box.PlannedHeight = FMath::CeilToDouble(Bottom + FitPadding - Box.PlannedY);
            if (!FMath::IsFinite(Box.PlannedX) || !FMath::IsFinite(Box.PlannedY)
                || !FMath::IsFinite(Box.PlannedWidth) || !FMath::IsFinite(Box.PlannedHeight)
                || Box.PlannedX < MIN_int32 || Box.PlannedX > MAX_int32
                || Box.PlannedY < MIN_int32 || Box.PlannedY > MAX_int32
                || Box.PlannedWidth <= 0 || Box.PlannedWidth > MAX_int32
                || Box.PlannedHeight <= 0 || Box.PlannedHeight > MAX_int32)
            { OutError = TEXT("Compact comment rectangle exceeds graph coordinate limits; unchanged."); return false; }
            Box.bWouldFit = Box.PlannedX != Box.X || Box.PlannedY != Box.Y
                || Box.PlannedWidth != Box.Width || Box.PlannedHeight != Box.Height;
            for (FNode& Node : OutPlan.Nodes)
                if (Node.Node == Box.Comment) { Node.PlannedX = Box.PlannedX; Node.PlannedY = Box.PlannedY; }
            for (const FNode& Node : OutPlan.Nodes)
            {
                // MembersOf returns engine object names; FNode.Key is the public
                // stable ID. Resolve the same identity convention used by fitting.
                if (Node.bIsComment || Box.MemberKeys.Contains(Node.Key)
                    || (Node.Node && Box.MemberKeys.Contains(Node.Node->GetName()))) continue;
                if (Node.PlannedX < Box.PlannedX + Box.PlannedWidth && Node.PlannedX + Node.Width > Box.PlannedX
                    && Node.PlannedY < Box.PlannedY + Box.PlannedHeight && Node.PlannedY + Node.Height > Box.PlannedY)
                {
                    OutError = FString::Printf(TEXT("Compact comment %s would overlap unrelated node %s; unchanged. The block plan needs more space before this comment can be fitted."), *Box.Key, *Node.Key);
                    return false;
                }
            }
        }
    }

    TSet<int32> SimpleLoopBodies;
    if (OutPlan.bCompactBlocks)
    {
        // Spatial lanes keep Body on the row, but are not semantic blocks. Split
        // at control handoffs before assigning pure expressions and box ownership.
        auto ReachExec=[&](TArray<int32> Pending)
        {
            TSet<int32> Seen;
            while (!Pending.IsEmpty())
            {
                const int32 I=Pending.Pop(EAllowShrinking::No); if (Seen.Contains(I)) continue; Seen.Add(I);
                for (const FWire& Wire:OutPlan.Wires) if (Wire.SourceNodeIndex==I && Wire.OutputPin
                    && Wire.OutputPin->PinType.PinCategory==TEXT("exec") && !Wire.bControlFeedback) Pending.Add(Wire.TargetNodeIndex);
            }
            return Seen;
        };
        for (int32 I=0;I<OutPlan.Nodes.Num();++I)
        {
            TArray<int32> Body,Completed;
            for (const FWire& Wire:OutPlan.Wires) if (Wire.SourceNodeIndex==I && Wire.OutputPin)
            {
                if (Wire.OutputPin->PinName==TEXT("LoopBody")) Body.Add(Wire.TargetNodeIndex);
                if (Wire.OutputPin->PinName==TEXT("Completed")) Completed.Add(Wire.TargetNodeIndex);
            }
            if (Body.IsEmpty()) continue;
            const TSet<int32> After=ReachExec(Completed); int32 ExecCount=0;
            for (int32 Member:ReachExec(Body)) if (!After.Contains(Member) && !OutPlan.Nodes[Member].bIsKnot) ++ExecCount;
            if (ExecCount==1) SimpleLoopBodies.Add(I);
        }
        auto Active=[&](int32 I){return !OutPlan.Nodes[I].bIsComment && !OutPlan.Nodes[I].bUnreachable
            && (OutPlan.SelectedNodeKeys.IsEmpty() || OutPlan.SelectedNodeKeys.Contains(OutPlan.Nodes[I].Key));};
        TSet<int32> Execution;
        for (int32 I=0;I<OutPlan.Nodes.Num();++I)
        {
            OutPlan.Nodes[I].BlockIndex=INDEX_NONE;
            if (Active(I) && OutPlan.Nodes[I].Node)
                for (const UEdGraphPin* Pin:OutPlan.Nodes[I].Node->Pins)
                    if (Pin && Pin->PinType.PinCategory==TEXT("exec")) { Execution.Add(I); break; }
        }
        OutPlan.Blocks.Reset(); OutPlan.BlockCount=0;
        TArray<int32> Pending=Execution.Array(); Pending.Sort(); TMap<int32,int32> ExecOrder;
        auto AddBlock=[&](const FString& Kind,int32 Parent,int32 LoopScope,const FString& Handoff)
        {
            FBlock Block; Block.Kind=Kind; Block.ParentIndex=Parent; Block.LoopContext=LoopScope; Block.Handoff=Handoff;
            const int32 Index=OutPlan.Blocks.Add(MoveTemp(Block)); OutPlan.BlockCount=OutPlan.Blocks.Num(); return Index;
        };
        while (!Pending.IsEmpty())
        {
            int32 Ready=INDEX_NONE;
            for (int32 I:Pending)
            {
                bool Wait=false;
                for (const FWire& Wire:OutPlan.Wires) if (Wire.TargetNodeIndex==I && Wire.OutputPin && !Wire.bControlFeedback
                    && Wire.OutputPin->PinType.PinCategory==TEXT("exec") && Pending.Contains(Wire.SourceNodeIndex)) { Wait=true; break; }
                if (!Wait) { Ready=I; break; }
            }
            if (Ready==INDEX_NONE) Ready=Pending[0]; // Ordering already validated; fixed external cycles remain anchors.
            const FWire* Incoming=nullptr; double Best=TNumericLimits<double>::Max();
            for (const FWire& Wire:OutPlan.Wires)
                if (Wire.TargetNodeIndex==Ready && Wire.OutputPin && !Wire.bControlFeedback && Wire.OutputPin->PinType.PinCategory==TEXT("exec")
                    && Execution.Contains(Wire.SourceNodeIndex) && OutPlan.Nodes[Wire.SourceNodeIndex].BlockIndex!=INDEX_NONE)
                {
                    const FNode& Source=OutPlan.Nodes[Wire.SourceNodeIndex]; const FNode& Target=OutPlan.Nodes[Ready];
                    const double Distance=FMath::Abs(AlignmentPinPosition(Wire.OutputPin).Y+Source.PlannedY-Source.Y
                        -AlignmentPinPosition(Wire.InputPin).Y-Target.PlannedY+Target.Y);
                    if (Distance<Best) { Best=Distance; Incoming=&Wire; }
                }
            int32 Block=INDEX_NONE;
            if (Incoming)
            {
                const int32 SourceIndex=Incoming->SourceNodeIndex; const FNode& Source=OutPlan.Nodes[SourceIndex];
                Block=Source.BlockIndex; TSet<const UEdGraphPin*> Outputs;
                for (const FWire& Wire:OutPlan.Wires) if (Wire.SourceNodeIndex==SourceIndex && Wire.OutputPin && !Wire.bControlFeedback
                    && Wire.OutputPin->PinType.PinCategory==TEXT("exec")) Outputs.Add(Wire.OutputPin);
                const bool Body=Incoming->OutputPin->PinName==TEXT("LoopBody"),Completed=Incoming->OutputPin->PinName==TEXT("Completed");
                bool Boundary=Outputs.Num()>1 || Body || Completed;
                if (Body && SimpleLoopBodies.Contains(SourceIndex)) Boundary=false;
                if (Source.Node && Source.Node->IsA<UK2Node_ExecutionSequence>())
                    for (const UEdGraphPin* Pin:Source.Node->Pins) if (Outputs.Contains(Pin)) { if (Pin==Incoming->OutputPin) Boundary=false; break; }
                if (Boundary)
                {
                    int32 Parent=Block; const int32 LoopScope=OutPlan.Blocks[Block].LoopContext;
                    if (Completed) Parent=LoopScope!=INDEX_NONE?LoopScope:OutPlan.Blocks[Block].ParentIndex;
                    Block=AddBlock(Body?TEXT("loop body"):Completed?TEXT("loop continuation"):TEXT("control lane"),Parent,LoopScope,
                        Source.Key+TEXT(":")+Incoming->OutputPin->PinName.ToString());
                    if (Body) OutPlan.Blocks[Block].LoopContext=Block;
                }
            }
            if (Block==INDEX_NONE) Block=AddBlock(TEXT("entry"),INDEX_NONE,INDEX_NONE,FString());
            OutPlan.Nodes[Ready].BlockIndex=Block; ExecOrder.Add(Ready,ExecOrder.Num()); Pending.RemoveSingle(Ready);
        }
        for (int32 I=0;I<OutPlan.Nodes.Num();++I) if (Active(I) && !Execution.Contains(I))
        {
            TArray<int32> Values{I}; TSet<int32> Seen; int32 BestOrder=MAX_int32,Block=INDEX_NONE;
            while (!Values.IsEmpty())
            {
                const int32 Value=Values.Pop(EAllowShrinking::No); if (Seen.Contains(Value)) continue; Seen.Add(Value);
                for (const FWire& Wire:OutPlan.Wires) if (Wire.SourceNodeIndex==Value && Wire.OutputPin && Wire.OutputPin->PinType.PinCategory!=TEXT("exec"))
                {
                    if (const int32* Order=ExecOrder.Find(Wire.TargetNodeIndex))
                    { if (*Order<BestOrder) { BestOrder=*Order; Block=OutPlan.Nodes[Wire.TargetNodeIndex].BlockIndex; } }
                    else if (OutPlan.Nodes[Wire.TargetNodeIndex].bIsPure || OutPlan.Nodes[Wire.TargetNodeIndex].bIsKnot) Values.Add(Wire.TargetNodeIndex);
                }
            }
            if (Block==INDEX_NONE && Execution.IsEmpty())
                Block=OutPlan.Blocks.IsEmpty()?AddBlock(TEXT("data"),INDEX_NONE,INDEX_NONE,FString()):0;
            OutPlan.Nodes[I].BlockIndex=Block;
        }
    }

    if (bCreateBoxes)
    {
        if (OutPlan.Nodes.Num()>512) { OutError=TEXT("Generated comments support at most 512 nodes; use CreateBoxes=false for larger graphs."); return false; }
        struct FGroup { TSet<int32> Members; FString Title; int32 RootBlock=INDEX_NONE; };
        TArray<FGroup> Groups;
        auto Reach = [&](TArray<int32> Pending)
        {
            TSet<int32> Seen;
            while (!Pending.IsEmpty())
            {
                const int32 Current=Pending.Pop(EAllowShrinking::No);
                if (Seen.Contains(Current)) continue;
                Seen.Add(Current);
                for (const FWire& Wire:OutPlan.Wires)
                    if (!Wire.bControlFeedback && Wire.SourceNodeIndex==Current && Wire.OutputPin
                        && Wire.OutputPin->PinType.PinCategory==TEXT("exec")) Pending.Add(Wire.TargetNodeIndex);
            }
            return Seen;
        };
        // A body's Completed descendants belong to its parent. A nested loop's
        // Completed descendants remain within the enclosing body.
        for (int32 I=0;I<OutPlan.Nodes.Num();++I)
        {
            TArray<int32> Body,Completed;
            for (const FWire& Wire:OutPlan.Wires)
                if (Wire.SourceNodeIndex==I && Wire.OutputPin && !Wire.bControlFeedback)
                {
                    if (Wire.OutputPin->PinName==TEXT("LoopBody")) Body.Add(Wire.TargetNodeIndex);
                    if (Wire.OutputPin->PinName==TEXT("Completed")) Completed.Add(Wire.TargetNodeIndex);
                }
            if (Body.IsEmpty() || SimpleLoopBodies.Contains(I)) continue;
            FGroup Group; Group.RootBlock=OutPlan.Nodes[Body[0]].BlockIndex; Group.Title=OutPlan.Nodes[I].Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT(" / LoopBody");
            const TSet<int32> After=Reach(Completed);
            for (int32 Member:Reach(Body)) if (!After.Contains(Member)) Group.Members.Add(Member);
            // Include pure dependencies only when every execution consumer is
            // inside this body. Shared values retain their outer ownership.
            for (int32 P=0;P<OutPlan.Nodes.Num();++P) if (OutPlan.Nodes[P].bIsPure)
            {
                TArray<int32> Pending{P}; TSet<int32> Seen; bool bInside=true; int32 Uses=0;
                while (!Pending.IsEmpty())
                {
                    const int32 Current=Pending.Pop(EAllowShrinking::No);
                    if (Seen.Contains(Current)) continue; Seen.Add(Current);
                    for (const FWire& Wire:OutPlan.Wires)
                        if (Wire.SourceNodeIndex==Current && Wire.OutputPin && Wire.OutputPin->PinType.PinCategory!=TEXT("exec"))
                        {
                            if (OutPlan.Nodes[Wire.TargetNodeIndex].bIsPure) Pending.Add(Wire.TargetNodeIndex);
                            else { ++Uses; bInside &= Group.Members.Contains(Wire.TargetNodeIndex); }
                        }
                }
                if (Uses && bInside) Group.Members.Add(P);
            }
            if (!Group.Members.IsEmpty()) Groups.Add(MoveTemp(Group));
        }
        const int32 LoopGroups=Groups.Num();
        for (int32 Block=0;Block<OutPlan.BlockCount;++Block)
        {
            FGroup Group; Group.Title=FString::Printf(TEXT("Block %d"),Block+1);
            for (const FNode& Controller:OutPlan.Nodes)
                if (Controller.Node && OutPlan.Blocks[Block].Handoff.StartsWith(Controller.Key+TEXT(":")))
                { Group.Title+=TEXT(": ")+Controller.Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"),TEXT(" "))+TEXT(" / ")+OutPlan.Blocks[Block].Handoff.Mid(Controller.Key.Len()+1); break; }
            for (int32 I=0;I<OutPlan.Nodes.Num();++I)
            {
                const FNode& Node=OutPlan.Nodes[I];
                if (Node.bIsComment || Node.bUnreachable || Node.BlockIndex!=Block) continue;
                Group.Members.Add(I);
            }
            bool Covered=false;
            for (int32 L=0;L<LoopGroups;++L) Covered |= Groups[L].RootBlock==Block;
            if (!Covered && !Group.Members.IsEmpty()) Groups.Add(MoveTemp(Group));
        }
        // Fit smaller sets first so parent bounds include every child title.
        Groups.StableSort([](const FGroup& A,const FGroup& B){return A.Members.Num()<B.Members.Num();});
        for (const FGroup& Group:Groups)
        {
            FBox Box; Box.bGenerated=true; Box.Text=Group.Title;
            Box.Key=FString::Printf(TEXT("generated-box-%d"),OutPlan.Boxes.Num());
            double L=TNumericLimits<double>::Max(),T=L,R=-L,B=-L;
            auto Include=[&](double X,double Y,double W,double H){L=FMath::Min(L,X);T=FMath::Min(T,Y);R=FMath::Max(R,X+W);B=FMath::Max(B,Y+H);};
            for (int32 I:Group.Members)
            { const FNode& N=OutPlan.Nodes[I]; Box.MemberKeys.Add(N.Key); Include(N.PlannedX,N.PlannedY,N.Width,N.Height); }
            for (const FGetterCopy& Copy:OutPlan.GetterCopies)
                for (int32 I:Group.Members) if (Copy.ConsumerPin && Copy.ConsumerPin->GetOwningNode()==OutPlan.Nodes[I].Node)
                { Include(Copy.PlannedX,Copy.PlannedY,Copy.Width,Copy.Height); break; }
            bool bAlreadyCovered=false;
            for (const FBox& Existing:OutPlan.Boxes)
            {
                bool bContains=true;
                for (const FString& Key:Box.MemberKeys)
                {
                    const FNode& N=OutPlan.Nodes[NodeIndexByKey[Key]];
                    bContains &= Existing.MemberKeys.Contains(Key) || (N.Node && Existing.MemberKeys.Contains(N.Node->GetName()));
                }
                if (!Existing.bGenerated && bContains) { bAlreadyCovered=true; break; }
                bool bChild=true;
                for (const FString& Key:Existing.MemberKeys)
                {
                    const int32* I=NodeIndexByKey.Find(Key);
                    bChild &= I && Group.Members.Contains(*I);
                }
                if (bChild && !Existing.MemberKeys.IsEmpty()) Include(Existing.PlannedX,Existing.PlannedY,Existing.PlannedWidth,Existing.PlannedHeight);
            }
            if (bAlreadyCovered) continue;
            Box.PlannedX=FMath::FloorToDouble(L-FitPadding); Box.PlannedY=FMath::FloorToDouble(T-TitleBarHeight-FitPadding);
            Box.PlannedWidth=FMath::CeilToDouble(R+FitPadding-Box.PlannedX); Box.PlannedHeight=FMath::CeilToDouble(B+FitPadding-Box.PlannedY);
            if (Box.PlannedX<MIN_int32 || Box.PlannedY<MIN_int32 || Box.PlannedX+Box.PlannedWidth>MAX_int32 || Box.PlannedY+Box.PlannedHeight>MAX_int32)
            { OutError=TEXT("Generated comment exceeds graph coordinate limits; unchanged."); return false; }
            auto IsChild=[&](const FBox& Other)
            {
                if (Other.MemberKeys.IsEmpty()) return false;
                for (const FString& Key:Other.MemberKeys)
                { const int32* Index=NodeIndexByKey.Find(Key); if (!Index || !Group.Members.Contains(*Index)) return false; }
                return true;
            };
            // A nested title may extend above the controller row into an earlier
            // sibling's box. Pack the entire body to the right, retaining row
            // alignment and all child offsets, rather than stretching every row.
            double OffsetX=0; bool Packed=false;
            for (int32 Attempt=0;Attempt<OutPlan.Nodes.Num()+OutPlan.Boxes.Num()+OutPlan.GetterCopies.Num()+4;++Attempt)
            {
                double Required=OffsetX;
                auto Avoid=[&](double X,double Y,double W,double H)
                {
                    if (Box.PlannedX+OffsetX<X+W && Box.PlannedX+OffsetX+Box.PlannedWidth>X
                        && Box.PlannedY<Y+H && Box.PlannedY+Box.PlannedHeight>Y)
                        Required=FMath::Max(Required,X+W+BoxGap-Box.PlannedX);
                };
                for (const FBox& Other:OutPlan.Boxes) if (!IsChild(Other)) Avoid(Other.PlannedX,Other.PlannedY,Other.PlannedWidth,Other.PlannedHeight);
                for (int32 I=0;I<OutPlan.Nodes.Num();++I)
                { const FNode& N=OutPlan.Nodes[I]; if (!N.bIsComment && !Group.Members.Contains(I)) Avoid(N.PlannedX,N.PlannedY,N.Width,N.Height); }
                for (const FGetterCopy& Copy:OutPlan.GetterCopies)
                {
                    bool Inside=false;
                    for (int32 I:Group.Members) if (Copy.ConsumerPin && Copy.ConsumerPin->GetOwningNode()==OutPlan.Nodes[I].Node) { Inside=true; break; }
                    if (!Inside) Avoid(Copy.PlannedX,Copy.PlannedY,Copy.Width,Copy.Height);
                }
                if (Required<=OffsetX) { Packed=true; break; }
                OffsetX=FMath::CeilToDouble(Required);
            }
            if (!Packed || Box.PlannedX+OffsetX+Box.PlannedWidth>MAX_int32)
            { OutError=TEXT("Nested block packing exceeds its bounded search or coordinate limits; unchanged."); return false; }
            if (OffsetX>0)
            {
                for (const FBox& Other:OutPlan.Boxes) if (!Other.bGenerated)
                    for (const FString& Key:Other.MemberKeys)
                        if (const int32* Index=NodeIndexByKey.Find(Key)) if (Group.Members.Contains(*Index))
                        { OutError=TEXT("Nested block packing would split an existing titled comment; increase VerticalGap or reuse the existing boxes; unchanged."); return false; }
                for (int32 I:Group.Members) OutPlan.Nodes[I].PlannedX+=OffsetX;
                for (FGetterCopy& Copy:OutPlan.GetterCopies)
                    for (int32 I:Group.Members) if (Copy.ConsumerPin && Copy.ConsumerPin->GetOwningNode()==OutPlan.Nodes[I].Node) { Copy.PlannedX+=OffsetX; break; }
                for (FBox& Child:OutPlan.Boxes) if (IsChild(Child)) Child.PlannedX+=OffsetX;
                Box.PlannedX+=OffsetX;
            }
            for (int32 I=0;I<OutPlan.Nodes.Num();++I)
            {
                const FNode& N=OutPlan.Nodes[I]; if (N.bIsComment || Group.Members.Contains(I)) continue;
                if (N.PlannedX<Box.PlannedX+Box.PlannedWidth && N.PlannedX+N.Width>Box.PlannedX
                    && N.PlannedY<Box.PlannedY+Box.PlannedHeight && N.PlannedY+N.Height>Box.PlannedY)
                { OutError=TEXT("Generated comment would contain or overlap an unrelated node; unchanged. Use fewer boxes or a less dense block layout."); return false; }
            }
            for (const FBox& Existing:OutPlan.Boxes)
            {
                const bool bOverlap=Box.PlannedX<Existing.PlannedX+Existing.PlannedWidth && Box.PlannedX+Box.PlannedWidth>Existing.PlannedX
                    && Box.PlannedY<Existing.PlannedY+Existing.PlannedHeight && Box.PlannedY+Box.PlannedHeight>Existing.PlannedY;
                const bool bContains=Box.PlannedX<=Existing.PlannedX && Box.PlannedY<=Existing.PlannedY
                    && Box.PlannedX+Box.PlannedWidth>=Existing.PlannedX+Existing.PlannedWidth && Box.PlannedY+Box.PlannedHeight>=Existing.PlannedY+Existing.PlannedHeight;
                if (bOverlap && (!bContains || Existing.MemberKeys.IsEmpty())) { OutError=TEXT("Generated comment would partially overlap another comment or acquire a fixed empty note; unchanged."); return false; }
            }
            Box.bWouldFit=true; OutPlan.Boxes.Add(MoveTemp(Box));
        }
        for (FBox& Box:OutPlan.Boxes) { Box.ParentIndex=INDEX_NONE; Box.ChildBoxIndices.Reset(); Box.Depth=0; }
        for (int32 I=0;I<OutPlan.Boxes.Num();++I)
        {
            FBox& Inner=OutPlan.Boxes[I]; double Smallest=TNumericLimits<double>::Max();
            for (int32 J=0;J<OutPlan.Boxes.Num();++J) if (I!=J)
            {
                const FBox& Outer=OutPlan.Boxes[J]; const double Area=Outer.PlannedWidth*Outer.PlannedHeight;
                if (Area>Inner.PlannedWidth*Inner.PlannedHeight && Area<Smallest && Inner.PlannedX>=Outer.PlannedX
                    && Inner.PlannedY>=Outer.PlannedY && Inner.PlannedX+Inner.PlannedWidth<=Outer.PlannedX+Outer.PlannedWidth
                    && Inner.PlannedY+Inner.PlannedHeight<=Outer.PlannedY+Outer.PlannedHeight) { Smallest=Area; Inner.ParentIndex=J; }
            }
        }
        for (int32 I=0;I<OutPlan.Boxes.Num();++I)
        {
            const int32 Parent=OutPlan.Boxes[I].ParentIndex;
            if (Parent!=INDEX_NONE) OutPlan.Boxes[Parent].ChildBoxIndices.Add(I);
            for (int32 Ancestor=Parent;Ancestor!=INDEX_NONE;Ancestor=OutPlan.Boxes[Ancestor].ParentIndex) ++OutPlan.Boxes[I].Depth;
        }
    }

	// -----------------------------------------------------------------------------------------
	// STEP 8: the measure, on the PLANNED positions.
	//
	// The reader's `PinPosition` reads a node's STORED coordinates, so it cannot measure a layout that
	// exists only as values. The SAME ARITHMETIC is applied to the planned numbers here, and the suite
	// asserts that the before-value EQUALS what `L-LAYOUT-REPORT` reports for the untouched graph. That
	// cross-tool check is what keeps the two implementations in step across a file boundary.
	// -----------------------------------------------------------------------------------------
	for (FNode& N : OutPlan.Nodes) { N.PlannedX = FMath::RoundToDouble(N.PlannedX); N.PlannedY = FMath::RoundToDouble(N.PlannedY); }
    if (Routing==TEXT("orthogonal") && !PrepareOrthogonalRoutes(OutPlan,ChannelWidth,OutError)) return false;
	auto TotalLengthOn = [&OutPlan](const bool bUsePlanned)
	{
		double Total = 0.0;
        auto PinPosition=[&](const UEdGraphPin* Pin)
        {
            return OutPlan.bCompactBlocks || !OutPlan.WireRoutes.IsEmpty() ? AlignmentPinPosition(Pin) : FTurboMCPLayoutReport::PinPosition(Pin);
        };
		for (int32 WireIndex=0;WireIndex<OutPlan.Wires.Num();++WireIndex)
		{
            const FWire& Wire=OutPlan.Wires[WireIndex];
			if (Wire.OutputPin == nullptr || Wire.InputPin == nullptr)
			{
				continue;
			}
			if (Wire.SourceNodeIndex == INDEX_NONE || Wire.TargetNodeIndex == INDEX_NONE)
			{
				continue;
			}
			const FNode& Source = OutPlan.Nodes[Wire.SourceNodeIndex];
			const FNode& Target = OutPlan.Nodes[Wire.TargetNodeIndex];

			double SourceX = bUsePlanned ? Source.PlannedX : Source.X;
			double SourceY = bUsePlanned ? Source.PlannedY : Source.Y;
            FVector2D SourceOffset = PinPosition(Wire.OutputPin) - FVector2D(Source.X, Source.Y);
			if (bUsePlanned)
			{
				// This consumer will read from its copy, not the original getter.
				for (const FGetterCopy& Copy : OutPlan.GetterCopies)
				{
					if (Copy.OriginalValuePin == Wire.OutputPin && Copy.ConsumerPin == Wire.InputPin)
					{
						SourceX = Copy.PlannedX;
						SourceY = Copy.PlannedY;
                        SourceOffset = Copy.OutputOffset;
						break;
					}
				}
			}
			const double TargetX = bUsePlanned ? Target.PlannedX : Target.X;
			const double TargetY = bUsePlanned ? Target.PlannedY : Target.Y;

			// Use exactly the report pin model, translated to planned positions.

			const FVector2D Start = FVector2D(SourceX, SourceY) + SourceOffset;
			const FVector2D End = PinPosition(Wire.InputPin) + FVector2D(TargetX - Target.X, TargetY - Target.Y);
			for (const auto& Segment:TidyWireSegments(OutPlan,WireIndex,bUsePlanned?1:0,Start,End))
                Total += FVector2D::Distance(Segment.Key,Segment.Value);
		}
		return Total;
	};

	OutPlan.WireLengthBefore = TotalLengthOn(false);
	OutPlan.WireLengthAfter = TotalLengthOn(true);

	for (const FBox& Box : OutPlan.Boxes)
		for (const FString& Member : Box.MemberKeys)
		{
			const int32* Index = NodeIndexByKey.Find(Member);
			if (!Index) { OutError = TEXT("comment membership could not be resolved; tidy refused"); return false; }
			const FNode& N = OutPlan.Nodes[*Index];
            if (N.bIsComment) continue;
            // Existing overflow on an untouched anchor is not caused by this tidy.
            if (N.PlannedX == N.X && N.PlannedY == N.Y && !Box.bWouldFit) continue;
			if (N.PlannedX < Box.PlannedX || N.PlannedY < Box.PlannedY + TitleBarHeight
				|| N.PlannedX + N.Width > Box.PlannedX + Box.PlannedWidth || N.PlannedY + N.Height > Box.PlannedY + Box.PlannedHeight)
			{ OutError = FString::Printf(TEXT("tidy would move node %s outside comment %s; unchanged. Include @unboxed with nodeIds to tidy only unboxed nodes and leave boxed continuations fixed."), *N.Key, *Box.Key); return false; }
		}

    if (!ValidatePlannedNodeOverlaps(OutPlan, OutError)) return false;

	// How many nodes and boxes the plan would actually change.
	for (const FNode& Entry : OutPlan.Nodes)
	{
		if (Entry.bIsComment || Entry.bUnreachable)
		{
			continue;
		}
		if (!FMath::IsNearlyEqual(Entry.PlannedX, Entry.X)
			|| !FMath::IsNearlyEqual(Entry.PlannedY, Entry.Y))
		{
			++OutPlan.NodesToMove;
			OutPlan.NodesToMoveKeys.Add(Entry.Key);
		}
	}
	for (const FBox& Box : OutPlan.Boxes)
	{
		if (Box.bWouldFit)
		{
			++OutPlan.BoxesToFit;
		}
	}

	// When knots are added, the AFTER number includes them. The knot pass is asked where it would put
	// them; whether they help TIDY's metric is decided by the measure below, which is the whole reason
	// the two capabilities are allowed to measure different things (`D-LAY-4`).

	// -----------------------------------------------------------------------------------------
	// STEP 9: better or not.
	//
	// Compare structural quality first; use wire length only when structural metrics tie.
	OutPlan.bAlreadyTidy = OutPlan.NodesToMove == 0 && OutPlan.BoxesToFit == 0
		&& OutPlan.GetterCopies.Num() == 0;
    for (const FWireRoute& Route:OutPlan.WireRoutes)
        if (!Route.bSkipped && !Route.KnotPositions.IsEmpty()) OutPlan.bAlreadyTidy=false;

    for(const FWire& W:OutPlan.Wires)
        if(OutPlan.IsWrapBoundary(W.SourceNodeIndex, W.TargetNodeIndex) && W.OutputPin && !W.bControlFeedback && W.OutputPin->PinType.PinCategory==TEXT("exec"))
        {
            const FNode& WrapSource=OutPlan.Nodes[W.SourceNodeIndex];
            const FNode& WrapTarget=OutPlan.Nodes[W.TargetNodeIndex];
            if(WrapTarget.PlannedY<=WrapSource.PlannedY+WrapSource.Height)
            {
                OutError=FString::Printf(TEXT("WrapAfter %s cannot separate this continuation into a lower row within its current scope/comment boundaries; include the complete continuation or choose another row boundary. Unchanged."),*WrapSource.Key);
                return false;
            }
        }

	if (OutPlan.bAlreadyTidy)
	{
		OutPlan.bWouldApply = false;
		OutPlan.RollbackReason = TEXT("the graph is already tidy: no node would move, no box would ")
			TEXT("change size, and no getter would be split.");
		return true;
	}

	const FStructuralQuality OriginalQuality = StructuralQuality(OutPlan,0);
    const FStructuralQuality PlannedQuality = StructuralQuality(OutPlan,1);
    FString QualityReason;
    OutPlan.bWouldApply = PlannedQuality.Overlaps <= OriginalQuality.Overlaps
        && (BetterLayout(OriginalQuality, PlannedQuality, OutPlan.WireLengthBefore, OutPlan.WireLengthAfter, &QualityReason) || bInForce);
    FString HandoffReason;
    if (FindScopedLoopBodyRegression(OutPlan, 1, HandoffReason) && !bInForce)
    {
        OutPlan.bWouldApply = false;
        OutPlan.RollbackReason = HandoffReason + TEXT(". Force can accept this row tradeoff explicitly; it does not bypass other safety checks.");
    }
    FString BoundaryReason;
    if (!CheckBoundaryGrowth(OutPlan, 1, BoundaryReason))
    {
        OutPlan.bWouldApply = false;
        OutPlan.RollbackReason = BoundaryReason;
    }
	if (!OutPlan.bWouldApply && OutPlan.RollbackReason.IsEmpty())
	{
        OutPlan.RollbackReason = QualityReason.IsEmpty() ? TEXT("candidate introduces node overlaps") : QualityReason;
	}

	return true;
}

// ---------------------------------------------------------------------------------------------
// RenderPreview
// ---------------------------------------------------------------------------------------------

namespace
{
 struct FResolvedTidyRoute
 {
  const FTurboMCPGraphTidy::FWireRoute* Plan = nullptr;
  UEdGraphPin* Source = nullptr;
  UEdGraphPin* Target = nullptr;
 };

 struct FTidyRouteUndo
 {
  TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
  TMap<UEdGraphPin*,UEdGraphPin*> SnappedChildren;
 };

 bool RestoreTidyRouteChains(UEdGraph* Graph, TArray<UK2Node_Knot*>& Created, const FTidyRouteUndo& Undo)
 {
  bool bRestored=true;
  for (UK2Node_Knot* Knot:Created)
  {
   if (Knot) Knot->DestroyNode();
   bRestored &= !Graph->Nodes.Contains(Knot);
  }
  Created.Reset();
  // No original node is reconstructed by existing-edge expansion. Preserve
  // exact fan-out ordering and native snapping relationships on rollback.
  for (const auto& Pair:Undo.Links) Pair.Key->LinkedTo=Pair.Value;
  for (const auto& Pair:Undo.Links) Pair.Key->ClearSnappedChild();
  for (const auto& Pair:Undo.SnappedChildren) Pair.Key->SetSnappedChild(Pair.Value);
  for (const auto& Pair:Undo.Links)
   bRestored &= Pair.Key->LinkedTo==Pair.Value && Pair.Key->GetSnappedChildPin()==Undo.SnappedChildren.FindRef(Pair.Key);
  return bRestored;
 }

 // Expand existing edges only. This does not create new logical connections or
 // ask the schema to insert conversions. The caller owns the graph transaction
 // (or a disposable preview graph) and retains Created for later rollback.
 bool CreateTidyRouteChains(UEdGraph* Graph, const TArray<FResolvedTidyRoute>& Routes,
     EObjectFlags Flags, TArray<UK2Node_Knot*>& Created, FTidyRouteUndo& Undo, FString& Error)
 {
  if (!Graph || !Created.IsEmpty())
  { Error=TEXT("Routing requires a graph and an empty created-node list."); return false; }
  Undo=FTidyRouteUndo();
  if (Routes.IsEmpty()) return true;
  auto& OriginalLinks=Undo.Links;
  auto& OriginalSnappedChildren=Undo.SnappedChildren;
  for (UEdGraphNode* Node:Graph->Nodes)
   if (Node) for (UEdGraphPin* Pin:Node->Pins) if (Pin)
   {
    OriginalLinks.Add(Pin,Pin->LinkedTo);
    if (UEdGraphPin* Child=Pin->GetSnappedChildPin()) OriginalSnappedChildren.Add(Pin,Child);
   }
  TSet<int32> SeenWires;
  TMap<UEdGraphPin*,TSet<UEdGraphPin*>> SeenConnections;
  for (const FResolvedTidyRoute& Route:Routes)
  {
   if (!Route.Plan || Route.Plan->bSkipped || Route.Plan->KnotPositions.IsEmpty()
       || SeenWires.Contains(Route.Plan->WireIndex)
       || !OriginalLinks.Contains(Route.Source) || !OriginalLinks.Contains(Route.Target)
       || Route.Source->Direction!=EGPD_Output || Route.Target->Direction!=EGPD_Input
       || !Route.Source->LinkedTo.Contains(Route.Target) || !Route.Target->LinkedTo.Contains(Route.Source)
       || Route.Source->PinType.PinCategory==TEXT("wildcard"))
   { Error=TEXT("A planned route has missing, repeated, untyped or disconnected endpoints; no route was applied."); return false; }
   if (SeenConnections.FindOrAdd(Route.Source).Contains(Route.Target))
   { Error=TEXT("The same connection was routed twice; no route was applied."); return false; }
   SeenConnections.FindOrAdd(Route.Source).Add(Route.Target);
   SeenWires.Add(Route.Plan->WireIndex);
   for (const FVector2D& P:Route.Plan->KnotPositions)
    if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || P.X<MIN_int32 || P.X>MAX_int32
        || P.Y<MIN_int32 || P.Y>MAX_int32 || P.X!=FMath::RoundToDouble(P.X) || P.Y!=FMath::RoundToDouble(P.Y))
    { Error=TEXT("A route contains invalid stored coordinates; no route was applied."); return false; }
  }
  TMap<UEdGraphPin*,TArray<UEdGraphPin*>> ExpectedLinks=OriginalLinks;
  for (const FResolvedTidyRoute& Route:Routes)
  {
   TArray<UK2Node_Knot*> Chain;
   for (const FVector2D& Position:Route.Plan->KnotPositions)
   {
    UK2Node_Knot* Knot=NewObject<UK2Node_Knot>(Graph,NAME_None,Flags);
    if (!Knot) { Error=TEXT("Could not allocate a reroute node; route expansion stopped."); if (!RestoreTidyRouteChains(Graph,Created,Undo)) Error+=TEXT(" Rollback is incomplete."); return false; }
    Knot->CreateNewGuid(); Graph->AddNode(Knot,false,false); Knot->AllocateDefaultPins();
    Created.Add(Knot); Chain.Add(Knot);
    if (Knot->Pins.Num()!=2 || !Knot->GetInputPin() || !Knot->GetOutputPin())
    { Error=TEXT("A reroute node did not allocate its pins; route expansion stopped."); if (!RestoreTidyRouteChains(Graph,Created,Undo)) Error+=TEXT(" Rollback is incomplete."); return false; }
    Knot->NodePosX=static_cast<int32>(Position.X); Knot->NodePosY=static_cast<int32>(Position.Y);
    Knot->GetInputPin()->PinType=Route.Source->PinType;
    Knot->GetOutputPin()->PinType=Route.Source->PinType;
   }
   Route.Source->BreakLinkTo(Route.Target);
   UEdGraphPin* Previous=Route.Source;
   for (UK2Node_Knot* Knot:Chain)
   {
    Previous->MakeLinkTo(Knot->GetInputPin()); Previous=Knot->GetOutputPin();
   }
   Previous->MakeLinkTo(Route.Target);
   // Replace the original slots rather than appending: output fan-out order
   // can determine execution order. Preserve input ordering as well.
   auto& SourceLinks=ExpectedLinks.FindChecked(Route.Source);
   auto& TargetLinks=ExpectedLinks.FindChecked(Route.Target);
   const int32 SourceSlot=SourceLinks.IndexOfByKey(Route.Target);
   const int32 TargetSlot=TargetLinks.IndexOfByKey(Route.Source);
   if (SourceSlot==INDEX_NONE || TargetSlot==INDEX_NONE)
   { Error=TEXT("The original connection order changed during routing; route expansion stopped."); if (!RestoreTidyRouteChains(Graph,Created,Undo)) Error+=TEXT(" Rollback is incomplete."); return false; }
   SourceLinks[SourceSlot]=Chain[0]->GetInputPin();
   TargetLinks[TargetSlot]=Chain.Last()->GetOutputPin();
   Route.Source->LinkedTo=SourceLinks;
   Route.Target->LinkedTo=TargetLinks;
   for (int32 Index=0;Index<Chain.Num();++Index)
   {
    UEdGraphPin* Upstream=Index==0?Route.Source:Chain[Index-1]->GetOutputPin();
    UEdGraphPin* Downstream=Index+1==Chain.Num()?Route.Target:Chain[Index+1]->GetInputPin();
    if (Chain[Index]->GetInputPin()->LinkedTo!=TArray<UEdGraphPin*>{Upstream}
        || Chain[Index]->GetOutputPin()->LinkedTo!=TArray<UEdGraphPin*>{Downstream})
    { Error=TEXT("Reroute chain read-back failed; route expansion stopped."); if (!RestoreTidyRouteChains(Graph,Created,Undo)) Error+=TEXT(" Rollback is incomplete."); return false; }
   }
  }
  for (const auto& Pair:ExpectedLinks)
   if (Pair.Key->LinkedTo!=Pair.Value)
   { Error=TEXT("Routing changed an unexpected connection; route expansion stopped."); if (!RestoreTidyRouteChains(Graph,Created,Undo)) Error+=TEXT(" Rollback is incomplete."); return false; }
  return true;
 }

 bool RenderNativePreview(const FTurboMCPGraphTidy::FPlan& Plan, FString& Result)
 {
  if (!FSlateApplication::IsInitialized() || !IsInGameThread()) { Result=TEXT("Slate/game thread required"); return false; }
  UEdGraph* Original=nullptr;
  for (const auto& N:Plan.Nodes) if (N.Node) { Original=N.Node->GetGraph(); break; }
  UBlueprint* Blueprint=Original?FBlueprintEditorUtils::FindBlueprintForGraph(Original):nullptr;
  if (!Blueprint) { Result=TEXT("Blueprint graph unavailable"); return false; }
  // Copy only graph objects into a transient Blueprint shell. Duplicating the entire
  // Blueprint would invoke PostDuplicateBlueprint and can compile/reinstance classes.
  TStrongObjectPtr<UBlueprint> Scratch(NewObject<UBlueprint>(GetTransientPackage(),Blueprint->GetClass(),NAME_None,RF_Transient));
  if (!Scratch.IsValid()) { Result=TEXT("could not create transient preview owner"); return false; }
  Scratch->ParentClass=Blueprint->ParentClass;
  Scratch->GeneratedClass=Blueprint->GeneratedClass;
  Scratch->SkeletonGeneratedClass=Blueprint->SkeletonGeneratedClass;
  Scratch->NewVariables=Blueprint->NewVariables;
  TMap<UObject*,UObject*> Created;
  FObjectDuplicationParameters Parameters(Original,Scratch.Get());
  Parameters.ApplyFlags=RF_Transient;
  Parameters.FlagMask &= ~(RF_Public|RF_Standalone|RF_Transactional);
  Parameters.bAssignExternalPackages=false;
  Parameters.DuplicationSeed.Add(Blueprint,Scratch.Get());
  Parameters.CreatedObjects=&Created;
  UEdGraph* Graph=Cast<UEdGraph>(StaticDuplicateObjectEx(Parameters));
  if (!Graph) { Result=TEXT("graph copy failed"); return false; }
  // Function-entry widgets derive their displayed title from the graph name.
  // Keep that name faithful without renaming or dirtying the source asset.
  Graph->Rename(*Original->GetName(),Scratch.Get(),REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
  if(Blueprint->FunctionGraphs.Contains(Original)) Scratch->FunctionGraphs.Add(Graph);
  else if(Blueprint->MacroGraphs.Contains(Original)) Scratch->MacroGraphs.Add(Graph);
  else Scratch->UbergraphPages.Add(Graph);
  TMap<FGuid,UEdGraphNode*> ByGuid;
  for(const auto& N:Plan.Nodes) if(N.Node)
  {
   UEdGraphNode* Copy=Cast<UEdGraphNode>(Created.FindRef(N.Node));
   if(!Copy || Copy->GetGraph()!=Graph) { Result=TEXT("preview node mapping failed"); return false; }
   ByGuid.Add(N.Node->NodeGuid,Copy);
  }
  auto FindPin=[](UEdGraphNode* N,const UEdGraphPin* P)->UEdGraphPin*
  {
   if (!N || !P) return nullptr;
   for (UEdGraphPin* Q:N->Pins) if(Q && Q->PinId==P->PinId) return Q;
   return nullptr;
  };
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("TurboMCP/Previews");
  IFileManager::Get().MakeDirectory(*Folder,true);
  const FString Stem=FGuid::NewGuid().ToString();
  Result=TEXT("Native graph previews (transient copy; no live asset changes):");
  for(int32 Side=0;Side<2;++Side)
  {
   TMap<int32,TArray<UK2Node_Knot*>> PreviewRouteKnots;
   if(Side)
   {
    TMap<const UEdGraphPin*,UEdGraphPin*> ReaderOutputs;
    for(const auto& N:Plan.Nodes) if(N.Node)
     if(UEdGraphNode** Copy=ByGuid.Find(N.Node->NodeGuid)) { (*Copy)->NodePosX=FMath::RoundToInt(N.PlannedX); (*Copy)->NodePosY=FMath::RoundToInt(N.PlannedY); }
    for(const auto& B:Plan.Boxes) if(B.bGenerated)
    {
     UEdGraphNode_Comment* Comment=NewObject<UEdGraphNode_Comment>(Graph,NAME_None,RF_Transient);
     Graph->AddNode(Comment,false,false); Comment->CreateNewGuid(); Comment->NodeComment=B.Text;
     Comment->NodePosX=FMath::RoundToInt(B.PlannedX); Comment->NodePosY=FMath::RoundToInt(B.PlannedY);
     Comment->NodeWidth=FMath::RoundToInt(B.PlannedWidth); Comment->NodeHeight=FMath::RoundToInt(B.PlannedHeight);
    }
    else if(B.Comment)
     if(UEdGraphNode** Copy=ByGuid.Find(B.Comment->NodeGuid)) { (*Copy)->NodePosX=FMath::RoundToInt(B.PlannedX); (*Copy)->NodePosY=FMath::RoundToInt(B.PlannedY); (*Copy)->NodeWidth=FMath::RoundToInt(B.PlannedWidth); (*Copy)->NodeHeight=FMath::RoundToInt(B.PlannedHeight); }
    for(const auto& C:Plan.GetterCopies)
    {
     UEdGraphNode* Source=ByGuid.FindRef(C.Original->NodeGuid);
     UEdGraphPin* Input=FindPin(ByGuid.FindRef(C.ConsumerPin->GetOwningNode()->NodeGuid),C.ConsumerPin);
     UEdGraphPin* SourcePin=FindPin(Source,C.OriginalValuePin);
     if(!Source || !Input || !CanReadValuePin(Source,SourcePin) || Source->GetGraph()!=Graph || Input->GetOwningNode()->GetGraph()!=Graph) { Result=TEXT("getter-copy preview mapping failed"); return false; }
     UEdGraphNode* Copy=DuplicateValueReader(Source,Graph,SourcePin,C.Original->GetGraph()->GetName());
     if(!Copy || !ReaderOutput(Copy,SourcePin)) { Result=TEXT("getter-copy preview has no value pin"); return false; }
     Copy->NodePosX=FMath::RoundToInt(C.PlannedX); Copy->NodePosY=FMath::RoundToInt(C.PlannedY);
     SourcePin->BreakLinkTo(Input); ReaderOutput(Copy,SourcePin)->MakeLinkTo(Input);
     ReaderOutputs.Add(C.ConsumerPin,ReaderOutput(Copy,SourcePin));
    }
    TArray<FResolvedTidyRoute> Routes;
    for (const auto& Route:Plan.WireRoutes)
    {
     if (Route.bSkipped || Route.KnotPositions.IsEmpty()) continue;
     if (!Plan.Wires.IsValidIndex(Route.WireIndex)) { Result=TEXT("preview route index is invalid"); return false; }
     const auto& Wire=Plan.Wires[Route.WireIndex];
     if (!Wire.OutputPin || !Wire.InputPin) { Result=TEXT("preview route pins are missing"); return false; }
     UEdGraphPin* Source=ReaderOutputs.FindRef(Wire.InputPin);
     if (!Source) Source=FindPin(ByGuid.FindRef(Wire.OutputPin->GetOwningNode()->NodeGuid),Wire.OutputPin);
     UEdGraphPin* TargetPin=FindPin(ByGuid.FindRef(Wire.InputPin->GetOwningNode()->NodeGuid),Wire.InputPin);
     Routes.Add({&Route,Source,TargetPin});
    }
    TArray<UK2Node_Knot*> PreviewKnots;
    FTidyRouteUndo PreviewUndo;
    if (!CreateTidyRouteChains(Graph,Routes,RF_Transient,PreviewKnots,PreviewUndo,Result)) return false;
    int32 KnotIndex=0;
    for (const FResolvedTidyRoute& Route:Routes)
     for (int32 I=0;I<Route.Plan->KnotPositions.Num();++I)
      PreviewRouteKnots.FindOrAdd(Route.Plan->WireIndex).Add(PreviewKnots[KnotIndex++]);
   }
   const TSharedRef<SGraphEditor> Editor=SNew(SGraphEditor).GraphToEdit(Graph).IsEditable(false).ShowGraphStateOverlay(false);
   const FVector2D Size(2048,1536);
   TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size,TF_Bilinear,true));
   if(!Target.IsValid()) { Result=TEXT("render target unavailable"); return false; }
   // Slate applies display gamma; keep the destination linear to avoid a second sRGB encode.
   Target->InitCustomFormat(2048,1536,PF_B8G8R8A8,true);
   Target->UpdateResourceImmediate(true);
   FWidgetRenderer Renderer(true);
   // Offscreen graphs have no active-timer tick: populate their node widgets explicitly.
   Editor->GetGraphPanel()->Update();
   Editor->SetViewLocation(FVector2f::ZeroVector,1.0f);
   Editor->SlatePrepass(1.0f);
   Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
   Editor->SlatePrepass(1.0f);
   // A second layout pass settles full-detail desired sizes before framing.
   Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
   for(UEdGraphNode* N:Graph->Nodes) if(N && !Editor->GetGraphPanel()->GetNodeWidgetFromGuid(N->NodeGuid).IsValid())
   { Result=TEXT("native preview node widgets did not populate; no blank image returned"); return false; }
   double Left=0,Top=0,Right=1,Bottom=1; bool First=true;
   for(UEdGraphNode* N:Graph->Nodes) if(N)
   {
    auto Rect=FTurboMCPLayoutReport::RectOf(N);
    FSlateRect Bounds;
    if(Editor->GetBoundsForNode(N,Bounds,0.f)) { Rect.MinX=Bounds.Left; Rect.MinY=Bounds.Top; Rect.MaxX=Bounds.Right; Rect.MaxY=Bounds.Bottom; }
    if(First) { Left=Rect.MinX; Top=Rect.MinY; Right=Rect.MaxX; Bottom=Rect.MaxY; First=false; }
    else { Left=FMath::Min(Left,Rect.MinX); Top=FMath::Min(Top,Rect.MinY); Right=FMath::Max(Right,Rect.MaxX); Bottom=FMath::Max(Bottom,Rect.MaxY); }
   }
   const FVector2D Canvas=Editor->GetGraphPanel()?Editor->GetGraphPanel()->GetTickSpaceGeometry().GetLocalSize():Size;
   auto SaveFrame = [&](const FVector2D& Centre, float Zoom, const FString& Suffix)->bool
   {
    Editor->SetViewLocation(FVector2f(Centre-Canvas/(2*Zoom)),Zoom);
    // A zoom change can replace low-detail node content. Settle desired sizes
    // before reading pixels, otherwise full labels use the overview's outlines.
    Editor->SlatePrepass(1.0f);
    Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
    Editor->SlatePrepass(1.0f);
    Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
    // Match the native capture path: drawing at a new scale requests SVGs,
    // but WidgetRenderer alone does not populate the shared vector atlas.
    FSlateRenderer* SlateRenderer=FSlateApplication::Get().GetRenderer();
    FSlateDrawBuffer& ResourceBuffer=SlateRenderer->AcquireDrawBuffer();
    SlateRenderer->DrawWindows(ResourceBuffer);
    SlateRenderer->ReleaseDrawBuffer(ResourceBuffer);
    Editor->Invalidate(EInvalidateWidgetReason::Paint);
    Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
    TArray<FColor> Pixels; FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
    if(!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags) || Pixels.Num()!=2048*1536)
     return false;
    for(FColor& Pixel:Pixels) Pixel.A=255;
    TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(2048,1536,Pixels,Png);
    const FString Path=Folder/(Stem+Suffix+TEXT(".png"));
    if(Png.IsEmpty() || !FFileHelper::SaveArrayToFile(Png,*Path)) return false;
    Result+=TEXT("\n")+Suffix.RightChop(1)+TEXT(": ")+FPaths::ConvertRelativePathToFull(Path);
    return true;
   };
   const FString SideName=Side?TEXT("after"):TEXT("before");
   const float Wanted=FMath::Min(1.0, FMath::Min(Canvas.X/(Right-Left+160),Canvas.Y/(Bottom-Top+160)));
   float Request=Wanted,Applied=1; FVector2f Offset;
   for(int32 Try=0;Try<64;++Try)
   {
    Editor->SetViewLocation(FVector2f::ZeroVector,Request); Editor->GetViewLocation(Offset,Applied);
    if(Applied<=Wanted+KINDA_SMALL_NUMBER) break;
    Request*=0.9f;
   }
   if(Applied<=Wanted+KINDA_SMALL_NUMBER && Applied>0)
   {
    if(!SaveFrame(FVector2D((Left+Right)/2,(Top+Bottom)/2),Applied,TEXT("-")+SideName))
     { Result+=TEXT("\nOverview image could not be saved."); return false; }
    Result+=FString::Printf(TEXT("\n%s overview zoom %.3f; %s"),*SideName,Applied,
     Plan.bCompactBlocks ? TEXT("use detail tiles to read dense blocks.") : TEXT("overview only; use layoutMode=blocks for readable detail tiles."));
   }
   else if(!Plan.bCompactBlocks)
    { Result=TEXT("graph exceeds native zoom range; use a smaller graph or selected group"); return false; }
   else Result+=TEXT("\n")+SideName+TEXT(" overview exceeds native zoom range; detail tiles follow.");

   if(Plan.bCompactBlocks)
   {
    // Bounds must also come from full-detail widgets, not the overview LOD.
    Editor->SetViewLocation(FVector2f::ZeroVector,1.0f);
    Editor->SlatePrepass(1.0f);
    Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
    Editor->SlatePrepass(1.0f);
    Renderer.DrawWidget(Target.Get(),Editor,Size,0.0f);
    // Detail views use native 1:1 rendering, never enlargement of the overview.
    // Block membership is identical on both sides. Large blocks are tiled with
    // overlap so a long wire cannot force their node labels to become unreadable.
    constexpr int32 MaxDetailTiles=24;
    int32 SavedTiles=0, OmittedTiles=0;
    const double TileWidth=FMath::Max(256.0,static_cast<double>(Canvas.X)-160.0);
    const double TileHeight=FMath::Max(256.0,static_cast<double>(Canvas.Y)-160.0);
    for(int32 Block=0;Block<Plan.BlockCount;++Block)
    {
     TArray<FSlateRect> Rectangles;
     auto AddBounds=[&](UEdGraphNode* Node)
     {
      if(!Node) return;
      FSlateRect Bounds;
      if(Editor->GetBoundsForNode(Node,Bounds,0.f)) Rectangles.Add(Bounds);
     };
     for(const auto& Node:Plan.Nodes)
      if(Node.Node && !Node.bIsComment && Node.BlockIndex==Block) AddBounds(ByGuid.FindRef(Node.Node->NodeGuid));
     // Generated readers have no original GUID. Their consumer still identifies
     // the block; include all new scratch readers attached to that consumer.
     if(Side)
      for(const auto& Copy:Plan.GetterCopies)
       for(const auto& Node:Plan.Nodes)
        if(Node.Node==Copy.ConsumerPin->GetOwningNode() && Node.BlockIndex==Block)
        {
         UEdGraphPin* Input=FindPin(ByGuid.FindRef(Node.Node->NodeGuid),Copy.ConsumerPin);
         if(Input) for(const UEdGraphPin* Link:Input->LinkedTo) if(Link) AddBounds(Link->GetOwningNode());
        }
     if (Side && !Rectangles.IsEmpty())
     {
      double L=Rectangles[0].Left,T=Rectangles[0].Top,R=Rectangles[0].Right,B=Rectangles[0].Bottom;
      for (const FSlateRect& Rect:Rectangles) { L=FMath::Min(L,double(Rect.Left)); T=FMath::Min(T,double(Rect.Top)); R=FMath::Max(R,double(Rect.Right)); B=FMath::Max(B,double(Rect.Bottom)); }
      for (const auto& Pair:PreviewRouteKnots)
      {
       const auto& Wire=Plan.Wires[Pair.Key];
       if (Plan.Nodes[Wire.SourceNodeIndex].BlockIndex!=Block && Plan.Nodes[Wire.TargetNodeIndex].BlockIndex!=Block) continue;
       for (UK2Node_Knot* Knot:Pair.Value)
        if (Knot->NodePosX>=L-80 && Knot->NodePosX<=R+80 && Knot->NodePosY>=T-80 && Knot->NodePosY<=B+80) AddBounds(Knot);
      }
     }
     for (const auto& Box:Plan.Boxes)
     {
      if (!Side && Box.bGenerated) continue;
      int32 TitleBlock=MAX_int32;
      for (const auto& Node:Plan.Nodes)
       if (Node.BlockIndex!=INDEX_NONE && (Box.MemberKeys.Contains(Node.Key)||(Node.Node && Box.MemberKeys.Contains(Node.Node->GetName())))) TitleBlock=FMath::Min(TitleBlock,Node.BlockIndex);
      if (TitleBlock==Block)
      {
       const double X=Side?Box.PlannedX:Box.X,Y=Side?Box.PlannedY:Box.Y,W=Side?Box.PlannedWidth:Box.Width;
       FSlateFontInfo Font=FAppStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>(TEXT("Graph.CommentBlock.TitleInlineEditableText")).TextStyle.Font;
       Font.Size=Box.Comment?Box.Comment->GetFontSize():GetDefault<UEdGraphNode_Comment>()->GetFontSize();
       const FVector2D Caption(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Box.Text,Font));
       // A parent's empty title strip must not consume every child's tile budget.
       // Capture the measured caption once in its first block, including native padding.
       Rectangles.Add(FSlateRect(X,Y,X+FMath::Min(W,Caption.X+48),Y+FMath::Max(FTurboMCPGraphTidy::TitleBarHeight,Caption.Y+16)));
      }
     }
     if(Rectangles.IsEmpty()) continue;
     double MinX=Rectangles[0].Left,MinY=Rectangles[0].Top,MaxX=Rectangles[0].Right,MaxY=Rectangles[0].Bottom;
     for(const FSlateRect& Rect:Rectangles)
     { MinX=FMath::Min(MinX,static_cast<double>(Rect.Left)); MinY=FMath::Min(MinY,static_cast<double>(Rect.Top));
       MaxX=FMath::Max(MaxX,static_cast<double>(Rect.Right)); MaxY=FMath::Max(MaxY,static_cast<double>(Rect.Bottom)); }
     // Bound enumeration as well as image writes. Pathological asset bounds
     // must not make a read-only preview iterate millions of empty grid cells.
     if(!FMath::IsFinite(MaxX-MinX) || !FMath::IsFinite(MaxY-MinY)
        || (MaxX-MinX)/TileWidth>64.0 || (MaxY-MinY)/TileHeight>64.0)
     {
      Result+=FString::Printf(TEXT("\n%s block %d detail omitted: bounds exceed the 64 x 64 tile-grid limit. Preview a smaller selected group; this image set is incomplete."),*SideName,Block);
      continue;
     }
     const int32 Columns=FMath::Max(1,FMath::CeilToInt((MaxX-MinX)/TileWidth));
     const int32 Rows=FMath::Max(1,FMath::CeilToInt((MaxY-MinY)/TileHeight));
     Result+=FString::Printf(TEXT("\n%s block %d: %d node rectangle(s), grid %d x %d; detail zoom 1:1, 80-unit border."),*SideName,Block,Rectangles.Num(),Columns,Rows);
     for(int32 Row=0;Row<Rows;++Row) for(int32 Column=0;Column<Columns;++Column)
     {
      const double X=MinX+Column*TileWidth,Y=MinY+Row*TileHeight;
      // Empty wire-only space is visible in the overview; spend the bounded
      // image budget on node-bearing tiles and report every omitted tile.
      bool bContainsNode=false;
      for(const FSlateRect& Rect:Rectangles)
       if(Rect.Left<X+TileWidth && Rect.Right>=X && Rect.Top<Y+TileHeight && Rect.Bottom>=Y) { bContainsNode=true; break; }
      if(!bContainsNode) continue;
      if(SavedTiles>=MaxDetailTiles) { ++OmittedTiles; continue; }
      const FString Suffix=FString::Printf(TEXT("-%s-block-%d-row-%d-col-%d"),*SideName,Block,Row,Column);
      if(!SaveFrame(FVector2D(X+TileWidth/2,Y+TileHeight/2),1.0f,Suffix))
       { Result+=TEXT("\nDetail tile could not be saved; the preview is incomplete."); return false; }
      ++SavedTiles;
     }
    }
    Result+=FString::Printf(TEXT("\n%s detail tiles: %d saved, %d omitted by the %d-image limit. Long channel knots outside the 80-unit block border are overview-only; empty wire-only tiles are not exported."),*SideName,SavedTiles,OmittedTiles,MaxDetailTiles);
    if(OmittedTiles) Result+=TEXT(" Use a selected-node preview for the omitted blocks; this image set is incomplete.");
   }
  }
  return true;
 }
}

// Suggest a new preview only after a rejected plan or a complete rollback.
// These are alternatives to inspect, not permission to move unselected nodes.
static FString TidyRecoveryGuidance(bool bSelectedScope)
{
    FString Advice = TEXT("Next step: inspect LookTurboMCPLayoutReport and LookTurboMCPGraphPositions for the affected graph.");
    if (bSelectedScope)
        Advice += TEXT(" Selected-node tidy keeps surrounding nodes fixed. If the inserted node has no forward space, preview tidy with the inserted node and the downstream chain selected together, or use PreviewTurboMCPGraphGroupMove to inspect an explicit shift of that chain. Check comment bounds and collisions before applying.");
    else
        Advice += TEXT(" Preview a smaller connected group, or use PreviewTurboMCPGraphGroupMove to inspect an explicit placement change. Check comment bounds and collisions before applying.");
    Advice += TEXT(" Do not enable Force merely to suppress this refusal; Force does not bypass collision or membership guards.");
    return Advice;
}

static FString JoinBoundedTidyLines(const TArray<FString>& Lines)
{
    constexpr int32 MaxCharacters = 32000;
    FString Output;
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        const int32 Added = Lines[Index].Len() + (Output.IsEmpty() ? 0 : 1);
        if (Output.Len() + Added > MaxCharacters)
        {
            Output += FString::Printf(TEXT("\nAdditional Tidy detail omitted after %d of %d sections. Select a smaller graph region or use LookTurboMCPGraphPositions and LookTurboMCPLayoutReport for exact geometry."), Index, Lines.Num());
            break;
        }
        if (!Output.IsEmpty()) Output += TEXT("\n");
        Output += Lines[Index];
    }
    return Output;
}

FString FTurboMCPGraphTidy::RenderPreview(const FPlan& InPlan, int32 GeometryOffset, int32 GeometryLimit, int32 PlanOffset, int32 PlanLimit)
{
	TArray<FString> Lines;
	if (InPlan.bAlreadyTidy)
		Lines.Add(TEXT("ALREADY TIDY: no node would move, no box would change size, and no getter would be split."));
	else if (InPlan.bWouldApply)
		Lines.Add(FString::Printf(TEXT("WOULD APPLY: %d node(s) would move, %d box(es) would be fitted, %d getter(s) would be split."), InPlan.NodesToMove, InPlan.BoxesToFit, InPlan.GetterCopies.Num()));
	else
	{
		Lines.Add(FString::Printf(TEXT("WOULD ROLL BACK: %s"), *InPlan.RollbackReason));
		Lines.Add(TidyRecoveryGuidance(InPlan.SelectedNodeKeys.Num() > 0));
	}
	FString HandoffTradeoff;
	if (InPlan.bForce && FindScopedLoopBodyRegression(InPlan, 1, HandoffTradeoff))
		Lines.Add(TEXT("Force requested for boundary tradeoff: ") + HandoffTradeoff
			+ TEXT(". Fixed-boundary safety and overlap checks still apply."));


    // The decision precedes all measurements. Label total wire length separately
    // from structural and compact scores so callers do not compare unlike values.
    const FStructuralQuality QualityBefore=StructuralQuality(InPlan,0), QualityAfter=StructuralQuality(InPlan,1);
    for(int32 I:InPlan.WrapAfterNodes)
        Lines.Add(FString::Printf(TEXT("row break after %s (execution links preserved)"),*InPlan.Nodes[I].Key));
    for (const FWire& Wire : InPlan.Wires)
        if (InPlan.IsWrapBoundary(Wire.SourceNodeIndex, Wire.TargetNodeIndex))
            Lines.Add(FString::Printf(TEXT("row boundary %s -> %s (connection preserved)"),
                *InPlan.Nodes[Wire.SourceNodeIndex].Key, *InPlan.Nodes[Wire.TargetNodeIndex].Key));
    if (InPlan.bCompactBlocks)
    {
        Lines.Add(CompactMeasureSummary(QualityBefore, QualityAfter));
        Lines.Add(GeometryEvidencePage(InPlan, GeometryOffset, GeometryLimit));
    }
    if (InPlan.bCompactBlocks) Lines.Add(FString::Printf(TEXT("compact diagnostics: Body row drift %.1f -> %.1f; Completed above body bottom %d -> %d. These diagnostics supplement the seven-measure ranking."), QualityBefore.BodyRowDrift, QualityAfter.BodyRowDrift, QualityBefore.CompletedAboveBody, QualityAfter.CompletedAboveBody));
    Lines.Add(FString::Printf(TEXT("quality priority: overlaps %d -> %d; linear exec drift %.1f -> %.1f (16-unit tolerance); fan-out row conflicts %d -> %d; backward exec %d -> %d; backward data %d -> %d; pure input row conflicts %d -> %d; input pin drift %.1f -> %.1f (8-unit tolerance). Wire length breaks structural ties."),QualityBefore.Overlaps,QualityAfter.Overlaps,QualityBefore.LinearExecDrift,QualityAfter.LinearExecDrift,QualityBefore.FanOutRowConflicts,QualityAfter.FanOutRowConflicts,QualityBefore.BackwardExec,QualityAfter.BackwardExec,QualityBefore.BackwardData,QualityAfter.BackwardData,QualityBefore.PureInputRowConflicts,QualityAfter.PureInputRowConflicts,QualityBefore.InputPinDrift,QualityAfter.InputPinDrift));
    if (InPlan.bCompactBlocks)
    {
        Lines.Last().ReplaceInline(TEXT("quality priority:"), TEXT("additional diagnostics (not compact ranking):"));
        Lines.Last().ReplaceInline(TEXT(" Wire length breaks structural ties."), TEXT(""));
    }
	// The ordinary preview retains its existing format. A coordinate page contains
	// only the verdict, scores and numbered plan rows, so a large graph cannot
	// silently lose the tail of its route-knot plan to the response limit.
	if (PlanLimit > 0)
	{
		TArray<FString> Detail;
		for (const FNode& Node : InPlan.Nodes)
			if (!Node.bIsComment)
				Detail.Add(FString::Printf(TEXT("planned %s x=%.0f y=%.0f"), *Node.Key,
					FMath::RoundToDouble(Node.PlannedX), FMath::RoundToDouble(Node.PlannedY)));
		for (int32 Index = 0; Index < InPlan.GetterCopies.Num(); ++Index)
		{
			const FGetterCopy& Copy = InPlan.GetterCopies[Index];
			Detail.Add(FString::Printf(TEXT("planned reader copy index=%d x=%.0f y=%.0f width=%.0f height=%.0f"),
				Index, Copy.PlannedX, Copy.PlannedY, Copy.Width, Copy.Height));
		}
		for (const FBox& Box : InPlan.Boxes)
			if (Box.bGenerated)
				Detail.Add(FString::Printf(TEXT("generated box %s title=\"%s\" x=%.0f y=%.0f width=%.0f height=%.0f members=%s"),
					*Box.Key, *Box.Text, Box.PlannedX, Box.PlannedY, Box.PlannedWidth, Box.PlannedHeight,
					*FString::Join(Box.MemberKeys, TEXT(","))));
		for (const FWireRoute& Route : InPlan.WireRoutes)
			for (int32 Index = 0; Index < Route.KnotPositions.Num(); ++Index)
				Detail.Add(FString::Printf(TEXT("route knot wire=%d index=%d x=%.0f y=%.0f"),
					Route.WireIndex, Index, Route.KnotPositions[Index].X, Route.KnotPositions[Index].Y));
		if (PlanOffset >= Detail.Num() && !Detail.IsEmpty())
			return FString::Printf(TEXT("error: PlanOffset %d is outside the %d planned coordinate rows; no graph edit occurred."), PlanOffset, Detail.Num());
		const int32 End = FMath::Min(PlanOffset + PlanLimit, Detail.Num());
		Lines.Add(FString::Printf(TEXT("plan detail: rows %d..%d of %d for this plan."), PlanOffset, End - 1, Detail.Num()));
		for (int32 Index = PlanOffset; Index < End; ++Index) Lines.Add(Detail[Index]);
		if (End < Detail.Num())
			Lines.Add(FString::Printf(TEXT("plan detail next: PlanOffset=%d PlanLimit=%d. Replan only while the graph is unchanged."), End, PlanLimit));
		return JoinBoundedTidyLines(Lines);
	}
	Lines.Add(FString::Printf(TEXT("preview: %s in %s : total wire length %.1f -> %.1f across %d wire(s)"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireLengthAfter, InPlan.WireCount));
    int32 FeedbackCount = 0;
    for (const FWire& Wire : InPlan.Wires) if (Wire.bControlFeedback) ++FeedbackCount;
    if (FeedbackCount > 0)
        Lines.Add(FString::Printf(TEXT("control feedback: %d stock Break/Reset wire(s) preserved; excluded from forward ordering and alignment penalties, included in total wire length."), FeedbackCount));

	if (InPlan.SelectedNodeKeys.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("scope: %d selected node(s); %d surrounding node(s) stay fixed. Box members stay grouped; selected boxes may grow without moving their corners."),
			InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount));
	}
	else
	{
		Lines.Add(TEXT("scope: whole graph; no fixed-node selection was supplied."));
	}

    for (const FWire& Wire:InPlan.Wires)
        if (InPlan.MergePredecessors.Contains(Wire.TargetNodeIndex) && InPlan.MergePredecessors[Wire.TargetNodeIndex]!=Wire.SourceNodeIndex
            && Wire.OutputPin && Wire.OutputPin->PinType.PinCategory==TEXT("exec"))
            Lines.Add(FString::Printf(TEXT("handoff class=intended rule=R6 source=%s target=%s; fallback returns to the primary merge row; crossings remain defects."),
                *InPlan.Nodes[Wire.SourceNodeIndex].Key,*InPlan.Nodes[Wire.TargetNodeIndex].Key));
    const int32 ShownBlocks = FMath::Min(InPlan.Blocks.Num(), 32);
    for (int32 Block=0;Block<ShownBlocks;++Block)
    {
        double L=TNumericLimits<double>::Max(),T=L,R=-L,B=-L; int32 Count=0;
        for (const FNode& Node:InPlan.Nodes) if (Node.BlockIndex==Block && !Node.bIsComment)
        { ++Count; L=FMath::Min(L,Node.PlannedX); T=FMath::Min(T,Node.PlannedY); R=FMath::Max(R,Node.PlannedX+Node.Width); B=FMath::Max(B,Node.PlannedY+Node.Height); }
        const FBlock& Entry=InPlan.Blocks[Block];
        Lines.Add(FString::Printf(TEXT("block %d kind=%s parent=%d nodes=%d x=%.0f y=%.0f width=%.0f height=%.0f handoff=%s"),
            Block,*Entry.Kind,Entry.ParentIndex,Count,L,T,R-L,B-T,*Entry.Handoff));
    }
    if (ShownBlocks < InPlan.Blocks.Num()) Lines.Add(FString::Printf(TEXT("block detail: %d of %d shown; inspect a smaller selected scope for the rest."), ShownBlocks, InPlan.Blocks.Num()));
    for (int32 I=0;I<InPlan.GetterCopies.Num();++I)
    { const FGetterCopy& Copy=InPlan.GetterCopies[I]; Lines.Add(FString::Printf(TEXT("planned reader copy index=%d x=%.0f y=%.0f width=%.0f height=%.0f"),I,Copy.PlannedX,Copy.PlannedY,Copy.Width,Copy.Height)); }
    for (const FBox& Box:InPlan.Boxes) if (Box.bGenerated)
        Lines.Add(FString::Printf(TEXT("generated box %s title=\"%s\" x=%.0f y=%.0f width=%.0f height=%.0f members=%s"),
            *Box.Key,*Box.Text,Box.PlannedX,Box.PlannedY,Box.PlannedWidth,Box.PlannedHeight,*FString::Join(Box.MemberKeys,TEXT(","))));
    for (const FBox& Box:InPlan.Boxes) if (Box.bGenerated)
        Lines.Add(FString::Printf(TEXT("box nesting key=%s parent=%s depth=%d"),*Box.Key,
            Box.ParentIndex==INDEX_NONE?TEXT("none"):*InPlan.Boxes[Box.ParentIndex].Key,Box.Depth));
    // Empty notes have no geometric membership: never invent an anchor or move them.
    for (const FBox& Note : InPlan.Boxes)
    {
        if (Note.MemberKeys.Num() != 0 || Note.ChildBoxIndices.Num() != 0) continue;
        auto GapToNote = [&Note](const FNode& Candidate, bool bPlanned)
        {
            const double NX = bPlanned ? Candidate.PlannedX : Candidate.X;
            const double NY = bPlanned ? Candidate.PlannedY : Candidate.Y;
            const double DX = FMath::Max(0.0, FMath::Max(Note.X - (NX + Candidate.Width), NX - (Note.X + Note.Width)));
            const double DY = FMath::Max(0.0, FMath::Max(Note.Y - (NY + Candidate.Height), NY - (Note.Y + Note.Height)));
            return FMath::Sqrt(DX * DX + DY * DY);
        };
        const FNode* Nearest = nullptr;
        double BeforeGap = 0.0;
        for (const FNode& Candidate : InPlan.Nodes)
        {
            if (Candidate.bIsComment) continue;
            const double Gap = GapToNote(Candidate, false);
            if (!Nearest || Gap < BeforeGap || (Gap == BeforeGap && Candidate.Key < Nearest->Key))
            {
                Nearest = &Candidate;
                BeforeGap = Gap;
            }
        }
        FString NoteText = Note.Text.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Left(160);
        if (Nearest)
            Lines.Add(FString::Printf(TEXT("empty note %s \"%s\": stays fixed; nearest node before tidy %s; edge distance to that same node %.1f -> %.1f px. Which node should this note describe? Proximity is not an assigned anchor."),
                *Note.Key, *NoteText, *Nearest->Key, BeforeGap, GapToNote(*Nearest, true)));
        else
            Lines.Add(FString::Printf(TEXT("empty note %s \"%s\": stays fixed; no non-comment node is available as an anchor."), *Note.Key, *NoteText));
    }

	if (InPlan.bWouldApply && InPlan.SelectedNodeKeys.Num() > 0)
		Lines.Add(FString::Printf(TEXT("scope moves only: %s"), *FString::Join(InPlan.NodesToMoveKeys, TEXT(", "))));

	if (InPlan.bForce && InPlan.WireLengthAfter >= InPlan.WireLengthBefore && !InPlan.bAlreadyTidy)
	{
		Lines.Add(TEXT("Force is set and wire length is not shorter. Structural metrics above explain whether the layout improves independently of wire length."));
	}

	// Report both positions and sizes so compact comment movement is explicit.
	if (InPlan.Boxes.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("boxes: %d, of which %d would be fitted. Rows/scoped layouts preserve corners; whole-graph blocks mode fits comments to their original members."), InPlan.Boxes.Num(), InPlan.BoxesToFit));
		for (const FBox& Box : InPlan.Boxes)
		{
			const FString Label = Box.Text.IsEmpty() ? Box.Key : Box.Text;
			Lines.Add(FString::Printf(TEXT("  %s at (%.0f,%.0f) -> (%.0f,%.0f) depth %d%s : %.0fx%.0f -> %.0fx%.0f"), *Label, Box.X, Box.Y, Box.PlannedX, Box.PlannedY, Box.Depth, Box.ParentIndex == INDEX_NONE ? TEXT("") : TEXT(" (nested)"), Box.Width, Box.Height, Box.PlannedWidth, Box.PlannedHeight));
		}
	}

	if (InPlan.GetterCopies.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("value readers: %d would be created beside consumers (shared getter copies or function-input getters)."), InPlan.GetterCopies.Num()));
	}

	if (InPlan.UnreachableCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("unreachable: %d node(s) are NOT moved, because disconnected groups are left where they are. R-GRAPH-DEAD removes them."), InPlan.UnreachableCount));
	}

	// `Plan` refuses before reaching this point unless the graph panel has measured every outline.
	Lines.Add(TEXT("geometry: every node has a Slate-measured size; no estimate was used."));


    if (InPlan.bCompactBlocks)
    {
        Lines.Add(TEXT("planned node positions (graph units; compare with post-edit GraphPositions):"));
        for (const FNode& Node : InPlan.Nodes)
            if (!Node.bIsComment)
                Lines.Add(FString::Printf(TEXT("planned %s x=%.0f y=%.0f"), *Node.Key,
                    FMath::RoundToDouble(Node.PlannedX), FMath::RoundToDouble(Node.PlannedY)));
    }

    int32 PlannedKnots=0;
    for (const FWireRoute& Route:InPlan.WireRoutes)
    {
        PlannedKnots+=Route.KnotPositions.Num();
        Lines.Add(FString::Printf(TEXT("route wire %d: %s; %s"),Route.WireIndex,
            Route.bSkipped?TEXT("skipped"):TEXT("planned"),*Route.Reason));
        for (int32 Index=0;Index<Route.KnotPositions.Num();++Index)
            Lines.Add(FString::Printf(TEXT("route knot wire=%d index=%d x=%.0f y=%.0f"),Route.WireIndex,Index,
                Route.KnotPositions[Index].X,Route.KnotPositions[Index].Y));
    }
    if (PlannedKnots>0) Lines.Add(FString::Printf(TEXT("routing: %d planned reroute node(s); edit uses one transaction with movement and comment fitting. No compile or save."),PlannedKnots));
    FString Images;
    if (!RenderNativePreview(InPlan, Images)) Images = TEXT("preview images unavailable: ") + Images;
    Lines.Insert(Images, FMath::Min(5, Lines.Num()));
	return JoinBoundedTidyLines(Lines);
}

FString FTurboMCPGraphTidy::Apply(const FPlan& InPlan)
{

    if (!IsInGameThread() || !GEditor || !GEditor->CanTransact() || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
        return TEXT("unchanged: tidy requires an idle editor with undo available, outside PIE and active transactions");
	// A GRAPH IS NEEDED TO CREATE ANYTHING, and the plan carries the live nodes rather than the graph,
	// so it is taken from the first node that has one.
	UEdGraph* Graph = nullptr;
	for (const FNode& Entry : InPlan.Nodes)
	{
		if (Entry.Node != nullptr)
		{
			Graph = Entry.Node->GetGraph();
			break;
		}
	}
	if (Graph == nullptr)
	{
		return TEXT("error: the plan has no graph to write to. Plan and Apply must be called on the same loaded asset.");
	}
	FString PlannedHandoffReason;
	if (!InPlan.bForce && FindScopedLoopBodyRegression(InPlan, 1, PlannedHandoffReason))
		return TEXT("unchanged: ") + PlannedHandoffReason
			+ TEXT(". No node moved and no link changed. Expand the selection and preview again, or use Force to accept this row tradeoff within the other safety limits.");

	// -----------------------------------------------------------------------------------------
	// NOTHING IS WRITTEN UNTIL THE MEASURE IS IN. The planned positions are VALUES in the plan, the
	// wire length was measured on them in `Plan`, and this function only touches the graph when the
	// result is shorter. THE ORDINARY ROLLBACK IS THEREFORE THAT NO WRITE EVER HAPPENED, which is the
	// strongest form and the reason the plan carries values rather than pointers.
	//
	// The exceptions are the getter copies, which must exist before the measure because they change the
	// wire set. They are created first, measured, and destroyed again if the result is not better - with
	// the links they replaced restored.
	// -----------------------------------------------------------------------------------------

    const auto CrossBoxBefore = FTurboMCPLayoutReport::MeasureCrossBox(Graph);
	if (InPlan.bAlreadyTidy)
	{
		return FString::Printf(TEXT("unchanged: %s in %s is already tidy (total wire length %.1f across %d wire(s)). No node moved, no box changed size and no getter was split."), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, InPlan.WireCount) + FString::Printf(TEXT(" Cross-box estimated length %.1f -> %.1f across %d connections."), CrossBoxBefore.Length, CrossBoxBefore.Length, CrossBoxBefore.Count);
	}

	// THE PLAN IS READ-ONLY, so the copies this function makes are recorded in a LOCAL array. `Plan`
	// decided WHERE each copy goes and wrote nothing; `Apply` is the half that creates, so the handle to
	// each created node lives here rather than being written back into a const plan.
	for (const FGetterCopy& Copy : InPlan.GetterCopies)
	{
		bool bConsumerLive = false;
		for (const UEdGraphNode* Node : Graph->Nodes)
			if (Node && Copy.ConsumerPin && Node->Pins.Contains(Copy.ConsumerPin)) { bConsumerLive = true; break; }
		if (!Copy.OriginalValuePin || !Graph->Nodes.Contains(Copy.Original) || !CanReadValuePin(Copy.Original, Copy.OriginalValuePin) || !bConsumerLive
			|| !Copy.OriginalValuePin->LinkedTo.Contains(Copy.ConsumerPin)
			|| !Copy.ConsumerPin->LinkedTo.Contains(Copy.OriginalValuePin))
			return TEXT("unchanged: getter split plan is stale; preview again before applying");
	}
	const bool bWasDirtyBefore = Graph->GetOutermost()->IsDirty();
	const int32 NodesBefore = Graph->Nodes.Num();
    FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphTidy", "TurboMCP tidy graph"));
    if (!Transaction.IsOutstanding()) return TEXT("unchanged: could not open tidy undo transaction");
    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph)) Blueprint->Modify();
    Graph->Modify();
    // Capture both ends of rewired links as well as moved nodes and comments.
    for (UEdGraphNode* Node : Graph->Nodes) if (Node) Node->Modify();
	TArray<FGetterCopy> Created = InPlan.GetterCopies;
	FString CopyFailure;

	// Create the getter copies. Each one takes over ONE consumer from the original.
	for (FGetterCopy& Entry : Created)
	{
		if (!CanReadValuePin(Entry.Original, Entry.OriginalValuePin) || Entry.ConsumerPin == nullptr)
		{
			CopyFailure = TEXT("a planned getter copy could not be created");
			break;
		}

        UEdGraphNode* Copy = DuplicateValueReader(Entry.Original, Graph, Entry.OriginalValuePin, Entry.Original->GetGraph()->GetName());
        if (!Copy)
        {
            CopyFailure = TEXT("a planned value-reader copy could not be created");
            break;
        }
        Copy->NodePosX = FMath::RoundToInt(Entry.PlannedX);
        Copy->NodePosY = FMath::RoundToInt(Entry.PlannedY);
        UEdGraphPin* CopyValuePin = ReaderOutput(Copy, Entry.OriginalValuePin);
		if (CopyValuePin == nullptr || CopyValuePin->PinType != Entry.OriginalValuePin->PinType)
		{
			// REFUSED RATHER THAN HALF-DONE. A copy with no value pin cannot take the consumer, and
			// destroying the original's link without somewhere to put it would sever the graph. So the
			// copy is removed and the consumer is left exactly as it was.
			Copy->DestroyNode();
			CopyFailure = TEXT("a planned getter copy could not be created");
			break;
		}

		UEdGraphPin* OriginalValuePin = Entry.OriginalValuePin != nullptr
			? Entry.OriginalValuePin : SplitValuePin(Entry.Original);
		if (OriginalValuePin != nullptr)
		{
			OriginalValuePin->BreakLinkTo(Entry.ConsumerPin);
		}
		CopyValuePin->MakeLinkTo(Entry.ConsumerPin);

		Entry.Copy = Copy;
		if (!CopyValuePin->LinkedTo.Contains(Entry.ConsumerPin)
			|| !Entry.ConsumerPin->LinkedTo.Contains(CopyValuePin)
			|| Entry.ConsumerPin->LinkedTo.Contains(OriginalValuePin))
		{
			CopyFailure = TEXT("getter-copy connection read-back failed");
			break;
		}
	}

	// -----------------------------------------------------------------------------------------
	// THE POSITIONS ARE WRITTEN NOW, because the whole point of the pass is the effect of the new
	// positions on the wire length, and every original position is kept so an unfit pass can be put back
	// EXACTLY.
	// -----------------------------------------------------------------------------------------
	TArray<TPair<UEdGraphNode*, TPair<int32, int32>>> OriginalPositions;
	TArray<FString> ActuallyMovedKeys;
	for (const FNode& Entry : InPlan.Nodes)
	{
		if (!CopyFailure.IsEmpty()) break;
		if (Entry.Node == nullptr || Entry.bIsComment || Entry.bUnreachable
			|| (InPlan.SelectedNodeKeys.Num() > 0 && !InPlan.SelectedNodeKeys.Contains(Entry.Key)))
		{
			continue;
		}
		const int32 NewX = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedX));
		const int32 NewY = static_cast<int32>(FMath::RoundToDouble(Entry.PlannedY));
		if (NewX == Entry.Node->NodePosX && NewY == Entry.Node->NodePosY)
		{
			continue;
		}
		OriginalPositions.Add(TPair<UEdGraphNode*, TPair<int32, int32>>(Entry.Node, TPair<int32, int32>(Entry.Node->NodePosX, Entry.Node->NodePosY)));
		ActuallyMovedKeys.Add(Entry.Key);
		Entry.Node->Modify();
		Entry.Node->NodePosX = NewX;
		Entry.Node->NodePosY = NewY;
	}

    FPlan ActualPlan=InPlan;
    TArray<UK2Node_Knot*> RoutingKnots;
    FTidyRouteUndo RoutingUndo;
    if (CopyFailure.IsEmpty())
    {
        TArray<FResolvedTidyRoute> Routes;
        for (const FWireRoute& Route:InPlan.WireRoutes)
        {
            if (Route.bSkipped || Route.KnotPositions.IsEmpty()) continue;
            if (!InPlan.Wires.IsValidIndex(Route.WireIndex)) { CopyFailure=TEXT("Route index is stale."); break; }
            const FWire& Wire=InPlan.Wires[Route.WireIndex];
            UEdGraphPin* Source=Wire.OutputPin;
            for (const FGetterCopy& Copy:Created)
                if (Copy.OriginalValuePin==Source && Copy.ConsumerPin==Wire.InputPin && Copy.Copy)
                { Source=ReaderOutput(Copy.Copy,Copy.OriginalValuePin); break; }
            Routes.Add({&Route,Source,Wire.InputPin});
        }
        if (CopyFailure.IsEmpty() && CreateTidyRouteChains(Graph,Routes,RF_Transactional,RoutingKnots,RoutingUndo,CopyFailure)
            && !Routes.IsEmpty())
        {
            int32 Offset=0;
            for (FWireRoute& Route:ActualPlan.WireRoutes)
            {
                Route.AppliedKnots.Reset();
                if (Route.bSkipped) continue;
                for (int32 Index=0;Index<Route.KnotPositions.Num();++Index) Route.AppliedKnots.Add(RoutingKnots[Offset++]);
            }
            if (PrepareAlignment(FBlueprintEditorUtils::FindBlueprintForGraph(Graph),Graph,CopyFailure))
                for (const FWireRoute& Route:ActualPlan.WireRoutes)
                    for (UK2Node_Knot* Knot:Route.AppliedKnots)
                    {
                        const FVector2D P(Knot->NodePosX,Knot->NodePosY);
                        if (!AlignmentNodeSize(Knot).Equals(Route.KnotSize,1.0)
                            || !(AlignmentPinPosition(Knot->GetInputPin())-P).Equals(Route.InputOffset,1.0)
                            || !(AlignmentPinPosition(Knot->GetOutputPin())-P).Equals(Route.OutputOffset,1.0))
                            CopyFailure=TEXT("Created reroute geometry differs from the measured plan.");
                    }
        }
    }

	// The measure is taken with the SAME function the reader uses, so the two numbers are comparable.
	double MeasuredAfter = 0.0;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}
            FVector2D Start = InPlan.bCompactBlocks || !InPlan.WireRoutes.IsEmpty() ? AlignmentPinPosition(Pin) : FTurboMCPLayoutReport::PinPosition(Pin);
            for (const FGetterCopy& Copy : Created)
                if (Copy.Copy == Node && ReaderOutput(Copy.Copy, Copy.OriginalValuePin) == Pin)
                { Start = FVector2D(Node->NodePosX, Node->NodePosY) + Copy.OutputOffset; break; }
            for (const UEdGraphPin* Other : Pin->LinkedTo)
                if (Other) MeasuredAfter += FVector2D::Distance(Start, InPlan.bCompactBlocks || !InPlan.WireRoutes.IsEmpty() ? AlignmentPinPosition(Other) : FTurboMCPLayoutReport::PinPosition(Other));
		}
	}

	const FStructuralQuality OriginalQuality = StructuralQuality(InPlan,0);
    const FStructuralQuality ActualQuality = StructuralQuality(ActualPlan,2,&Created);
    FString ActualQualityReason;
    const bool bBetter = BetterLayout(OriginalQuality, ActualQuality, InPlan.WireLengthBefore, MeasuredAfter, &ActualQualityReason);
    FString BoundaryReason;
    if (!CheckBoundaryGrowth(InPlan, 2, BoundaryReason, &Created) && CopyFailure.IsEmpty())
        CopyFailure = BoundaryReason;
    FString HandoffReason;
    const bool bHandoffWorse = FindScopedLoopBodyRegression(InPlan, 2, HandoffReason);
    if (bHandoffWorse && !InPlan.bForce && CopyFailure.IsEmpty())
        CopyFailure = HandoffReason;
    if (ActualQuality.Overlaps > OriginalQuality.Overlaps && CopyFailure.IsEmpty())
        CopyFailure = TEXT("post-edit geometry introduced overlaps, including getter copies");
	if (!CopyFailure.IsEmpty() || (!bBetter && !InPlan.bForce))
	{
		// ROLLBACK. The positions are put back EXACTLY, and every copy is destroyed with the consumer it
		// took restored to the original getter. Then the result is RE-MEASURED AND COMPARED, because a
		// rollback whose result is not checked is a rollback nobody knows works.
		for (const auto& Undo : OriginalPositions)
		{
			Undo.Key->NodePosX = Undo.Value.Key;
			Undo.Key->NodePosY = Undo.Value.Value;
		}

        const bool bRoutesRestored=RestoreTidyRouteChains(Graph,RoutingKnots,RoutingUndo);
		int32 Undone = 0;
		int32 RestorationFailed = bRoutesRestored ? 0 : 1;
		for (const FGetterCopy& Entry : Created)
		{
			if (Entry.Copy == nullptr)
			{
				continue;
			}
			if (Entry.OriginalValuePin != nullptr && Entry.ConsumerPin != nullptr)
			{
				// Break the copy's link FIRST, then re-make the original's, so the consumer is never
				// left with two links or with none.
				if (UEdGraphPin* CopyPin = ReaderOutput(Entry.Copy, Entry.OriginalValuePin))
				{
					CopyPin->BreakLinkTo(Entry.ConsumerPin);
				}
				Entry.OriginalValuePin->MakeLinkTo(Entry.ConsumerPin);

				// *** THE RESTORATION IS READ BACK, BECAUSE `MakeLinkTo` RETURNS `void` AND THEREFORE
				// CANNOT REPORT A REFUSAL. The consumer's input must point at the ORIGINAL getter again;
				// when it does not, the rollback left this reader severed and saying "rolled back" would
				// be a restatement of the request rather than a measurement of the result. This is the
				// shape `WidgetRetarget.cpp:355` and `DataTableRows.cpp:574` already use. ***
				if (Entry.ConsumerPin->LinkedTo.Contains(Entry.OriginalValuePin) == false)
				{
					++RestorationFailed;
				}
			}
			else
			{
				++RestorationFailed;
			}
			Entry.Copy->DestroyNode();
			++Undone;
		}
		Graph->MarkPackageDirty();

		double Restored = 0.0;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output || Pin->LinkedTo.Num() == 0)
				{
					continue;
				}
				if (InPlan.WireRoutes.IsEmpty()) Restored += FTurboMCPLayoutReport::WireLength(Pin);
                else for (const UEdGraphPin* Other:Pin->LinkedTo)
                    if (Other) Restored += FVector2D::Distance(AlignmentPinPosition(Pin),AlignmentPinPosition(Other));
			}
		}
		const bool bRestored = FMath::IsNearlyEqual(Restored, InPlan.WireLengthBefore, 1.0);
		bool bPositionsRestored = true;
		for (const auto& Undo : OriginalPositions)
			bPositionsRestored &= Undo.Key->NodePosX == Undo.Value.Key && Undo.Key->NodePosY == Undo.Value.Value;
		const bool bRollbackComplete = bRestored && RestorationFailed == 0
			&& bPositionsRestored && Graph->Nodes.Num() == NodesBefore;
		if (bRollbackComplete && !bWasDirtyBefore) Graph->GetOutermost()->SetDirtyFlag(false);
        // Rebuild widgets from the restored graph, including removed reader copies.
        Graph->NotifyGraphChanged();
		FString Out = FString::Printf(TEXT("%s: %s in %s did not complete an acceptable layout (total wire length %.1f -> %.1f), so rollback was attempted. Preview predicted %.1f; the post-write measurement was %.1f. %d node position(s) and %d getter copy(ies) were undone, and the rollback %s the original length (%.1f)."), bRollbackComplete ? TEXT("unchanged") : TEXT("partial"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, InPlan.WireLengthAfter, MeasuredAfter, OriginalPositions.Num(), Undone, bRestored ? TEXT("RESTORED") : TEXT("DID NOT RESTORE"), Restored);
		if (InPlan.SelectedNodeKeys.Num() > 0)
		{
			Out += FString::Printf(TEXT(" Scope was %d selected node(s); %d surrounding node(s) were fixed anchors. The plan allowed %d comment box(es) to be fitted; restoration status is reported above."), InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount, InPlan.BoxesToFit);
		}

		// *** THE WIRE LENGTH COMING BACK IS NOT THE SAME AS EVERY READER BEING RE-ATTACHED, SO THE
		// SECOND QUESTION IS ASKED SEPARATELY. `bRestored` compares a total; this reports the consumers
		// whose original link was NOT re-made. `MakeLinkTo` returns `void`, so the check is a read of the
		// pin's own link list, taken after the attempt. ***
		if (RestorationFailed > 0)
		{
			Out += FString::Printf(
				TEXT(" *** %d CONSUMER(S) WERE NOT RE-ATTACHED TO THEIR ORIGINAL GETTER: the total length ")
				TEXT("came back but those readers were left severed, which is a defect in this tool and not ")
				TEXT("a clean rollback. ***"), RestorationFailed);
		}
		if (!bBetter && !InPlan.bForce) Out += TEXT(" Quality: ") + ActualQualityReason + TEXT(".");
		if (!CopyFailure.IsEmpty()) Out += TEXT(" Cause: ") + CopyFailure + TEXT(". Force does not bypass safety failures.");
		if (!bRollbackComplete) Out += TEXT(" rollback=incomplete; package remains dirty; inspect before retrying or saving.");
        else if (CopyFailure.IsEmpty()) Out += TEXT(" ") + TidyRecoveryGuidance(InPlan.SelectedNodeKeys.Num() > 0);
        const auto CrossBoxRestored = FTurboMCPLayoutReport::MeasureCrossBox(Graph);
        Out += FString::Printf(TEXT(" Cross-box estimated length before %.1f; after rollback %.1f; connections %d -> %d."), CrossBoxBefore.Length, CrossBoxRestored.Length, CrossBoxBefore.Count, CrossBoxRestored.Count);
		return Out;
	}

	// -----------------------------------------------------------------------------------------
	// Apply comment rectangles only after node placement passes the post-write checks.
    // Modify keeps comment movement and resizing in the same undo transaction.
	// -----------------------------------------------------------------------------------------
	int32 BoxesResized = 0;
	for (const FBox& Box : InPlan.Boxes)
	{
        if (Box.bGenerated)
        {
            UEdGraphNode_Comment* Comment=NewObject<UEdGraphNode_Comment>(Graph,NAME_None,RF_Transactional);
            Graph->AddNode(Comment,false,false); Comment->CreateNewGuid(); Comment->NodeComment=Box.Text;
            Comment->NodePosX=FMath::RoundToInt(Box.PlannedX); Comment->NodePosY=FMath::RoundToInt(Box.PlannedY);
            Comment->NodeWidth=FMath::RoundToInt(Box.PlannedWidth); Comment->NodeHeight=FMath::RoundToInt(Box.PlannedHeight);
            ++BoxesResized; continue;
        }
		if (Box.Comment == nullptr || !Box.bWouldFit)
		{
			continue;
		}
		Box.Comment->Modify();
        Box.Comment->NodePosX = FMath::RoundToInt(Box.PlannedX);
        Box.Comment->NodePosY = FMath::RoundToInt(Box.PlannedY);
		Box.Comment->NodeWidth = static_cast<int32>(FMath::RoundToDouble(Box.PlannedWidth));
		Box.Comment->NodeHeight = static_cast<int32>(FMath::RoundToDouble(Box.PlannedHeight));
		++BoxesResized;
	}

	int32 GettersSplit = 0;
	for (const FGetterCopy& Entry : Created)
	{
		if (Entry.Copy != nullptr)
		{
			++GettersSplit;
		}
	}

    // Routing, placement and reader copies share this transaction. Compilation remains separate.
	Graph->MarkPackageDirty();
    // Direct position/size writes do not notify open graph panels. The default
    // action purges their visual representation and schedules a native rebuild;
    // do this once after all nodes, copies and comment sizes are final.
    Graph->NotifyGraphChanged();

	TArray<FString> Lines;

	const bool bWorse = MeasuredAfter >= InPlan.WireLengthBefore;
	const bool bScoreMatchesPlan = FMath::IsNearlyEqual(MeasuredAfter, InPlan.WireLengthAfter, 1.0);
	Lines.Add(FString::Printf(TEXT("changed: tidied %s in %s : total wire length %.1f -> %.1f%s"), *InPlan.GraphName, *InPlan.Asset, InPlan.WireLengthBefore, MeasuredAfter, bWorse ? (InPlan.bForce ? TEXT(" (longer wires; Force enabled)") : TEXT(" (longer wires; structural quality improved)")) : TEXT("")));
	if (bHandoffWorse && InPlan.bForce)
		Lines.Add(TEXT("Force accepted boundary tradeoff: ") + HandoffReason
			+ TEXT(". The surrounding nodes remained fixed."));
    int32 FeedbackCount = 0;
    for (const FWire& Wire : InPlan.Wires) if (Wire.bControlFeedback) ++FeedbackCount;
    if (FeedbackCount > 0)
        Lines.Add(FString::Printf(TEXT("control feedback: %d stock Break/Reset wire(s) preserved; excluded from forward ordering and alignment penalties, included in total wire length."), FeedbackCount));
	Lines.Add(FString::Printf(TEXT("wire length: current plan predicted %.1f; post-edit measured %.1f (%s)."), InPlan.WireLengthAfter, MeasuredAfter, bScoreMatchesPlan ? TEXT("matches the current plan") : TEXT("differs from the current plan")));
    if (InPlan.bCompactBlocks) Lines.Add(CompactMeasureSummary(OriginalQuality, ActualQuality));
    if (InPlan.bCompactBlocks) Lines.Add(FString::Printf(TEXT("compact diagnostics: Body row drift %.1f -> %.1f; Completed above body bottom %d -> %d. These diagnostics supplement the seven-measure ranking."), OriginalQuality.BodyRowDrift, ActualQuality.BodyRowDrift, OriginalQuality.CompletedAboveBody, ActualQuality.CompletedAboveBody));
    Lines.Add(FString::Printf(TEXT("quality priority: overlaps %d -> %d; linear exec drift %.1f -> %.1f (16-unit tolerance); fan-out row conflicts %d -> %d; backward exec %d -> %d; backward data %d -> %d; pure input row conflicts %d -> %d; input pin drift %.1f -> %.1f (8-unit tolerance). Wire length breaks structural ties."),OriginalQuality.Overlaps,ActualQuality.Overlaps,OriginalQuality.LinearExecDrift,ActualQuality.LinearExecDrift,OriginalQuality.FanOutRowConflicts,ActualQuality.FanOutRowConflicts,OriginalQuality.BackwardExec,ActualQuality.BackwardExec,OriginalQuality.BackwardData,ActualQuality.BackwardData,OriginalQuality.PureInputRowConflicts,ActualQuality.PureInputRowConflicts,OriginalQuality.InputPinDrift,ActualQuality.InputPinDrift));
    if (InPlan.bCompactBlocks)
    {
        Lines.Last().ReplaceInline(TEXT("quality priority:"), TEXT("additional diagnostics (not compact ranking):"));
        Lines.Last().ReplaceInline(TEXT(" Wire length breaks structural ties."), TEXT(""));
    }
    const auto CrossBoxAfter = FTurboMCPLayoutReport::MeasureCrossBox(Graph);
    Lines.Add(FString::Printf(TEXT("cross-box: estimated straight-line length %.1f -> %.1f (delta %+.1f); connections %d -> %d. Ownership uses the smallest enclosing comment at each state; copies or box growth can change the count."), CrossBoxBefore.Length, CrossBoxAfter.Length, CrossBoxAfter.Length-CrossBoxBefore.Length, CrossBoxBefore.Count, CrossBoxAfter.Count));
	Lines.Add(FString::Printf(TEXT("moved %d node(s). Fitted %d of %d box(es), using the planned comment rectangles."), OriginalPositions.Num(), BoxesResized, InPlan.Boxes.Num()));
	if (InPlan.SelectedNodeKeys.Num() > 0)
	{
		Lines.Add(FString::Printf(TEXT("scope: only the %d selected node(s) were eligible to move; %d surrounding node(s) remained fixed anchors. Fitted %d comment box(es) to the planned rectangles. Existing selected boxes retained their top-left positions; surrounding boxes stayed unchanged."), InPlan.SelectedNodeKeys.Num(), InPlan.AnchorCount, BoxesResized));
		Lines.Add(FString::Printf(TEXT("scope moved: %s"), *FString::Join(ActuallyMovedKeys, TEXT(", "))));
	}
	if (GettersSplit > 0)
	{
		Lines.Add(FString::Printf(TEXT("split %d variable getter(s), one copy per extra consumer, placed beside the node that reads it."), GettersSplit));
	}
	if (InPlan.UnreachableCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("left %d unreachable node(s) in place. Use PreviewTurboMCPGraphDead to inspect disconnected groups before deciding whether to remove them."), InPlan.UnreachableCount));
	}
	Lines.Add(TEXT("geometry: every node had a Slate-measured size; no estimate was used."));
	if (RoutingKnots.IsEmpty()) Lines.Add(TEXT("No reroute nodes were added."));
    else Lines.Add(FString::Printf(TEXT("routing: added %d reroute node(s); original logical connections preserved through checked chains; included in the same undo transaction."),RoutingKnots.Num()));
	Lines.Add(TEXT("undo=one; compile=not_run; save=not_requested"));
	Lines.Add(TEXT("Changes are in memory only. Use EditTurboMCPSave when ready to save the asset."));

	return FString::Join(Lines, TEXT("\n"));
}

// Reject newly planned intersections without changing the live graph or plan.
bool FTurboMCPGraphTidy::ValidatePlannedNodeOverlaps(const FPlan& InPlan, FString& OutError)
{
    OutError.Reset();
    for (int32 A=0; A<InPlan.Nodes.Num(); ++A)
    for (int32 B=A+1; B<InPlan.Nodes.Num(); ++B)
    {
        const FNode& L=InPlan.Nodes[A]; const FNode& R=InPlan.Nodes[B];
        if (L.bIsComment || R.bIsComment) continue;
        const bool Moved=L.PlannedX!=L.X || L.PlannedY!=L.Y || R.PlannedX!=R.X || R.PlannedY!=R.Y;
        if (Moved && L.PlannedX<R.PlannedX+R.Width && L.PlannedX+L.Width>R.PlannedX && L.PlannedY<R.PlannedY+R.Height && L.PlannedY+L.Height>R.PlannedY)
        {
            OutError = FString::Printf(TEXT("Planned tidy overlaps node rectangles; nothing changed.\nnode_a: %s; planned rectangle: x=%.1f y=%.1f width=%.1f height=%.1f\nnode_b: %s; planned rectangle: x=%.1f y=%.1f width=%.1f height=%.1f\nInspect these node IDs with LookTurboMCPGraphPositions or LookTurboMCPLayoutReport. Try a smaller tidy selection around the conflicting nodes. This is the first detected pair, not a complete overlap list."),
                *L.Key, L.PlannedX, L.PlannedY, L.Width, L.Height,
                *R.Key, R.PlannedX, R.PlannedY, R.Width, R.Height);
            return false;
        }
    }
    return true;
}

bool FTurboMCPGraphTidy::PrepareOrthogonalRoutes(FPlan& Plan, double Clearance, FString& Error)
{
    Plan.WireRoutes.Reset();
    if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress()
        || !FMath::IsFinite(Clearance) || Clearance<20 || Clearance>400)
    { Error=TEXT("Route planning requires an idle editor and clearance from 20 to 400."); return false; }
    UEdGraph* Graph=nullptr;
    for (const FNode& Node:Plan.Nodes) if (Node.Node) { Graph=Node.Node->GetGraph(); break; }
    UBlueprint* Blueprint=Graph?FBlueprintEditorUtils::FindBlueprintForGraph(Graph):nullptr;
    if (!Blueprint || !Graph->GetSchema() || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    { Error=TEXT("Route planning requires a Blueprint K2 graph."); return false; }

    TGuardValue<ITransaction*> SuppressUndo(GUndo,nullptr);
    TStrongObjectPtr<UBlueprint> Scratch(NewObject<UBlueprint>(GetTransientPackage(),NAME_None,RF_Transient));
    Scratch->ParentClass=Blueprint->ParentClass;
    Scratch->GeneratedClass=Blueprint->GeneratedClass;
    Scratch->SkeletonGeneratedClass=Blueprint->SkeletonGeneratedClass;
    UEdGraph* ProbeGraph=NewObject<UEdGraph>(Scratch.Get(),NAME_None,RF_Transient);
    ProbeGraph->Schema=UEdGraphSchema_K2::StaticClass();
    Scratch->UbergraphPages.Add(ProbeGraph);
    UK2Node_Knot* Probe=NewObject<UK2Node_Knot>(ProbeGraph,NAME_None,RF_Transient);
    ProbeGraph->AddNode(Probe,false,false); Probe->CreateNewGuid(); Probe->AllocateDefaultPins();
    FString ProbeError;
    const bool bMeasured=PrepareAlignment(Scratch.Get(),ProbeGraph,ProbeError);
    FVector2D Size=FVector2D::ZeroVector,Input=Size,Output=Size;
    if (bMeasured)
    {
        Size=AlignmentNodeSize(Probe);
        Input=AlignmentPinPosition(Probe->GetInputPin())-FVector2D(Probe->NodePosX,Probe->NodePosY);
        Output=AlignmentPinPosition(Probe->GetOutputPin())-FVector2D(Probe->NodePosX,Probe->NodePosY);
    }
    // The probe replaces the shared Slate measurement cache. Restore the source
    // graph even when probing fails, before any caller reads its pin offsets.
    if (!PrepareAlignment(Blueprint,Graph,Error)) return false;
    if (!bMeasured) { Error=TEXT("Could not measure native reroute geometry: ")+ProbeError; return false; }
    return PlanOrthogonalRoutes(Plan,Size,Input,Output,Clearance,Error);
}

// Internal routing stage. Public Tidy still leaves routing disabled until these
// plans are consumed by scratch preview, live apply, scoring and exact rollback.
bool FTurboMCPGraphTidy::PlanOrthogonalRoutes(FPlan& Plan, const FVector2D& KnotSize,
    const FVector2D& InputOffset, const FVector2D& OutputOffset, double Clearance, FString& Error)
{
    Error.Reset();
    Plan.WireRoutes.Reset();
    if (Plan.Nodes.Num()>512 || Plan.Wires.Num()>4096)
    { Error=TEXT("Orthogonal routing supports at most 512 nodes and 4096 logical wires per graph; use RouteWires=off for larger graphs."); return false; }
    if (!FMath::IsFinite(Clearance) || Clearance < 20 || Clearance > 400
        || !FMath::IsFinite(KnotSize.X) || !FMath::IsFinite(KnotSize.Y) || KnotSize.X <= 0 || KnotSize.Y <= 0
        || !FMath::IsFinite(InputOffset.X) || !FMath::IsFinite(InputOffset.Y)
        || !FMath::IsFinite(OutputOffset.X) || !FMath::IsFinite(OutputOffset.Y))
    { Error=TEXT("Orthogonal routing requires measured knot geometry and clearance from 20 to 400."); return false; }
    struct FObstacle { double L,T,R,B; int32 NodeIndex; };
    auto Crosses=[](const FVector2D& A,const FVector2D& B,const FObstacle& Rect)
    {
        double Enter=0,Leave=1;
        auto Clip=[&](double Start,double Delta,double Min,double Max)
        {
            if(FMath::Abs(Delta)<0.000001) return Start>Min && Start<Max;
            double T0=(Min-Start)/Delta,T1=(Max-Start)/Delta;
            if(T0>T1) Swap(T0,T1);
            Enter=FMath::Max(Enter,T0); Leave=FMath::Min(Leave,T1); return Enter<Leave;
        };
        return Clip(A.X,B.X-A.X,Rect.L,Rect.R) && Clip(A.Y,B.Y-A.Y,Rect.T,Rect.B);
    };
    // Route lower fan-out destinations first. Their long drop belongs nearest
    // the source; upper destinations can then use an outer channel without
    // crossing the lower route. This changes planning order, never link order.
    TArray<int32> RouteOrder;
    for(int32 I=0;I<Plan.Wires.Num();++I) RouteOrder.Add(I);
    RouteOrder.StableSort([&](int32 L,int32 R)
    {
        const FWire& A=Plan.Wires[L]; const FWire& B=Plan.Wires[R];
        if(A.SourceNodeIndex!=B.SourceNodeIndex) return A.SourceNodeIndex<B.SourceNodeIndex;
        if(Plan.Nodes.IsValidIndex(A.TargetNodeIndex) && Plan.Nodes.IsValidIndex(B.TargetNodeIndex))
        {
            const double AY=Plan.Nodes[A.TargetNodeIndex].PlannedY;
            const double BY=Plan.Nodes[B.TargetNodeIndex].PlannedY;
            if(AY!=BY) return AY>BY;
        }
        return L<R;
    });
    for(int32 WireIndex:RouteOrder)
    {
        const FWire& Wire=Plan.Wires[WireIndex];
        if(!Wire.OutputPin || !Wire.InputPin || !Plan.Nodes.IsValidIndex(Wire.SourceNodeIndex)
            || !Plan.Nodes.IsValidIndex(Wire.TargetNodeIndex)) continue;
        const FNode& Source=Plan.Nodes[Wire.SourceNodeIndex]; const FNode& Target=Plan.Nodes[Wire.TargetNodeIndex];
        FWireRoute Route; Route.WireIndex=WireIndex; Route.KnotSize=KnotSize;
        Route.InputOffset=InputOffset; Route.OutputOffset=OutputOffset;
        if(!Plan.SelectedNodeKeys.IsEmpty() && (!Plan.SelectedNodeKeys.Contains(Source.Key) || !Plan.SelectedNodeKeys.Contains(Target.Key)))
        { Route.bSkipped=true; Route.Reason=TEXT("Fixed boundary link retained."); Plan.WireRoutes.Add(MoveTemp(Route)); continue; }
        if(Source.bUnreachable || Target.bUnreachable || Source.bIsKnot || Target.bIsKnot)
        { Route.bSkipped=true; Route.Reason=TEXT("Existing reroute or unreachable endpoint retained."); Plan.WireRoutes.Add(MoveTemp(Route)); continue; }
        FVector2D A=AlignmentPinPosition(Wire.OutputPin)+FVector2D(Source.PlannedX-Source.X,Source.PlannedY-Source.Y);
        const FVector2D B=AlignmentPinPosition(Wire.InputPin)+FVector2D(Target.PlannedX-Target.X,Target.PlannedY-Target.Y);
        // Copies have their own obstacle identity: the original reader is no
        // longer the source of this connection once its copy is planned.
        int32 SourceObstacleIndex=Wire.SourceNodeIndex;
        for(int32 CopyIndex=0;CopyIndex<Plan.GetterCopies.Num();++CopyIndex)
        {
            const FGetterCopy& Copy=Plan.GetterCopies[CopyIndex];
            if(Copy.OriginalValuePin==Wire.OutputPin && Copy.ConsumerPin==Wire.InputPin)
            {
                A=FVector2D(Copy.PlannedX,Copy.PlannedY)+Copy.OutputOffset;
                SourceObstacleIndex=Plan.Nodes.Num()+CopyIndex;
            }
        }
        TArray<FObstacle> Obstacles;
        for(int32 I=0;I<Plan.Nodes.Num();++I)
        {
            const FNode& Node=Plan.Nodes[I]; if(Node.bIsComment) continue;
            Obstacles.Add({Node.PlannedX-8,Node.PlannedY-8,Node.PlannedX+Node.Width+8,Node.PlannedY+Node.Height+8,I});
        }
        for(int32 CopyIndex=0;CopyIndex<Plan.GetterCopies.Num();++CopyIndex)
        {
            const FGetterCopy& Copy=Plan.GetterCopies[CopyIndex];
            Obstacles.Add({Copy.PlannedX-8,Copy.PlannedY-8,Copy.PlannedX+Copy.Width+8,Copy.PlannedY+Copy.Height+8,Plan.Nodes.Num()+CopyIndex});
        }
        // Later routes must leave room for knots already accepted by this plan.
        for(const FWireRoute& Accepted:Plan.WireRoutes)
            for(const FVector2D& Position:Accepted.KnotPositions)
                Obstacles.Add({Position.X-8,Position.Y-8,Position.X+Accepted.KnotSize.X+8,Position.Y+Accepted.KnotSize.Y+8,INDEX_NONE});
        for(const FBox& Box:Plan.Boxes)
        {
            auto Owns=[&](const FNode& Node) { return Box.MemberKeys.Contains(Node.Key) || (Node.Node && Box.MemberKeys.Contains(Node.Node->GetName())); };
            if(!Owns(SourceObstacleIndex>=Plan.Nodes.Num()?Target:Source) && !Owns(Target)) Obstacles.Add({Box.PlannedX,Box.PlannedY,Box.PlannedX+Box.PlannedWidth,Box.PlannedY+Box.PlannedHeight,INDEX_NONE});
        }
        bool bBlocked=false;
        for(const FObstacle& Obstacle:Obstacles)
            if(Obstacle.NodeIndex!=SourceObstacleIndex && Obstacle.NodeIndex!=Wire.TargetNodeIndex && Crosses(A,B,Obstacle)) { bBlocked=true; break; }
        const double DX=FMath::Abs(B.X-A.X),DY=FMath::Abs(B.Y-A.Y);
        const bool bExecutionHandoff=Wire.OutputPin->PinType.PinCategory==TEXT("exec") && DY>16;
        if(!bBlocked && !bExecutionHandoff && !(FVector2D::Distance(A,B)>800 && DX>1 && DY>1)) continue;
        TArray<TArray<FVector2D>> Candidates;
        const FVector2D OffsetDelta=OutputOffset-InputOffset;
        // Derive narrow channels from actual obstacle edges as well as the
        // requested pitch. Fixed multiples alone miss the few clear graph units
        // between a controller's right edge and the first input reader.
        TArray<double> VerticalChannels{A.X+Clearance*0.5,A.X+Clearance*0.75,A.X+Clearance,A.X+Clearance*1.25,A.X+Clearance*1.5,A.X+Clearance*2,B.X-Clearance-OffsetDelta.X};
        for (const FObstacle& Obstacle:Obstacles)
        {
            for (double X:{Obstacle.R+InputOffset.X+1,Obstacle.L-KnotSize.X+InputOffset.X-1})
                if (X>FMath::Min(A.X,B.X) && X<FMath::Max(A.X,B.X)) VerticalChannels.AddUnique(X);
        }
        // Two knots form an exact vertical segment despite their distinct pins.
        for(double X:VerticalChannels)
            Candidates.Add({FVector2D(X,A.Y)-InputOffset,FVector2D(X+OffsetDelta.X,B.Y-OffsetDelta.Y)-InputOffset});
        TArray<double> Channels{FMath::Min(A.Y,B.Y)-Clearance,FMath::Max(A.Y,B.Y)+Clearance};
        for(const FObstacle& Obstacle:Obstacles)
        { Channels.AddUnique(Obstacle.T-Clearance); Channels.AddUnique(Obstacle.B+Clearance); }
        for(double Y:Channels)
        {
            const FVector2D P0=FVector2D(A.X+Clearance,A.Y)-InputOffset;
            const FVector2D P1=FVector2D((P0+OutputOffset).X,Y)-InputOffset;
            const FVector2D P2=FVector2D(B.X-Clearance-OffsetDelta.X,(P1+OutputOffset).Y)-InputOffset;
            const FVector2D P3=FVector2D((P2+OutputOffset).X,B.Y-OffsetDelta.Y)-InputOffset;
            Candidates.Add({P0,P1,P2,P3});
        }
        int32 BestCrossings=MAX_int32,BestSplineCrossings=MAX_int32;
        double BestLength=TNumericLimits<double>::Max();
        auto ProperCrossing=[](const FVector2D& P,const FVector2D& Q,const FVector2D& R,const FVector2D& S)
        {
            auto Cross=[](const FVector2D& U,const FVector2D& V){return U.X*V.Y-U.Y*V.X;};
            const double C0=Cross(Q-P,R-P),C1=Cross(Q-P,S-P),C2=Cross(S-R,P-R),C3=Cross(S-R,Q-R);
            return ((C0>0.001 && C1<-0.001)||(C0<-0.001 && C1>0.001))
                && ((C2>0.001 && C3<-0.001)||(C2<-0.001 && C3>0.001));
        };
        auto SplineSegments=[](const FVector2D& P,const FVector2D& Q)
        {
            TArray<TPair<FVector2D,FVector2D>> Segments;
            const FVector2D Tangent(GetDefault<UGraphEditorSettings>()->ComputeSplineTangent(P,Q));
            FVector2D Previous=P;
            for (int32 Step=1;Step<=16;++Step)
            {
                const double T=Step/16.0,T2=T*T,T3=T2*T;
                const FVector2D Next=P*(2*T3-3*T2+1)+Tangent*(T3-2*T2+T)+Q*(-2*T3+3*T2)+Tangent*(T3-T2);
                Segments.Emplace(Previous,Next); Previous=Next;
            }
            return Segments;
        };
        TArray<TPair<FVector2D,FVector2D>> OtherSplines;
        if (Wire.OutputPin->PinType.PinCategory==TEXT("exec"))
            for (int32 OtherIndex=0;OtherIndex<Plan.Wires.Num();++OtherIndex)
            {
                const FWire& Other=Plan.Wires[OtherIndex];
                if (OtherIndex==WireIndex || !Other.OutputPin || !Other.InputPin || Other.OutputPin->PinType.PinCategory!=TEXT("exec")
                    || !Plan.Nodes.IsValidIndex(Other.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(Other.TargetNodeIndex)) continue;
                const FNode& OS=Plan.Nodes[Other.SourceNodeIndex]; const FNode& OT=Plan.Nodes[Other.TargetNodeIndex];
                const FVector2D OA=AlignmentPinPosition(Other.OutputPin)+FVector2D(OS.PlannedX-OS.X,OS.PlannedY-OS.Y);
                const FVector2D OB=AlignmentPinPosition(Other.InputPin)+FVector2D(OT.PlannedX-OT.X,OT.PlannedY-OT.Y);
                for (const auto& Segment:TidyWireSegments(Plan,OtherIndex,1,OA,OB)) OtherSplines.Append(SplineSegments(Segment.Key,Segment.Value));
            }
        for(TArray<FVector2D> Candidate:Candidates)
        {
            // Evaluate the integer coordinates that Unreal will actually store.
            bool bRepresentable=true;
            for(FVector2D& Position:Candidate)
            {
                if(!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y)
                    || Position.X<MIN_int32 || Position.X>MAX_int32
                    || Position.Y<MIN_int32 || Position.Y>MAX_int32)
                { bRepresentable=false; break; }
                Position.X=FMath::RoundToDouble(Position.X);
                Position.Y=FMath::RoundToDouble(Position.Y);
            }
            if(!bRepresentable) continue;
            bool bClear=true; int32 Crossings=0,SplineCrossings=0; double Length=0; FVector2D Previous=A;
            for(int32 I=0;I<=Candidate.Num() && bClear;++I)
            {
                const FVector2D Next=I<Candidate.Num()?Candidate[I]+InputOffset:B;
                // Rounding can introduce at most one unit of pin-axis drift.
                if ((I==0 && Next.X<A.X-1) || (I==Candidate.Num() && Previous.X>B.X+1)) bClear=false;
                if(FMath::Abs(Next.X-Previous.X)>1 && FMath::Abs(Next.Y-Previous.Y)>1)
                    bClear=false;
                for(int32 Other=0;Other<Candidate.Num() && bClear;++Other)
                {
                    if(Other==I || Other==I-1) continue;
                    const FVector2D P=Candidate[Other];
                    if(Crosses(Previous,Next,{P.X-8,P.Y-8,P.X+KnotSize.X+8,P.Y+KnotSize.Y+8,INDEX_NONE})) bClear=false;
                }
                for(const FObstacle& Obstacle:Obstacles)
                {
                    if((I==0 && Obstacle.NodeIndex==SourceObstacleIndex) || (I==Candidate.Num() && Obstacle.NodeIndex==Wire.TargetNodeIndex)) continue;
                    if(Crosses(Previous,Next,Obstacle)) { bClear=false; break; }
                }
                // Prefer channels that do not cross other execution paths.
                // Previously shortest-path selection could introduce a crossing
                // while avoiding a node, especially on Sequence lower outputs.
                if(Wire.OutputPin->PinType.PinCategory==TEXT("exec"))
                    for(int32 OtherIndex=0;OtherIndex<Plan.Wires.Num();++OtherIndex)
                    {
                        if(OtherIndex==WireIndex) continue;
                        const FWire& Other=Plan.Wires[OtherIndex];
                        if(!Other.OutputPin || !Other.InputPin || Other.OutputPin->PinType.PinCategory!=TEXT("exec")
                            || !Plan.Nodes.IsValidIndex(Other.SourceNodeIndex) || !Plan.Nodes.IsValidIndex(Other.TargetNodeIndex)) continue;
                        const FNode& OS=Plan.Nodes[Other.SourceNodeIndex]; const FNode& OT=Plan.Nodes[Other.TargetNodeIndex];
                        const FVector2D OA=AlignmentPinPosition(Other.OutputPin)+FVector2D(OS.PlannedX-OS.X,OS.PlannedY-OS.Y);
                        const FVector2D OB=AlignmentPinPosition(Other.InputPin)+FVector2D(OT.PlannedX-OT.X,OT.PlannedY-OT.Y);
                        for(const auto& Segment:TidyWireSegments(Plan,OtherIndex,1,OA,OB))
                            if(ProperCrossing(Previous,Next,Segment.Key,Segment.Value)) ++Crossings;
                    }
                // Native reroute splines retain horizontal tangents on a vertical
                // drop. Break straight-score ties using their sampled curves so
                // adjacent clear channels do not still cross in the screenshot.
                if (!OtherSplines.IsEmpty()) for (const auto& Curve:SplineSegments(Previous,Next))
                    for (const auto& Other:OtherSplines) if (ProperCrossing(Curve.Key,Curve.Value,Other.Key,Other.Value)) ++SplineCrossings;
                Length+=FVector2D::Distance(Previous,Next);
                if(I<Candidate.Num())
                {
                    const FVector2D P=Candidate[I];
                    for(int32 Other=0;Other<I;++Other)
                    {
                        const FVector2D Q=Candidate[Other];
                        if(P.X<Q.X+KnotSize.X+8 && P.X+KnotSize.X+8>Q.X
                            && P.Y<Q.Y+KnotSize.Y+8 && P.Y+KnotSize.Y+8>Q.Y) bClear=false;
                    }
                    if(!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || P.X<MIN_int32 || P.X>MAX_int32 || P.Y<MIN_int32 || P.Y>MAX_int32) bClear=false;
                    for(const FObstacle& Obstacle:Obstacles)
                        if(P.X<Obstacle.R && P.X+KnotSize.X>Obstacle.L && P.Y<Obstacle.B && P.Y+KnotSize.Y>Obstacle.T) { bClear=false; break; }
                    Previous=P+OutputOffset;
                }
            }
            if(bClear && (Crossings<BestCrossings || (Crossings==BestCrossings
                && (SplineCrossings<BestSplineCrossings || (SplineCrossings==BestSplineCrossings && Length<BestLength)))))
            { BestCrossings=Crossings; BestSplineCrossings=SplineCrossings; BestLength=Length; Route.KnotPositions=Candidate; }
        }
        Route.bSkipped=Route.KnotPositions.IsEmpty();
        Route.Reason=Route.bSkipped?TEXT("No collision-free orthogonal candidate; original wire retained."):TEXT("Measured orthogonal channel; preserves the original logical connection.");
        Plan.WireRoutes.Add(MoveTemp(Route));
    }
    return true;
}
